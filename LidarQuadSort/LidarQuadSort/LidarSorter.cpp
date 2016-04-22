//
//  LidarSorter.cpp
//  LidarQuadSort
//
//  Created by Steve Gifford on 4/8/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#include "LidarSorter.hpp"

LidarMultiWrapper::LidarMultiWrapper(const std::string &file)
: reader(NULL), ifs(NULL)
{
    files.push_back(file);
}

LidarMultiWrapper::LidarMultiWrapper(const std::vector<std::string> &files)
: files(files), reader(NULL), ifs(NULL)
{
}

LidarMultiWrapper::~LidarMultiWrapper()
{
    if (reader)
        delete reader;
    reader = NULL;
    if (ifs)
        delete ifs;
    ifs = NULL;
}

void LidarMultiWrapper::removeFile()
{
    for (const auto &file : files)
        remove(file.c_str());
}

bool LidarMultiWrapper::init()
{
    valid = false;
    
    bool firstFile = true;
    for (auto &fileName : files)
    {
        // Open the file and take a look at the header
        std::ifstream ifs;
        ifs.open(fileName, std::ios::in | std::ios::binary);
        if (!ifs)
        {
            fprintf(stderr,"Failed to open file %s",fileName.c_str());
            return false;
        }
        
        liblas::ReaderFactory f;
        liblas::Reader thisReader = f.CreateWithStream(ifs);
        liblas::Header thisHeader = thisReader.GetHeader();
        
        if (firstFile)
        {
            header = thisHeader;
            firstFile = false;
        } else {
            // Basic comparison should pass
            if (header.GetSRS().GetProj4() != thisHeader.GetSRS().GetProj4())
            {
                fprintf(stderr,"Projection doesn't match for all input files.\n");
                return false;
            }
            if (header.GetOffsetX() != thisHeader.GetOffsetX() ||
                header.GetOffsetY() != thisHeader.GetOffsetY() ||
                header.GetOffsetZ() != thisHeader.GetOffsetZ())
            {
                fprintf(stderr,"Offset doesn't match for all input files.\n");
                return false;
            }
            if (header.GetScaleX() != thisHeader.GetScaleX() ||
                header.GetScaleY() != thisHeader.GetScaleY() ||
                header.GetScaleZ() != thisHeader.GetScaleZ())
            {
                fprintf(stderr,"Scale doesn't match for all input files.\n");
                return false;
            }
            
            // Merge in bounding box
            double minX,minY,maxX,maxY,minZ,maxZ;
            minX = std::min(header.GetMinX(),thisHeader.GetMinX());
            minY = std::min(header.GetMinY(),thisHeader.GetMinY());
            minZ = std::min(header.GetMinZ(),thisHeader.GetMinZ());
            maxX = std::max(header.GetMaxX(),thisHeader.GetMaxX());
            maxY = std::max(header.GetMaxY(),thisHeader.GetMaxY());
            maxZ = std::max(header.GetMaxZ(),thisHeader.GetMaxZ());
            header.SetMin(minX, minY, minZ);
            header.SetMax(maxX, maxY, maxZ);
            
            // Add in the point count
            header.SetPointRecordsCount(header.GetPointRecordsCount()+thisHeader.GetPointRecordsCount());
        }
    }
    
    fprintf(stdout,"Checked all %ld files for %u points\n",files.size(),header.GetPointRecordsCount());
    
    valid = true;    
    whichFile = -1;
    whichPointInFile = 0;
    whichPointOverall = 0;
    
    return valid;
}

bool LidarMultiWrapper::hasNextPoint()
{
    return whichPointOverall < header.GetPointRecordsCount();
}

liblas::Point LidarMultiWrapper::getNextPoint()
{
    // Open the next (or first) file
    if (whichFile < 0 || whichPointInFile >= reader->GetHeader().GetPointRecordsCount())
    {
        whichFile++;
        if (reader)
        {
            delete reader;
            reader = NULL;
        }
        whichPointInFile = 0;

        const std::string &fileName = files[whichFile];

        // Open the file and take a look at the header
        ifs = new std::ifstream(fileName, std::ios::in | std::ios::binary);
        if (!(*ifs))
        {
            throw (std::string)"Failed to open file " + fileName;
        }

        liblas::ReaderFactory f;
        liblas::Reader thisReader = f.CreateWithStream(*ifs);
        reader = new liblas::Reader(thisReader);
    }
    
    whichPointInFile++;
    whichPointOverall++;
    if (!reader->ReadNextPoint())
        throw "Unable to read input point";
    return reader->GetPoint();
}

LidarSorter::LidarSorter(const char *tmp_dir)
: tmpDir(tmp_dir), minPointLimit(1000), maxPointLimit(1500), totalWrittenPoints(0),maxLevel(0)
{
}

bool LidarSorter::process(LidarMultiWrapper *inputDB,LidarDatabase *lidarDB)
{
    bool ret = process(inputDB,TileIdent(0,0,0),lidarDB,false);
    
    return ret;
}

