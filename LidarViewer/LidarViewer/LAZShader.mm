//
//  LAZShader.m
//  LidarViewer
//
//  Created by Steve Gifford on 10/27/15.
//  Copyright © 2015 mousebird consulting. All rights reserved.
//

#import "LAZShader.h"

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
