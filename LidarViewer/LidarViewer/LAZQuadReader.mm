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

// Keep track of tile size (height in particular)
class TileSizeInfo
{
public:
    TileSizeInfo(MaplyTileID tileID) : tileID(tileID), minZ(0.0), maxZ(0.0) { }
    TileSizeInfo() { }
    
    bool operator < (const TileSizeInfo &that) const
    {
        if (tileID.level == that.tileID.level)
        {
            if (tileID.y == that.tileID.y)
                return tileID.x < that.tileID.x;
            return tileID.y < that.tileID.y;
        }
        return tileID.level < that.tileID.level;
    }
    
    MaplyTileID tileID;
    double minZ,maxZ;
};

typedef std::set<TileSizeInfo> TileSizeSet;

@implementation LAZQuadReader
{
    FMDatabase *db;
    FMDatabaseQueue *queue;
    std::ifstream *ifs;
    liblas::Reader *lazReader;
    TileSizeSet tileSizes;
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
    
    // Note: Should check the LAZ reader
    
    return self;
}

- (id)initWithDB:(NSString *)sqliteFileName
{
    self = [super init];
    if (!self)
        return nil;
    
    _zOffset = 0.0;
    
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
    if (srs && [srs length])
    {
        _coordSys = [[MaplyProj4CoordSystem alloc] initWithString:srs];
        MaplyCoordinate ll,ur;
        ll.x = _minX;  ll.y = _minY;  ur.x = _maxX;  ur.y = _maxY;
        [_coordSys setBoundsLL:&ll ur:&ur];
    } else {
        // Nebraska state plane.  Because why not.
        _coordSys = [[MaplyProj4CoordSystem alloc] initWithString:@"+proj=lcc +lat_1=43 +lat_2=40 +lat_0=39.83333333333334 +lon_0=-100 +x_0=500000.00001016 +y_0=0 +ellps=GRS80 +to_meter=0.3048006096012192 +no_defs "];
        MaplyCoordinate ll,ur;
        ll.x = _minX;  ll.y = _minY;  ur.x = _maxX;  ur.y = _maxY;
        [_coordSys setBoundsLL:&ll ur:&ur];
    }
    
    queue = [FMDatabaseQueue databaseQueueWithPath:sqlitePath];

    return self;
}

