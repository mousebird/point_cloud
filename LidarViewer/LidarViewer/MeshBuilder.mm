//
//  MeshBuilder.cpp
//  LidarViewer
//
//  Created by Steve Gifford on 6/29/16.
//  Copyright © 2016 mousebird consulting. All rights reserved.
//

#include "MeshBuilder.h"

using namespace WhirlyKit;

MeshBuilder::MeshBuilder(int sizeX,int sizeY,const Point2d &ll,const Point2d &ur)
: sizeX(sizeX), sizeY(sizeY), ll(ll), ur(ur)
{
    span = ur - ll;
    minZs.resize(sizeX*sizeY,std::numeric_limits<double>::max());
}

void MeshBuilder::addPoint(const Point3d &pt)
{
    Point2d df((pt.x()-ll.x())/span.x() * sizeX,(pt.y()-ll.y())/span.y() * sizeY);
    int whichX = df.x(), whichY = df.y();
    whichX = std::min(sizeX-1,std::max(0,whichX));
    whichY = std::min(sizeY-1,std::max(0,whichY));
    
    double &z = minZs[whichY*sizeX+whichX];
    z = std::min(z,pt.z());
}

VectorTrianglesRef MeshBuilder::makeMesh()
{
    // Look for the minimum overall
    double minZ = std::numeric_limits<double>::max();
    for (auto z : minZs)
        minZ = std::min(z,minZ);
    
    // Didn't get any real samples
    if (minZ == std::numeric_limits<double>::max())
        return NULL;
    
    // Fill out missing samples with the minimum
    for (auto &z : minZs)
        if (z == std::numeric_limits<double>::max())
            z = minZ;
    
    VectorTrianglesRef mesh = VectorTriangles::createTriangles();

    // Generate the points
    Point2d span2((ur.x()-ll.x())/sizeX,(ur.y()-ll.y())/sizeY);
    mesh->pts.reserve((sizeX+1)*(sizeY+1));
    for (int iy=0;iy<=sizeY;iy++)
    {
        for (int ix=0;ix<=sizeX;ix++)
        {
            Point2d pt = ll + Point2d(ix*span2.x(),iy*span2.y());
            int whichX = std::max(ix,sizeX-1);
            int whichY = std::max(iy,sizeY-1);
            double z = minZs[whichY*sizeX+whichX];
            mesh->pts.push_back(Point3f(pt.x(),pt.y(),z));
        }
    }

    // Generate the triangles
    mesh->tris.reserve(sizeX*sizeY);
    for (int iy=0;iy<=sizeY;iy++)
        for (int ix=0;ix<=sizeX;ix++)
        {
            VectorTriangles::Triangle tri;
            tri.pts[0] = iy*(sizeX+1)+ix;
            tri.pts[1] = iy*(sizeX+1)+ix+1;
            tri.pts[2] = (iy+1)*(sizeX+1)+ix+1;
            mesh->tris.push_back(tri);
            tri.pts[0] = iy*(sizeX+1)+ix;
            tri.pts[1] = (iy+1)*(sizeX+1)+ix+1;
            tri.pts[2] = (iy+1)*(sizeX+1)+ix;
            mesh->tris.push_back(tri);
        }
    
    return mesh;
}
