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


#include "urlan.h"
#include "os.h"


#ifdef DEBUG
//#define GC_REPORT   1
#endif

//#define GC_TIME     1
#ifdef GC_TIME
//#include <time.h>

__inline__ uint64_t rdtsc()
{
    uint32_t lo, hi;
    /* We cannot use "=A", since this would use %rax on x86_64 */
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return (uint64_t) hi << 32 | lo;
}
#endif


//#define bitIsSet(array,n)    (array[(n)>>3] & 1<<((n)&7))
#define setBit(array,n)      (array[(n)>>3] |= 1<<((n)&7))


#ifdef GC_REPORT
void ur_gcReport( UBuffer* store, UThread* ut )
{
    static const char datatypeChar[] = ".!nlcidDty+3w'::ob0s%v[(/|<CeFf~~~~~~";
    int used = 0;
    int unused = 0;
    UBuffer* it  = store->ptr.buf;
    UBuffer* end = it + store->used;

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
#endif


/*
  Have custom datatypes perform the given recycle phase.
*/
static void _recyclePhase( UThread* ut, int phase )
{
    UDatatype** it  = ut->types + UT_BI_COUNT;
    UDatatype** end = ut->types + ur_datatypeCount( ut );
    while( it != end )
    {
        UDatatype* dt = *it;
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
    UDatatype** datatypes = ut->types;
    UBuffer* gcBits = &ut->gcBits;

#ifdef GC_TIME
    //clock_t t1, t1e;
    uint64_t t1, t1e;
#endif

#ifdef GC_REPORT
    dprint( "\nRecycle UThread %p:\n\n", (void*) ut );
    ur_gcReport( &ut->dataStore, ut );
#endif

#ifdef GC_TIME
    //t1 = clock();
    t1 = rdtsc();
#endif


    _recyclePhase( ut, UR_RECYCLE_MARK );


    // Mark all buffers as unused.

    byteSize = (ut->dataStore.used + 7) / 8;
    ur_binReserve(gcBits, byteSize);
    gcBits->used = byteSize;
    markBits = gcBits->ptr.b;
    memSet(markBits, 0, byteSize);


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
        while( n > -1 )
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

    t1e = rdtsc() - t1;
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
