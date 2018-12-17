/*
  Copyright 2009-2012 Karl Robillard

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
    type        UT_CONTEXT
    elemSize    Recursion marker
    form        Sorted count (first half)
    flags       Sorted count (second half)
    used        Number of words used (sorted + unsorted)
    ptr.cell    Cell values
    ptr.i[-1]   Number of words available
*/


#include "env.h"
#include "urlan_atoms.h"


#define SEARCH_LEN      2
#define ENTRIES(buf)    ((UAtomEntry*) (buf->ptr.cell + ur_avail(buf)))
#define FORWARD         8


typedef struct
{
    uint8_t     type;
    uint8_t     recursion;
    uint16_t    sorted;
    UIndex      used;
    UCell*      cell;
}
UContext;

#define CC(buf) ((UContext*) buf)


/** \struct UBindTarget
  \ingroup urlan_core
  Holds information for binding functions.
*/
/** \var const UBuffer* UBindTarget::ctx
  Context buffer to lookup words in.
*/
/** \var UIndex UBindTarget::ctxN
  Words in the block which are found in the UBindTarget::ctx will have 
  their UCell.word.ctx set to this identifier.
  For bindType \c UR_BIND_THREAD and \c UR_BIND_ENV this will reference the
  UBindTarget::ctx buffer in the appropriate dataStore.
  Other binding types may use this value differently.
*/
/** \var int UBindTarget::bindType
  Binding type to make.
  \c UR_BIND_THREAD, \c UR_BIND_ENV, etc.
*/


/** \defgroup dt_context Datatype Context
  \ingroup urlan
  Contexts are a table of word/value pairs.
  @{
*/

/** \def ur_ctxCell
  Get pointer of UCell in context by index.
*/


/**
  Generate and initialize a single context.

  If you need multiple buffers then ur_genBuffers() should be used.

  \param size   Number of words to reserve.

  \return Buffer id of context.
*/
UIndex ur_makeContext( UThread* ut, int size )
{
    UIndex bufN;
    ur_ctxInit( ur_genBuffers( ut, 1, &bufN ), size );
    return bufN;
}


/**
  Generate a single context and set cell to reference it.

  If you need multiple buffers then ur_genBuffers() should be used.

  \param size   Number of words to reserve.
  \param cell   Cell to initialize.

  \return Pointer to context buffer.
*/
UBuffer* ur_makeContextCell( UThread* ut, int size, UCell* cell )
{
    UBuffer* buf;
    UIndex bufN;

    buf = ur_genBuffers( ut, 1, &bufN );
    ur_ctxInit( buf, size );

    ur_setId( cell, UT_CONTEXT );
    ur_setSeries( cell, bufN, 0 );

    return buf;
}


/**
  Allocates enough memory to hold size words.
  buf->used is not changed.

  \param buf    Initialized context buffer.
  \param size   Number of words to reserve.
*/
void ur_ctxReserve( UBuffer* buf, int size )
{
    uint8_t* mem;
    int avail;
    int na;

    avail = ur_testAvail( buf );
    if( size <= avail )
        return;

    /* Double the buffer size (unless that is not big enough). */
    na = avail * 2;
    if( na < size )
        na = (size < 4) ? 4 : size;

    mem = (uint8_t*) memAlloc( FORWARD +
                               (sizeof(UAtomEntry) + sizeof(UCell)) * na  );
    assert( mem );

    if( buf->ptr.b )
    {
        if( buf->used )
        {
            uint8_t* dest = mem + FORWARD;
            memCpy( dest, buf->ptr.cell, buf->used * sizeof(UCell) );
            dest += na * sizeof(UCell);
            memCpy( dest, ENTRIES(buf), buf->used * sizeof(UAtomEntry) );
        }
        memFree( buf->ptr.b - FORWARD );
    }

    buf->ptr.b = mem + FORWARD;
    ur_avail(buf) = na;
}


