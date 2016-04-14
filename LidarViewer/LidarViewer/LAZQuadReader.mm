//
//  LAZQuadReader.m
//  LidarViewer
//
//  Created by Steve Gifford on 4/13/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#include <fstream>
#include <iostream>
#include <liblas/liblas.hpp>
#import "LAZQuadReader.h"
#import "sqlite3.h"
#import "FMDatabase.h"
#import "FMDatabaseQueue.h"

@implementation LAZQuadReader
{
    FMDatabase *db;
    FMDatabaseQueue *queue;
    std::ifstream *ifs;
    liblas::Reader *lazReader;
    bool hasColors;
}

- (id)initWithDB:(NSString *)lazFileName indexFile:(NSString *)sqliteFileName
{
    self = [self initWithDB:sqliteFileName];
    if (!self)
        return nil;
    
    NSString *lazPath = nil;
    // See if that was a direct path first
    if ([[NSFileManager defaultManager] fileExistsAtPath:lazFileName])
        lazPath = lazFileName;
    else {
        // Now try looking for it in the bundle
        lazPath = [[NSBundle mainBundle] pathForResource:lazFileName ofType:@"laz"];
        if (!lazPath)
            return nil;
    }

    // Open the LAZ File
    liblas::ReaderFactory f;
    ifs = new std::ifstream();
    ifs->open([lazPath cStringUsingEncoding:NSASCIIStringEncoding], std::ios::in | std::ios::binary);
    liblas::Reader reader = f.CreateWithStream(*ifs);
    lazReader = new liblas::Reader(reader);
    liblas::Header header = lazReader->GetHeader();
    hasColors = header.GetDataFormatId() != liblas::ePointFormat0 && header.GetDataFormatId() != liblas::ePointFormat1;

    // Note: Should check the LAZ reader
    
    return self;
}

- (id)initWithDB:(NSString *)sqliteFileName
{
    self = [super init];
    if (!self)
        return nil;
    
    _zOffset = 1.0;
    
    NSString *sqlitePath = nil;
    // See if that was a direct path first
    if ([[NSFileManager defaultManager] fileExistsAtPath:sqliteFileName])
        sqlitePath = sqliteFileName;
    else {
        // Now try looking for it in the bundle
        sqlitePath = [[NSBundle mainBundle] pathForResource:sqliteFileName ofType:@"sqlite"];
        if (!sqlitePath)
            return nil;
    }
    
    // Open the sqlite index file
    db = [[FMDatabase alloc] initWithPath:sqlitePath];
    if (!db)
        return nil;
    
    [db openWithFlags:SQLITE_OPEN_READONLY];
    
    FMResultSet *res = [db executeQuery:@"SELECT minx,miny,minz,maxx,maxy,maxz,minlevel,maxlevel,minpoints,maxpoints,srs FROM manifest"];
    NSString *srs = nil;
    if ([res next])
    {
        _minX = [res doubleForColumn:@"minx"];
        _minY = [res doubleForColumn:@"miny"];
        _minZ = [res doubleForColumn:@"minz"];
        _maxX = [res doubleForColumn:@"maxx"];
        _maxY = [res doubleForColumn:@"maxy"];
        _maxZ = [res doubleForColumn:@"maxz"];
        self.minZoom = [res intForColumn:@"minlevel"];
        self.maxZoom = [res intForColumn:@"maxlevel"];
        _minTilePoints = [res intForColumn:@"minpoints"];
        _maxTilePoints = [res intForColumn:@"maxpoints"];
        srs = [res stringForColumn:@"srs"];
    }
    
    // Note: If this isn't set up right, we need to fake it
    if (srs)
    {
        _coordSys = [[MaplyProj4CoordSystem alloc] initWithString:srs];
        MaplyCoordinate ll,ur;
        ll.x = _minX;  ll.y = _minY;  ur.x = _maxX;  ur.y = _maxY;
        [_coordSys setBoundsLL:&ll ur:&ur];
    }
    
    queue = [FMDatabaseQueue databaseQueueWithPath:sqlitePath];

    return self;
}

- (void)dealloc
{
    if (ifs)
    {
        delete ifs;
        delete lazReader;
    }
}

- (int)getNumTilesFromMaxPoints:(int)maxPoints
{
    int numTiles = maxPoints/_minTilePoints;
    if (numTiles < 1)  numTiles = 1;
    if (numTiles > 256)  numTiles = 256;
    
    return numTiles;
}

- (MaplyCoordinate)getCenter
{
    // Figure out the bounds
    double midX = (_minX+_maxX)/2.0;
    double midY = (_minY+_maxY)/2.0;
    
    return MaplyCoordinateMake(midX,midY);
}

