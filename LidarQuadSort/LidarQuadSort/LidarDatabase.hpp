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
    typedef enum {FullData,IndexOnly} Type;
    
    // Construct with an empty SQLite database and the type.
    // If this is FullData we'll store data in it
    // If not, we'll just store offsets into another file.
    LidarDatabase(Kompex::SQLiteDatabase *db,Type type);
    
    // Set the header info after creation
    bool setHeader(const char *srs,const char *name,double minX,double minY,double minZ,double maxX,double maxY,double maxZ,int minLevel,int maxLevel,int minPoints,int maxPoints,int pointType,int maxColor);
    
    // Add data for a tile
    bool addTile(const void *tileData,int dataSize,int x,int y,int level);
    
    // Add tile offset information
    bool addTileOffset(long long start,int length,int x,int y,int level);
    
    // Close any open statements and such
    void flush();
    
    Type getType() { return type; }
    
    // Check this after opening
    bool isValid() { return valid; }

protected:
    Type type;
    bool valid;
    Kompex::SQLiteDatabase *db;
    
    // Precompiled insert statement
    Kompex::SQLiteStatement *insertStmt;
};

#endif /* LidarDatabase_hpp */
