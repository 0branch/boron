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


/*
#include "urlan.h"
#include <math.h>
*/

#ifdef _WIN32
// Included in os.h
//#include <winsock2.h>
//#include <windows.h>
#else
#include <sys/select.h>
#include <errno.h>
#endif


#define MAX_PORTS   4       // LIMIT: Maximum ports wait can handle.


typedef struct
{
    const UCell* cell;
    int fd;
}
PortInfo;


typedef struct
{
    struct timeval tv;
    fd_set readfds;
    int nfds;
    uint16_t timeout;
    uint16_t portCount;
    PortInfo ports[ MAX_PORTS ];
}
WaitInfo;


static void _waitOnPort( UThread* ut, WaitInfo* wi, const UCell* portC )
{
    int fd;
    PORT_SITE(dev, pbuf, portC);
    if( dev )
    {
        fd = dev->waitFD( pbuf );
        if( fd > -1 )
        {
            assert( wi->portCount < MAX_PORTS );
            wi->ports[ wi->portCount ].cell = portC;
            wi->ports[ wi->portCount ].fd   = fd;
            ++wi->portCount;

            FD_SET( fd, &wi->readfds );

            ++fd;
            if( fd > wi->nfds )
                wi->nfds = fd;
        }
    }
}


static int _fillWaitInfo( UThread* ut, WaitInfo* wi,
                          const UCell* it, const UCell* end )
{
    while( it != end )
    {
        if( ur_is(it, UT_INT) )
        {
            wi->tv.tv_sec  = ur_int(it);
            wi->tv.tv_usec = 0;
            wi->timeout = 1;
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
        else if( ur_is(it, UT_DECIMAL) || ur_is(it, UT_TIME) )
        {
            double n;

            n = floor( ur_decimal(it) );

            wi->tv.tv_sec  = n ? ((long) n) : 0;
            wi->tv.tv_usec = (long) ((ur_decimal(it) - n) * 1000000.0);
            wi->timeout = 1;
        }

        ++it;
    }
    return UR_OK;
}


/*-cf-
    wait
        target  int!/decimal!/time!/block!/port!
    return: Port ready for reading or none.

    Wait for data on ports.
*/
// (target -- port)
CFUNC( cfunc_wait )
{
    WaitInfo wi;
    int n;


    wi.nfds = 0;
    wi.timeout = 0;
    wi.portCount = 0;

    FD_ZERO( &wi.readfds );
    //FD_SET( 0, &wi.readfds );    // stdin

    if( ur_is(a1, UT_BLOCK) )
    {
        UBlockIter bi;
        ur_blkSlice( ut, &bi, a1 );
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
#ifdef _WIN32
    if( n == SOCKET_ERROR )
    {
        return ur_error( ut, UR_ERR_INTERNAL,
                         "select - %d\n", WSAGetLastError() );
    }
#else
    if( n == -1 )
    {
        return ur_error( ut, UR_ERR_INTERNAL,
                         "select - %s\n", strerror(errno) );
    }
#endif
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
    ur_setId( res, UT_NONE );
    return UR_OK;
}


/*EOF*/
