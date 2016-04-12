//
//  main.cpp
//  LidarQuadSort
//
//  Created by Steve Gifford on 4/7/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#include <stdio.h>
#include <sys/stat.h>
#include <iostream>
#include <liblas/liblas.hpp>
#include <fstream>
#include <iostream>
#include <ogr_srs_api.h>
#include <cpl_port.h>
#include <ogr_spatialref.h>
#include <gdal.h>
#include <boost/filesystem.hpp>
#include "LidarSorter.hpp"

int main(int argc, const char * argv[])
{
    if (argc < 4)
    {
        fprintf(stderr,"syntax: %s <in_las> <tmp_dir> <out_sqlite>\n",argv[0]);
    }
    const char *inFile = argv[1];
    const char *tmpDir = argv[2];
    const char *outFile = argv[3];
    
    std::remove(outFile);
    boost::filesystem::remove_all(boost::filesystem::path(tmpDir));
    mkdir(tmpDir,0775);
    
    // Set up the recursive sorter and let it run
    LidarSorter sorter(tmpDir);
    sorter.setPointLimit(1000,1500);
    if (sorter.process(inFile))
    {
        fprintf(stdout,"Wrote a total of %d points",sorter.getNumPointsWritten());
        return 0;
    }
    
    fprintf(stderr,"Failed to sort LIDAR file.");

    return -1;
}
