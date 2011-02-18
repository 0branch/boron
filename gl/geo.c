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


#include <assert.h>
#include "geo.h"
#include "os_math.h"
#include "math3d.h"


void geo_init( Geometry* geo, const char* attrib,
               int vertCount, int idxCount, int primCount )
{
    int c;
    int off = 0;

    geo->flags    = 0;
    geo->attrSize = 0;
    geo->vertOff  = -1;
    geo->normOff  = -1;
    geo->colorOff = -1;
    geo->uvOff    = -1;

    while( (c = *attrib++) )
    {
        switch( c )
        {
            case 'c':
                geo->colorOff = off;
                goto offset_3;

            case 'n':
                geo->normOff = off;
                goto offset_3;

            case 't':
                geo->uvOff = off;
                off += 2;
                geo->attrSize += 2;
                break;

            case 'v':
                geo->vertOff = off;
offset_3:
                off += 3;
                geo->attrSize += 3;
                break;
        }
    }

    ur_arrInit( &geo->idx,  sizeof(uint16_t),   idxCount );
    ur_arrInit( &geo->attr, sizeof(float),      vertCount * geo->attrSize );
    ur_arrInit( &geo->prim, sizeof(Primitives), primCount );

    glGenBuffers( 2, geo->buf );
    // geo_emitBind() must be called or buffers will never be deleted.
}


void geo_free( Geometry* geo )
{
    ur_arrFree( &geo->idx );
    ur_arrFree( &geo->attr );
    ur_arrFree( &geo->prim );
}


void geo_reserve( Geometry* geo, int vertCount, int idxCount, int primCount )
{
    ur_arrReserve( &geo->idx,  geo->idx.used + idxCount );
    ur_arrReserve( &geo->attr, geo->attr.used + (vertCount * geo->attrSize) );
    ur_arrReserve( &geo->prim, geo->prim.used + primCount );
}


float* geo_addVerts( Geometry* geo, int vcount )
{
    float* vert;
    UBuffer* arr = &geo->attr;
    int size = geo->attrSize * vcount;

    ur_arrReserve( arr, arr->used + size );
    vert = arr->ptr.f + arr->used;
    arr->used += size;

    return vert;
}


void geo_primBegin( Geometry* geo, GLenum mode )
{
    Primitives* prim;
    UBuffer* arr = &geo->prim;

    ur_arrExpand1( Primitives, arr, prim );

    prim->mode   = mode;
    prim->count  = 0;
    prim->offset = geo->idx.used;
}


void geo_primEnd( Geometry* geo )
{
    if( geo->prim.used )
    {
        Primitives* prim = (Primitives*) geo->prim.ptr.v;
        prim += geo->prim.used - 1;
        prim->count = geo->idx.used - prim->offset;
    }
}


uint16_t* geo_addIndices( Geometry* geo, int count )
{
    uint16_t* ptr;
    UBuffer* arr = &geo->idx;

    ur_arrReserve( arr, arr->used + count );
    ptr = arr->ptr.u16 + arr->used;
    arr->used += count;

    return ptr;
}


/*
   Call before geo_addVerts().
*/
void geo_addOrderedIndices( Geometry* geo, int count )
{
    int idx = geo_vertCount( geo );
    uint16_t* it  = geo_addIndices( geo, count );
    uint16_t* end = it + count;
    while( it != end )
        *it++ = idx++;
}


void geo_bufferData( const Geometry* geo )
{
    if( geo->attr.used )
    {
        glBindBuffer( GL_ARRAY_BUFFER, geo->buf[0] );
        glBufferData( GL_ARRAY_BUFFER,
                      sizeof(float) * geo->attr.used, geo->attr.ptr.f,
                      GL_STATIC_DRAW );
    }

    if( geo->idx.used )
    {
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, geo->buf[1] );
        glBufferData( GL_ELEMENT_ARRAY_BUFFER,
                      sizeof(uint16_t) * geo->idx.used, geo->idx.ptr.v,
                      GL_STATIC_DRAW );
    }
}


/*--------------------------------------------------------------------------*/