/*
  Copy cells and clone any child blocks.

  \param dest    Destination cells.
  \param src     Soruces cells.
  \param count   Number of cells to copy.

  The cells from src are copied to dest before any new blocks are generated.
  This means that if dest is part of a UBuffer in the dataStore, the used
  member should be set to count before calling ur_deepCopyCells so that all
  cells are held from garbage collection.
*/
void ur_deepCopyCells( UThread* ut, UCell* dest, const UCell* src, int count )
{
    UCell* end;
    UBuffer* copy;
    const UBuffer* orig;
    UIndex bufN;

    memCpy( dest, src, count * sizeof(UCell) );

    end = dest + count;
    for( ; dest != end; ++dest, ++src )
    {
        int type = ur_type(dest);
        if( ur_isBlockType( type ) )
        {
            copy = ur_genBuffers( ut, 1, &bufN );
            orig = ur_bufferSer( dest );
            ur_blkInit( copy, UT_BLOCK, orig->used );
            copy->used = orig->used;
            dest->series.buf = bufN;
            ur_deepCopyCells( ut, copy->ptr.cell, orig->ptr.cell, orig->used );
        }
        else if( type >= UT_BINARY )
        {
            // Copy method requires different src and dest.
            ut->types[ type ]->copy( ut, src, dest );
        }
    }
}


/**
  Clone a new context and set cell to reference it.

  \param src    Context to copy.
  \param cell   Cell to initialize.

  \return Pointer to context buffer.
*/
UBuffer* ur_ctxClone( UThread* ut, const UBuffer* src, UCell* cell )
{
    if( src->used )
    {
        // Save src members; src will be invalid after ur_makeContextCell.
        UCell* srcCells = src->ptr.cell;
        UAtomEntry* srcEntries = ENTRIES(src);
        int sorted = CC(src)->sorted;
        int size = src->used;

        UBuffer* nc = ur_makeContextCell( ut, size, cell );     // gc!
        UIndex hold = ur_hold( cell->context.buf );

        memCpy( ENTRIES(nc), srcEntries, size * sizeof(UAtomEntry) );
        CC(nc)->sorted = sorted;
        nc->used = size;
        ur_deepCopyCells( ut, nc->ptr.cell, srcCells, size );   // gc!

        nc = ur_buffer( cell->context.buf );        // Re-aquire
        ur_ctxSort( nc );
        ur_bind( ut, nc, nc, UR_BIND_SELF );
        ur_release( hold );
        return nc;
    }
    return ur_makeContextCell( ut, 0, cell );
}


/**
  Create a shallow copy of a context and set cell to reference the new context.
  No binding is done, so any blocks remain bound to the source context.

  \param src    Context to copy.
  \param cell   Cell to initialize.

  \return Pointer to context buffer.
*/
UBuffer* ur_ctxMirror( UThread* ut, const UBuffer* src, UCell* cell )
{
    // Save src members; src will be invalid after ur_makeContextCell.
    UCell* srcCells = src->ptr.cell;
    UAtomEntry* srcEntries = ENTRIES(src);
    int sorted = CC(src)->sorted;
    int size = src->used;

    UBuffer* nc = ur_makeContextCell( ut, size, cell );     // gc!

    memCpy( ENTRIES(nc), srcEntries, size * sizeof(UAtomEntry) );
    CC(nc)->sorted = sorted;
    nc->used = size;
    memCpy( nc->ptr.cell, srcCells, size * sizeof(UCell) );

    return nc;
}


/**
  Add the set-word! values in a series of cells to the words in a context.

  \param ctx    Destination context.
  \param it     Start of cells.
  \param end    End of cells.
*/
void ur_ctxSetWords( UBuffer* ctx, const UCell* it, const UCell* end )
{
    while( it != end )
    {
        if( ur_is(it, UT_SETWORD) )
            ur_ctxAddWordI( ctx, ur_atom(it) );
        ++it;
    }
}


/**
  Initialize context buffer.

  \param size   Number of words to reserve.
*/
void ur_ctxInit( UBuffer* buf, int size )
{
    *((uint32_t*) buf) = 0;
    buf->type  = UT_CONTEXT;
    buf->used  = 0;
    buf->ptr.v = 0;

    if( size > 0 )
        ur_ctxReserve( buf, size );
}


/**
  Free context data.

  buf->ptr and buf->used are set to zero.
*/
void ur_ctxFree( UBuffer* buf )
{
    if( buf->ptr.b )
    {
        memFree( buf->ptr.b - FORWARD );
        buf->ptr.b = 0;
    }
    CC(buf)->sorted = 0;
    buf->used = 0;
}


