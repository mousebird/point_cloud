//
//  LidarSorter.hpp
//  LidarQuadSort
//
//  Created by Steve Gifford on 4/8/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#ifndef LidarSorter_hpp
#define LidarSorter_hpp

#include <stdio.h>
#include <string>
#include "LidarDatabase.hpp"
#include <sys/stat.h>
#include <iostream>
#include <liblas/liblas.hpp>
#include <fstream>
#include <iostream>
#include <ogr_srs_api.h>
#include <cpl_port.h>
#include <ogr_spatialref.h>
#include <gdal.h>

class TileIdent
{
public:
    TileIdent() { }
    TileIdent(int x,int y,int z) : x(x), y(y), z(z) { }
    int x,y,z;
};

/* The LIDAR multi wrapper opens a group of files and makes
    up a header to describe them all.
  */
class LidarMultiWrapper
{
public:
    // Construct for the single file case
    LidarMultiWrapper(const std::string &file);
    
    // Construct with the filenames to open
    LidarMultiWrapper(const std::vector<std::string> &files);
    
    ~LidarMultiWrapper();
    
    // Open the files and figure out the various header parameters
    //  to describe them all.
    bool init();
    
    // Check if the reader has another point available
    bool hasNextPoint();
    
    // Fetch the next point, irrespective of the file it's in
    liblas::Point getNextPoint();
    
    // Header to cover the whole area
    liblas::Header header;
    
    // Remove the input file
    void removeFile();
    
protected:
    std::vector<std::string> files;
    bool valid;

    int whichFile;
    int whichPointOverall;
    int whichPointInFile;
    std::ifstream *ifs;
    liblas::Reader *reader;
};

/* The Lidar sorter recursively sorts LIDAR point files.
  */
class LidarSorter
{
public:
    LidarSorter(const char *tmp_dir);
    
    // Maximum number of points in a tile
    void setPointLimit(int minLimit,int maxLimit) { minPointLimit = minLimit; maxPointLimit = maxLimit; }
    
    // Process the top level file and recurse from there
    bool process(LidarMultiWrapper *inputDB,LidarDatabase *lidarDB);
    
    // Number of points written in various files
    int getNumPointsWritten() { return totalWrittenPoints; }
    
protected:
    bool process(LidarMultiWrapper *inputDB,TileIdent tileID,LidarDatabase *lidarDB,bool removeAfterDone);

    int minPointLimit,maxPointLimit;
    int maxLevel;
    std::string tmpDir;
    int totalWrittenPoints;
};

#endif /* LidarSorter_hpp */
