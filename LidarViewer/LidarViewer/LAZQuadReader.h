//
//  LAZQuadReader.h
//  LidarViewer
//
//  Created by Steve Gifford on 4/13/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "WhirlyGlobeComponent.h"

/// Override the coordinate system
extern NSString * const kLAZReaderCoordSys;
/// Add in a z offset
extern NSString * const kLAZReaderZOffset;
/// Scale the color values.  By default this is (1<<16)-1
extern NSString * const kLAZReaderColorScale;

/** @brief The LAZ Quad Reader will page a Lidar (LAZ or LAS) database organized
    into tiles in a sqlite database.
  */
@interface LAZQuadReader : NSObject<MaplyPagingDelegate>

// Coordinate system this database is in
@property (strong,readonly) MaplyCoordinateSystem *coordSys;

// Name as read from the database
@property (nonatomic,readonly) NSString *name;

// Bounding box from the LAZ database
@property (readonly) double minX,minY,minZ,maxX,maxY,maxZ;

// Nudge the elevation, usually to get it above the earth to see it
@property (nonatomic) double zOffset;

// Min and max zoom levels
@property (nonatomic) int minZoom,maxZoom;

// Min/max number of point potentially in each tile
@property (nonatomic,readonly) int minTilePoints,maxTilePoints;

// The shader to use for rendering.  Need to assign this.
@property (nonatomic,strong) MaplyShader *shader;

// Point size to pass through to the shader
@property (nonatomic) float pointSize;

/** @brief Initialize with the file name of the sqlite db and the LAZ file
    @details Initialize the the reader with a LAZ file and a separate index file.
  */
//- (id)initWithDB:(NSString *)fileName indexFile:(NSString *)indexFileName desc:(NSDictionary *)desc;

/** @brief Initialize the pager with a LAZ sqlite database.
    @details This opens the sqlite database and reads the manifest.
    @param fileName The sqlite LAZ database to read.  Pass in the full path.
    @param desc Overrides for the setup.  We put attributes in here that aren't quite right in the database.
  */
- (id)initWithDB:(NSString *)fileName desc:(NSDictionary *)desc;

// Based on the number of points we want to display, how many tiles should we fetch?
- (int)getNumTilesFromMaxPoints:(int)maxPoints;

// Return the center from the bounding box
- (MaplyCoordinate)getCenter;

// Calculate the bounding box including offsets
- (void)getBoundsLL:(MaplyCoordinate3dD *)ll ur:(MaplyCoordinate3dD *)ur;

// Set if the points have their own color
- (bool)hasColor;

@end
