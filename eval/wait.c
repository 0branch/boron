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


#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <errno.h>
#endif

#include "boron.h"
#include "boron_internal.h"


#define MAX_PORTS   16      // LIMIT: Maximum ports wait can handle.


typedef struct
{
    const UCell* cell;
    int fd;
}
PortInfo;


typedef struct
{
#ifdef _WIN32
    DWORD portCount;
    DWORD timeout;
    HANDLE handles[ MAX_PORTS ];
#else
    struct timeval tv;
    fd_set readfds;
    int nfds;
    uint16_t timeout;
    uint16_t portCount;
#endif
    PortInfo ports[ MAX_PORTS ];
}
WaitInfo;


static void _waitOnPort( UThread* ut, WaitInfo* wi, const UCell* portC )
{
    int fd;
    PORT_SITE(dev, pbuf, portC);
    if( dev )
    {
        assert( wi->portCount < MAX_PORTS );
#ifdef _WIN32
        fd = dev->waitFD( pbuf, wi->handles + wi->portCount );
        if( fd > -1 )
        {
            wi->ports[ wi->portCount ].cell = portC;
            wi->ports[ wi->portCount ].fd   = fd;
            ++wi->portCount;
        }
#else
        fd = dev->waitFD( pbuf );
        if( fd > -1 )
        {
            wi->ports[ wi->portCount ].cell = portC;
            wi->ports[ wi->portCount ].fd   = fd;
            ++wi->portCount;

            FD_SET( fd, &wi->readfds );

            ++fd;
            if( fd > wi->nfds )
                wi->nfds = fd;
        }
#endif
    }
}


static int _fillWaitInfo( UThread* ut, WaitInfo* wi,
                          const UCell* it, const UCell* end )
{
    while( it != end )
    {
        if( ur_is(it, UT_INT) )
        {
#ifdef _WIN32
            wi->timeout = ur_int(it) * 1000;
#else
            wi->tv.tv_sec  = ur_int(it);
            wi->tv.tv_usec = 0;
            wi->timeout = 1;
#endif
        }
        else if( ur_is(it, UT_WORD) )
        {
            const UCell* cell;
            if( ! (cell = ur_wordCell( ut, it )) )
                return UR_THROW;
            if( ur_is(cell, UT_PORT) )
                _waitOnPort( ut, wi, cell );
        }
        else if( ur_is(it, UT_PORT) )
        {
            _waitOnPort( ut, wi, it );
        }
        else if( ur_is(it, UT_DOUBLE) || ur_is(it, UT_TIME) )
        {
#ifdef _WIN32
            wi->timeout = (DWORD) (ur_double(it) * 1000.0);
#else
            double n = floor( ur_double(it) );

            wi->tv.tv_sec  = n ? ((long) n) : 0;
            wi->tv.tv_usec = (long) ((ur_double(it) - n) * 1000000.0);
            wi->timeout = 1;
#endif
        }

        ++it;
    }
    return UR_OK;
}


/*-cf-
    wait
        target  int!/double!/time!/block!/port!
    return: Port ready for reading or none.
    group: io

    Wait for data on ports.
*/
// (target -- port)
CFUNC_PUB( cfunc_wait )
{
    WaitInfo wi;

#ifdef _WIN32
    DWORD n;

    wi.timeout = INFINITE;
    wi.portCount = 0;

    if( ur_is(a1, UT_BLOCK) )
    {
        UBlockIt bi;
        ur_blockIt( ut, &bi, a1 );
        if( ! _fillWaitInfo( ut, &wi, bi.it, bi.end ) )
            return UR_THROW;
    }
    else
    {
        if( ! _fillWaitInfo( ut, &wi, a1, a1 + 1 ) )
            return UR_THROW;
    }

    n = WaitForMultipleObjects( wi.portCount, wi.handles, FALSE, wi.timeout );
    if( n == WAIT_FAILED )
    {
        return ur_error( ut, UR_ERR_INTERNAL,
                         "WaitForMultipleObjects - %d\n", GetLastError() );
    }
    else if( n != WAIT_TIMEOUT )
    {
        WSANETWORKEVENTS nev;
        int sock;

        n -= WAIT_OBJECT_0;
        assert( n >= 0 && n < wi.portCount );

        // Sockets must reset the event object.
        sock = wi.ports[ n ].fd;
        if( sock != UR_PORT_HANDLE )
            WSAEnumNetworkEvents( sock, wi.handles[ n ], &nev );

        *res = *wi.ports[ n ].cell;
        return UR_OK;
    }
#else
    int n;

    wi.nfds = 0;
    wi.timeout = 0;
    wi.portCount = 0;

    FD_ZERO( &wi.readfds );
    //FD_SET( 0, &wi.readfds );    // stdin

    if( ur_is(a1, UT_BLOCK) )
    {
        UBlockIt bi;
        ur_blockIt( ut, &bi, a1 );
        if( ! _fillWaitInfo( ut, &wi, bi.it, bi.end ) )
            return UR_THROW;
    }
    else
    {
        if( ! _fillWaitInfo( ut, &wi, a1, a1 + 1 ) )
            return UR_THROW;
    }

    n = select( wi.nfds, &wi.readfds, NULL, NULL,
                wi.timeout ? &wi.tv : NULL );
    if( n == -1 )
    {
        return ur_error( ut, UR_ERR_INTERNAL,
                         "select - %s\n", strerror(errno) );
    }
    else if( n )
    {
        PortInfo* pi;
        PortInfo* pend;

        /*
        if( FD_ISSET( 0, &readfds ) )
            //stdin...
        else
        */

        pi   = wi.ports;
        pend = pi + wi.portCount;
        while( pi != pend )
        {
            if( FD_ISSET( pi->fd, &wi.readfds ) )
            {
                *res = *pi->cell;
                return UR_OK;
            }
            ++pi;
        }
    }
#endif

    ur_setId( res, UT_NONE );
    return UR_OK;
}


/*EOF*/
