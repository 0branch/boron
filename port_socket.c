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


//#include <stdio.h>
#include "boron.h"
#include "os.h"

#ifdef _WIN32

#include <time.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define ssize_t     int
#define SOCKET_ERR  WSAGetLastErrorMessage()

static char* WSAGetLastErrorMessage()
{
    static char buf[40];
    sprintf( buf, "%x", WSAGetLastError() );
    return buf;
}

#else

#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>

#define SOCKET      int
#define SOCKET_ERR  strerror(errno)
#define closesocket close

#endif


#define FD      used
#define TCP     elemSize


typedef struct
{
    UPortDevice* dev;
    struct sockaddr addr;
    socklen_t addrlen;
}
SocketExt;


typedef struct
{
    const char* node;
    const char* service;
    int socktype;
}
NodeServ;


UPortDevice port_listenSocket;
UPortDevice port_socket;


/*
   Initialize NodeServ from strings like "myhost.net",
   "udp://myhost.net:180", "tcp://:4600", etc.
   Pointers are to the Boron temporary thread binary buffer.
*/
static void stringToNodeServ( UThread* ut, const UCell* strC, NodeServ* ns )
{
    char* start;
    char* cp;

    ns->node = ns->service = 0;
    ns->socktype = SOCK_STREAM;

    start = boron_cstr( ut, strC, 0 );
    if( start )
    {
        ns->node = start;
        for( cp = start; *cp; ++cp )
        {
            if( *cp == ':' )
            {
                if( cp[1] == '/' && cp[2] == '/' )
                {
                    if( start[0] == 'u' && start[1] == 'd' && start[2] == 'p' )
                        ns->socktype = SOCK_DGRAM;
                    if( cp[3] == ':' )
                        ns->node = 0;
                    else
                        ns->node = cp + 3;
                    cp += 2;
                }
                else
                {
                    *cp++ = '\0';
                    ns->service = cp;
                    break;
                }
            }
        }
    }
    //printf( "KR node: {%s} service: {%s}\n", ns->node, ns->service );
}


static int makeSockAddr( UThread* ut, SocketExt* ext, const NodeServ* ns )
{
    struct addrinfo hints;
    struct addrinfo* res;
    int err;

    memset( &hints, 0, sizeof(hints) );
    hints.ai_family   = AF_INET;  // AF_UNSPEC
    hints.ai_socktype = ns->socktype;
    if( ! ns->node )
        hints.ai_flags = AI_PASSIVE;

    err = getaddrinfo( ns->node, ns->service, &hints, &res );
    if( err )
    {
        return ur_error( ut, UR_ERR_SCRIPT, "getaddrinfo (%s)",
                         gai_strerror( err ) );
    }

    memcpy( &ext->addr, res->ai_addr, res->ai_addrlen );
    ext->addrlen = res->ai_addrlen;

    freeaddrinfo( res );
    return UR_OK;
}


/*-cf-
    set-addr
        socket      port!
        address     string!
    return: unset!

    Set Internet address of socket.
*/
CFUNC_PUB( cfunc_set_addr )
{
    const UCell* addr = a1+1;

    ur_setId(res, UT_UNSET);

    if( ur_is(a1, UT_PORT) && ur_is(addr, UT_STRING) )
    {
        UBuffer* pbuf = ur_buffer( a1->series.buf );
        if( (pbuf->form == UR_PORT_EXT) && pbuf->ptr.v )
        {
            SocketExt* ext = ur_ptr(SocketExt, pbuf);
            if( ext->dev == &port_socket )
            {
                NodeServ ns;
                stringToNodeServ( ut, addr, &ns );
                return makeSockAddr( ut, ext, &ns );
            }
        }
        return ur_error( ut, UR_ERR_SCRIPT, "set-addr expected socket port" );
    }
    return ur_error( ut, UR_ERR_TYPE, "set-addr expected port! string!" );
}


/*
   \param ext  If non-zero, then bind socket to ext->addr.
*/
static int _openUdpSocket( UThread* ut, SocketExt* ext, int nowait )
{
    SOCKET fd;

    fd = socket( AF_INET, SOCK_DGRAM, 0 );
    if( fd < 0 )
    {
        ur_error( ut, UR_ERR_ACCESS, "socket %s", SOCKET_ERR );
        return -1;
    }

    if( nowait )
    {
#ifdef _WIN32
        u_long flags = 1;
        ioctlsocket( fd, FIONBIO, &flags );
#else
        fcntl( fd, F_SETFL, fcntl( fd, F_GETFL, 0 ) | O_NONBLOCK );
#endif
    }

    if( ext )
    {
        if( bind( fd, &ext->addr, ext->addrlen ) < 0 )
        {
            closesocket( fd );
            ur_error( ut, UR_ERR_ACCESS, "bind %s", SOCKET_ERR );
            return -1;
        }
    }

    return fd;
}


