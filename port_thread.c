/* Thread Port */


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
        ur_blkFree( &ext->queueA );
        ur_blkFree( &ext->queueB );
        memFree( port->ptr.v );
    }
    port->ptr.v = 0;
}


#define ur_unbind(c)    (c)->word.ctx = UR_INVALID_BUF

static int thread_read( UThread* ut, UBuffer* port, UCell* dest, int part )
{
    ThreadExt* ext = (ThreadExt*) port->ptr.v;
    int type;
    (void) ut;
    (void) part;

    ur_setId(dest, UT_NONE);

    if( port->SIDE == SIDE_A )
    {
        mutexLock( ext->mutexB );
        if( ext->itB < ext->queueB.used )
        {
            *dest = ext->queueB.ptr.cell[ ext->itB ];
            ++ext->itB;
            if( ext->itB == ext->queueB.used )
                ext->itB = ext->queueB.used = 0;
        }
        mutexUnlock( ext->mutexB );
    }
    else
    {
        mutexLock( ext->mutexA );
        if( ext->itA < ext->queueA.used )
        {
            *dest = ext->queueA.ptr.cell[ ext->itA ];
            ++ext->itA;
            if( ext->itA == ext->queueA.used )
                ext->itA = ext->queueA.used = 0;
        }
        mutexUnlock( ext->mutexA );
    }

    type = ur_type(dest);
    if( ur_isWordType(type) )
    {
        if( ur_binding(dest) == UR_BIND_THREAD )
            ur_unbind(dest);
    }
    else if( ur_isSeriesType(type) )
    {
        if( ! ur_isShared( dest->series.buf ) )
            ur_setId(dest, UT_NONE);
    }

    return UR_OK;
}


static int thread_write( UThread* ut, UBuffer* port, const UCell* data )
{
    ThreadExt* ext = (ThreadExt*) port->ptr.v;
    (void) ut;

    /*
    if( ur_type(data) > UT_OPTION )
    {
        return ur_error( ut, UR_ERR_TYPE,
                         "Cannot send buffers through thread port" );
    }
    */

    if( port->SIDE == SIDE_A )
    {
        mutexLock( ext->mutexA );
        if( ext->queueA.END_OPEN )
            ur_blkPush( &ext->queueA, data );
        mutexUnlock( ext->mutexA );
    }
    else
    {
        mutexLock( ext->mutexB );
        if( ext->queueB.END_OPEN )
            ur_blkPush( &ext->queueB, data );
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
