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
    type        UT_BINARY
    elemSize    Unused
    form        Unused
    flags       Unused
    used        Number of bytes used
    ptr.b       Data
    ptr.i[-1]   Number of bytes available
*/


#include "urlan.h"
#include "os.h"


#define FORWARD     sizeof(int32_t)


/** \defgroup dt_binary Datatype Binary
  \ingroup urlan
  The binary datatype is a series of bytes.
  @{
*/

/**
  Generate and initialize a single binary buffer.

  If you need multiple buffers then ur_genBuffers() should be used.

  The caller must create a UCell for this block in a held block before the
  next ur_recycle() or else it will be garbage collected.

  \param size   Number of bytes to reserve.

  \return  Buffer id of binary.
*/
UIndex ur_makeBinary( UThread* ut, int size )
{
    UIndex bufN;
    ur_genBuffers( ut, 1, &bufN );
    ur_binInit( ur_buffer( bufN ), size );
    return bufN;
}


/**
  Generate a single binary and set cell to reference it.

  If you need multiple buffers then ur_genBuffers() should be used.

  \param size   Number of bytes to reserve.
  \param cell   Cell to initialize.

  \return  Pointer to binary buffer.
*/
UBuffer* ur_makeBinaryCell( UThread* ut, int size, UCell* cell )
{
    UBuffer* buf;
    UIndex bufN;

    ur_genBuffers( ut, 1, &bufN );
    buf = ur_buffer( bufN );
    ur_binInit( buf, size );

    ur_setId( cell, UT_BINARY );
    ur_setSeries( cell, bufN, 0 );

    return buf;
}


/**
  Initialize buffer to type UT_BINARY.

  \param size   Number of bytes to reserve.
*/
void ur_binInit( UBuffer* buf, int size )
{
    *((uint32_t*) buf) = 0;
    buf->type = UT_BINARY;
    buf->used = 0;

    if( size > 0 )
    {
        buf->ptr.b = (uint8_t*) memAlloc( size + FORWARD );
        if( buf->ptr.b )
        {
            buf->ptr.b += FORWARD;
            ur_avail(buf) = size;
        }
    }
    else
    {
        buf->ptr.b = 0;
    }
}


/**
  Free binary data.

  buf->ptr and buf->used are set to zero.
*/
void ur_binFree( UBuffer* buf )
{
    if( buf->ptr.b )
    {
        memFree( buf->ptr.b - FORWARD );
        buf->ptr.b = 0;
    }
    buf->used = 0;
}


/**
  Allocates enough memory to hold size bytes.
  buf->used is not changed.

  \param buf    Initialized binary buffer.
  \param size   Total number of bytes.
*/
void ur_binReserve( UBuffer* buf, int size )
{
    uint8_t* mem;
    int avail;

    avail = ur_testAvail( buf );
    if( size <= avail )
        return;

    /* Double the buffer size (unless that is not big enough). */
    avail *= 2;
    if( avail < size )
        avail = (size < 8) ? 8 : size;

    if( buf->ptr.b )
        mem = (uint8_t*) memRealloc( buf->ptr.b - FORWARD, avail + FORWARD );
    else
        mem = (uint8_t*) memAlloc( avail + FORWARD );
    assert( mem );
    //printf( "realloc %d\n", mem == (buf->ptr.b - fsize) );

    buf->ptr.b = mem + FORWARD;
    ur_avail(buf) = avail;
}


/**
  Remove bytes from the binary.

  \param buf    Initialized binary buffer.
  \param start  Start index of erase.
  \param count  Number of bytes to remove.
*/
void ur_binErase( UBuffer* buf, int start, int count )
{
    if( start >= buf->used )
        return;
    if( (start + count) < buf->used )
    {
        uint8_t* mem = buf->ptr.b + start;
        memMove( mem, mem + count, buf->used - start - count );
        buf->used -= count;
    }
    else
        buf->used = start;
}


/**
  Create space in the binary for count bytes starting at index.
  The memory in the new space is uninitialized.

  \param buf    Initialized binary buffer.
  \param index  Position to expand at.
  \param count  Number of bytes to expand.
*/
void ur_binExpand( UBuffer* buf, int index, int count )
{
    ur_binReserve( buf, buf->used + count );
    if( index < buf->used )
    {
        uint8_t* mem = buf->ptr.b + index;
        memMove( mem + count, mem, buf->used - index );
    }
    buf->used += count;
}


/**
  Append data to binary buffer.

  \param buf    Initialized binary buffer.
  \param data   Data to append.
  \param len    Number of bytes from data to append.
*/
void ur_binAppendData( UBuffer* buf, const uint8_t* data, int len )
{
    ur_binReserve( buf, buf->used + len );
    memCpy( buf->ptr.b + buf->used, data, len );
    buf->used += len;
}


/**
  Set UBinaryIter to binary slice.

  \param bi    Iterator struct to fill.
  \param cell  Pointer to a valid binary cell.

  \return Initializes bi.  If slice is empty then bi->it and bi->end are set
          to zero.
*/
void ur_binSlice( UThread* ut, UBinaryIter* bi, const UCell* cell )
{
    const UBuffer* buf;
    bi->buf = buf = ur_bufferSer(cell);
    if( buf->ptr.b )
    {
        UIndex end = buf->used;
        if( (cell->series.end > -1) && (cell->series.end < end) )
            end = cell->series.end;
        if( end > cell->series.it )
        {
            bi->it  = buf->ptr.b + cell->series.it;
            bi->end = buf->ptr.b + end;
            return;
        }
    }
    bi->it = bi->end = 0;
}


/**
  Set UBinaryIterM to binary slice.

  If cell references a binary in shared storage then an error is generated
  and UR_THROW is returned.
  Otherwise, bi is initialized.  If the slice is empty then bi->it and
  bi->end are set to zero.

  \param bi    Iterator struct to fill.
  \param cell  Pointer to a valid binary cell.

  \return UR_OK/UR_THROW
*/
int ur_binSliceM( UThread* ut, UBinaryIterM* bi, const UCell* cell )
{
    UBuffer* buf = ur_bufferSerM(cell);
    if( ! buf )
        return UR_THROW;
    bi->buf = buf;
    if( buf->ptr.b )
    {
        UIndex end = buf->used;
        if( (cell->series.end > -1) && (cell->series.end < end) )
            end = cell->series.end;
        if( end > cell->series.it )
        {
            bi->it  = buf->ptr.b + cell->series.it;
            bi->end = buf->ptr.b + end;
            return UR_OK;
        }
    }
    bi->it = bi->end = 0;
    return UR_OK;
}


/**
  Convert binary buffer to string buffer.
  The data that buf->ptr points to is not touched.

  \param buf        Initialized binary buffer.
  \param encoding   Encoding
*/
void ur_binToStr( UBuffer* buf, int encoding )
{
    buf->type = UT_STRING;
    buf->form = encoding;
    if( encoding == UR_ENC_UCS2 )
    {
        buf->elemSize = 2;
        buf->used /= 2;
        ur_avail(buf) /= 2;
    }
    else
        buf->elemSize = 1;
}


/** @} */


/*EOF*/
