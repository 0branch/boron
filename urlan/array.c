/*
  Copyright 2009 Karl Robillard

  This file is part of the Urlan datatype system.

  Urlan is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Urlan is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Urlan.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
  UBuffer members:
    type        Set by user, defaults to UT_UNSET
    elemSize    Element byte size
    form        Unused
    flags       Unused
    used        Number of elements used
    ptr.v       Elements
    ptr.i[-1]   Number of elements available
*/


#include "urlan.h"
#include "os.h"


// Align for 64-bit pointers or doubles if size > 4.
#define FORWARD(s)      ((s > 4) ? 8 : 4)


/**
  Initialize array buffer.
  The buf type, form, flags, and used members are set to zero.

  \param buf    Uninitialized buffer.
  \param size   Element byte size.  Must be less than 256.
  \param count  Number of elements to allocate.
*/
void ur_arrInit( UBuffer* buf, int size, int count )
{
    assert( size );
    assert( size < 256 );

    *((uint32_t*) buf) = 0;
    buf->elemSize = size;
    buf->used = 0;

    if( count > 0 )
    {
        int fwd = FORWARD(size);

        buf->ptr.b = (uint8_t*) memAlloc( (size * count) + fwd );
        if( buf->ptr.b )
        {
            buf->ptr.b += fwd;
            ur_avail(buf) = count;
        }
    }
    else
    {
        buf->ptr.b = 0;
    }
}


/**
  Free array data.

  buf->ptr and buf->used are set to zero.
*/
void ur_arrFree( UBuffer* buf )
{
    if( buf->ptr.b )
    {
        memFree( buf->ptr.b - FORWARD(buf->elemSize) );
        buf->ptr.b = 0;
    }
    buf->used = 0;
}


/**
  Allocates enough memory to hold count elements.
  buf->used is not changed.
 
  \param buf    Initialized array buffer.
  \param count  Total number of elements.
*/
void ur_arrReserve( UBuffer* buf, int count )
{
    uint8_t* mem;
    int avail;
    int fwd;

    avail = ur_testAvail( buf );
    if( count <= avail )
        return;

    /* Double the buffer size (unless that is not big enough). */
    avail *= 2;
    if( avail < count )
        avail = (count < 8) ? 8 : count;

    fwd = FORWARD(buf->elemSize);

    if( buf->ptr.b )
        mem = (uint8_t*) memRealloc( buf->ptr.b - fwd,
                                     (buf->elemSize * avail) + fwd );
    else
        mem = (uint8_t*) memAlloc( (buf->elemSize * avail) + fwd );
    assert( mem );
    //printf( "realloc %d\n", mem == (buf->ptr.b - fsize) );

    buf->ptr.b = mem + fwd;
    ur_avail(buf) = avail;
}


/**
  Remove elements from the array.

  \param buf    Initialized array buffer.
  \param start  Start index of erase.
  \param count  Number of elements to remove.
*/
void ur_arrErase( UBuffer* buf, int start, int count )
{
    if( start >= buf->used )
        return;
    if( (start + count) < buf->used )
    {
        int size = buf->elemSize;
        uint8_t* mem = buf->ptr.b + (size * start);
        memMove(mem, mem + (size * count), size * (buf->used - start - count));
        buf->used -= count;
    }
    else
        buf->used = start;
}


/**
  Create space in the array for count elements starting at index.
  The memory in the new space is uninitialized.

  \param buf    Initialized array buffer.
  \param index  Position to expand at.
  \param count  Number of elements to expand.
*/
void ur_arrExpand( UBuffer* buf, int index, int count )
{
    ur_arrReserve( buf, buf->used + count );
    if( index < buf->used )
    {
        int size = buf->elemSize;
        uint8_t* mem = buf->ptr.b + (size * index);
        memMove( mem + (size * count), mem, size * (buf->used - index) );
    }
    buf->used += count;
}


/**
  Append int32_t to array.

  \param buf  Array buffer with elemSize of 4.
  \param n    Number to append.
*/
void ur_arrAppendInt32( UBuffer* buf, int32_t n )
{
    ur_arrReserve( buf, buf->used + 1 );
    buf->ptr.i[ buf->used++ ] = n;
}


/*EOF*/
