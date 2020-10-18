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
                      (geo->flags & GEOM_DYN_ATTR) ? GL_DYNAMIC_DRAW
                                                   : GL_STATIC_DRAW );
    }

    if( geo->idx.used )
    {
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, geo->buf[1] );
        glBufferData( GL_ELEMENT_ARRAY_BUFFER,
                      sizeof(uint16_t) * geo->idx.used, geo->idx.ptr.v,
                      (geo->flags & GEOM_DYN_IDX) ? GL_DYNAMIC_DRAW
                                                  : GL_STATIC_DRAW );
    }
}


/*--------------------------------------------------------------------------*/


/*
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
*/


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
  \param rect   View rectangle (x,y,width,height)
  \param tsize  Texture size (x,y)
  \param tc     Texture coordinates (left,bot,right,top)
*/
void quad_init( QuadDim* qd, const int16_t* rect, const int16_t* tsize,
                const int16_t* tc )
{
    qd->x   = (float) rect[0];
    qd->y   = (float) rect[1];
    qd->x2  = qd->x + (float) rect[2];
    qd->y2  = qd->y + (float) rect[3];
    qd->us  = 1.0 / ((float) tsize[0]);
    qd->vs  = 1.0 / ((float) tsize[1]);
    qd->tx1 = ((float) tc[0]) * qd->us;
    qd->tx2 = ((float) (tc[2] + 1)) * qd->us;
#ifdef IMAGE_BOTTOM_AT_0
    qd->ty1 = ((float) tc[1]) * qd->vs;
    qd->ty2 = ((float) (tc[3] + 1)) * qd->vs;
#else
    qd->ty1 = 1.0 - ((float) tc[1]) * qd->vs;
    qd->ty2 = 1.0 - ((float) (tc[3] + 1)) * qd->vs;
#endif

    //printf( "KR quad %f,%f %f,%f\n", qd->x,qd->y, qd->x2,qd->y2 );
    //printf( "        %f,%f %f,%f\n", qd->tx1,qd->ty1, qd->tx2,qd->ty2 );
}


#define Q_VERT(s,t,x,y) \
    tp[0] = s; \
    tp[1] = t; \
    tp += stride; \
    vp[0] = x; \
    vp[1] = y; \
    vp[2] = 0.0f; \
    vp += stride

#define Q_VER_(s,t,x,y) \
    tp[0] = s; \
    tp[1] = t; \
    vp[0] = x; \
    vp[1] = y; \
    vp[2] = 0.0f

/*
   Emit two triangles (6 vertices).
*/
void quad_emitVT( const QuadDim* qd, float* vp, float* tp, int stride )
{
    Q_VERT( qd->tx1, qd->ty1, qd->x,  qd->y );
    Q_VERT( qd->tx2, qd->ty1, qd->x2, qd->y );
    Q_VERT( qd->tx2, qd->ty2, qd->x2, qd->y2 );

    Q_VERT( qd->tx1, qd->ty2, qd->x,  qd->y2 );
    Q_VERT( qd->tx1, qd->ty1, qd->x,  qd->y );
    Q_VER_( qd->tx2, qd->ty2, qd->x2, qd->y2 );
}


/*
   Emit two triangles (4 vertices).
*/
void quad_emitFanVT( const QuadDim* qd, float* vp, float* tp, int stride )
{
    Q_VERT( qd->tx1, qd->ty1, qd->x,  qd->y );
    Q_VERT( qd->tx2, qd->ty1, qd->x2, qd->y );
    Q_VERT( qd->tx2, qd->ty2, qd->x2, qd->y2 );
    Q_VER_( qd->tx1, qd->ty2, qd->x,  qd->y2 );
}


