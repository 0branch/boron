/*
  Copyright 2009,2010,2013 Karl Robillard

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
    type        UT_BLOCK, UT_PAREN, etc.
    elemSize    16, sizeof(UCell)
    form        Unused
    flags       Unused
    used        Number of cells used
    ptr.cell    Cells
    ptr.i[-1]   Number of cells available
*/


#include "urlan.h"
#include "os.h"


/** \defgroup dt_block Datatype Block
  \ingroup urlan
  Blocks are a series of UCells.
  @{
*/ 

/** \def ur_blkFree()
  A block is a simple array.
*/

/**
  Generate a single block of type UT_BLOCK.

  If you need multiple buffers then ur_genBuffers() should be used.

  The caller must create a UCell for this block in a held block before the
  next ur_recycle() or else it will be garbage collected.

  \param size   Number of cells to reserve.

  \return  Buffer id of block.
*/
UIndex ur_makeBlock( UThread* ut, int size )
{
    UIndex bufN;
    ur_blkInit( ur_genBuffers( ut, 1, &bufN ), UT_BLOCK, size );
    return bufN;
}


/**
  Generate a single block and set cell to reference it.

  If you need multiple buffers then ur_genBuffers() should be used.

  \param type   Type (UT_BLOCK, UT_PAREN, etc.).
  \param size   Number of cells to reserve.
  \param cell   Cell to initialize.

  \return  Pointer to block buffer.
*/
UBuffer* ur_makeBlockCell( UThread* ut, int type, int size, UCell* cell )
{
    UBuffer* buf;
    UIndex bufN;

    buf = ur_genBuffers( ut, 1, &bufN );
    ur_blkInit( buf, type, size );

    ur_initSeries( cell, type, bufN );

    return buf;
}


/**
  Initialize block buffer.

  \param type   Buffer type (Normally UT_BLOCK).
  \param size   Number of cells to reserve.
*/
void ur_blkInit( UBuffer* buf, int type, int size )
{
    ur_arrInit( buf, sizeof(UCell), size );
    buf->type = type;
}


/**
  Add cell to end of block.

  \return  Pointer to new cell.
           Only the cell id is initialized; other members are unset.
*/
UCell* ur_blkAppendNew( UBuffer* buf, int type )
{
    UCell* cell;
    ur_arrExpand1( UCell, buf, cell );
    ur_setId( cell, type );
    return cell;
}


/**
  Append cells to block.

  \param buf    Initialized block buffer.
  \param cells  Cells to copy.
  \param count  Number of cells to copy.
*/
void ur_blkAppendCells( UBuffer* buf, const UCell* cells, int count )
{
    ur_arrReserve( buf, buf->used + count );
    memCpy( buf->ptr.cell + buf->used, cells, count * sizeof(UCell) );
    buf->used += count;
}


/**
  Insert cells into block.

  This performs a memcpy, so the source cells must not overlap the buf cells.

  \param buf    Initialized block buffer.
  \param it     Position in buf.
  \param cells  Cells to copy.
  \param count  Number of cells to copy.
*/
void ur_blkInsert( UBuffer* buf, UIndex it, const UCell* cells, int count )
{
    ur_arrExpand( buf, it, count );
    memCpy( buf->ptr.cell + it, cells, count * sizeof(UCell) );
}


/**
  Copy cell to end of block.
*/
void ur_blkPush( UBuffer* buf, const UCell* cell )
{
    ur_arrReserve( buf, buf->used + 1 );
    buf->ptr.cell[ buf->used ] = *cell;
    ++buf->used;
}


/**
  Remove cell from end of block.

  \return Pointer to what was the last cell, or zero if block is empty.
*/
UCell* ur_blkPop( UBuffer* buf )
{
    if( buf->used > 0 )
    {
        --buf->used;
        return buf->ptr.cell + buf->used;
    }
    return 0;
}


