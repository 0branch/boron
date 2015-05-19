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

#ifdef __linux
#define USE_EVENTFD
#include <sys/eventfd.h>
#elif ! defined(_WIN32)
#include <sys/socket.h>
#endif


#define SIDE_A      0
#define SIDE_B      1
#define SIDE        elemSize    // Port UBuffer
#define END_OPEN    buf.form    // ThreadQueue


typedef struct
{
    OSMutex mutex;
    OSCond  cond;
    UBuffer buf;
    UIndex  readIt;
#ifdef USE_EVENTFD
    int     eventFD;
#elif defined(_WIN32)
    HANDLE  eventH;
#else
    int     socketFD[2];    // 0 is read, 1 is write.
#endif
}
ThreadQueue;


typedef struct
{
    const UPortDevice* dev;
    ThreadQueue A;      // SIDE_A writes here.
    ThreadQueue B;      // SIDE_B writes here.
}
ThreadExt;


void boron_installThreadPort( UThread* ut, const UCell* portC, UThread* utB )
{
    static char portStr[] = "thread-port";
    UBuffer* ctx;
    const UBuffer* port;
    UBuffer* buf;
    UCell* cell;
    ThreadExt* ext;


    port = ur_bufferSer( portC );
    ext = (ThreadExt*) port->ptr.v;
    ext->A.END_OPEN = 1;


    // Make port for SIDE_B.

    ctx = ur_threadContext( utB );
    cell = ur_ctxAddWord( ctx, ur_internAtom(ut, portStr, portStr + 11) );
    buf = boron_makePort( utB, ext->dev, ext, cell );
    buf->SIDE = SIDE_B;
}


static int _initThreadQueue( ThreadQueue* queue )
{
    if( mutexInitF( queue->mutex ) )
        return 0;

    condInit( queue->cond );
    ur_blkInit( &queue->buf, UT_BLOCK, 0 );
    queue->readIt = 0;
#ifdef USE_EVENTFD
    queue->eventFD = eventfd( 0, EFD_CLOEXEC );
    if( queue->eventFD == -1 )
    {
        perror( "eventfd" );
        return 0;
    }
#elif defined(_WIN32)
    queue->eventH = CreateEvent( NULL, FALSE, FALSE, NULL );
    if( queue->eventH == NULL )
    {
        fprintf( stderr, "CreateEvent error (%ld)\n", GetLastError() );
        return 0;
    }
#else
    if( socketpair( AF_UNIX, SOCK_STREAM, 0, queue->socketFD ) )
    {
        queue->socketFD[0] = queue->socketFD[1] = -1;
        perror( "socketpair" );
        return 0;
    }
#endif
    return 1;
}


static void _freeThreadQueue( ThreadQueue* queue )
{
    // TODO: Free buffers in queue.

    mutexFree( queue->mutex );
    condFree( queue->cond );
    ur_blkFree( &queue->buf );
#ifdef USE_EVENTFD
    if( queue->eventFD > -1 )
        close( queue->eventFD );
#elif defined(_WIN32)
    if( queue->eventH != NULL )
        CloseHandle( queue->eventH );
#else
    if( queue->socketFD[0] > -1 )
    {
        close( queue->socketFD[0] );
        close( queue->socketFD[1] );
    }
#endif
}


static int thread_open( UThread* ut, const UPortDevice* pdev,
                        const UCell* from, int opt, UCell* res )
{
    UBuffer* port;
    ThreadExt* ext;
    (void) from;
    (void) opt;

    ext = (ThreadExt*) memAlloc( sizeof(ThreadExt) );
    if( ! ext )
        return ur_error( ut, UR_ERR_INTERNAL, "Could not alloc thread port" );

    if( ! _initThreadQueue( &ext->A ) )
    {
fail:
        memFree( ext );
        return ur_error( ut, UR_ERR_INTERNAL, "Could not create thread queue" );
    }

    if( ! _initThreadQueue( &ext->B ) )
    {
        mutexFree( ext->A.mutex );
        goto fail;
    }

    ext->A.END_OPEN = 0;
    ext->B.END_OPEN = 1;

    port = boron_makePort( ut, pdev, ext, res );
    port->SIDE = SIDE_A;
    return UR_OK;
}


static void thread_close( UBuffer* port )
{
    ThreadExt* ext = (ThreadExt*) port->ptr.v;
    ThreadQueue* queue;
    uint8_t endOpen;

    queue = (port->SIDE == SIDE_A) ? &ext->A : &ext->B;
    mutexLock( queue->mutex );
    endOpen = queue->END_OPEN;
    mutexUnlock( queue->mutex );

    if( endOpen )
    {
        queue = (port->SIDE == SIDE_A) ? &ext->B : &ext->A;
        mutexLock( queue->mutex );
        queue->END_OPEN = 0;
        mutexUnlock( queue->mutex );
    }
    else
    {
        _freeThreadQueue( &ext->A );
        _freeThreadQueue( &ext->B );
        memFree( port->ptr.v );

        //printf( "KR thread_close\n" );
    }
    //pbuf->ptr.v = 0;      // Done by port_destroy().
}