/**
  Get word atoms in order.

  \param ctx    Valid context buffer
  \param atoms  Array large enough to hold ctx->used atoms.
*/
void ur_ctxWordAtoms( const UBuffer* ctx, UAtom* atoms )
{
    if( ctx->used > 0 )
    {
        const UAtomEntry* it = ENTRIES(ctx);
        const UAtomEntry* end = it + ctx->used;
        while( it != end )
        {
            atoms[it->index] = it->atom;
            ++it;
        }
    }
}


/**
  Find an atom in a UAtomEntry table using a binary search.
  The table must have been previously sorted with ur_atomsSort().

  \return Index of atom in table or -1 if not found.
*/
int ur_atomsSearch( const UAtomEntry* entries, int count, UAtom atom )
{
    UAtom midAtom;
    int mid;
    int low = 0;
    int high = count - 1;

    while( low <= high )
    {
        mid = ((unsigned int) (low + high)) >> 1;
        midAtom = entries[ mid ].atom;

        if( midAtom < atom )
            low = mid + 1;
        else if( midAtom > atom )
            high = mid - 1;
        else
            return entries[ mid ].index;
    }

    // Atom not found.
    return -1;
}


/**
  Append word to context.  This should only be called if the word is known
  to not already exist in the context.

  The new word is initialized as unset.

  \return  Index of word in context.

  \sa ur_ctxAddWordI, ur_ctxAddWord
*/
int ur_ctxAppendWord( UBuffer* ctx, UAtom atom )
{
    int wrdN;
    UAtomEntry* went;


    wrdN = ctx->used;
    ur_ctxReserve( ctx, wrdN + 1 );
    ++ctx->used;

    ur_setId( ctx->ptr.cell + wrdN, UT_UNSET );

    went = ENTRIES(ctx) + wrdN;
    went->atom  = atom;
    went->index = wrdN;

    return wrdN;
}


/**
  Add word to context if it does not already exist.
  If added, the word is initialized as unset.

  Note that the ctx->ptr may be changed by this function.

  \return  Index of word in context.

  \sa ur_ctxAppendWord
*/
int ur_ctxAddWordI( UBuffer* ctx, UAtom atom )
{
    if( ctx->used )
    {
        int wrdN = ur_ctxLookup( ctx, atom );
        if( wrdN >= 0 )
            return wrdN;
    }
    return ur_ctxAppendWord( ctx, atom );
}


/**
  Similar to ur_ctxAddWordI(), but safely returns the cell pointer.

  \return  Pointer to value cell of word.
*/
UCell* ur_ctxAddWord( UBuffer* ctx, UAtom atom )
{
    int wordN = ur_ctxAddWordI( ctx, atom );
    return ctx->ptr.cell + wordN;
}


#define QS_VAL(a)   entries[a].atom

#define QS_SWAP(a,b) \
    swapTmp = entries[a]; \
    entries[a] = entries[b]; \
    entries[b] = swapTmp

/**
  Perform quicksort on UAtomEntry table.
  Pass low of 0 and high of (count - 1) to sort the entire table.

  \param entries    Array of initialized UAtomEntry structs.
  \param low        First entry in sort partition.
  \param high       Last entry in sort partition.
*/
void ur_atomsSort( UAtomEntry* entries, int low, int high )
{
    int i, j;
    UAtom val;
    UAtomEntry swapTmp;

    if( low >= high )
        return;

    val = QS_VAL(low);
    i = low;
    j = high+1;
    for(;;)
    {
        do i++; while( i <= high && QS_VAL(i) < val );
        do j--; while( QS_VAL(j) > val );
        if( i > j )
            break;
        QS_SWAP( i, j );
    }
    QS_SWAP( low, j );
    ur_atomsSort( entries, low, j-1 );
    ur_atomsSort( entries, j+1, high );
}


/**
  Sort the internal context search table so ur_ctxLookup() is faster.
  If the context is already sorted then nothing is done.
  Each time new words are appended to the context it will become un-sorted.

  \param ctx    Initialized context buffer.

  \return The ctx argument.
*/
UBuffer* ur_ctxSort( UBuffer* ctx )
{
    int used = ctx->used;
    if( used > SEARCH_LEN && CC(ctx)->sorted != used )
    {
      //printf( "KR ctxSort %p %d,%d\n", (void*) ctx, used, CC(ctx)->sorted );
        ur_atomsSort( ENTRIES(ctx), 0, used - 1 );
        CC(ctx)->sorted = used;
    }
    return ctx;
}


