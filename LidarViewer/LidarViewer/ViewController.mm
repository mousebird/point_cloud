//
//  ViewController.m
//  LidarViewer
//
//  Created by Steve Gifford on 10/19/15.
//  Copyright © 2015 mousebird consulting. All rights reserved.
//

#import "ViewController.h"
#import "WhirlyGlobeComponent.h"
#include <liblas/liblas.hpp>
#include <fstream>
#include <iostream>
#include <ogr_srs_api.h>
#include <cpl_port.h>
#include <ogr_spatialref.h>
#include <gdal.h>
#import "AFHTTPRequestOperation.h"
#import "PointShader.h"

static const char *vertexShaderTriPoint =
"uniform mat4  u_mvpMatrix;"
""
"attribute vec3 a_position;"
"attribute vec4 a_color;"
"attribute float a_size;"
""
"varying vec4 v_color;"
""
"void main()"
"{"
"   v_color = a_color;"
//"   gl_PointSize = a_size;"
"   gl_PointSize = 6.0;"
"   gl_Position = u_mvpMatrix * vec4(a_position,1.0);"
"}"
;

static const char *fragmentShaderTriPoint =
"precision lowp float;"
""
"varying vec4      v_color;"
""
"void main()"
"{"
"  gl_FragColor = v_color;"
"}"
;

MaplyShader *BuildPointShader(MaplyBaseViewController *viewC)
{
    MaplyShader *shader = [[MaplyShader alloc] initWithName:@"Lidar Point Shader" vertex:[NSString stringWithFormat:@"%s",vertexShaderTriPoint] fragment:[NSString stringWithFormat:@"%s",fragmentShaderTriPoint] viewC:viewC];
    
    if (shader)
        [viewC addShaderProgram:shader sceneName:shader.name];
    
    return shader;
}

@interface ViewController ()

@end

@implementation ViewController
{
    WhirlyGlobeViewController *globeViewC;
    NSArray *pointObjs;
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
    
    // If we're fetching one of the JSON tile specs, kick that off
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
    
    pointShader = BuildPointShader(globeViewC);


    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
    ^{
        NSString *path = [[NSBundle mainBundle] pathForResource:@"st-helens" ofType:@"laz"];
        if (path)
            [self readPoints:path];
    });
}

- (void)readPoints:(NSString *)fileName
{
    std::ifstream ifs;
    ifs.open([fileName cStringUsingEncoding:NSASCIIStringEncoding], std::ios::in | std::ios::binary);
    
//    std::string wktSRS = "NAD83 / UTM zone 10N|NAD83|";
    OGRSpatialReference inSRS("PROJCS[\"NAD83 / UTM zone 10N\",GEOGCS[\"NAD83\",DATUM[\"North_American_Datum_1983\",SPHEROID[\"GRS 1980\",6378137,298.257222101,AUTHORITY[\"EPSG\",\"7019\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6269\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4269\"]],PROJECTION[\"Transverse_Mercator\"],PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",-123],PARAMETER[\"scale_factor\",0.9996],PARAMETER[\"false_easting\",500000],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"Easting\",EAST],AXIS[\"Northing\",NORTH],AUTHORITY[\"EPSG\",\"26910\"]]");
//    inSRS.importFromProj4("+proj=utm +zone=10 +ellps=GRS80 +datum=NAD83 +units=m +no_defs");
    OGRSpatialReference outSRS;
    outSRS.importFromProj4("+proj=geocent +datum=WGS84");
    OGRCoordinateTransformation *trans = OGRCreateCoordinateTransformation(&inSRS,&outSRS);

    liblas::ReaderFactory f;
    liblas::Reader reader = f.CreateWithStream(ifs);
    liblas::Header header = reader.GetHeader();
    liblas::SpatialReference srs = header.GetSRS();
    std::string proj4Str = srs.GetProj4();
    
//    double scaleX = header.GetScaleX();
//    double scaleY = header.GetScaleY();
//    double scaleZ = header.GetScaleZ();
    double scaleX = 1.0, scaleY = 1.0, scaleZ = 1.0;
    
    int skip = 6;
    int maxPoints = 400000 * skip;
    int allPoints = 0;
    int displayedPoints = 0;

    bool done = false;
    NSMutableArray *pointsObjMut = [NSMutableArray array];
    while (!done)
    {
        @autoreleasepool
        {
            int numPoints = 0;
            
            MaplyPoints *points = [[MaplyPoints alloc] initWithNumPoints:(maxPoints/skip)];
            while (reader.ReadNextPoint() && numPoints < maxPoints)
            {
                // Get the point and convert to geocentric
                liblas::Point const& p = reader.GetPoint();
                double x,y,z;
                x = p.GetX(), y = p.GetY(); z = p.GetZ();
                trans->TransformEx(1, &x, &y, &z);
                
                x *= scaleX;  y *= scaleY;  z *= scaleZ;
                
                // Scale that to our fake display coordinates
                x /= 6371000; y /= 6371000; z /= 6371000;

                // Note: Nudging above the datum
                x *= 1.0005;  y *= 1.0005;  z *= 1.0005;

                if (numPoints % skip == 0) {
                    liblas::Color color = p.GetColor();
                    float red = color.GetRed() / 255.0,green = color.GetGreen() / 255.0,blue = color.GetBlue() / 255.0;
                    [points addDispCoordDoubleX:x y:y z:z];
                    [points addColorR:red g:green b:blue a:1.0];
                    displayedPoints++;
                }
                
                numPoints++;
                allPoints++;
            }
            
            if (numPoints < maxPoints)
                done = true;
            
            if (numPoints > 0)
            {
                MaplyComponentObject *compObj = [globeViewC addPoints:@[points] desc:
                                                 @{kMaplyColor: [UIColor redColor],
                                                   kMaplyPointSize: @(6.0),
                                                   kMaplyDrawPriority: @(10000000),
                                                   kMaplyShader: pointShader.name,
                                                   kMaplyZBufferRead: @(YES),
                                                   kMaplyZBufferWrite: @(YES)
                                                   }
                                                                 mode:MaplyThreadAny];
                if (compObj)
                    [pointsObjMut addObject:compObj];
            }
            
    //        if (allPoints > 1000000)
    //            break;
        }
    }
    
    pointObjs = pointsObjMut;
    
    NSLog(@"Displaying a total of %d points",displayedPoints);
}

@end
