/* Boron Thread */


#include <math.h>
#include <time.h>


#ifdef _WIN32
static DWORD WINAPI threadRoutine( LPVOID arg )
#else
static void* threadRoutine( void* arg )
#endif
{
    UThread* ut = (UThread*) arg;
    UBuffer* bin = &BT->tbin;
    if( ! boron_evalUtf8( ut, bin->ptr.c, bin->used ) )
    {
        UBuffer str;
        UCell* ex = ur_exception( ut );
        if( ! ur_is(ex, UT_WORD) || ur_atom(ex) != UR_ATOM_QUIT )
        {
            ur_strInit( &str, UR_ENC_UTF8, 0 );
            ur_toText( ut, ex, &str );
            ur_strTermNull( &str );
            puts( str.ptr.c );
            ur_strFree( &str );
        }
    }
    ur_destroyThread( ut );
    return 0;
}


extern void boron_installThreadPort( UThread*, const UCell*, UThread* );
extern void boron_setJoinThread( UThread*, const UCell*, OSThread );

/*-cf-
    thread
        routine string!/block!
        /port   Create thread port.
    return: Thread port or unset!

    Create a new thread to run a routine.

    A string! routine argument is preferred as a block! will be converted to
    a string! before it can be passed to the thread.

    If the /port option is used the new thread will have a port! bound to the
    'thread-port word that is connected to the one returned from the function.
    This can be used for bi-directional communication.

    Each thread has it's own data store, so series values passed through the
    port will become empty on write as ownership is transferred.
    Only one series can be sent through the port on each write, so writing a
    block! that contains series values will throw an error.
    Words not bound to the shared environment will become unbound in the
    reading thread.
*/
CFUNC( cfunc_thread )
{
#define OPT_THREAD_PORT 0x01
    OSThread osThr;
    UThread* child;
    UBuffer code;
#ifdef _WIN32
    DWORD winId;
#endif

    ur_strInit( &code, UR_ENC_UTF8, 0 );

    if( ur_is(a1, UT_STRING) )
    {
        USeriesIter si;
        ur_seriesSlice( ut, &si, a1 );
        ur_strAppend( &code, si.buf, si.it, si.end );
    }
    else if( ur_is(a1, UT_BLOCK) )
    {
        ur_toStr( ut, a1, &code, -1 );
    }
    else
        return ur_error( ut, UR_ERR_TYPE, "thread expected string!/block!" );

    child = ur_makeThread( ut );
    if( ! child )
    {
        ur_strFree( &code );
        return ur_error( ut, UR_ERR_INTERNAL, "No memory for thread" );
    }

    // Copy code to child temp. binary.
    {
    UBuffer* bin = &((BoronThread*) child)->tbin;
    bin->used = 0;
    ur_binAppendData( bin, code.ptr.b, code.used );
    ur_strFree( &code );
    }

    ur_setId(res, UT_UNSET);

    if( CFUNC_OPTIONS & OPT_THREAD_PORT )
    {
        if( ! port_thread.open( ut, &port_thread, res, 0, res ) )
            return UR_THROW;
        boron_installThreadPort( ut, res, child );
    }

#ifdef _WIN32
    osThr = CreateThread( NULL, 0, threadRoutine, child, 0, &winId );
    if( osThr == NULL )
#else
    if( pthread_create( &osThr, 0, threadRoutine, child ) != 0 )
#endif
    {
        return ur_error( ut, UR_ERR_INTERNAL, "Could not create thread" );
    }

    if( CFUNC_OPTIONS & OPT_THREAD_PORT )
    {
        // Will pthread_join() when port closed.
        boron_setJoinThread( ut, res, osThr );
    }
#ifndef _WIN32
    else
    {
        // Detach to automatically release thread memory when it exits.
        pthread_detach( osThr );
    }
#endif

    return UR_OK;
}


/*EOF*/
