//
//  LidarDatabase.hpp
//  LidarQuadSort
//
//  Created by Steve Gifford on 4/12/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#ifndef LidarDatabase_hpp
#define LidarDatabase_hpp

#include <stdio.h>
#include "KompexSQLitePrerequisites.h"
#include "KompexSQLiteDatabase.h"
#include "KompexSQLiteStatement.h"
#include "KompexSQLiteException.h"
#include "KompexSQLiteStreamRedirection.h"
#include "KompexSQLiteBlob.h"
#include "KompexSQLiteException.h"

/* Interface to sqlite LIDAR database.
 */
class LidarDatabase
{
public:
    LidarDatabase(Kompex::SQLiteDatabase *db);
    
    // Set the header info after creation
    bool setHeader(const char *srs,double minX,double minY,double minZ,double maxX,double maxY,double maxZ,int minLevel,int maxLevel);
    
    // Add data for a tile
    bool addTile(const void *tileData,int dataSize,int x,int y,int level);
    
    // Close any open statements and such
    void flush();
    
    // Check this after opening
    bool isValid() { return valid; }

protected:
    bool valid;
    Kompex::SQLiteDatabase *db;
    
    // Precompiled insert statement
    Kompex::SQLiteStatement *insertStmt;
};

#endif /* LidarDatabase_hpp */