float* geo_attrTV( Geometry* geo, float* vert, const float* tv )
{
    float* attr;

    attr = vert + geo->uvOff;
    *attr++ = *tv++;
    *attr   = *tv++;

    attr = vert + geo->vertOff;
    *attr++ = *tv++;
    *attr++ = *tv++;
    *attr   = *tv;

    return vert + geo->attrSize;
}


float* geo_attrNTV( Geometry* geo, float* vert, const float* ntv )
{
    float* attr;

    if( geo->normOff > -1 )
    {
        attr = vert + geo->normOff;
        *attr++ = *ntv++;
        *attr++ = *ntv++;
        *attr   = *ntv++;
    }
    else
    {
        ntv += 3;
    }

    attr = vert + geo->uvOff;
    *attr++ = *ntv++;
    *attr   = *ntv++;

    attr = vert + geo->vertOff;
    *attr++ = *ntv++;
    *attr++ = *ntv++;
    *attr   = *ntv;

    return vert + geo->attrSize;
}


/*
   Draw textured rectangle.
   If skinned, draw 9 quads with a single triangle strip.

  \param rect   View rectangle (x,y,width,height)
  \param tsize  Texture size (x,y)
  \param tc     Texture coordinates (left,bot,right,top,corner-x,corner-y)
  \param skin   If non-zero then tc corner size must be set.
*/
void geo_quadSkin( Geometry* geo, const int16_t* rect,
                   const int16_t* tsize, const int16_t* tc, int skin )
{
    GLint x  = rect[0];
    GLint y  = rect[1];
    GLint x2 = x + rect[2];
    GLint y2 = y + rect[3];
    GLfloat ws = 1.0 / ((GLfloat) tsize[0]);
    GLfloat hs = 1.0 / ((GLfloat) tsize[1]);
    GLfloat tx1 = ((GLfloat) tc[0]) * ws;
    GLfloat ty1 = 1.0 - ((GLfloat) tc[1]) * hs;
    GLfloat tx2 = ((GLfloat) (tc[2] + 1)) * ws;
    GLfloat ty2 = 1.0 - ((GLfloat) (tc[3] + 1)) * hs;
    float* vert;
    float tv[5];

#define QS_VERT(tx,ty,x,y) \
    tv[0] = tx; \
    tv[1] = ty; \
    tv[2] = x; \
    tv[3] = y; \
    vert = geo_attrTV( geo, vert, tv )

    tv[4] = 0.0f;

    if( skin )
    {
        GLint cx = tc[4];
        GLint cy = tc[5];
        GLfloat tcx = ((GLfloat) cx) * ws;
        GLfloat tcy = ((GLfloat) cy) * hs;


        geo_primBegin( geo, GL_TRIANGLE_STRIP );

        geo_addOrderedIndices( geo, 22 );
        vert = geo_addVerts( geo, 22 );

        /*
           15\ 17\ 19\ 21
            - \16 \18 \20

           14 /12 /10 / 8
           13/ 11/  9/  -

            1\  3\  5\  7
            0 \ 2 \ 4 \ 6
        */

        QS_VERT( tx1,       ty1,       x,       y );
        QS_VERT( tx1,       ty1 - tcy, x,       y + cy );
        QS_VERT( tx1 + tcx, ty1,       x  + cx, y );
        QS_VERT( tx1 + tcx, ty1 - tcy, x  + cx, y + cy );
        QS_VERT( tx2 - tcx, ty1,       x2 - cx, y );
        QS_VERT( tx2 - tcx, ty1 - tcy, x2 - cx, y + cy );
        QS_VERT( tx2,       ty1,       x2,      y );
        QS_VERT( tx2,       ty1 - tcy, x2,      y + cy );

        QS_VERT( tx2,       ty2 + tcy, x2,      y2 - cy );
        QS_VERT( tx2 - tcx, ty1 - tcy, x2 - cx, y  + cy );
        QS_VERT( tx2 - tcx, ty2 + tcy, x2 - cx, y2 - cy );
        QS_VERT( tx1 + tcx, ty1 - tcy, x  + cx, y  + cy );
        QS_VERT( tx1 + tcx, ty2 + tcy, x  + cx, y2 - cy );
        QS_VERT( tx1,       ty1 - tcy, x,       y  + cy );
        QS_VERT( tx1,       ty2 + tcy, x,       y2 - cy );

        QS_VERT( tx1,       ty2,       x,       y2 );
        QS_VERT( tx1 + tcx, ty2 + tcy, x  + cx, y2 - cy );
        QS_VERT( tx1 + tcx, ty2,       x  + cx, y2 );
        QS_VERT( tx2 - tcx, ty2 + tcy, x2 - cx, y2 - cy );
        QS_VERT( tx2 - tcx, ty2,       x2 - cx, y2 );
        QS_VERT( tx2,       ty2 + tcy, x2,      y2 - cy );
        QS_VERT( tx2,       ty2,       x2,      y2 );
    }
    else
    {
        geo_primBegin( geo, GL_TRIANGLE_FAN );

        geo_addOrderedIndices( geo, 4 );
        vert = geo_addVerts( geo, 4 );

        QS_VERT( tx1, ty1, x,  y );
        QS_VERT( tx2, ty1, x2, y );
        QS_VERT( tx2, ty2, x2, y2 );
        QS_VERT( tx1, ty2, x,  y2 );
    }

    geo_primEnd( geo );
}