/*
  Emit strip of 20 triangles (2 degenerate, 22 vertices).

  \param cx     Corner X size (texel width)
  \param cy     Corner Y size (texel height)
*/
void quad_emitSkinVT( const QuadDim* qd, float cx, float cy,
                      float* vp, float* tp, int stride )
{
    float tcx = cx * qd->us;
    float tcy = cy * qd->vs;
    float xA, xB;
    float yA, yB;
    float uA, uB;
    float vA, vB;

    /*
       15\ 17\ 19\ 21
        - \16 \18 \20

       14 /12 /10 / 8
       13/ 11/  9/  -

        1\  3\  5\  7
        0 \ 2 \ 4 \ 6
    */

    xA = qd->x + cx;
    xB = qd->x2 - cx;
    yA = qd->y;
    yB = qd->y + cy;
    uA = qd->tx1 + tcx;
    uB = qd->tx2 - tcx;
#ifdef IMAGE_BOTTOM_AT_0
    vA = qd->ty1 + tcy;
    vB = qd->ty2 - tcy;
#else
    vA = qd->ty1 - tcy;
    vB = qd->ty2 + tcy;
#endif

    Q_VERT( qd->tx1, qd->ty1, qd->x,  yA );
    Q_VERT( qd->tx1, vA,      qd->x,  yB );
    Q_VERT( uA,      qd->ty1, xA,     yA );
    Q_VERT( uA,      vA,      xA,     yB );
    Q_VERT( uB,      qd->ty1, xB,     yA );
    Q_VERT( uB,      vA,      xB,     yB );
    Q_VERT( qd->tx2, qd->ty1, qd->x2, yA );
    Q_VERT( qd->tx2, vA,      qd->x2, yB );

    yA  = qd->y2 - cy;

    Q_VERT( qd->tx2, vB,      qd->x2, yA );
    Q_VERT( uB,      vA,      xB,     yB );
    Q_VERT( uB,      vB,      xB,     yA );
    Q_VERT( uA,      vA,      xA,     yB );
    Q_VERT( uA,      vB,      xA,     yA );
    Q_VERT( qd->tx1, vA,      qd->x,  yB );
    Q_VERT( qd->tx1, vB,      qd->x,  yA );

    yB = qd->y2;

    Q_VERT( qd->tx1, qd->ty2, qd->x,  yB );
    Q_VERT( uA,      vB,      xA,     yA );
    Q_VERT( uA,      qd->ty2, xA,     yB );
    Q_VERT( uB,      vB,      xB,     yA );
    Q_VERT( uB,      qd->ty2, xB,     yB );
    Q_VERT( qd->tx2, vB,      qd->x2, yA );
    Q_VER_( qd->tx2, qd->ty2, qd->x2, yB );
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
    QuadDim qd;
    float* vert;


    quad_init( &qd, rect, tsize, tc );

    if( skin )
    {
        geo_primBegin( geo, GL_TRIANGLE_STRIP );
        geo_addOrderedIndices( geo, 22 );
        vert = geo_addVerts( geo, 22 );

        quad_emitSkinVT( &qd, (float) tc[4], (float) tc[5],
                         vert + geo->vertOff, vert + geo->uvOff,
                         geo->attrSize );
    }
    else
    {
        geo_primBegin( geo, GL_TRIANGLE_FAN );
        geo_addOrderedIndices( geo, 4 );
        vert = geo_addVerts( geo, 4 );

        quad_emitFanVT( &qd, vert + geo->vertOff, vert + geo->uvOff,
                        geo->attrSize );
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

    IMG_VERT( 0.0f, 0.0f, x,  y );
    IMG_VERT( 1.0f, 0.0f, x2, y );
    IMG_VERT( 1.0f, 1.0f, x2, y2 );
    IMG_VERT( 0.0f, 1.0f, x,  y2 );

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

    geo_reserve( geo, 4 * 6, 6 * 6, 1 );

    geo_primBegin( geo, GL_TRIANGLES );
    for( i = 0; i < 6; ++i )
    {
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

        ip = geo_addIndices( geo, 6 );

        *ip++ = idx;
        if( inside )
        {
            *ip++ = idx + 3;
            *ip++ = idx + 2;
            *ip++ = idx + 2;
            *ip++ = idx + 1;
        }
        else
        {
            *ip++ = idx + 1;
            *ip++ = idx + 2;
            *ip++ = idx + 2;
            *ip++ = idx + 3;
        }
        *ip = idx;
    }
    geo_primEnd( geo );
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
    int astart;


    n = slices * stacks;
    geo_reserve( geo, n + stacks, n * 2, slices );
    if( inside )
        geo->flags |= GEOM_INSIDE;

    astart = geo_vertCount( geo );
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
        int istart = astart + 2;

        it = geo->idx.ptr.u16 + geo->idx.used;

        // TODO: Optimize this to generate a single draw call.

        for( s = 0; s < slices; ++s )
        {
            geo_primBegin( geo, GL_TRIANGLE_STRIP );

            ia = istart++;
          //ib = (s == (slices - 1)) ? 2 : ia + 1;
            ib = ia + 1;

            *it++ = astart;
            for( n = 1; n < stacks; ++n )
            {
                *it++ = ia;
                *it++ = ib;
                ia += slices + 1;
                ib += slices + 1;
            }
            *it++ = astart + 1;

            geo->idx.used += stacks * 2;

            geo_primEnd( geo );
        }
    }
}


/*EOF*/