static UIndex thread_dequeue( UBuffer* qbuf, UIndex it, UCell* dest,
                              UBuffer* transitBuf )
{
    *dest = qbuf->ptr.cell[ it ];
    ++it;
    if( ur_isSeriesType(ur_type(dest)) && ! ur_isShared( dest->series.buf ) )
    {
        memCpy( transitBuf, qbuf->ptr.cell + it, sizeof(UBuffer) );
        ++it;
    }
    if( it == qbuf->used )
        it = qbuf->used = 0;
    return it;
}


static inline void readEvent( ThreadQueue* q )
{
#ifdef USE_EVENTFD
    uint64_t n;
    read( q->eventFD, &n, sizeof(n) );
#elif defined(_WIN32)
    DWORD result;
    result = WaitForSingleObject( q->eventH, INFINITE );
    if( result != WAIT_OBJECT_0 )
        fprintf( stderr, "WaitForSingleObject error (%ld)\n", GetLastError() );
#else
    uint16_t n;
    read( q->socketFD[0], &n, sizeof(n) );
#endif
}

static inline void writeEvent( ThreadQueue* q )
{
#ifdef USE_EVENTFD
    uint64_t n = 1;
    write( q->eventFD, &n, sizeof(n) );
#elif defined(_WIN32)
    if( ! SetEvent( q->eventH ) )
        fprintf( stderr, "SetEvent error (%ld)\n", GetLastError() );
#else
    uint16_t n = 1;
    write( q->socketFD[1], &n, sizeof(n) );
#endif
}


#define ur_unbind(c)    (c)->word.ctx = UR_INVALID_BUF

static int thread_read( UThread* ut, UBuffer* port, UCell* dest, int part )
{
    UBuffer tbuf;
    ThreadExt* ext = (ThreadExt*) port->ptr.v;
    ThreadQueue* queue;
    (void) part;

    tbuf.type = 0;

    queue = (port->SIDE == SIDE_A) ? &ext->B : &ext->A;

    if( ! queue->readIt )
        readEvent( queue );     // Waits until data is available.

    mutexLock( queue->mutex );
    while( queue->readIt >= queue->buf.used )
    {
        if( condWaitF( queue->cond, queue->mutex ) )
        {
            mutexUnlock( queue->mutex );
            goto waitError;
        }
    }
    queue->readIt = thread_dequeue( &queue->buf, queue->readIt, dest, &tbuf );
    mutexUnlock( queue->mutex );

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


static void thread_queue( UBuffer* qbuf, const UCell* data, UBuffer* dataBuf )
{
    ur_arrReserve( qbuf, qbuf->used + 2 );
    qbuf->ptr.cell[ qbuf->used ] = *data;
    ++qbuf->used;
    if( dataBuf )
    {
        memCpy( qbuf->ptr.cell + qbuf->used, dataBuf, sizeof(UBuffer) );
        ++qbuf->used;

        // Buffer is directly transferred through port to avoid copying.
        dataBuf->used  = 0;
        dataBuf->ptr.v = 0;
    }
}


static int thread_write( UThread* ut, UBuffer* port, const UCell* data )
{
    UBuffer* buf;
    ThreadExt* ext = (ThreadExt*) port->ptr.v;
    ThreadQueue* queue;
    int type = ur_type(data);

    if( ur_isSeriesType(type) && ! ur_isShared( data->series.buf ) )
    {
        buf = ur_bufferSerM( data );
        if( ! buf )
            return UR_THROW;

        if( ur_isBlockType(type) )
        {
            UCell* it  = buf->ptr.cell;
            UCell* end = it + buf->used;
            while( it != end )
            {
                type = ur_type(it);
                if( ur_isWordType(type) )
                {
                    if( ur_binding(it) == UR_BIND_THREAD )
                        ur_unbind(it);
                }
                else if( ur_isSeriesType(type) )
                {
                    return ur_error( ut, UR_ERR_INTERNAL,
                        "Cannot write block containing series to thread port" );
                }
                ++it;
            }
        }
    }
    else
        buf = 0;

    queue = (port->SIDE == SIDE_A) ? &ext->A : &ext->B;

    mutexLock( queue->mutex );
    if( queue->END_OPEN )
    {
        thread_queue( &queue->buf, data, buf );
        condSignal( queue->cond );
        writeEvent( queue );
    }
    mutexUnlock( queue->mutex );

    return UR_OK;
}


static int thread_seek( UThread* ut, UBuffer* port, UCell* pos, int where )
{
    (void) port;
    (void) pos;
    (void) where;
    return ur_error( ut, UR_ERR_SCRIPT, "Cannot seek on thread port" );
}


#ifdef _WIN32
static int thread_waitFD( UBuffer* port, void** handle )
{
    ThreadExt* ext = (ThreadExt*) port->ptr.v;
    *handle = (port->SIDE == SIDE_A) ? ext->B.eventH : ext->A.eventH;
    return UR_PORT_HANDLE;
}
#else
static int thread_waitFD( UBuffer* port )
{
#ifdef USE_EVENTFD
    ThreadExt* ext = (ThreadExt*) port->ptr.v;
    return (port->SIDE == SIDE_A) ? ext->B.eventFD : ext->A.eventFD;
#else
    ThreadExt* ext = (ThreadExt*) port->ptr.v;
    return (port->SIDE == SIDE_A) ? ext->B.socketFD[0] : ext->A.socketFD[0];
#endif
}
#endif


UPortDevice port_thread =
{
    thread_open, thread_close, thread_read, thread_write, thread_seek,
    thread_waitFD, 0
};


/*EOF*/
