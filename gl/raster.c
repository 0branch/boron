/*
  Boron OpenGL Module
  Copyright 2008-2012 Karl Robillard

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


#include "os.h"
#include "boron-gl.h"


static const uint8_t _bytesPerPixel[ 4 ] = { 0, 1, 3, 4 };


int ur_rasterElementSize( const RasterHead* r )
{
    return _bytesPerPixel[ r->format ];
}


static int _rasterBinarySize( int format, int depth, int w, int h )
{
    int size = depth / 8 * w * h;
    if( format == UR_RAST_RGB )
        size *= 3;
    else if( format == UR_RAST_RGBA )
        size *= 4;
    return size + sizeof(RasterHead);
}


/**
  \return Binary index and initialied res.  If binp is non-zero, it is set.
*/
UBuffer* ur_makeRaster( UThread* ut, int format, int w, int h, UCell* res )
{
    UIndex binN;
    UBuffer* bin;
    RasterHead* rh;
    int size;


    size = _rasterBinarySize( format, 8, w, h );
    binN = ur_makeBinary( ut, size );
    bin = ur_buffer( binN );
    bin->used = size;

    rh = (RasterHead*) bin->ptr.v;
    rh->format = format;
    rh->depth  = 8;
    rh->width  = w;
    rh->height = h;
    rh->bytesPerRow = _bytesPerPixel[ format ] * w;

    ur_setId( res, UT_RASTER );
    ur_setSeries( res, binN, 0 );

    return bin;
}


struct BlitRect
{
    int x, y, w, h;
};


static void setBlitRect( struct BlitRect* br, const RasterHead* rh,
                         uint16_t* subr )
{
    if( subr )
    {
        br->x = subr[0];
        br->y = subr[1];
        br->w = subr[2];
        br->h = subr[3];

        if( (br->x + br->w) > rh->width )
            br->w = rh->width - br->x;
        if( (br->y + br->h) > rh->height )
            br->h = rh->height - br->y;
    }
    else
    {
        br->x = 0;
        br->y = 0;
        br->w = rh->width;
        br->h = rh->height;
    }
}


void ur_rasterBlit( const RasterHead* src, uint16_t* srcRect,
                    RasterHead* dest, uint16_t* destRect )
{
    const uint8_t* sp;
    uint8_t* dp;
    struct BlitRect sr;
    struct BlitRect dr;
    int ses, des;
    int sskip, dskip;

    setBlitRect( &sr, src, srcRect );
    setBlitRect( &dr, dest, destRect );

    if( sr.w > dr.w )
        sr.w = dr.w;
    if( sr.h > dr.h )
        sr.h = dr.h;

    sskip = ur_rasterBytesPerRow( src );
    dskip = ur_rasterBytesPerRow( dest );

    ses = ur_rasterElementSize( src );
    des = ur_rasterElementSize( dest );

    sp = ur_rasterElements( src )  + (sr.x * ses) + (sr.y * sskip);
    dp = ur_rasterElements( dest ) + (dr.x * des) + (dr.y * dskip);

    if( src->format == dest->format )
    {
        ses *= sr.w;
        while( sr.h-- )
        {
            memCpy( dp, sp, ses );
            sp += sskip;
            dp += dskip;
        }
    }
#if 0
    else if( src->format == UR_RAST_GRAY )
    {
        // Need some way to specify what happens on each channel.
        // For example, [255 255 255 gray] and [gray gray gray 255] are
        // very different.
        char* end;
        int gray;

        sskip -= sr.w;
        dskip -= dr.w * des;

        if( dest->format == UR_RAST_RGBA )
        {
            while( sr.h-- )
            {
                end = sp + sr.w;
                while( sp != end )
                {
                    gray = *sp++;
                    *dp++ = gray;
                    *dp++ = gray;
                    *dp++ = gray;
                    *dp++ = 255;
                }
                sp += sskip;
                dp += dskip;
            }
        }
        else if( dest->format == UR_RAST_RGB )
        {
            while( sr.h-- )
            {
                end = sp + sr.w;
                while( sp != end )
                {
                    gray = *sp++;
                    *dp++ = gray;
                    *dp++ = gray;
                    *dp++ = gray;
                }
                sp += sskip;
                dp += dskip;
            }
        }
    }
#endif
}


typedef struct
{
    int dx, dy;
}
DPoint;


static int _distSq( const DPoint* pnt )
{
    return pnt->dx * pnt->dx + pnt->dy * pnt->dy;
}


static void _propagateDist( DPoint* pnt, int width, int height, int x, int y,
                            int xo, int yo )
{
    DPoint tmp;
    DPoint* adjacent;

    x += xo;
    if( x < 0 || x >= width )
        return;
    y += yo;
    if( y < 0 || y >= height )
        return;

    adjacent = pnt + (yo * width) + xo; 
    tmp.dx = adjacent->dx + xo;
    tmp.dy = adjacent->dy + yo;
    if( _distSq( &tmp ) < _distSq( pnt ) )
        *pnt = tmp;
}