/**
  Set UBlockIter to block slice.  If the UBlockIter::buf pointer is not needed
  it is recommended to use ur_blockIt() instead.

  \param bi    Iterator struct to fill.
  \param cell  Pointer to a valid block cell.

  \return Initializes bi.  If slice is empty then bi->it and bi->end are set
          to zero.

  \sa ur_blockIt, ur_blkSliceM
*/
void ur_blkSlice( UThread* ut, UBlockIter* bi, const UCell* cell )
{
    const UBuffer* buf;
    bi->buf = buf = ur_bufferSer(cell);
    if( buf->ptr.cell )
    {
        UIndex end = buf->used;
        if( (cell->series.end > -1) && (cell->series.end < end) )
            end = cell->series.end;
        if( end > cell->series.it )
        {
            bi->it  = buf->ptr.cell + cell->series.it;
            bi->end = buf->ptr.cell + end;
            return;
        }
    }
    bi->it = bi->end = 0;
}


/**
  Set UBlockIterM to block slice.

  If cell references a block in shared storage then an error is generated
  and UR_THROW is returned.
  Otherwise, bi is initialized.  If the slice is empty then bi->it and
  bi->end are set to zero.

  \param bi    Iterator struct to fill.
  \param cell  Pointer to a valid block cell.

  \return UR_OK/UR_THROW

  \sa ur_blockIt, ur_blkSlice
*/
UStatus ur_blkSliceM( UThread* ut, UBlockIterM* bi, const UCell* cell )
{
    UBuffer* buf = ur_bufferSerM(cell);
    if( ! buf )
        return UR_THROW;
    bi->buf = buf;
    if( buf->ptr.cell )
    {
        UIndex end = buf->used;
        if( (cell->series.end > -1) && (cell->series.end < end) )
            end = cell->series.end;
        if( end > cell->series.it )
        {
            bi->it  = buf->ptr.cell + cell->series.it;
            bi->end = buf->ptr.cell + end;
            return UR_OK;
        }
    }
    bi->it = bi->end = 0;
    return UR_OK;
}


extern void ur_deepCopyCells( UThread*, UCell*, const UCell* src, int );

/**
  Make deep copy of block.

  \param blkN   Buffer id of block to clone.

  \return Buffer id of new block.
*/
UIndex ur_blkClone( UThread* ut, UIndex blkN )
{
    UIndex n;
    UIndex hold;
    UBuffer* copy;
    const UBuffer* orig;

    copy = ur_genBuffers( ut, 1, &n );
    orig = ur_bufferE( blkN );
    ur_blkInit( copy, UT_BLOCK, orig->used );
    copy->used = orig->used;

    hold = ur_hold( n );
    ur_deepCopyCells( ut, copy->ptr.cell, orig->ptr.cell, orig->used );
    ur_release( hold );

    return n;
}


UCell* ur_findCell( UThread* ut, UCell* it, const UCell* end,
                    const UCell* value )
{
    while( it != end )
    {
        if( ur_equal( ut, value, it ) )
            return it;
        ++it;
    }
    return 0;
}


/**
  Find all values of a certain type and append them to another block.

  \param blkCell    Cell of block or paren to recursively search.
  \param typeMask   Bit mask of datatypes to collect.
  \param dest       Matching values are copied to this block buffer.
  \param unique     Only add equal values once to dest.
*/
void ur_blkCollectType( UThread* ut, const UCell* blkCell,
                        uint32_t typeMask, UBuffer* dest, int unique )
{
    UBlockIt bi;
    int type;

    ur_blockIt( ut, &bi, blkCell );
    ur_foreach( bi )
    {
        type = ur_type(bi.it);
        if( (1 << type) & typeMask )
        {
            if( unique && ur_findCell( ut, dest->ptr.cell,
                                       dest->ptr.cell + dest->used, bi.it ) )
                continue;
            ur_blkPush( dest, bi.it );
        }
        if( type == UT_BLOCK || type == UT_PAREN )
            ur_blkCollectType( ut, bi.it, typeMask, dest, unique );
    }
}


/** @} */ 


//EOF