/*
  Draw textured rectangle with UVs set to show entire texture.
*/
void geo_image( Geometry* geo, GLfloat x, GLfloat y, GLfloat x2, GLfloat y2 )
{
    float* vert;
    float* uv;

    geo_primBegin( geo, GL_TRIANGLE_FAN );

    geo_addOrderedIndices( geo, 4 );
    vert = geo_addVerts( geo, 4 );

    assert( geo->uvOff > -1 );
    uv = vert + geo->uvOff;
    vert += geo->vertOff;

#define IMG_VERT(tx,ty,x,y) \
    uv[0] = tx; \
    uv[1] = ty; \
    uv += geo->attrSize; \
    vert[0] = x; \
    vert[1] = y; \
    vert[2] = 0.0f; \
    vert += geo->attrSize

    IMG_VERT( 0.0f, 1.0f, x,  y );
    IMG_VERT( 1.0f, 1.0f, x2, y );
    IMG_VERT( 1.0f, 0.0f, x2, y2 );
    IMG_VERT( 0.0f, 0.0f, x,  y2 );

    geo_primEnd( geo );
}


static void negateVec3( float* v )
{
    v[0] = -v[0];
    v[1] = -v[1];
    v[2] = -v[2];
}


void geo_box( Geometry* geo, const float* minv, const float* maxv, int inside )
{
    static float boxNormal[ 8 ] = { 0.0, 0.0, -1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };

#define BOX_xyz 0
#define BOX_xyZ 1
#define BOX_xYz 2
#define BOX_xYZ 3
#define BOX_Xyz 4
#define BOX_XyZ 5
#define BOX_XYz 6
#define BOX_XYZ 7

    static uint8_t boxMax[ 4 * 6 ] =
    {
        BOX_Xyz, BOX_xyz, BOX_xYz, BOX_XYz,     // -Z
        BOX_xyz, BOX_Xyz, BOX_XyZ, BOX_xyZ,     // -Y
        BOX_xyz, BOX_xyZ, BOX_xYZ, BOX_xYz,     // -X
        BOX_xyZ, BOX_XyZ, BOX_XYZ, BOX_xYZ,     //  Z
        BOX_xYz, BOX_xYZ, BOX_XYZ, BOX_XYz,     //  Y
        BOX_XyZ, BOX_Xyz, BOX_XYz, BOX_XYZ      //  X
    };
    int uv;
    float* vert;
    float ntv[8];
    uint16_t* ip;
    int idx;
    int i, j;
    int maxMask;
    uint8_t* maskPtr = boxMax;

    geo_reserve( geo, 4 * 6, 4 * 6, 6 );

    for( i = 0; i < 6; ++i )
    {
        geo_primBegin( geo, GL_TRIANGLE_FAN );

            idx  = geo_vertCount( geo );
            vert = geo_addVerts( geo, 4 );

            ntv[0] = boxNormal[ i ];
            ntv[1] = boxNormal[ i + 1 ];
            ntv[2] = boxNormal[ i + 2 ];

            if( inside )
                negateVec3( ntv );

            uv = 0x78;      // uvs = 0,0 1,0 1,1 0,1 (binary mask 01111000)
            for( j = 0; j < 4; ++j )
            {
                ntv[3] = (uv & 2) ? 1.0f : 0.0f;
                ntv[4] = (uv & 1) ? 1.0f : 0.0f;
                uv >>= 2;

                maxMask = *maskPtr++;
                ntv[5] = (maxMask & 4) ? maxv[0] : minv[0];
                ntv[6] = (maxMask & 2) ? maxv[1] : minv[1];
                ntv[7] = (maxMask & 1) ? maxv[2] : minv[2];
                vert = geo_attrNTV( geo, vert, ntv );
            }

            ip = geo_addIndices( geo, 4 );

            if( inside )
            {
                *ip++ = idx + 3;
                *ip++ = idx + 2;
                *ip++ = idx + 1;
                *ip   = idx;
            }
            else
            {
                *ip++ = idx;
                *ip++ = idx + 1;
                *ip++ = idx + 2;
                *ip   = idx + 3;
            }

        geo_primEnd( geo );
    }
}


