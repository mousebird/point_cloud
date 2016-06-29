//
//  MeshBuilder.hpp
//  LidarViewer
//
//  Created by Steve Gifford on 6/29/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#ifndef MeshBuilder_h
#define MeshBuilder_h

#import <Foundation/Foundation.h>
#import <WhirlyGlobe.h>

/** Used to build a mesh underneath a random collection of points.
    We can use the mesh for tap intersection.
  */
class MeshBuilder
{
public:
    // Construct with the size of the grid we'll build and the extents
    MeshBuilder(int sizeX,int sizeY,const WhirlyKit::Point2d &ll,const WhirlyKit::Point2d &ur);

    // Add a point for evaluation.  We'll snap to the edges
    void addPoint(const WhirlyKit::Point3d &pt);
    
    WhirlyKit::VectorTrianglesRef makeMesh();
    
protected:
    int sizeX,sizeY;
    WhirlyKit::Point2d ll,ur;
    WhirlyKit::Point2d span;
    std::vector<double> minZs;
};

#endif /* MeshBuilder_hpp */
