//
//  LAZQuadReader.h
//  LidarViewer
//
//  Created by Steve Gifford on 4/13/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "WhirlyGlobeComponent.h"

@interface LAZQuadReader : NSObject<MaplyPagingDelegate>

// Coordinate system this database is in
@property (strong,readonly) MaplyCoordinateSystem *coordSys;

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

// Initialize with the file name of the sqlite db and the LAZ file
- (id)initWithDB:(NSString *)fileName indexFile:(NSString *)indexFileName;

// Initialize the variant that just uses the SQLite database
- (id)initWithDB:(NSString *)fileName;

// Based on the number of points we want to display, how many tiles should we fetch?
- (int)getNumTilesFromMaxPoints:(int)maxPoints;

// Return the center from the bounding box
- (MaplyCoordinate)getCenter;

// Calculate the bounding box including offsets
- (void)getBoundsLL:(MaplyCoordinate3dD *)ll ur:(MaplyCoordinate3dD *)ur;

@end
