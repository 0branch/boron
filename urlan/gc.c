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


#include "urlan.h"
#include "os.h"

extern void block_markBuf( UThread*, UBuffer* );


#ifdef DEBUG
//#define GC_REPORT   1
#endif

//#define GC_TIME     1
#ifdef GC_TIME
#include "cpuCounter.h"
#endif


//#define bitIsSet(array,n)    (array[(n)>>3] & 1<<((n)&7))
#define setBit(array,n)      (array[(n)>>3] |= 1<<((n)&7))


#ifdef GC_REPORT
#include "env.h"

void ur_gcReport( const UBuffer* store, UThread* ut )
{
    static const char datatypeChar[] = ".!nlcidDty+3w'::ob0s%v[(/|<CeFfp~~~~~";
    int used = 0;
    int unused = 0;
    const UBuffer* it  = store->ptr.buf;
    const UBuffer* end = it + store->used;

    while( it != end )
    {
        dprint( "%c", datatypeChar[it->type] );
        if( it->type == UT_UNSET )
            ++unused;
        else
            ++used;
        ++it;
    }

    dprint( "\n  dataStore used: %d  unused: %d", used, unused );
    if( ut )
    {
        UIndex* hi   = ut->holds.ptr.i;
        UIndex* hend = hi + ut->holds.used;

        if( ut->freeBufCount != unused )
            dprint( " (freeBufCount: %d)", ut->freeBufCount );

        dprint( "\n  holds: %d ", ut->holds.used );
        while( hi != hend )
        {
            dprint( "%c", (*hi < 0) ? '-' : 'h' );
            ++hi;
        }
    }
    dprint( "\n\n" );
}


void ur_blkReport( const UBuffer* store, const char* name )
{
    int used   = 0;
    int excess = 0;
    int count  = 0;
    int sizeCount[4] = { 0, 0, 0, 0 };
    const UBuffer* it  = store->ptr.buf;
    const UBuffer* end = it + store->used;

    while( it != end )
    {
        if( ur_isBlockType(it->type) )
        {
            if( it->ptr.cell )
            {
                used   += it->used * sizeof(UCell);
                excess += (ur_avail(it) - it->used) * sizeof(UCell);
            }
            if( it->used < 4 )
                ++sizeCount[ it->used ];
            ++count;
        }
        ++it;
    }
    dprint( "%s blocks: %5d (%3d %3d %3d %3d)"
            "  mem-used: %7d  mem-excess: %7d\n",
            name, count, sizeCount[0], sizeCount[1], sizeCount[2], sizeCount[3],
            used, excess
          );
}
#endif


/*
  Have custom datatypes perform the given recycle phase.
*/
static void _recyclePhase( UThread* ut, int phase )
{
    const UDatatype** it  = ut->types + UT_BI_COUNT;
    const UDatatype** end = ut->types + ur_datatypeCount( ut );
    while( it != end )
    {
        const UDatatype* dt = *it;
        if( dt->recycle )
            dt->recycle( ut, phase );
        ++it;
    }
}


