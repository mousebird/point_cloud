//
//  ViewController.m
//  LidarViewer
//
//  Created by Steve Gifford on 10/19/15.
//  Copyright © 2015 mousebird consulting. All rights reserved.
//

#import "ViewController.h"
#import "WhirlyGlobeComponent.h"
#import "AFHTTPRequestOperation.h"
#import "LAZShader.h"
#import "LAZReader.h"

@interface ViewController () <WhirlyGlobeViewControllerDelegate>

@end

@implementation ViewController
{
    WhirlyGlobeViewController *globeViewC;
    LAZReader *lazReader;
    MaplyShader *pointShader;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    // Set up the globe
    globeViewC = [[WhirlyGlobeViewController alloc] init];
    [self.view addSubview:globeViewC.view];
    globeViewC.view.frame = self.view.bounds;
    [self addChildViewController:globeViewC];
    globeViewC.frameInterval = 2;
    globeViewC.performanceOutput = true;
    globeViewC.delegate = self;
    
    // Give us a tilt
    [globeViewC setTiltMinHeight:0.001 maxHeight:0.01 minTilt:1.21771169 maxTilt:0.0];
    
    // Start location
    WhirlyGlobeViewControllerAnimationState *viewState = [[WhirlyGlobeViewControllerAnimationState alloc] init];
    viewState.heading = -3.118891;
    viewState.height = 0.003194;
    viewState.tilt   = 0.988057;
    viewState.pos = MaplyCoordinateDMake(-2.132972, 0.808100);
    [globeViewC setViewState:viewState];
    
    // Add a base layer
    NSString * baseCacheDir = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) objectAtIndex:0];
    NSString * cartodbTilesCacheDir = [NSString stringWithFormat:@"%@/cartodbtiles/", baseCacheDir];
    int maxZoom = 18;
    MaplyRemoteTileSource *tileSource = [[MaplyRemoteTileSource alloc] initWithBaseURL:@"http://otile1.mqcdn.com/tiles/1.0.0/sat/" ext:@"png" minZoom:0 maxZoom:maxZoom];
    tileSource.cacheDir = cartodbTilesCacheDir;
    MaplyQuadImageTilesLayer *layer = [[MaplyQuadImageTilesLayer alloc] initWithCoordSystem:tileSource.coordSys tileSource:tileSource];
    layer.handleEdges = true;
    layer.coverPoles = true;
    layer.drawPriority = 0;
    [globeViewC addLayer:layer];
    
    // Custom point shader (may get more complex later)
    pointShader = BuildPointShader(globeViewC);
    
    lazReader = [[LAZReader alloc] init];
    lazReader.shader = pointShader;
    
    // Database to read
//    NSString *dbName = @"st-helens";
//    NSString *dbName = @"Mount St Helens Nov 20 2004";
    NSString *dbName = @"CA_DayFire_2007_000472";
    NSString *dbExt = @"laz";

    NSString *docDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *dbPath = [[docDir stringByAppendingPathComponent:dbName] stringByAppendingPathExtension:dbExt];
    if (![[NSFileManager defaultManager] fileExistsAtPath:dbPath])
    {
        dbPath = [[NSBundle mainBundle] pathForResource:dbName ofType:dbExt];
        if (![[NSFileManager defaultManager] fileExistsAtPath:dbPath])
            dbPath = nil;
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
    ^{
        if (dbPath)
            [lazReader readPoints:dbPath viewC:globeViewC];
    });
}

//- (void)globeViewController:(WhirlyGlobeViewController *__nonnull)viewC didMove:(MaplyCoordinate *__nonnull)corners
//{
//    WhirlyGlobeViewControllerAnimationState *viewState = [viewC getViewState];
//    
//    NSLog(@"Loc = (%f,%f), heading = %f, tilt = %f, height = %f",viewState.pos.x,viewState.pos.y,viewState.heading,viewState.tilt,viewState.height);
//}

@end
