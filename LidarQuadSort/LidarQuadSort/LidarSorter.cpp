//
//  LidarSorter.cpp
//  LidarQuadSort
//
//  Created by Steve Gifford on 4/8/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#include "LidarSorter.hpp"

LidarMultiWrapper::LidarMultiWrapper(const std::string &file)
: reader(NULL)
{
    files.push_back(file);
}

LidarMultiWrapper::LidarMultiWrapper(const std::vector<std::string> &files)
: files(files), reader(NULL)
{
}

LidarMultiWrapper::~LidarMultiWrapper()
{
    if (reader)
    {
        laszip_close_reader(reader);
        laszip_destroy(reader);
    }
    reader = NULL;
}

void LidarMultiWrapper::removeFile()
{
    for (const auto &file : files)
        remove(file.c_str());
}

long long getNumRecords(laszip_header_struct *header)
{
    if (header->extended_number_of_point_records > 0)
        return header->extended_number_of_point_records;
    return header->number_of_point_records;
}

long long getNumRecords(laszip_POINTER reader)
{
    laszip_header *theHeader;
    
    laszip_get_header_pointer(reader,&theHeader);
    return getNumRecords(theHeader);
}

long long getNumRecords(laszip_header_struct &header)
{
    return getNumRecords(&header);
}

// Generate a proj4 compatible string
bool GenerateProjStr(laszip_header_struct *thisHeader,std::string &str)
{
    ST_TIFF *tags = ST_Create();
    
    // Work through the VLRS
    // Heavily inpsired by liblas (spatialreference.cpp)
    for (int ii = 0; ii< thisHeader->number_of_variable_length_records;ii++)
    {
        laszip_vlr_struct *vlrs = &thisHeader->vlrs[ii];
        std::string uid = vlrs->user_id;
        
        if (uid == "LASF_Projection")
        {
            std::vector<uint8_t> data(vlrs->record_length_after_header);
            memcpy(&data.front(),vlrs->data,vlrs->record_length_after_header);

            switch (vlrs->record_id)
            {
                case 34735:
                {
                    int count = vlrs->record_length_after_header/sizeof(int16_t);
                    short *data_short = reinterpret_cast<short *>(&data.front());
                    
                    // Apparently there are blank geotags
                    while ( count > 4 &&
                            data_short[count-1] == 0 && data_short[count-2] == 0 &&
                            data_short[count-3] == 0 && data_short[count-4] == 0)
                    {
                        count -= 4;
                        data_short[3] -= 1;
                    }

                    ST_SetKey(tags, vlrs->record_id, count, STT_SHORT, data_short);
                }
                    break;
                case 34736:
                {
                    int count = (int)(data.size()/sizeof(double));
                    ST_SetKey(tags, vlrs->record_id, count, STT_DOUBLE, &data.front());
                }
                    break;
                case 34737:
                {
                    int count = (int)(data.size()/sizeof(uint8_t));
                    ST_SetKey(tags, vlrs->record_id, count, STT_ASCII, &data.front());
                }
                    break;
            }
        }
    }
    
    // Now for something that works with proj4
    GTIFDefn defn;
    GTIF *geoTags = GTIFNewSimpleTags(tags);
    if (GTIFGetDefn(geoTags, &defn))
    {
        char *proj4 = GTIFGetProj4Defn(&defn);
        str = std::string(proj4);
        GTIFFreeMemory(proj4);
        return true;
    }
    GTIFFree(geoTags);
    ST_Destroy(tags);
    
    return false;
}