UBuffer* ur_ctxSortU( UBuffer* ctx, int unsorted )
{
    int used = ctx->used;
    if( used > SEARCH_LEN && (CC(ctx)->sorted + unsorted) < used )
    {
      //printf( "KR ctxSort %p %d,%d\n", (void*) ctx, used, CC(ctx)->sorted );
        ur_atomsSort( ENTRIES(ctx), 0, used - 1 );
        CC(ctx)->sorted = used;
    }
    return ctx;
}


/**
  Get context and make sure it is ready for ur_ctxLookup().

  If the context is shared and has not been sorted, then an error is generated
  with ur_error() and zero is returned.

  \param cell   Valid context cell.

  \return Pointer to context buffer or zero.
*/
const UBuffer* ur_sortedContext( UThread* ut, const UCell* cell )
{
    UBuffer* ctx;
    UIndex n = cell->context.buf;
    int used;

    // Same as ur_bufferSeries().
    ctx = ur_isShared(n) ? ut->env->sharedStore.ptr.buf - n
                         : ut->dataStore.ptr.buf + n;
    used = ctx->used;
    if( used > SEARCH_LEN && CC(ctx)->sorted != used )
    {
        if( ur_isShared(n) )
        {
            ur_error(ut, UR_ERR_INTERNAL, "Shared context %d is not sorted", n);
            return 0;
        }
        ur_atomsSort( ENTRIES(ctx), 0, used - 1 );
        CC(ctx)->sorted = used;
    }
    return ctx;
}


/**
  Find word in context by atom.

  \param cxt    Initialized context buffer.
  \param atom   Atom of word to find.

  \return  Word index or -1 if not found.
*/
int ur_ctxLookup( const UBuffer* ctx, UAtom atom )
{
    const UAtomEntry* went;
    int sorted;
    int i;

    if( ! ctx->used )
        return -1;

    went = ENTRIES(ctx);
    sorted = CC(ctx)->sorted;
    i = ur_atomsSearch( went, sorted, atom );
    if( i < 0 && ctx->used > sorted )
    {
        const UAtomEntry* it  = went + sorted;
        const UAtomEntry* end = went + ctx->used;
        while( it != end )
        {
            if( it->atom == atom )
                return it->index;
            ++it;
        }
    }
    return i;
}


/**
  Bind an array of cells to a target.
  This recursively binds all blocks in the range of cells.

  \param it     First cell.
  \param end    End of cell array.
  \param bt     Bind target.
*/
void ur_bindCells( UThread* ut, UCell* it, UCell* end, const UBindTarget* bt )
{
    int wrdN;
    int type;
    int self = bt->self;

    for( ; it != end; ++it )
    {
        type = ur_type(it);
        switch( type )
        {
            case UT_WORD:
            case UT_LITWORD:
            case UT_SETWORD:
            case UT_GETWORD:
                wrdN = ur_ctxLookup( bt->ctx, ur_atom(it) );
                if( wrdN > -1 )
                {
                    ur_setBinding( it, bt->bindType );
                    it->word.ctx = bt->ctxN;
                    it->word.index = wrdN;
                }
                else if( ur_atom(it) == self )
                {
                    ur_setBinding( it, UR_BIND_SELF );
                    it->word.ctx = bt->ctxN;
                    it->word.index = 0;
                }
                break;

            case UT_BLOCK:
            case UT_PAREN:
            case UT_PATH:
            case UT_LITPATH:
            case UT_SETPATH:
                if( ! ur_isShared(it->series.buf) )
                {
                    UBuffer* blk = ur_buffer(it->series.buf);
                    ur_bindCells( ut, blk->ptr.cell,
                                      blk->ptr.cell + blk->used, bt );
                }
                break;

            default:
                if( type >= UT_BI_COUNT )
                    ut->types[ type ]->bind( ut, it, bt );
                break;
        }
    }
}