- (void)getBoundsLL:(MaplyCoordinate3dD *)ll ur:(MaplyCoordinate3dD *)ur
{
    ll->x = _minX;  ll->y = _minY;  ll->z = _minZ+_zOffset;
    ur->x = _maxX;  ur->y = _maxY;  ur->z = _maxZ+_zOffset;
}

- (void)startFetchForTile:(MaplyTileID)tileID forLayer:(MaplyQuadPagingLayer *__nonnull)layer
{
    double scaleX = 1.0, scaleY = 1.0, scaleZ = 1.0;

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
   ^{
       // Put together the precalculated quad index.  This is faster
       //  than x,y,level
       int quadIdx = 0;
       for (int iq=0;iq<tileID.level;iq++)
           quadIdx += (1<<iq)*(1<<iq);
       quadIdx += tileID.y*(1<<tileID.level)+tileID.x;

       MaplyComponentObject * __block compObj = nil;
       
       // We're either using the index with an external LAZ files or we're grabbing the raw data itself
       [queue inDatabase:^(FMDatabase *theDb) {
           FMResultSet *res = nil;
           if (lazReader)
               res = [db executeQuery:[NSString stringWithFormat:@"SELECT start,count FROM tileaddress WHERE quadindex=%d;",quadIdx]];
           else
               res = [db executeQuery:[NSString stringWithFormat:@"SELECT data FROM lidartiles WHERE quadindex=%d;",quadIdx]];
           if ([res next])
           {
               long long pointStart = 0;
               int count = 0;
               liblas::Reader *thisReader = NULL;
               std::stringstream *tileStream = NULL;
               if (lazReader)
               {
                   pointStart = [res longLongIntForColumn:@"start"];
                   count = [res intForColumn:@"count"];
                   thisReader = lazReader;
               } else {
                   NSData *data = [res dataForColumn:@"data"];
                   tileStream = new std::stringstream();
                   tileStream->write(reinterpret_cast<const char *>([data bytes]),[data length]);
                   liblas::ReaderFactory f;
                   liblas::Reader reader = f.CreateWithStream(*tileStream);
                   thisReader = new liblas::Reader(reader);
                   liblas::Header header = reader.GetHeader();
                   count = header.GetPointRecordsCount();
                   hasColors = header.GetDataFormatId() != liblas::ePointFormat0 && header.GetDataFormatId() != liblas::ePointFormat1;
               }
           
               MaplyPoints *points = [[MaplyPoints alloc] initWithNumPoints:count];
               long long which = 0;
               while (which < count)
               {
                   // Get the point and convert to geocentric
                   if (lazReader)
                   {
                       if (!thisReader->ReadPointAt(which+pointStart))
                           break;
                   } else {
                       if (!thisReader->ReadNextPoint())
                           break;
                   }
                   liblas::Point const& p = thisReader->GetPoint();
                   //                double x,y,z;
                   //                x = p.GetX(), y = p.GetY(); z = p.GetZ();
                   //                trans->TransformEx(1, &x, &y, &z);
                   MaplyCoordinate3dD coord;
                   coord.x = p.GetX();  coord.y = p.GetY();  coord.z = p.GetZ();
                   coord.z += _zOffset;

                   MaplyCoordinate3dD dispCoord = [layer.viewC displayCoordD:coord fromSystem:_coordSys];
                   
                   float red = 1.0,green = 1.0, blue = 1.0;
                   if (hasColors)
                   {
                       liblas::Color color = p.GetColor();
                       red = color.GetRed() / 255.0;  green = color.GetGreen() / 255.0;  blue = color.GetBlue() / 255.0;
                   }
                   [points addDispCoordDoubleX:dispCoord.x y:dispCoord.y z:dispCoord.z];
                   [points addColorR:red g:green b:blue a:1.0];
                   
                   which++;
               }

               compObj = [layer.viewC addPoints:@[points] desc:
                                                @{kMaplyColor: [UIColor redColor],
                                                  kMaplyPointSize: @(6.0),
                                                  kMaplyDrawPriority: @(10000000),
                                                  kMaplyShader: _shader.name,
                                                  kMaplyZBufferRead: @(YES),
                                                  kMaplyZBufferWrite: @(YES)
                                                  }
                                                    mode:MaplyThreadCurrent];
               [layer addData:@[compObj] forTile:tileID style:MaplyDataStyleAdd];

               if (!lazReader)
               {
                   delete thisReader;
                   delete tileStream;
               }
           }
           
           [res close];
       }];
       
       if (compObj)
       {
//           NSLog(@"Tile did load %d: (%d,%d)",tileID.level,tileID.x,tileID.y);
           [layer tileDidLoad:tileID];
       } else {
//           NSLog(@"Tile did not load %d: (%d,%d)",tileID.level,tileID.x,tileID.y);
           [layer tileFailedToLoad:tileID];
       }
   });
}

@end
