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
    UBuffer* bin = ur_buffer( BT->tempN );
    if( ! boron_doCStr( ut, bin->ptr.c, bin->used ) )
    {
        UBuffer str;
        UCell* ex = boron_exception( ut );
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

/*-cf-
    thread
        body    block!
        /port   Create thread port
    return: Thread port or unset!

    Create a new thread.
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
    UBuffer* bin = child->dataStore.ptr.buf + ((BoronThread*) child)->tempN;
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

#ifndef _WIN32
    // Detach to automatically release thread memory when it exits.
    pthread_detach( osThr );
#endif

    return UR_OK;
}


/*EOF*/
