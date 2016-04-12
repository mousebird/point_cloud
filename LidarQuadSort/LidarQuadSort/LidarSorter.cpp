//
//  LidarSorter.cpp
//  LidarQuadSort
//
//  Created by Steve Gifford on 4/8/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#include "LidarSorter.hpp"
#include <sys/stat.h>
#include <iostream>
#include <liblas/liblas.hpp>
#include <fstream>
#include <iostream>
#include <ogr_srs_api.h>
#include <cpl_port.h>
#include <ogr_spatialref.h>
#include <gdal.h>

LidarSorter::LidarSorter(const char *tmp_dir)
: tmpDir(tmp_dir), minPointLimit(1000), maxPointLimit(1500), totalWrittenPoints(0),maxLevel(0)
{
}

bool LidarSorter::process(const std::string &fileName,LidarDatabase *lidarDB)
{
    return process(fileName,TileIdent(0,0,0),lidarDB,false);
}

bool LidarSorter::process(const std::string &fileName,TileIdent tileID,LidarDatabase *lidarDB,bool removeAfterDone)
{
    try {
        // Input file
        std::ifstream ifs;
        ifs.open(fileName, std::ios::in | std::ios::binary);
        
        liblas::ReaderFactory f;
        liblas::Reader reader = f.CreateWithStream(ifs);
        liblas::Header header = reader.GetHeader();
        liblas::SpatialReference srs = header.GetSRS();
        
        // Tile output
        liblas::Header tileHeader = header;
        tileHeader.SetPointRecordsCount(0);
        tileHeader.SetCompressed(true);
        std::string tileFile = tmpDir + "/" + std::to_string(tileID.x) + "_" + std::to_string(tileID.y) + "_" + std::to_string(tileID.z) + ".laz";
        std::stringstream ofs(std::stringstream::out);
        liblas::Writer *tileW = new liblas::Writer(ofs,tileHeader);
        
        // Figure out which points we're keeping and which we're outputting
        bool allPoints = header.GetPointRecordsCount() <= maxPointLimit;
        float fracToKeep = (float)minPointLimit / (float)header.GetPointRecordsCount();

        liblas::Writer *subTiles[4] = {NULL,NULL,NULL,NULL};
        std::ofstream *subOfStreams[4] = {NULL,NULL,NULL,NULL};
        int subTileCount[4] = {0,0,0,0};
        TileIdent subTileIDs[4];
        std::string subTileNames[4];
        double spanX_2 = (header.GetMaxX()-header.GetMinX())/2.0;
        double spanY_2 = (header.GetMaxY()-header.GetMinY())/2.0;
        if (!allPoints)
        {
            for (unsigned int sy=0;sy<2;sy++)
                for (unsigned int sx=0;sx<2;sx++)
                {
                    TileIdent &subIdent = subTileIDs[sy*2+sx];
                    subIdent.x = 2*tileID.x + sx;  subIdent.y = 2*tileID.y + sy;  subIdent.z = tileID.z+1;

                    liblas::Header subHeader = header;
                    subHeader.SetPointRecordsCount(0);
                    subHeader.SetCompressed(true);
                    subHeader.SetMin(header.GetMinX()+sx*spanX_2, header.GetMinY()+sy*spanY_2, header.GetMinZ());
                    subHeader.SetMax(header.GetMinX()+(sx+1)*spanX_2, header.GetMinY()+(sy+1)*spanY_2, header.GetMaxZ());

                    // Set up the write for this sub-tile
                    std::string subFile = tmpDir + "/" + "src_" + std::to_string(subIdent.x) + "_" + std::to_string(subIdent.y) + "_" + std::to_string(subIdent.z) + ".laz";
                    std::ofstream *subofs = new std::ofstream(subFile,std::ios::out | std::ios::binary);
                    subTileNames[sy*2+sx] = subFile;
                    liblas::Writer *subW = new liblas::Writer(*subofs,subHeader);
                    subOfStreams[sy*2+sx] = subofs;
                    subTiles[sy*2+sx] = subW;
                }
        }
        
        // Work through the points in the input file
        int numToCopy = header.GetPointRecordsCount();
        int numCopiedToTile = 0;
        for (unsigned int ii=0;ii<numToCopy;ii++)
        {
            if (!reader.ReadNextPoint())
                throw "Unable to read input point";
            liblas::Point p = reader.GetPoint();
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
                int whichX = (x-header.GetMinX())/spanX_2;
                int whichY = (y-header.GetMinY())/spanY_2;
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
        fprintf(stdout,"%sTile %d: (%d,%d) saved %d of %d points\n",indent.c_str(),tileID.z,tileID.x,tileID.y,numCopiedToTile,header.GetPointRecordsCount());
        
        // Close down the main tile
        delete tileW;
        std::string tileStr = ofs.str();
        lidarDB->addTile(tileStr.c_str(), (int)tileStr.size(), tileID.x, tileID.y, tileID.z);
        
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
                        if (!process(subFile,subIdent,lidarDB,true))
                            throw (std::string)"Failed to write tile " + std::to_string(subIdent.z) + ": (" + std::to_string(subIdent.x) + "," + std::to_string(subIdent.y) + ")";
                    }
                }
        
        // Note: Remove the starting file if we need to
        if (removeAfterDone)
            std::remove(fileName.c_str());
        
        maxLevel = std::max(maxLevel,tileID.z);

        // If this is the top file, we'll set up the output header
        if (!removeAfterDone)
        {
            std::string proj4Str = header.GetSRS().GetProj4();
            lidarDB->setHeader(proj4Str.c_str(), header.GetMinX(), header.GetMinY(), header.GetMinZ(), header.GetMaxX(), header.GetMaxY(), header.GetMaxZ(), 0, maxLevel,minPointLimit,maxPointLimit);
        }
    }
    catch (const std::string &reason)
    {
        fprintf(stderr,"%s\n",reason.c_str());
        return false;
    }
    
    return true;
}