static int _openTcpClient( UThread* ut, struct sockaddr* addr,
                           socklen_t addrlen )
{
    SOCKET fd;

    fd = socket( AF_INET, SOCK_STREAM, 0 );
    if( fd < 0 )
    {
        ur_error( ut, UR_ERR_ACCESS, "socket %s", SOCKET_ERR );
        return -1;
    }

    if( connect( fd, addr, addrlen ) < 0 )
    {
        closesocket( fd );
        ur_error( ut, UR_ERR_ACCESS, "connect %s", SOCKET_ERR );
        return -1;
    }

    return fd;
}


static int _openTcpServer( UThread* ut, struct sockaddr* addr,
                           socklen_t addrlen, int backlog )
{
    SOCKET fd;
    int yes = 1;

    fd = socket( AF_INET, SOCK_STREAM, 0 );
    if( fd < 0 )
    {
        ur_error( ut, UR_ERR_ACCESS, "socket %s", SOCKET_ERR );
        return -1;
    }

    if( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int) ) < 0 )
    {
        closesocket( fd );
        ur_error( ut, UR_ERR_ACCESS, "setsockopt %s", SOCKET_ERR );
        return -1;
    }

    if( bind( fd, addr, addrlen ) < 0 )
    {
        closesocket( fd );
        ur_error( ut, UR_ERR_ACCESS, "bind %s", SOCKET_ERR );
        return -1;
    }

    if( listen( fd, backlog ) < 0 )
    {
        closesocket( fd );
        ur_error( ut, UR_ERR_ACCESS, "listen %s", SOCKET_ERR );
        return -1;
    }

    return fd;
}


/*
  "tcp://host:port"
  "tcp://:port"
  ['udp local-port]
  ['udp local-port "host" host-port 'nowait]
  ['tcp "host" host-port]
*/
static int socket_open( UThread* ut, UBuffer* portBuf, const UCell* from,
                        int opt )
{
    NodeServ ns;
    SocketExt* ext;
    int socket;
    int nowait = opt & UR_PORT_NOWAIT;
    //int port = 0;
    //int hostPort = 0;
    //UCell* initAddr = 0;
    (void) opt;


    if( ! ur_is(from, UT_STRING) )
        return ur_error( ut, UR_ERR_TYPE, "socket open expected string" );

    ext = (SocketExt*) memAlloc( sizeof(SocketExt) );
    ext->addrlen = 0;

    //if( ur_is(from, UT_STRING) )
    {
        stringToNodeServ( ut, from, &ns );
        if( ! makeSockAddr( ut, ext, &ns ) )
            goto fail;
    }
#if 0
    else if( ur_is(from, UT_BLOCK) )
    {
        UBlockIter bi;
        UAtom atoms[3];

        ur_internAtoms( ut, "tcp udp nowait", atoms );

        ur_blkSlice( ut, &bi, from );
        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_INT) )
            {
                if( initAddr )
                    hostPort = ur_int(bi.it);
                else
                    port = ur_int(bi.it);
            }
            else if( ur_is(bi.it, UT_STRING) )
            {
                stringToNodeServ( ut, bi.it, &ns );
            }
            else if( ur_is(bi.it, UT_WORD) )
            {
                UAtom atom = ur_atom(bi.it);
                if( atom == atoms[0] )
                    ns.socktype = SOCK_STREAM;
                else if( atom == atoms[1] )
                    ns.socktype = SOCK_DGRAM;
                else if( atom == atoms[2] )
                    nowait = 1;
            }
        }

        if( initAddr && hostPort )
        {
            if( ! makeSockAddr( ut, ext, &ns ) )
                goto fail;
        }
    }
#endif

    if( ! ns.service )
    {
        ur_error( ut, UR_ERR_SCRIPT,
                  "Socket port requires hostname and/or port" );
        goto fail;
    }

    if( ! boron_requestAccess( ut, "Open socket %s", ns.service ) )
        goto fail;

    if( ns.socktype == SOCK_DGRAM )
    {
        socket = _openUdpSocket( ut, ns.node ? 0 : ext, nowait );
    }
    else
    {
        if( ns.node )
        {
            socket = _openTcpClient( ut, &ext->addr, ext->addrlen );
        }
        else
        {
            socket = _openTcpServer( ut, &ext->addr, ext->addrlen, 10 );
            portBuf->ptr.v = &port_listenSocket;
        }
    }

    if( socket > -1 )
    {
        portBuf->TCP = (ns.socktype == SOCK_STREAM) ? 1 : 0;
        portBuf->FD  = socket;

        boron_extendPort( portBuf, (UPortDevice**) ext );

        //printf( "KR socket_open %d %d\n", portBuf->FD, portBuf->TCP );
        return UR_OK;
    }