bool LidarSorter::process(LidarMultiWrapper *inputDB,TileIdent tileID,LidarDatabase *lidarDB,bool removeAfterDone)
{
    try {
        liblas::SpatialReference srs = inputDB->header.GetSRS();
        
        // Tile output
        std::stringstream *ofs = NULL;
        liblas::Writer *tileW = NULL;
        liblas::Header tileHeader = inputDB->header;
        tileHeader.SetPointRecordsCount(0);
        tileHeader.SetCompressed(true);
        ofs = new std::stringstream(std::stringstream::out);
        if (!(*ofs))
        {
            fprintf(stderr,"Unable to open string stream.");
            return false;
        }
        tileW = new liblas::Writer(*ofs,tileHeader);
        
        // Figure out which points we're keeping and which we're outputting
        bool allPoints = inputDB->header.GetPointRecordsCount() <= maxPointLimit;
        float fracToKeep = (float)minPointLimit / (float)inputDB->header.GetPointRecordsCount();

        liblas::Writer *subTiles[4] = {NULL,NULL,NULL,NULL};
        std::ofstream *subOfStreams[4] = {NULL,NULL,NULL,NULL};
        int subTileCount[4] = {0,0,0,0};
        TileIdent subTileIDs[4];
        std::string subTileNames[4];
        double spanX_2 = (inputDB->header.GetMaxX()-inputDB->header.GetMinX())/2.0;
        double spanY_2 = (inputDB->header.GetMaxY()-inputDB->header.GetMinY())/2.0;
        if (!allPoints)
        {
            for (unsigned int sy=0;sy<2;sy++)
                for (unsigned int sx=0;sx<2;sx++)
                {
                    TileIdent &subIdent = subTileIDs[sy*2+sx];
                    subIdent.x = 2*tileID.x + sx;  subIdent.y = 2*tileID.y + sy;  subIdent.z = tileID.z+1;

                    liblas::Header subHeader = inputDB->header;
                    subHeader.SetPointRecordsCount(0);
                    subHeader.SetCompressed(false);
                    subHeader.SetMin(inputDB->header.GetMinX()+sx*spanX_2, inputDB->header.GetMinY()+sy*spanY_2, inputDB->header.GetMinZ());
                    subHeader.SetMax(inputDB->header.GetMinX()+(sx+1)*spanX_2, inputDB->header.GetMinY()+(sy+1)*spanY_2, inputDB->header.GetMaxZ());

                    // Set up the write for this sub-tile
                    std::string subFile = tmpDir + "/" + "src_" + std::to_string(subIdent.x) + "_" + std::to_string(subIdent.y) + "_" + std::to_string(subIdent.z) + ".las";
                    std::ofstream *subofs = new std::ofstream(subFile,std::ios::out | std::ios::binary);
                    subTileNames[sy*2+sx] = subFile;
                    liblas::Writer *subW = new liblas::Writer(*subofs,subHeader);
                    subOfStreams[sy*2+sx] = subofs;
                    subTiles[sy*2+sx] = subW;
                }
        }
        
        // Work through the points in the input file
        uint32_t numToCopy = inputDB->header.GetPointRecordsCount();
        uint32_t numCopiedToTile = 0;
        for (unsigned int ii=0;ii<numToCopy;ii++)
        {
            liblas::Point p = inputDB->getNextPoint();
            double randNum = drand48();
            bool tilePoint = randNum <= fracToKeep;
            // This point goes out to the tile
            if (tilePoint || allPoints)
            {
                if (!tileW->WritePoint(p))
                    throw (std::string)"Failed to write point in tile";
                numCopiedToTile++;
                totalWrittenPoints++;
            } else {
                double x = p.GetX(), y = p.GetY();
                // This point goes in one of the subtiles
                int whichX = (x-inputDB->header.GetMinX())/spanX_2;
                int whichY = (y-inputDB->header.GetMinY())/spanY_2;
                // Shouldn't be necessary, but you can't be too careful
                whichX = std::min(whichX,1); whichY = std::min(whichY,1);
                whichX = std::max(whichX,0); whichY = std::max(whichY,0);
                
                int whichTile = whichY*2+whichX;
                liblas::Writer *w = subTiles[whichTile];
                subTileCount[whichTile]++;
                if (!w->WritePoint(p))
                    throw (std::string)"Failed to write point in sub tile";
            }
        }
        
        std::string indent = "";
        for (int ii=0;ii<tileID.z;ii++)
            indent += " ";
        fprintf(stdout,"%sTile %d: (%d,%d) saved %d of %u points\n",indent.c_str(),tileID.z,tileID.x,tileID.y,numCopiedToTile,inputDB->header.GetPointRecordsCount());

        // Save the tile and close out the in-memory tile file
        delete tileW;
        std::string tileStr = ofs->str();
        lidarDB->addTile(tileStr.c_str(), (int)tileStr.size(), tileID.x, tileID.y, tileID.z);
        delete ofs;
        
        // Close down the subtiles
        for (unsigned int ii=0;ii<4;ii++)
            if (subTiles[ii])
            {
                delete subTiles[ii];
                subOfStreams[ii]->close();
                delete subOfStreams[ii];
                
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
            std::string proj4Str = inputDB->header.GetSRS().GetProj4();
            lidarDB->setHeader(proj4Str.c_str(),inputDB->header.GetFileSignature().c_str(), inputDB->header.GetMinX(), inputDB->header.GetMinY(), inputDB->header.GetMinZ(), inputDB->header.GetMaxX(), inputDB->header.GetMaxY(), inputDB->header.GetMaxZ(), 0, maxLevel,minPointLimit,maxPointLimit,(int)inputDB->header.GetDataFormatId());
        }
    }
    catch (const std::string &reason)
    {
        fprintf(stderr,"%s\n",reason.c_str());
        return false;
    }
    
    return true;
}
