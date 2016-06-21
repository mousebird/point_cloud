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
#include <vector>
#include "LidarDatabase.hpp"
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <iostream>

#include <geotiff.h>
#include <geo_simpletags.h>
#include <geo_normalize.h>
#include <geo_simpletags.h>
#include <geovalues.h>

#import "laszip_api.h"

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
    laszip_point_struct *getNextPoint();
    
    // Header to cover the whole area
    laszip_header_struct header;
    
    // Remove the input file
    void removeFile();
    
    // Return a proj4 compatible string (if we were able to make one)
    std::string getProj4Str() { return projStr; }
    
protected:
    std::vector<std::string> files;
    bool valid;

    int whichFile;
    unsigned int whichPointOverall;
    unsigned int whichPointInFile;
    laszip_POINTER reader;
    std::string projStr;
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
    long long getNumPointsWritten() { return totalWrittenPoints; }
    
protected:
    bool process(LidarMultiWrapper *inputDB,TileIdent tileID,LidarDatabase *lidarDB,bool removeAfterDone);

    int minPointLimit,maxPointLimit;
    int maxLevel;
    std::string tmpDir;
    long long totalWrittenPoints;
};

#endif /* LidarSorter_hpp */
