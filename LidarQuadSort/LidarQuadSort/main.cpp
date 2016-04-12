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

    // Set up a SQLITE output db
    Kompex::SQLiteDatabase *sqliteDb = NULL;
    sqliteDb = new Kompex::SQLiteDatabase(outFile, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    if (!sqliteDb->GetDatabaseHandle())
    {
        fprintf(stderr, "Invalid sqlite database: %s\n",outFile);
        return -1;
    }
    LidarDatabase *lidarDb = new LidarDatabase(sqliteDb);
    if (!lidarDb->isValid())
    {
        fprintf(stderr,"Failed to set up sqlite output.");
        return -1;
    }
    
    // This speeds up writing
    {
//        Kompex::SQLiteStatement transactStmt(sqliteDb);
//        transactStmt.SqlStatement((std::string)"BEGIN TRANSACTION");
    }

    // Set up the recursive sorter and let it run
    LidarSorter sorter(tmpDir);
    sorter.setPointLimit(1000,2000);
    if (sorter.process(inFile,lidarDb))
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