- (void)setShader:(MaplyShader *)shader
{
    _shader = shader;
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

- (void)getBoundingBox:(MaplyTileID)tileID ll:(MaplyCoordinate3dD *)ll ur:(MaplyCoordinate3dD *)ur
{
    @synchronized (self) {
        TileSizeInfo dummy(tileID);
        TileSizeSet::iterator it = tileSizes.find(dummy);
        if (it != tileSizes.end())
        {
            ll->z = it->minZ;
            ur->z = it->maxZ;
        }
    }
}

- (void)tileDidUnload:(MaplyTileID)tileID
{
    @synchronized (self) {
        TileSizeInfo dummy(tileID);
        TileSizeSet::iterator it = tileSizes.find(dummy);
        if (it != tileSizes.end())
            tileSizes.erase(it);
    }
}

- (void)startFetchForTile:(MaplyTileID)tileID forLayer:(MaplyQuadPagingLayer *__nonnull)layer
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
   ^{
       // Put together the precalculated quad index.  This is faster
       //  than x,y,level
       int quadIdx = 0;
       for (int iq=0;iq<tileID.level;iq++)
           quadIdx += (1<<iq)*(1<<iq);
       quadIdx += tileID.y*(1<<tileID.level)+tileID.x;

       // Information set up from the database or from the global file
       liblas::Reader * __block thisReader = NULL;
       std::stringstream * __block tileStream = NULL;
       MaplyComponentObject * __block compObj = nil;
       NSData * __block data = nil;
       long long __block pointStart = 0;
       int __block count = 0;
       bool __block hasColors = false;
       
       // We're either using the index with an external LAZ files or we're grabbing the raw data itself
       [queue inDatabase:^(FMDatabase *theDb) {
           FMResultSet *res = nil;
           if (lazReader)
               res = [db executeQuery:[NSString stringWithFormat:@"SELECT start,count FROM tileaddress WHERE quadindex=%d;",quadIdx]];
           else
               res = [db executeQuery:[NSString stringWithFormat:@"SELECT data FROM lidartiles WHERE quadindex=%d;",quadIdx]];
           if ([res next])
           {
               if (lazReader)
               {
                   pointStart = [res longLongIntForColumn:@"start"];
                   count = [res intForColumn:@"count"];
                   thisReader = lazReader;
                   liblas::Header header = thisReader->GetHeader();
                   hasColors = header.GetDataFormatId() != liblas::ePointFormat0 && header.GetDataFormatId() != liblas::ePointFormat1;
               } else {
                   data = [res dataForColumn:@"data"];
                   tileStream = new std::stringstream();
                   tileStream->write(reinterpret_cast<const char *>([data bytes]),[data length]);
                   liblas::ReaderFactory f;
                   liblas::Reader reader = f.CreateWithStream(*tileStream);
                   thisReader = new liblas::Reader(reader);
                   liblas::Header header = reader.GetHeader();
                   count = header.GetPointRecordsCount();
                   hasColors = header.GetDataFormatId() != liblas::ePointFormat0 && header.GetDataFormatId() != liblas::ePointFormat1;
               }

               [res close];
           }
       }];
       
       if (thisReader)
       {
           MaplyPoints *points = [[MaplyPoints alloc] initWithNumPoints:count];
           int elevID = [points addAttributeType:@"a_elev" type:MaplyShaderAttrTypeFloat];
           
           // Center the coordinates around the tile center
           MaplyCoordinate3dD tileCenter;
           tileCenter.x = (thisReader->GetHeader().GetMinX()+thisReader->GetHeader().GetMaxX())/2.0;
           tileCenter.y = (thisReader->GetHeader().GetMinY()+thisReader->GetHeader().GetMaxY())/2.0;
           tileCenter.z = 0.0;
           MaplyCoordinate3dD tileCenterDisp = [layer.viewC displayCoordD:tileCenter fromSystem:_coordSys];
           points.transform = [[MaplyMatrix alloc] initWithTranslateX:tileCenterDisp.x y:tileCenterDisp.y z:tileCenterDisp.z];
           
           long long which = 0;
           double minZ=MAXFLOAT,maxZ=-MAXFLOAT;
           while (which < count)
           {
               // Get the point and convert to geocentric
               if (lazReader)
               {
                   if (!thisReader->ReadPointAt((size_t)(which+pointStart)))
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
               
               minZ = std::min(coord.z,minZ);
               maxZ = std::max(coord.z,maxZ);

               MaplyCoordinate3dD dispCoord = [layer.viewC displayCoordD:coord fromSystem:_coordSys];
               MaplyCoordinate3dD dispCoordCenter = MaplyCoordinate3dDMake(dispCoord.x-tileCenterDisp.x, dispCoord.y-tileCenterDisp.y, dispCoord.z-tileCenterDisp.z);
               
               float red = 1.0,green = 1.0, blue = 1.0;
               if (hasColors)
               {
                   liblas::Color color = p.GetColor();
                   red = color.GetRed() / 255.0;  green = color.GetGreen() / 255.0;  blue = color.GetBlue() / 255.0;
               }
               [points addDispCoordDoubleX:dispCoordCenter.x y:dispCoordCenter.y z:dispCoordCenter.z];
               [points addColorR:red g:green b:blue a:1.0];
               [points addAttribute:elevID fVal:coord.z];
               
               which++;
           }
           
           // Keep track of tile size
           if (minZ == maxZ)
               maxZ += 1.0;
           @synchronized (self) {
               TileSizeInfo tileInfo(tileID);
               tileInfo.minZ = minZ;  tileInfo.maxZ = maxZ;
               tileSizes.insert(tileInfo);
           }

           compObj = [layer.viewC addPoints:@[points] desc:
                                            @{kMaplyColor: [UIColor redColor],
                                              kMaplyPointSize: @(6.0),
                                              kMaplyDrawPriority: @(10000000),
                                              kMaplyShader: _shader.name,
                                              kMaplyShaderUniforms:
                                                  @{@"u_zmin": @(_minZ),
                                                    @"u_zmax": @(_maxZ)},
                                              kMaplyZBufferRead: @(YES),
                                              kMaplyZBufferWrite: @(YES)
                                              }
                                                mode:MaplyThreadCurrent];
           [layer addData:@[compObj] forTile:tileID style:MaplyDataStyleAdd];
       }

       if (!lazReader)
       {
           if (thisReader)
           {
               delete thisReader;
               delete tileStream;
           }
       }
       
       if (compObj)
           [layer tileDidLoad:tileID];
       else
           [layer tileFailedToLoad:tileID];
   });
}

@end
