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
#include "KompexSQLitePrerequisites.h"
#include "KompexSQLiteDatabase.h"
#include "KompexSQLiteStatement.h"
#include "KompexSQLiteException.h"
#include "KompexSQLiteStreamRedirection.h"
#include "KompexSQLiteBlob.h"
#include "KompexSQLiteException.h"
#include "LidarSorter.hpp"
#include "LidarDatabase.hpp"

bool FullDataMode = true;

int main(int argc, const char * argv[])
{
    if (argc < 2)
    {
        fprintf(stderr,"syntax: %s [<in_las> ...] [-tmp <tmp_dir>] [-o <out_sqlite>] [-filelist <fileList.txt>] [-pts <min> <max>]\n",argv[0]);
        return -1;
    }

    std::vector<std::string> inFiles;
    const char *fileList = NULL;
    std::string tmpDir = "lidarquadsort";
    const char *outSqlite = NULL;
    int inc = 0;
    int minPts=20000,maxPts=25000;
    for (unsigned int arg=1;arg<argc;arg+=inc)
    {
        if (!strcmp(argv[arg],"-tmp"))
        {
            inc = 2;
            if (arg+inc > argc)
            {
                fprintf(stderr,"Expecting one argument for -tmp\n");
                return -1;
            }
            tmpDir = argv[arg+1];
        } else if (!strcmp(argv[arg],"-o"))
        {
            inc = 2;
            if (arg+inc > argc)
            {
                fprintf(stderr,"Expecting one argument for -o\n");
                return -1;
            }
            outSqlite = argv[arg+1];
        } else if (!strcmp(argv[arg],"-filelist"))
        {
            inc = 2;
            if (arg+inc > argc)
            {
                fprintf(stderr,"Expecting one argument for -filelist\n");
                return -1;
            }
            fileList = argv[arg+1];
        } else if (!strcmp(argv[arg],"-pts"))
        {
            inc = 3;
            if (arg+inc > argc)
            {
                fprintf(stderr,"Expecting two arguments for -pts");
                return -1;
            }
            minPts = atoi(argv[arg+1]);
            maxPts = atoi(argv[arg+1]);
        } else {
            inc = 1;
            inFiles.push_back(argv[arg]);
        }
    }
    
    if (!fileList && inFiles.empty())
    {
        fprintf(stderr,"Expecting at least one input file or -filelist\n");
        return -1;
    }
    if (!outSqlite)
    {
        fprintf(stderr,"Expecting -o argument for output file.\n");
        return -1;
    }
    if (minPts <= 0 || maxPts < minPts)
    {
        fprintf(stderr,"-pts arguments don't make sense.\n");
        return -1;
    }
    
    // Load the list of files from a text file
    if (fileList)
    {
        FILE *fp = fopen(fileList,"r");
        if (!fp)
        {
            fprintf(stderr,"Failed to open file list: %s\n",fileList);
            return -1;
        }
        char line[2048];
        while (fgets(line,2047,fp))
        {
            if (strlen(line) > 0)
            {
                size_t last = strlen(line);
                if (line[last-1] == '\n')
                    line[last-1] = 0;
                inFiles.push_back(line);
            }
        }
        fclose(fp);
    }
    
    if (inFiles.empty())
    {
        fprintf(stderr,"Need input files.\n");
        return -1;
    }
    
    std::remove(outSqlite);
    boost::filesystem::remove_all(boost::filesystem::path(tmpDir));
    mkdir(tmpDir.c_str(),0775);

    // Set up a SQLITE output db
    Kompex::SQLiteDatabase *sqliteDb = NULL;
    sqliteDb = new Kompex::SQLiteDatabase(outSqlite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    if (!sqliteDb->GetDatabaseHandle())
    {
        fprintf(stderr, "Invalid sqlite database: %s\n",outSqlite);
        return -1;
    }
    LidarDatabase *lidarDb = new LidarDatabase(sqliteDb,(FullDataMode ? LidarDatabase::FullData : LidarDatabase::IndexOnly));
    if (!lidarDb->isValid())
    {
        fprintf(stderr,"Failed to set up sqlite output.\n");
        return -1;
    }
    
    // This speeds up writing
    {
//        Kompex::SQLiteStatement transactStmt(sqliteDb);
//        transactStmt.SqlStatement((std::string)"BEGIN TRANSACTION");
    }
    
    // Set up the input data
    LidarMultiWrapper lidarWrap(inFiles);
    if (!lidarWrap.init())
    {
        fprintf(stderr,"Failed to read input files.  Giving up.\n");
        return -1;
    }

    // Set up the recursive sorter and let it run
    LidarSorter sorter(tmpDir.c_str());
    sorter.setPointLimit(minPts,maxPts);
    if (sorter.process(&lidarWrap,lidarDb))
    {
        fprintf(stdout,"Wrote a total of %d points",sorter.getNumPointsWritten());
        return 0;
    }

    // Speeds up writing
    {
//        Kompex::SQLiteStatement transactStmt(sqliteDb);
//        transactStmt.SqlStatement((std::string)"END TRANSACTION");
    }

    lidarDb->flush();
    try {
        sqliteDb->Close();
    }
    catch (Kompex::SQLiteException &except)
    {
        fprintf(stderr,"Failed to write blob to database:\n%s\n",except.GetString().c_str());
        return false;
    }
    
    fprintf(stderr,"Failed to sort LIDAR file.");

    return -1;
}
