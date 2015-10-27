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

@interface ViewController ()

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
    
    // Give us a tilt
    [globeViewC setTiltMinHeight:0.001 maxHeight:0.01 minTilt:1.21771169 maxTilt:0.0];
    
    // Add a base layer
    NSString *thisCacheDir = @"mapbox_satellite";
    NSString *jsonTileSpec = @"http://a.tiles.mapbox.com/v3/examples.map-zyt2v9k2.json";
    if (jsonTileSpec)
    {
        NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:jsonTileSpec]];
        
        AFHTTPRequestOperation *operation = [[AFHTTPRequestOperation alloc] initWithRequest:request];
        operation.responseSerializer = [AFJSONResponseSerializer serializer];
        [operation setCompletionBlockWithSuccess:^(AFHTTPRequestOperation *operation, id responseObject)
         {
             // Add a quad earth paging layer based on the tile spec we just fetched
             MaplyRemoteTileSource *tileSource = [[MaplyRemoteTileSource alloc] initWithTilespec:responseObject];
             tileSource.cacheDir = thisCacheDir;
             MaplyQuadImageTilesLayer *layer = [[MaplyQuadImageTilesLayer alloc] initWithCoordSystem:tileSource.coordSys tileSource:tileSource];
             layer.handleEdges = true;
             [globeViewC addLayer:layer];
         }
                                         failure:^(AFHTTPRequestOperation *operation, NSError *error)
         {
             NSLog(@"Failed to reach JSON tile spec at: %@",jsonTileSpec);
         }
         ];
        
        [operation start];
    }
    
    // Custom point shader (may get more complex later)
    pointShader = BuildPointShader(globeViewC);
    
    lazReader = [[LAZReader alloc] init];
    lazReader.shader = pointShader;

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
    ^{
        NSString *path = [[NSBundle mainBundle] pathForResource:@"st-helens" ofType:@"laz"];
        if (path)
            [lazReader readPoints:path viewC:globeViewC];
    });
}

@end
