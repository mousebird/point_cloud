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

class TileIdent
{
public:
    TileIdent() { }
    TileIdent(int x,int y,int z) : x(x), y(y), z(z) { }
    int x,y,z;
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
    bool process(const std::string &fileName,LidarDatabase *lidarDB);
    
    // Number of points written in various files
    int getNumPointsWritten() { return totalWrittenPoints; }
    
protected:
    bool process(const std::string &fileName,TileIdent tileID,LidarDatabase *lidarDB,bool removeAfterDone);

    int minPointLimit,maxPointLimit;
    int maxLevel;
    std::string tmpDir;
    int totalWrittenPoints;
};

#endif /* LidarSorter_hpp */
