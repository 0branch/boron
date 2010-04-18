/*
  Copyright 2010 Karl Robillard

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


#include "boron.h"
#include "os.h"


#define SIDE_A      0
#define SIDE_B      1
#define SIDE        elemSize    // Port buffer
#define END_OPEN    form        // Queue buffer


typedef struct
{
    UPortDevice* dev;
    OSMutex mutexA;
    OSMutex mutexB;
    OSCond  condA;
    OSCond  condB;
    UBuffer queueA;     // SIDE_A writes here.
    UBuffer queueB;     // SIDE_B writes here.
    UIndex  itA;
    UIndex  itB;
}
ThreadExt;


void boron_installThreadPort( UThread* ut, const UCell* portC, UThread* utB )
{
    static char portStr[] = "port";
    UBuffer* ctx;
    const UBuffer* port;
    UBuffer* buf;
    UCell* cell;
    ThreadExt* ext;
    UIndex bufN;


    port = ur_bufferSer( portC );
    ext = (ThreadExt*) port->ptr.v;
    ext->queueA.END_OPEN = 1;


    // Make port for SIDE_B.

    ur_genBuffers( utB, 1, &bufN );
    buf = utB->dataStore.ptr.buf + bufN;    //ur_buffer( bufN )

    buf->type  = UT_PORT;
    buf->form  = UR_PORT_EXT;
    buf->SIDE  = SIDE_B;
    buf->ptr.v = ext;

    ctx = ur_threadContext( utB );
    cell = ur_ctxAddWord( ctx, ur_internAtom(ut, portStr, portStr + 4) );
    ur_setId(cell, UT_PORT);
    ur_setSeries(cell, bufN, 0);
}


static int thread_open( UThread* ut, UBuffer* port, const UCell* from )
{
    ThreadExt* ext;
    (void) from;

    ext = (ThreadExt*) memAlloc( sizeof(ThreadExt) );
    if( ! ext )
        return ur_error( ut, UR_ERR_INTERNAL, "Could not alloc thread port" );

    if( mutexInitF( ext->mutexA ) )
    {
fail:
        memFree( ext );
        return ur_error( ut, UR_ERR_INTERNAL, "Could not create thread mutex" );
    }
    if( mutexInitF( ext->mutexB ) )
    {
        mutexFree( ext->mutexA );
        goto fail;
    }
    condInit( ext->condA );
    condInit( ext->condB );
    ur_blkInit( &ext->queueA, UT_BLOCK, 0 );
    ur_blkInit( &ext->queueB, UT_BLOCK, 0 );
    ext->queueA.END_OPEN = 0;
    ext->queueB.END_OPEN = 1;
    ext->itA = ext->itB = 0;

    port->SIDE = SIDE_A;
    boron_extendPort( port, (UPortDevice**) ext );
    return UR_OK;
}


static void thread_close( UBuffer* port )
{
    ThreadExt* ext = (ThreadExt*) port->ptr.v;
    uint8_t endOpen;

    if( port->SIDE == SIDE_A )
    {
        mutexLock( ext->mutexA );
        endOpen = ext->queueA.END_OPEN;
        mutexUnlock( ext->mutexA );
        if( endOpen )
        {
            mutexLock( ext->mutexB );
            ext->queueB.END_OPEN = 0;
            mutexUnlock( ext->mutexB );
        }
    }
    else
    {
        mutexLock( ext->mutexB );
        endOpen = ext->queueB.END_OPEN;
        mutexUnlock( ext->mutexB );
        if( endOpen )
        {
            mutexLock( ext->mutexA );
            ext->queueA.END_OPEN = 0;
            mutexUnlock( ext->mutexA );
        }
    }

    if( ! endOpen )
    {
        mutexFree( ext->mutexA );
        mutexFree( ext->mutexB );
        condFree( ext->condA );
        condFree( ext->condB );
        ur_blkFree( &ext->queueA );
        ur_blkFree( &ext->queueB );
        memFree( port->ptr.v );
    }
    port->ptr.v = 0;
}


static UIndex thread_dequeue( UBuffer* queue, UIndex it, UCell* dest,
                              UBuffer* transitBuf )
{
    *dest = queue->ptr.cell[ it ];
    ++it;
    if( ur_isSeriesType(ur_type(dest)) && ! ur_isShared( dest->series.buf ) )
    {
        memCpy( transitBuf, queue->ptr.cell + it, sizeof(UBuffer) );
        ++it;
    }
    if( it == queue->used )
        it = queue->used = 0;
    return it;
}


#define ur_unbind(c)    (c)->word.ctx = UR_INVALID_BUF

static int thread_read( UThread* ut, UBuffer* port, UCell* dest, int part )
{
    UBuffer tbuf;
    ThreadExt* ext = (ThreadExt*) port->ptr.v;
    (void) part;

    tbuf.type = 0;

    if( port->SIDE == SIDE_A )
    {
        mutexLock( ext->mutexB );
        while( ext->itB >= ext->queueB.used )
        {
            if( condWaitF( ext->condB, ext->mutexB ) )
            {
                mutexUnlock( ext->mutexB );
                goto waitError;
            }
        }
        ext->itB = thread_dequeue( &ext->queueB, ext->itB, dest, &tbuf );
        mutexUnlock( ext->mutexB );
    }
    else
    {
        mutexLock( ext->mutexA );
        while( ext->itA >= ext->queueA.used )
        {
            if( condWaitF( ext->condA, ext->mutexA ) )
            {
                mutexUnlock( ext->mutexA );
                goto waitError;
            }
        }
        ext->itA = thread_dequeue( &ext->queueA, ext->itA, dest, &tbuf );
        mutexUnlock( ext->mutexA );
    }

    if( tbuf.type )
    {
        UIndex bufN;

        dest->series.buf = UR_INVALID_BUF;
        ur_genBuffers( ut, 1, &bufN );
        dest->series.buf = bufN;

        memCpy( ur_buffer( bufN ), &tbuf, sizeof(UBuffer) );
    }
    else
    {
        int type = ur_type(dest);
        if( ur_isWordType(type) )
        {
            if( ur_binding(dest) == UR_BIND_THREAD )
                ur_unbind(dest);
        }
    }

    return UR_OK;

waitError:

    return ur_error( ut, UR_ERR_INTERNAL, "thread_read condWait failed" );
}


static void thread_queue( UBuffer* queue, const UCell* data, UBuffer* buf )
{
    ur_arrReserve( queue, queue->used + 2 );
    queue->ptr.cell[ queue->used ] = *data;
    ++queue->used;
    if( buf )
    {
        memCpy( queue->ptr.cell + queue->used, buf, sizeof(UBuffer) );
        ++queue->used;

        // Buffer is directly transferred through port to avoid copying.
        buf->used  = 0;
        buf->ptr.v = 0;
    }
}


static int thread_write( UThread* ut, UBuffer* port, const UCell* data )
{
    UBuffer* buf;
    ThreadExt* ext = (ThreadExt*) port->ptr.v;
    int type = ur_type(data);

    if( ur_isSeriesType(type) && ! ur_isShared( data->series.buf ) )
    {
        buf = ur_bufferSerM( data );
        if( ! buf )
            return UR_THROW;
    }
    else
        buf = 0;

    if( port->SIDE == SIDE_A )
    {
        mutexLock( ext->mutexA );
        if( ext->queueA.END_OPEN )
        {
            thread_queue( &ext->queueA, data, buf );
            condSignal( ext->condA );
        }
        mutexUnlock( ext->mutexA );
    }
    else
    {
        mutexLock( ext->mutexB );
        if( ext->queueB.END_OPEN )
        {
            thread_queue( &ext->queueB, data, buf );
            condSignal( ext->condB );
        }
        mutexUnlock( ext->mutexB );
    }
    return UR_OK;
}


static int thread_seek( UThread* ut, UBuffer* port, UCell* pos )
{
    (void) port;
    (void) pos;
    return ur_error( ut, UR_ERR_SCRIPT, "Cannot seek on thread port" );
}


UPortDevice port_thread =
{
    thread_open, thread_close, thread_read, thread_write, thread_seek
};


/*EOF*/
