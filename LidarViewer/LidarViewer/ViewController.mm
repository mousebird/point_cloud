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
#import "LAZQuadReader.h"

@interface ViewController () <WhirlyGlobeViewControllerDelegate>

@end

@implementation ViewController
{
    WhirlyGlobeViewController *globeViewC;
    MaplyShader *pointShaderRamp,*pointShaderColor;
}

// Look for a file in the bundle or in the doc dir
- (NSString *)findFile:(NSString *)base ext:(NSString *)ext
{
    NSString *docDir = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *dbPath = [[docDir stringByAppendingPathComponent:base] stringByAppendingPathExtension:ext];
    if (![[NSFileManager defaultManager] fileExistsAtPath:dbPath])
    {
        dbPath = [[NSBundle mainBundle] pathForResource:base ofType:ext];
        if (![[NSFileManager defaultManager] fileExistsAtPath:dbPath])
            dbPath = nil;
    }
    
    return dbPath;
}

// Maximum number of points we'd like to display
static int MaxDisplayedPoints = 3000000;

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
    layer.color = [UIColor colorWithWhite:0.5 alpha:1.0];
    [globeViewC addLayer:layer];
    
    // Shader Shaders for color and ramp versions
    pointShaderColor = BuildPointShader(globeViewC);
    pointShaderRamp = BuildRampPointShader(globeViewC,[UIImage imageNamed:@"colorramp.png"]);
    
    [self addLaz:@"st-helens-quad-data" shader:pointShaderColor];
    [self addLaz:@"10SGC3969_1-quad-data" shader:pointShaderRamp];
    [self addLaz:@"stadium-utm-quad-data" shader:pointShaderColor];
    [self addLaz:@"44123A1305_ALL-quad-data" shader:pointShaderRamp];
    
    // Note: Should toss up a warning if we can't find the files

//    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
//    ^{
//        if (indexPath && lazPath)
//            [lazReader readPoints:lazPath viewC:globeViewC];
//    });
}

- (void)addLaz:(NSString *)dbName shader:(MaplyShader *)shader
{
    // Database to read
    NSString *indexPath = [self findFile:dbName ext:@"sqlite"];

    if (indexPath)
    {
        // Set up the paging logic
        //        quadDelegate = [[LAZQuadReader alloc] initWithDB:lazPath indexFile:indexPath];
        MaplyCoordinate3dD ll,ur;
        LAZQuadReader *quadDelegate = [[LAZQuadReader alloc] initWithDB:indexPath];
        quadDelegate.shader = shader;
        [quadDelegate getBoundsLL:&ll ur:&ur];
        
        // Start location
        WhirlyGlobeViewControllerAnimationState *viewState = [[WhirlyGlobeViewControllerAnimationState alloc] init];
        viewState.heading = -3.118891;
        viewState.height = 0.003194;
        viewState.tilt   = 0.988057;
        MaplyCoordinate center = [[quadDelegate coordSys] localToGeo:[quadDelegate getCenter]];
        viewState.pos = MaplyCoordinateDMake(center.x,center.y);
        [globeViewC setViewState:viewState];
        
        MaplyQuadPagingLayer *lazLayer = [[MaplyQuadPagingLayer alloc] initWithCoordSystem:quadDelegate.coordSys delegate:quadDelegate];
        // It takes no real time to fetch from the database.
        // All the threading is in projecting coordinates
        lazLayer.numSimultaneousFetches = 4;
        lazLayer.maxTiles = [quadDelegate getNumTilesFromMaxPoints:MaxDisplayedPoints];
        NSLog(@"Loading %d tiles, max",lazLayer.maxTiles);
        lazLayer.importance = 128*128;
        lazLayer.minTileHeight = ll.z;
        lazLayer.maxTileHeight = ur.z;
        [globeViewC addLayer:lazLayer];
    }
}

//- (void)globeViewController:(WhirlyGlobeViewController *__nonnull)viewC didMove:(MaplyCoordinate *__nonnull)corners
//{
//    WhirlyGlobeViewControllerAnimationState *viewState = [viewC getViewState];
//    
//    NSLog(@"Loc = (%f,%f), heading = %f, tilt = %f, height = %f",viewState.pos.x,viewState.pos.y,viewState.heading,viewState.tilt,viewState.height);
//}

@end