/**
  Perform garbage collection on thread dataStore.

  This is a precise, tracing, mark-sweep collector.
  If starts with held buffers and the datatypes trace any buffers they
  reference.

  Any UBuffer pointers to the thread dataStore must be considered invalid
  after this call.  Note that while the buffer structures may move, the data
  that they point to (the UBuffer::ptr member) will not change.
*/
void ur_recycle( UThread* ut )
{
    int byteSize;
    int mask;
    uint8_t* markBits;
    UBuffer* buf;
    UBuffer* bufStart = ut->dataStore.ptr.buf;
    const UDatatype** datatypes = ut->types;
    UBuffer* gcBits = &ut->gcBits;

#ifdef GC_TIME
    //clock_t t1, t1e;
    uint64_t t1, t1e;
#endif

#ifdef GC_REPORT
    dprint( "\nRecycle UThread %p:\n\n", (void*) ut );
    ur_blkReport( &ut->env->dataStore, "Env" );
    ur_blkReport( &ut->dataStore,      "Thr" );
    ur_gcReport( &ut->dataStore, ut );
#endif

#ifdef GC_TIME
    //t1 = clock();
    t1 = cpuCounter();
#endif


    _recyclePhase( ut, UR_RECYCLE_MARK );


    // Mark all buffers as unused.

    byteSize = (ut->dataStore.used + 7) / 8;
    ur_binReserve(gcBits, byteSize);
    gcBits->used = byteSize;
    markBits = gcBits->ptr.b;
    memSet(markBits, 0, byteSize);


    // Mark buffers referenced by stack as used.
    block_markBuf( ut, &ut->stack );


    // Mark held buffers as used.
    {
    UIndex bufN;
    uint8_t* byte;
    void (*markBuf)( UThread*, UBuffer* );
    UIndex* it  = ut->holds.ptr.i;
    UIndex* end = it + ut->holds.used;

    while( it != end )
    {
        bufN = *it++;
        if( bufN < 0 )
            continue;

        // Same as ur_markBuffer().
        byte = markBits + (bufN >> 3);
        mask = 1 << (bufN & 7);
        if( *byte & mask )
            continue;
        *byte |= mask;

        buf = bufStart + bufN;
        markBuf = datatypes[ buf->type ]->markBuf;
        if( markBuf )
            markBuf( ut, buf );
    }
    }


#define MARK_FREE
#ifdef MARK_FREE
    // Mark free buffers as used to speed up sweep. (Need to test this)
    if( ut->freeBufCount )
    {
        UIndex n = ut->freeBufList;
        while( n > -1 )     // FREE_TERM
        {
            setBit( markBits, n );
            n = bufStart[n].used;
        }
    }
#endif


    _recyclePhase( ut, UR_RECYCLE_SWEEP );

    // Sweep unused buffers.
    {
    int padBits;
#ifndef MARK_FREE
    UBuffer* bufTmp;
#endif
    uint8_t* it  = markBits;
    uint8_t* end = it + gcBits->used;


    // Mark padding bits at end as used.
    padBits = ut->dataStore.used & 7;
    if( padBits )
        end[-1] |= 0xff << padBits;

#ifdef MARK_FREE
#define FREE_BUFFER(bufExp)     ur_destroyBuffer(ut, bufExp);
#else
#define FREE_BUFFER(bufExp) \
    bufTmp = bufExp; \
    if( bufTmp->type != UT_UNSET ) \
        ur_destroyBuffer(ut, bufTmp);
#endif

    buf = bufStart;
    while( it != end )
    {
        if( *it != 0xff )
        {
            mask = *it;

            if( (mask & 0x0f) != 0x0f )
            {
                if( (mask & 0x01) == 0 ) { FREE_BUFFER( buf ) }
                if( (mask & 0x02) == 0 ) { FREE_BUFFER( buf + 1 ) }
                if( (mask & 0x04) == 0 ) { FREE_BUFFER( buf + 2 ) }
                if( (mask & 0x08) == 0 ) { FREE_BUFFER( buf + 3 ) }
            }

            if( (mask & 0xf0) != 0xf0 )
            {
                if( (mask & 0x10) == 0 ) { FREE_BUFFER( buf + 4 ) }
                if( (mask & 0x20) == 0 ) { FREE_BUFFER( buf + 5 ) }
                if( (mask & 0x40) == 0 ) { FREE_BUFFER( buf + 6 ) }
                if( (mask & 0x80) == 0 ) { FREE_BUFFER( buf + 7 ) }
            }
        }
        buf += 8;
        ++it;
    }
    }


#ifdef GC_TIME
    //t1e = clock() - t1;
    //printf( "gc seconds: %g\n", ((double) t1e) / CLOCKS_PER_SEC );

    t1e = cpuCounter() - t1;
    printf( "gc ticks: %ld\n", t1e );
#endif

#ifdef GC_REPORT
    ur_gcReport( &ut->dataStore, ut );
#endif
}


/**
  Makes sure the buffer is marked as used.

  If the buffer had not already been marked as used, then non-zero is
  returned, and the caller is expected to invoke the UDatatype::markBuf method.

  \note This may only be called from inside a UDatatype::mark or
  UDatatype::markBuf method.

  \param bufN   Buffer index into thread dataStore.

  \return Zero if already marked as used.
*/
int ur_markBuffer( UThread* ut, UIndex bufN )
{
    uint8_t* byte = ut->gcBits.ptr.b + (bufN >> 3);
    int mask = 1 << (bufN & 7);
    if( *byte & mask )
        return 0;
    *byte |= mask;
    return 1;
}


//EOF
