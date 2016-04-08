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

int main(int argc, const char * argv[])
{
    if (argc < 4)
    {
        fprintf(stderr,"syntax: %s <in_las> <tmp_dir> <out_sqlite>\n",argv[0]);
    }
    const char *inFile = argv[1];
    const char *tmpDir = argv[2];
    const char *outFile = argv[3];
    
    std::ifstream ifs;
    ifs.open(inFile, std::ios::in | std::ios::binary);

    liblas::ReaderFactory f;
    liblas::Reader reader = f.CreateWithStream(ifs);
    liblas::Header header = reader.GetHeader();
    liblas::SpatialReference srs = header.GetSRS();
    
    mkdir(tmpDir,0775);

    return 0;
}