static void _transform8SSEDT( DPoint* df, int width, int height )
{
    DPoint* row;
    DPoint* pnt;
    int x, y;
#define PROPAGATE(xo,yo)    _propagateDist( pnt, width, height, x, y, xo, yo )

    // First pass.
    for( y = 0; y < height; ++y )
    {
        row = df + (y * width);
        for( x = 0; x < width; ++x )
        {
            pnt = row + x;
            PROPAGATE( -1,  0 ); 
            PROPAGATE(  0, -1 ); 
            PROPAGATE( -1, -1 ); 
            PROPAGATE(  1, -1 ); 
        }
        for( x = width - 1; x >= 0; --x )
        {
            pnt = row + x;
            PROPAGATE( 1, 0 ); 
        }
    }

    // Second pass.
    for( y = height - 1; y >= 0; --y )
    {
        row = df + (y * width);
        for( x = width - 1; x >= 0; --x )
        {
            pnt = row + x;
            PROPAGATE(  1, 0 ); 
            PROPAGATE(  0, 1 ); 
            PROPAGATE( -1, 1 ); 
            PROPAGATE(  1, 1 ); 
        }
        for( x = 0; x < width; ++x )
        {
            pnt = row + x;
            PROPAGATE( -1, 0 ); 
        }
    }
}


UBuffer* makeDistanceField( UThread* ut, const RasterHead* src,
                            int (*inside)( uint8_t*, void* ), void* user,
                            double scale, UCell* res )
{
    DPoint* df1;
    DPoint* df2;
    DPoint* dr1;
    DPoint* dr2;
    uint8_t* pp;
    int es;
    int x, y;
    UBuffer* rbuf;


    // Initialize the inside and outside distance fields.

    df1 = (DPoint*) malloc( src->width * src->height * sizeof(DPoint) * 2 );
    df2 = df1 + (src->width * src->height);

    es = ur_rasterElementSize(src);
    dr1 = df1;
    dr2 = df2;
    for( y = 0; y < src->height; ++y )
    {
        pp = ur_rasterElements(src) + (y * src->bytesPerRow);
        for( x = 0; x < src->width; ++x, pp += es )
        {
            if( inside( pp, user ) )
            {
                dr1[x].dx = 0;
                dr1[x].dy = 0;
                dr2[x].dx = 9999;
                dr2[x].dy = 9999;
            }
            else
            {
                dr1[x].dx = 9999;
                dr1[x].dy = 9999;
                dr2[x].dx = 0;
                dr2[x].dy = 0;
            }
        }
        dr1 += src->width;
        dr2 += src->width;
    }


    // Compute the distance fields.

    _transform8SSEDT( df1, src->width, src->height );
    _transform8SSEDT( df2, src->width, src->height );


    // Combine the inside & outside fields into a new raster.

    {
#define DDIST(fx) \
    (sqrt((double) _distSq( dr1 + fx )) - sqrt((double) _distSq( dr2 + fx )))

    const RasterHead* rh;
    int d;
#define QUARTER_SIZE
#ifdef QUARTER_SIZE
    int fi, fx;
    static int sampleOffset[ 16 ];

    for( y = 0, fi = 0; y < 16; y += 4, fi += src->width )
    {
        for( x = 0; x < 4; ++x )
            sampleOffset[ y + x ] = fi + x;
    }

    rbuf = ur_makeRaster( ut, UR_RAST_GRAY, src->width/4, src->height/4, res );
#else
    rbuf = ur_makeRaster( ut, UR_RAST_GRAY, src->width, src->height, res );
#endif
    rh = (const RasterHead*) rbuf->ptr.v;
    es = ur_rasterElementSize(rh);
    dr1 = df1;
    dr2 = df2;
    for( y = 0; y < rh->height; ++y )
    {
        pp = ur_rasterElements(rh) + (y * rh->bytesPerRow);
        for( x = 0; x < rh->width; ++x, pp += es )
        {
#ifdef QUARTER_SIZE
            double sum = 0.0;
            for( fi = 0; fi < 16; ++fi )
            {
                fx = x * 4 + sampleOffset[ fi ];
                sum += DDIST( fx );
            }
            d = (int) (sum / 16.0 * scale);
#else
            d = (int) (DDIST( x ) * scale);
#endif
            d = d + 128;
            if( d < 0 )
                d = 0;
            else if( d > 255 )
                d = 255;
            *pp = d;
        }
#ifdef QUARTER_SIZE
        dr1 += src->width * 4;
        dr2 += src->width * 4;
#else
        dr1 += src->width;
        dr2 += src->width;
#endif
    }
    }

    free( df1 );

    return rbuf;
}


/*EOF*/
