//
//  LidarDatabase.cpp
//  LidarQuadSort
//
//  Created by Steve Gifford on 4/12/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#include "LidarDatabase.hpp"

using namespace Kompex;

LidarDatabase::LidarDatabase(Kompex::SQLiteDatabase *db)
    : valid(true), insertStmt(NULL), db(db)
{
    SQLiteStatement stmt(db);

    // Create the manifest (table)
    try {
        stmt.SqlStatement((std::string)"CREATE TABLE manifest (minx REAL, miny REAL, minz REAL, maxx REAL, maxy REAL, maxz REAL);");
        stmt.SqlStatement((std::string)"ALTER TABLE manifest ADD minlevel INTEGER DEFAULT 0 NOT NULL;");
        stmt.SqlStatement((std::string)"ALTER TABLE manifest ADD maxlevel INTEGER DEFAULT 0 NOT NULL;");
        stmt.SqlStatement((std::string)"ALTER TABLE manifest ADD srs TEXT DEFAULT '' NOT NULL;");

        stmt.SqlStatement("CREATE TABLE lidartiles (data BLOB,level INTEGER,x INTEGER,y INTEGER,quadindex INTEGER PRIMARY KEY);");
    } catch (SQLiteException &exc) {
        fprintf(stderr,"Failed to write to database:\n%s\n",exc.GetString().c_str());
        valid = false;
    }

    valid = true;
}

bool LidarDatabase::setHeader(const char *srs,double minX,double minY,double minZ,double maxX,double maxY,double maxZ,int minLevel,int maxLevel)
{
    SQLiteStatement stmt(db);

    char stmtStr[1024];
    sprintf(stmtStr,"INSERT INTO manifest (minx,miny,minz,maxx,maxy,maxz,minlevel,maxlevel,srs) VALUES (%f,%f,%f,%f,%f,%f,%d,%d,'%s');",minX,minY,minZ,maxX,maxY,maxZ,minLevel,maxLevel,(srs ? srs : ""));
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

void LidarDatabase::flush()
{
    if (insertStmt)
        delete insertStmt;
    insertStmt = NULL;    
}
