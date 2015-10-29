//
//  LAZReader.h
//  LidarViewer
//
//  Created by Steve Gifford on 10/27/15.
//  Copyright © 2015 mousebird consulting. All rights reserved.
//

#import "WhirlyGlobeComponent.h"

/** The LAZReader will work its way through the a LAZ file, converting points
    to visual as it goes.
  */
@interface LAZReader : NSObject

// Shader to use for the points
@property (nonatomic) MaplyShader *shader;

// Maximum number of point we'll display (e.g. 2,000,000)
@property (nonatomic) int maxPoints;

// Number of points to skip
@property (nonatomic) int skipPoints;

// This will read the points until it's done.  Run it on another thread.
- (void)readPoints:(NSString *)fileName viewC:(MaplyBaseViewController *)viewC;

@end
