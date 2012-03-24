#ifndef GEOMETRY_H
#define GEOMETRY_H
/*
  Boron OpenGL Draw Program
  Copyright 2008-2010 Karl Robillard

  This file is part of the Boron programming language.

  Boron is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Boron is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Boron.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "glh.h"
#include "urlan.h"


typedef struct
{
    GLenum  mode;
    GLsizei count;
    GLuint  offset;
}
Primitives;


//#define GEOM_NORMALS  0x01
//#define GEOM_TEXTURE  0x02
//#define GEOM_COLORS   0x04
#define GEOM_INSIDE     0x08
#define GEOM_DYN_ATTR   0x10
#define GEOM_DYN_IDX    0x20

typedef struct
{
    UBuffer attr;
    UBuffer idx;
    UBuffer prim;
    GLuint buf[2];
    uint16_t flags;
    int16_t attrSize;
    int16_t vertOff;
    int16_t normOff;
    int16_t colorOff;
    int16_t uvOff;
}
Geometry;


void      geo_init( Geometry*, const char* attrib,
                    int vertCount, int idxCount, int primCount );
void      geo_free( Geometry* );
void      geo_reserve( Geometry*, int vertCount, int idxCount, int primCount );
float*    geo_addVerts( Geometry*, int vcount );
float*    geo_attrTV( Geometry*, float* vert, const float* tv );
float*    geo_attrNTV( Geometry*, float* vert, const float* ntv );
void      geo_primBegin( Geometry*, GLenum mode );
void      geo_primEnd( Geometry* );
uint16_t* geo_addIndices( Geometry*, int count );
void      geo_addOrderedIndices( Geometry*, int count );
void      geo_bufferData( const Geometry* );

void geo_quadSkin( Geometry*, const int16_t* rect,
                   const int16_t* tsize, const int16_t* tc, int skin );
void geo_image( Geometry*, GLfloat x, GLfloat y, GLfloat x2, GLfloat y2 );
void geo_box( Geometry*, const float* minv, const float* maxv, int inside );
void geo_sphere( Geometry*, float radius, int slices, int stacks,
                 int inside );


#define geo_vertCount(geo)  ((geo)->attr.used / (geo)->attrSize)


#endif /*GEOMETRY_H*/