static void sphereVertex( Geometry* geo, float lat, float lon,
                          float x, float y, float z )
{
    float* it;
    float* vert = geo_addVerts( geo, 1 );

    if( geo->normOff > -1 )
    {
        it = vert + geo->normOff;
        if( geo->flags & GEOM_INSIDE )
        {
            *it++ = -x;
            *it++ = -y;
            *it++ = -z;
        }
        else
        {
            *it++ = x;
            *it++ = y;
            *it++ = z;
        }
        ur_normalize( it - 3 );
    }

    if( geo->uvOff > -1 )
    {
        it = vert + geo->uvOff;
        *it++ = lon / (2.0 * M_PI);     // s
        *it++ = lat / M_PI;             // t
    }

    if( geo->vertOff > -1 )
    {
        it = vert + geo->vertOff;
        *it++ = x;
        *it++ = y;
        *it++ = z;
    }
}


void geo_sphere( Geometry* geo, float radius, int slices, int stacks,
                 int inside )
{
    float z;
    float lat;
    float lon;
    float stackRadius;
    float latInc = M_PI / stacks;
    float lonInc = (2.0 * M_PI) / slices;
    int n, s;


    n = slices * stacks;
    geo_reserve( geo, n + stacks, n * 2, slices );
    if( inside )
        geo->flags |= GEOM_INSIDE;

    sphereVertex( geo,  0.0, M_PI, 0.0, 0.0,  radius );
    sphereVertex( geo, M_PI, M_PI, 0.0, 0.0, -radius );

    for( n = 1, lat = latInc; n < stacks; ++n, lat += latInc )
    {
        stackRadius = radius * mathSineF( lat );
        z = radius * mathCosineF( lat );

        for( s = -1, lon = 0.0f; s < slices; ++s, lon += lonInc )
        {
            sphereVertex( geo, lat, lon,
                          stackRadius * mathCosineF( lon ),
                          stackRadius * mathSineF( lon ),
                          z );
        }
    }

    {
        uint16_t* it;
        int ia, ib;
        int istart = 2;

        it = geo->idx.ptr.u16;

        for( s = 0; s < slices; ++s )
        {
            geo_primBegin( geo, GL_TRIANGLE_STRIP );

            ia = istart++;
          //ib = (s == (slices - 1)) ? 2 : ia + 1;
            ib = ia + 1;

            *it++ = 0;
            for( n = 1; n < stacks; ++n )
            {
                *it++ = ia;
                *it++ = ib;
                ia += slices + 1;
                ib += slices + 1;
            }
            *it++ = 1;

            geo->idx.used += stacks * 2;

            geo_primEnd( geo );
        }
    }
}


/*EOF*/