fail:

    memFree( ext );
    return UR_THROW;
}


static void socket_close( UBuffer* portBuf )
{
    //printf( "KR socket_close %d\n", portBuf->FD );

    if( portBuf->FD > -1 )
    {
        closesocket( portBuf->FD );
        portBuf->FD = -1;
    }

    memFree( portBuf->ptr.v );
    portBuf->ptr.v = 0;
}


/*
  http://linac.fnal.gov/LINAC/software/locsys/syscode/ipsoftware/IPFragmentation.html

  Maximum datagram size is 64K bytes.
  Ethernet frame size limit is 1500 bytes.
  Limit game packets to 1448 bytes?
*/

#define RECV_BUF_SIZE       2048

static int socket_read( UThread* ut, UBuffer* port, UCell* dest, int part )
{
    UBuffer* buf = 0;
    ssize_t len;
    ssize_t count;
    SOCKET fd = port->FD; 

    //printf( "KR socket_read %d %d\n", fd, port->TCP );

    if( part )
        len = part;
    else
        len = RECV_BUF_SIZE;

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
        if( port->TCP )
        {
            count = recv( fd, buf->ptr.c, len, 0 );
        }
        else
        {
            SocketExt* ext = ur_ptr(SocketExt, port);
            count = recvfrom( fd, buf->ptr.c, len, 0,
                              &ext->addr, &ext->addrlen );
        }
        if( count == -1 )
        {
#ifdef _WIN32
            int err = WSAGetLastError();
            if( (err == WSAEWOULDBLOCK) || (err == WSAEINTR) )
#else
            if( (errno == EAGAIN) || (errno == EINTR) )
#endif
            {
                ur_setId(dest, UT_NONE);
            }
            else
            {
                return ur_error( ut, UR_ERR_ACCESS,
                                 "recvfrom %s", SOCKET_ERR );
            }
        }
        else
        {
            buf->used = count;
        }
    }
    return UR_OK;
}


static int socket_accept( UThread* ut, UBuffer* port, UCell* dest, int part )
{
    SOCKET fd;
    SocketExt* ext;
    UBuffer* buf;
    UIndex bufN;
    (void) part;


    ext = (SocketExt*) memAlloc( sizeof(SocketExt) );
    ext->dev = &port_socket;

    fd = accept( port->FD, &ext->addr, &ext->addrlen );
    if( fd < 0 )
    {
        memFree( ext );
        return ur_error( ut, UR_ERR_INTERNAL, SOCKET_ERR );
    }

    //ur_makePort( ut, dest, );
    ur_genBuffers( ut, 1, &bufN );
    buf = ur_buffer( bufN );

    buf->type  = UT_PORT;
    buf->form  = UR_PORT_EXT;
    buf->TCP   = 1;
    buf->FD    = fd;
    buf->ptr.v = ext;

    ur_setId(dest, UT_PORT);
    ur_setSeries(dest, bufN, 0);
    return UR_OK;
}


extern int boron_sliceMem( UThread* ut, const UCell* cell, const void** ptr );


static int socket_write( UThread* ut, UBuffer* port, const UCell* data )
{
    const void* buf;
    int n;
    ssize_t len = boron_sliceMem( ut, data, &buf );

    if( port->FD > -1 && len )
    {
        if( port->TCP )
        {
            n = send( port->FD, buf, len, 0 );
        }
        else
        {
            SocketExt* ext = ur_ptr(SocketExt, port);
            n = sendto( port->FD, buf, len, 0, &ext->addr, ext->addrlen );
        }

        if( n == -1 )
        {
            ur_error( ut, UR_ERR_ACCESS, "send %d", SOCKET_ERR );

            // An error occured; the socket must not be used again.
            closesocket( port->FD );
            port->FD = -1;
            return UR_THROW;
        }
        else if( n != len )
        {
            return ur_error( ut, UR_ERR_ACCESS,
                             "send only sent %d of %d bytes", n, len );
        }
    }
    return UR_OK;
}


static int socket_seek( UThread* ut, UBuffer* port, UCell* pos, int where )
{
    (void) port;
    (void) pos;
    (void) where;
    return ur_error( ut, UR_ERR_SCRIPT, "Cannot seek on socket port" );
}


static int socket_waitFD( UBuffer* port )
{
    return port->FD;
}


UPortDevice port_socket =
{
    socket_open, socket_close, socket_read, socket_write, socket_seek,
    socket_waitFD
};


UPortDevice port_listenSocket =
{
    socket_open, socket_close, socket_accept, socket_write, socket_seek,
    socket_waitFD
};

/*EOF*/
