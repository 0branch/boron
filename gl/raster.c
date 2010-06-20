/*
  Boron OpenGL Module
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


#include "os.h"
#include "boron-gl.h"


int ur_rasterElementSize( const RasterHead* r )
{
    switch( r->format )
    {
        case UR_RAST_GRAY:  return 1;
        case UR_RAST_RGB:   return 3;
        case UR_RAST_RGBA:  return 4;
    }
    return 0;
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
    const char* sp;
    char* dp;
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


/*EOF*/
