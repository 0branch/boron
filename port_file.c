/* File Port */


#include "boron.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


#define FD  used


static int file_open( UThread* ut, UBuffer* port, const UCell* from )
{
    int fd = -1;
    int flags;
    const char* path;

    if( ur_is(from, UT_FILE) )
    {
        path = boron_cstr( ut, from, 0 );

#if 0
        if( ! ur_userAllows( ut, "Open file \"%s\"", path ) )
            return ur_error( ut, UR_ERR_ACCESS, "User denied open" );
#endif

        flags = O_CREAT | O_RDWR;

        fd = open( path, flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH );
        if( fd == -1 )
            return ur_error( ut, UR_ERR_ACCESS, strerror( errno ) );
    }

    port->FD = fd;
    return UR_OK;
}


static void file_close( UBuffer* port )
{
    if( port->FD > -1 )
    {
        //printf( "KR file_close %d\n", port->FD );
        close( port->FD );
        port->FD = -1;
    }
}


static int file_read( UThread* ut, UBuffer* port, UCell* dest, int part )
{
    const int BUF_SIZE = 2048;
    UBuffer* buf = 0;
    ssize_t len;
    ssize_t count;

    if( part )
        len = part;
    else
        len = BUF_SIZE;

    if( ur_is(dest, UT_NONE) )
    {
        buf = ur_makeBinaryCell( ut, len, dest );
    }

    if( buf )
    {
        count = read( port->FD, buf->ptr.v, len );
        if( count == -1 )
            return ur_error( ut, UR_ERR_ACCESS, strerror( errno ) );
        buf->used = count;
    }
    return UR_OK;
}


int boron_sliceMem( UThread* ut, const UCell* cell, const void** ptr )
{
    int len = 0;
    switch( ur_type(cell) )
    {
        case UT_BINARY:
        {
            UBinaryIter bi;
            ur_binSlice( ut, &bi, cell );
            *ptr = bi.it;
            len  = bi.end - bi.it;
        }
            break;

        case UT_STRING:
        case UT_FILE:
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, cell );
            len = si.end - si.it;
            if( ur_strIsUcs2(si.buf) )
            {
                *ptr = si.buf->ptr.u16 + si.it;
                len *= 2;
            }
            else
            {
                *ptr = si.buf->ptr.b + si.it;
            }
        }
            break;
    }
    return len;
}


static int file_write( UThread* ut, UBuffer* port, const UCell* data )
{
    const void* buf;
    ssize_t count;
    ssize_t len = boron_sliceMem( ut, data, &buf );
    if( len )
    {
        count = write( port->FD, buf, len );
        if( count != len )
            return ur_error( ut, UR_ERR_ACCESS, strerror( errno ) );
    }
    return UR_OK;
}


static int file_seek( UThread* ut, UBuffer* port, UCell* pos )
{
    if( lseek( port->FD, ur_int(pos), SEEK_SET ) == -1 )
        return ur_error( ut, UR_ERR_ACCESS, strerror( errno ) );
    return UR_OK;
}


UPortDevice port_file =
{
    file_open, file_close, file_read, file_write, file_seek
};


/*EOF*/