bool LidarMultiWrapper::init()
{
    valid = false;
    
    bool firstFile = true;
    for (auto &fileName : files)
    {
        laszip_POINTER thisReader;
        laszip_create(&thisReader);
        laszip_BOOL is_compressed;
        if (laszip_open_reader(thisReader, fileName.c_str(), &is_compressed))
        {
            fprintf(stderr,"Failed to open file %s",fileName.c_str());
            return false;
        }
        laszip_header_struct *thisHeader;
        laszip_get_header_pointer(thisReader,&thisHeader);
        
        GenerateProjStr(thisHeader,projStr);
        
        if (firstFile)
        {
            header = *thisHeader;
            firstFile = false;
        } else {
            std::string thisProjStr;
            GenerateProjStr(thisHeader,thisProjStr);
            if (projStr != thisProjStr)
            {
                fprintf(stderr,"Projection doesn't match for all input files.\n");
                return false;
            }
            
            if (header.x_offset != thisHeader->x_offset ||
                header.y_offset != thisHeader->y_offset ||
                header.z_offset != thisHeader->z_offset)
            {
                fprintf(stderr,"Offset doesn't match for all input files.\n");
                return false;
            }
            if (header.x_scale_factor != thisHeader->x_scale_factor ||
                header.y_scale_factor != thisHeader->y_scale_factor ||
                header.z_scale_factor != thisHeader->z_scale_factor)
            {
                fprintf(stderr,"Scale doesn't match for all input files.\n");
                return false;
            }
            
            // Merge in bounding box
            double minX,minY,maxX,maxY,minZ,maxZ;
            minX = std::min(header.min_x,thisHeader->min_x);
            minY = std::min(header.min_y,thisHeader->min_y);
            minZ = std::min(header.min_z,thisHeader->min_z);
            maxX = std::max(header.max_x,thisHeader->max_x);
            maxY = std::max(header.max_y,thisHeader->max_y);
            maxZ = std::max(header.max_z,thisHeader->max_z);
            header.min_x = minX;  header.min_y = minY;  header.min_z = minZ;
            header.max_x = maxX;  header.max_y = maxY;  header.max_z = maxZ;
            
            // Add in the point count
            header.extended_number_of_point_records = getNumRecords(header)+getNumRecords(thisHeader);
            header.number_of_point_records = (laszip_U32)header.extended_number_of_point_records;
            if (header.number_of_point_records != header.extended_number_of_point_records)
                header.number_of_point_records = 0;
        }
    }
    
    fprintf(stdout,"Checked all %ld files for %lld points\n",files.size(),getNumRecords(header));
    
    valid = true;    
    whichFile = -1;
    whichPointInFile = 0;
    whichPointOverall = 0;
    
    return valid;
}

bool LidarMultiWrapper::hasNextPoint()
{
    return whichPointOverall < getNumRecords(header);
}

laszip_point_struct *LidarMultiWrapper::getNextPoint()
{
    // Open the next (or first) file
    if (whichFile < 0 || whichPointInFile >= getNumRecords(reader))
    {
        whichFile++;
        if (reader)
        {
            laszip_close_reader(reader);
            laszip_destroy(reader);
            reader = NULL;
        }
        whichPointInFile = 0;

        const std::string &fileName = files[whichFile];

        laszip_create(&reader);
        laszip_BOOL is_compressed;
        if (laszip_open_reader(reader, fileName.c_str(), &is_compressed))
        {
            throw (std::string)"failed to open file " + fileName;
        }
    }
    
    whichPointInFile++;
    whichPointOverall++;
    if (laszip_read_point(reader))
        throw "Unable to read input point";

    laszip_point_struct *p;
    laszip_get_point_pointer(reader, &p);
    return p;
}

LidarSorter::LidarSorter(const char *tmp_dir)
: tmpDir(tmp_dir), minPointLimit(1000), maxPointLimit(1500), totalWrittenPoints(0),maxLevel(0), maxColor(0)
{
}

bool LidarSorter::process(LidarMultiWrapper *inputDB,LidarDatabase *lidarDB)
{
    fullMinX = inputDB->header.min_x;
    fullMinY = inputDB->header.min_y;
    fullMaxX = inputDB->header.max_x;
    fullMaxY = inputDB->header.max_y;
    
    bool ret = process(inputDB,TileIdent(0,0,0),lidarDB,false);
    
    return ret;
}

