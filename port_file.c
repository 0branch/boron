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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#define open    _open
#define close   _close
#define read    _read
#define write   _write
#define lseek   _lseek
#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#define S_IRGRP 0
#define S_IROTH 0
#define ssize_t int
#else
#include <unistd.h>
#endif


#define FD  used


static int file_open( UThread* ut, UBuffer* port, const UCell* from, int opt )
{
    int fd = -1;
    int flags;
    const char* path;

    if( ur_is(from, UT_INT) )
    {
        // Standard I/O
        // 0,1,2 - stdin, stdout, stderr
        fd = ur_int(from);
        if( fd < 0 || fd > 2 )
            return ur_error( ut, UR_ERR_SCRIPT, "Cannot open std file %d", fd );
    }
    else if( ur_is(from, UT_FILE) )
    {
        path = boron_cstr( ut, from, 0 );

        if( ! boron_requestAccess( ut, "Open file \"%s\"", path ) )
            return UR_THROW;

        if( opt & UR_PORT_READ )
            flags = O_RDONLY;
        else if( opt & UR_PORT_WRITE )
            flags = O_CREAT | O_WRONLY;
        else
            flags = O_CREAT | O_RDWR;

        if( opt & UR_PORT_NEW )
            flags |= O_TRUNC;

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

    if( ur_is(dest, UT_BINARY) )
    {
        buf = ur_bufferSerM( dest );
        if( ! buf )
            return UR_THROW;
        count = ur_testAvail( buf );
        if( count < len )
            ur_binReserve( buf, len );
        else
            len = count;
    }
    else if( ur_is(dest, UT_STRING) )
    {
        buf = ur_bufferSerM( dest );
        if( ! buf )
            return UR_THROW;
        count = ur_testAvail( buf );
        if( count < len )
            ur_arrReserve( buf, len );
        else
            len = count;
    }
    else
    {
        // NOTE: Make invalidates port.
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


static int file_seek( UThread* ut, UBuffer* port, UCell* pos, int where )
{
    if( ur_is(pos, UT_INT) )
    {
        switch( where )
        {
            case UR_PORT_HEAD:
                where = SEEK_SET;
                break;
            case UR_PORT_TAIL:
                where = SEEK_END;
                break;
            case UR_PORT_SKIP:
            default:
                where = SEEK_CUR;
                break;
        }
        if( lseek( port->FD, ur_int(pos), where ) == -1 )
            return ur_error( ut, UR_ERR_ACCESS, strerror( errno ) );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "file seek expected int!" );
}


static int file_waitFD( UBuffer* port )
{
    return port->FD;
}


UPortDevice port_file =
{
    file_open, file_close, file_read, file_write, file_seek,
    file_waitFD
};


/*EOF*/
