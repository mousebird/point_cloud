//
//  LidarDatabase.cpp
//  LidarQuadSort
//
//  Created by Steve Gifford on 4/12/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#include "LidarDatabase.hpp"

using namespace Kompex;

LidarDatabase::LidarDatabase(Kompex::SQLiteDatabase *db,Type type)
    : valid(true), insertStmt(NULL), db(db)
{
    SQLiteStatement stmt(db);

    // Create the manifest (table)
    try {
        stmt.SqlStatement((std::string)"CREATE TABLE manifest (minx REAL, miny REAL, minz REAL, maxx REAL, maxy REAL, maxz REAL);");
        stmt.SqlStatement((std::string)"ALTER TABLE manifest ADD minlevel INTEGER DEFAULT 0 NOT NULL;");
        stmt.SqlStatement((std::string)"ALTER TABLE manifest ADD maxlevel INTEGER DEFAULT 0 NOT NULL;");
        stmt.SqlStatement((std::string)"ALTER TABLE manifest ADD minpoints INTEGER DEFAULT 0 NOT NULL;");
        stmt.SqlStatement((std::string)"ALTER TABLE manifest ADD maxpoints INTEGER DEFAULT 0 NOT NULL;");
        stmt.SqlStatement((std::string)"ALTER TABLE manifest ADD srs TEXT DEFAULT '' NOT NULL;");
        stmt.SqlStatement((std::string)"ALTER TABLE manifest ADD pointtype INTEGER DEFAULT 0 NOT NULL;");
        stmt.SqlStatement((std::string)"ALTER TABLE manifest ADD name TEXT DEFAULT '' NOT NULL;");
        stmt.SqlStatement((std::string)"ALTER TABLE manifest ADD maxcolor INTEGER DEFAULT 0 NOT NULL;");

        switch (type)
        {
            case FullData:
                stmt.SqlStatement("CREATE TABLE lidartiles (data BLOB,level INTEGER,x INTEGER,y INTEGER,quadindex INTEGER PRIMARY KEY);");
                break;
            case IndexOnly:
                stmt.SqlStatement("CREATE TABLE tileaddress (start BIGINT, count INTEGER, level INTEGER,x INTEGER,y INTEGER,quadindex INTEGER PRIMARY KEY);");
                break;
        }
    } catch (SQLiteException &exc) {
        fprintf(stderr,"Failed to write to database:\n%s\n",exc.GetString().c_str());
        valid = false;
    }

    valid = true;
}

bool LidarDatabase::setHeader(const char *srs,const char *name,double minX,double minY,double minZ,double maxX,double maxY,double maxZ,int minLevel,int maxLevel,int minPoints,int maxPoints,int pointType,int maxColor)
{
    SQLiteStatement stmt(db);

    char stmtStr[1024];
    sprintf(stmtStr,"INSERT INTO manifest (minx,miny,minz,maxx,maxy,maxz,minlevel,maxlevel,minpoints,maxpoints,srs,name,pointtype,maxcolor) VALUES (%f,%f,%f,%f,%f,%f,%d,%d,%d,%d,'%s','%s',%d,%d);",minX,minY,minZ,maxX,maxY,maxZ,minLevel,maxLevel,minPoints,maxPoints,(srs ? srs : ""),name,pointType,maxColor);
    stmt.SqlStatement(stmtStr);
    
    return true;
}

bool LidarDatabase::addTile(const void *tileData,int dataSize,int x,int y,int level)
{
    // Calculate a quad index for later use
    int quadIndex = 0;
    for (int iq=0;iq<level;iq++)
        quadIndex += (1<<iq)*(1<<iq);
    quadIndex += y*(1<<level) + x;

    if (!insertStmt)
    {
        insertStmt = new SQLiteStatement(db);
        insertStmt->Sql("INSERT INTO lidartiles (data,level,x,y,quadindex) VALUES (@data,@level,@x,@y,@quadindex);");
    }

    // Here we've got data to insert
    if (tileData)
    {
        // Now insert the samples into the database as a blob
        try {
            insertStmt->BindBlob(1, tileData, dataSize);
            insertStmt->BindInt(2, level);
            insertStmt->BindInt(3, x);
            insertStmt->BindInt(4, y);
            insertStmt->BindInt(5, quadIndex);
            insertStmt->Execute();
            insertStmt->Reset();
        }
        catch (SQLiteException &except)
        {
            fprintf(stderr,"Failed to write blob to database:\n%s\n",except.GetString().c_str());
            return false;
        }
    }

    return true;
}

bool LidarDatabase::addTileOffset(long long start,int length,int x,int y,int level)
{
    // Calculate a quad index for later use
    int quadIndex = 0;
    for (int iq=0;iq<level;iq++)
        quadIndex += (1<<iq)*(1<<iq);
    quadIndex += y*(1<<level) + x;
    
    if (!insertStmt)
    {
        insertStmt = new SQLiteStatement(db);
        insertStmt->Sql("INSERT INTO tileaddress (start,count,level,x,y,quadindex) VALUES (@start,@count,@level,@x,@y,@quadindex);");
    }

    // Now insert the samples into the database as a blob
    try {
        insertStmt->BindInt64(1, start);
        insertStmt->BindInt(2, length);
        insertStmt->BindInt(3, level);
        insertStmt->BindInt(4, x);
        insertStmt->BindInt(5, y);
        insertStmt->BindInt(6, quadIndex);
        insertStmt->Execute();
        insertStmt->Reset();
    }
    catch (SQLiteException &except)
    {
        fprintf(stderr,"Failed to write blob to database:\n%s\n",except.GetString().c_str());
        return false;
    }

    return true;
}

void LidarDatabase::flush()
{
    if (insertStmt)
        delete insertStmt;
    insertStmt = NULL;    
}