/**
  Bind block to context.
  This recursively binds all sub-blocks.
 
  \param blk        Block to bind.
  \param ctx        Context.  This may be a stand-alone context outside of
                    any dataStore.
  \param bindType   UR_BIND_THREAD, UR_BIND_ENV, etc.
*/
void ur_bind( UThread* ut, UBuffer* blk, const UBuffer* ctx, int bindType )
{
    UBindTarget bt;

    assert( blk->type == UT_BLOCK || blk->type == UT_CONTEXT);
    assert( ctx->type == UT_CONTEXT );

    bt.ctx = ctx;
    if( bindType == UR_BIND_SELF )
    {
        bt.bindType = bindType = UR_BIND_THREAD;
        bt.self = UR_ATOM_SELF;
    }
    else
    {
        bt.bindType = bindType;
        bt.self = UR_INVALID_ATOM;
    }

    if( bindType == UR_BIND_THREAD )
        bt.ctxN = ctx - ut->dataStore.ptr.buf;
    else if( bindType == UR_BIND_ENV )
        bt.ctxN = -(ctx - ut->env->sharedStore.ptr.buf);
    else
        bt.ctxN = UR_INVALID_BUF;

    ur_bindCells( ut, blk->ptr.cell, blk->ptr.cell + blk->used, &bt );
}


/**
  Recursively bind blocks to the bindings found in a context.

  \param bt     Bind target.
  \param it     First cell.
  \param end    End of cell array.
*/
void ur_bindCopy( UThread* ut, const UBuffer* ctx, UCell* it, UCell* end )
{
    const UCell* cell;
    int wrdN;

    for( ; it != end; ++it )
    {
        switch( ur_type(it) )
        {
            case UT_WORD:
            case UT_LITWORD:
            case UT_SETWORD:
            case UT_GETWORD:
                wrdN = ur_ctxLookup( ctx, ur_atom(it) );
                if( wrdN > -1 )
                {
                    cell = ur_ctxCell( ctx, wrdN );
                    it->word.binding = cell->word.binding;
                    it->word.ctx     = cell->word.ctx;
                    it->word.index   = cell->word.index;
                }
                break;

            case UT_BLOCK:
            case UT_PAREN:
            case UT_PATH:
            case UT_LITPATH:
            case UT_SETPATH:
                if( ! ur_isShared(it->series.buf) )
                {
                    UBlockIterM bi;
                    ur_blkSliceM( ut, &bi, it );
                    ur_bindCopy( ut, ctx, bi.it, bi.end );
                }
                break;
        }
    }
}


/**
  Unbind all words an array of cells.

  \param it     First cell.
  \param end    End of cell array.
  \param deep   Recursively unbinds all blocks if non-zero.
*/
void ur_unbindCells( UThread* ut, UCell* it, UCell* end, int deep )
{
    for( ; it != end; ++it )
    {
        switch( ur_type(it) )
        {
            case UT_WORD:
            case UT_LITWORD:
            case UT_SETWORD:
            case UT_GETWORD:
                ur_setBinding( it, UR_BIND_UNBOUND );
                it->word.ctx = UR_INVALID_BUF;
                break;

            case UT_BLOCK:
            case UT_PAREN:
            case UT_PATH:
            case UT_LITPATH:
            case UT_SETPATH:
                if( deep && ! ur_isShared(it->series.buf) )
                {
                    UBlockIterM bi;
                    ur_blkSliceM( ut, &bi, it );
                    ur_unbindCells( ut, bi.it, bi.end, 1 );
                }
                break;
        }
    }
}


/**
  Replace words in cells with their values from a context.
  This recursively infuses all sub-blocks.
 
  Unlike bind, this only modifies UT_WORD cells in UT_BLOCK or UT_PAREN
  slices.

  \param bi     Cells to infuse.
  \param ctx    Context.
*/
void ur_infuse( UThread* ut, UCell* it, UCell* end, const UBuffer* ctx )
{
    int wrdN;
    assert( ctx->type == UT_CONTEXT );

    for( ; it != end; ++it )
    {
        switch( ur_type(it) )
        {
            case UT_WORD:
                wrdN = ur_ctxLookup( ctx, ur_atom(it) );
                if( wrdN > -1 )
                {
                    *it = *ur_ctxCell( ctx, wrdN );
                }
                break;

            case UT_BLOCK:
            case UT_PAREN:
                if( ! ur_isShared(it->series.buf) )
                {
                    UBlockIterM bi;
                    ur_blkSliceM( ut, &bi, it );
                    ur_infuse( ut, bi.it, bi.end, ctx );
                }
                break;
        }
    }
}


/** @} */


//EOF