bool LidarSorter::process(LidarMultiWrapper *inputDB,TileIdent tileID,LidarDatabase *lidarDB,bool removeAfterDone)
{
    try {
        std::string proj4Str = inputDB->getProj4Str();
        
        // Tile output
        std::stringstream *ofs = NULL;
        ofs = new std::stringstream(std::stringstream::out);
        if (!(*ofs))
        {
            fprintf(stderr,"Unable to open string stream.");
            return false;
        }
        laszip_POINTER tileW;
        laszip_create(&tileW);
        laszip_set_header(tileW,&inputDB->header);
        laszip_open_stream_writer(tileW,ofs,true);
        
        // Figure out which points we're keeping and which we're outputting
        bool allPoints = getNumRecords(inputDB->header) <= maxPointLimit;
        float fracToKeep = (float)minPointLimit / (float)getNumRecords(inputDB->header);

        laszip_POINTER subTiles[4] = {NULL,NULL,NULL,NULL};
        long long subTileCount[4] = {0,0,0,0};
        TileIdent subTileIDs[4];
        std::string subTileNames[4];
        
        double spanX = (fullMaxX-fullMinX)/(1<<tileID.z);
        double spanY = (fullMaxY-fullMinY)/(1<<tileID.z);
        double tileXmin = spanX * tileID.x + fullMinX, tileXmax = spanX * (tileID.x+1) + fullMinX;
        double tileYmin = spanY * tileID.y + fullMinY, tileYmax = spanY * (tileID.y+1) + fullMinY;
        double spanX_2 = spanX/2.0;
        double spanY_2 = spanY/2.0;
        
        if (!allPoints)
        {
            for (unsigned int sy=0;sy<2;sy++)
                for (unsigned int sx=0;sx<2;sx++)
                {
                    TileIdent &subIdent = subTileIDs[sy*2+sx];
                    subIdent.x = 2*tileID.x + sx;  subIdent.y = 2*tileID.y + sy;  subIdent.z = tileID.z+1;

                    // Set up the write for this sub-tile
                    std::string subFile = tmpDir + "/" + "src_" + std::to_string(subIdent.x) + "_" + std::to_string(subIdent.y) + "_" + std::to_string(subIdent.z) + ".las";
                    subTileNames[sy*2+sx] = subFile;

                    laszip_POINTER subW;
                    laszip_create(&subW);
                    laszip_set_header(subW, &inputDB->header);

                    laszip_open_writer(subW, subFile.c_str(), false);
                    laszip_header_struct *subHeader;
                    laszip_get_header_pointer(subW, &subHeader);

                    subHeader->number_of_point_records = 0;
                    subHeader->extended_number_of_point_records = 0;
                    subHeader->number_of_point_records = 0;
                    subHeader->min_x = tileXmin;
                    subHeader->min_y = tileYmin;
                    subHeader->min_z = inputDB->header.min_z;
                    subHeader->max_x = tileXmax;
                    subHeader->max_y = tileYmax;
                    subHeader->max_z = inputDB->header.max_z;

                    subTiles[sy*2+sx] = subW;
                }
        }
        
        // Work through the points in the input file
        long long numToCopy = getNumRecords(inputDB->header);
        long long numCopiedToTile = 0;
        for (long long ii=0;ii<numToCopy;ii++)
        {
            laszip_point_struct *p = inputDB->getNextPoint();
            if (inputDB->header.point_data_format > 2)
                maxColor = std::max(std::max(std::max(std::max(maxColor,(int)p->rgb[0]),(int)p->rgb[1]),(int)p->rgb[2]),(int)p->rgb[3]);
            double randNum = drand48();
            bool tilePoint = randNum <= fracToKeep;
            // This point goes out to the tile
            if (tilePoint || allPoints)
            {
                if (laszip_set_point(tileW,p) ||
                    laszip_write_point(tileW) ||
                    laszip_update_inventory(tileW))
                    throw (std::string)"Failed to write point in tile";
                numCopiedToTile++;
                totalWrittenPoints++;
            } else {
                double x = p->X * inputDB->header.x_scale_factor + inputDB->header.x_offset;
                double y = p->Y * inputDB->header.y_scale_factor + inputDB->header.y_offset;
                // This point goes in one of the subtiles
                int whichX = (x-tileXmin)/spanX_2;
                int whichY = (y-tileYmin)/spanY_2;
                // Shouldn't be necessary, but you can't be too careful
                whichX = std::min(whichX,1); whichY = std::min(whichY,1);
                whichX = std::max(whichX,0); whichY = std::max(whichY,0);
                
                int whichTile = whichY*2+whichX;
                laszip_POINTER w = subTiles[whichTile];
                subTileCount[whichTile]++;
                if (laszip_set_point(w, p) ||
                    laszip_write_point(w) ||
                    laszip_update_inventory(w))
                    throw (std::string)"Failed to write point in sub tile";
            }
        }
        
        std::string indent = "";
        for (int ii=0;ii<tileID.z;ii++)
            indent += " ";
        fprintf(stdout,"%sTile %d: (%d,%d) saved %lld of %llu points\n",indent.c_str(),tileID.z,tileID.x,tileID.y,numCopiedToTile,getNumRecords(inputDB->header));

        // Save the tile and close out the in-memory tile file
        {
            laszip_header_struct *header;
            laszip_get_header_pointer(tileW, &header);
            header->number_of_point_records = (laszip_U32)numCopiedToTile;
            laszip_close_writer(tileW);
            laszip_destroy(tileW);
        }
        std::string tileStr = ofs->str();
        lidarDB->addTile(tileStr.c_str(), (int)tileStr.size(), tileID.x, tileID.y, tileID.z);
        delete ofs;
        
        // Close down the subtiles
        for (unsigned int ii=0;ii<4;ii++)
            if (subTiles[ii])
            {
                laszip_POINTER lasFile = subTiles[ii];
                
                laszip_close_writer(lasFile);
                laszip_destroy(lasFile);
                
                // Remove the file if there were no points
                if (subTileCount[ii] == 0)
                {
                    std::remove(subTileNames[ii].c_str());
                    subTileNames[ii] = "";
                }
            }
        
        // Now keep going recursively
        if (!allPoints)
            for (unsigned int sy=0;sy<2;sy++)
                for (unsigned int sx=0;sx<2;sx++)
                {
                    TileIdent subIdent(2*tileID.x + sx,2*tileID.y + sy,tileID.z+1);
                    
                    std::string subFile = subTileNames[sy*2+sx];
                    if (!subFile.empty())
                    {
                        LidarMultiWrapper subWrap(subFile);
                        if (!subWrap.init())
                            throw (std::string)"Failed to read temp tile file " + std::to_string(subIdent.z) + ": (" + std::to_string(subIdent.x) + "," + std::to_string(subIdent.y) + ")";
                        if (!process(&subWrap,subIdent,lidarDB,true))
                            throw (std::string)"Failed to write tile " + std::to_string(subIdent.z) + ": (" + std::to_string(subIdent.x) + "," + std::to_string(subIdent.y) + ")";
                    }
                }
        
        // Note: Remove the starting file if we need to
        if (removeAfterDone)
            inputDB->removeFile();
        
        maxLevel = std::max(maxLevel,tileID.z);

        // If this is the top file, we'll set up the output header
        if (!removeAfterDone)
        {
            std::string proj4Str = inputDB->getProj4Str();
            lidarDB->setHeader(proj4Str.c_str(),inputDB->header.system_identifier,
                               inputDB->header.min_x, inputDB->header.min_y, inputDB->header.min_z,
                               inputDB->header.max_x, inputDB->header.max_y, inputDB->header.max_z,
                               0, maxLevel,
                               minPointLimit,maxPointLimit,
                               (int)inputDB->header.point_data_format,maxColor);
        }
    }
    catch (const std::string &reason)
    {
        fprintf(stderr,"%s\n",reason.c_str());
        return false;
    }
    
    return true;
}
