//
//  LAZShader.h
//  LidarViewer
//
//  Created by Steve Gifford on 10/27/15.
//  Copyright © 2015 mousebird consulting. All rights reserved.
//

#import "WhirlyGlobeComponent.h"

// Name of the point size uniform attribute.
extern NSString* const kLAZShaderPointSize;

// Name of the zMin uniform attribute (for the ramp shader)
extern NSString* const kLAZShaderZMin;

// Name of the zMax uniform attribute (for the ramp shader)
extern NSString* const kLAZShaderZMax;

// This is a simple point shader that passes colors in
MaplyShader *BuildPointShader(MaplyBaseViewController *viewC);

// This shader uses a ramp shader texture to look up colors
MaplyShader *BuildRampPointShader(MaplyBaseViewController *viewC,UIImage *colorRamp);
