/*
  Copyright 2016 Karl Robillard

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


#include <mbedtls/net.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>


typedef struct
{
    SocketExt se;
    mbedtls_net_context nc;
    mbedtls_ssl_context sc;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
}
SSLExt;


static void ssl_init( SSLExt* ext )
{
    mbedtls_net_init( &ext->nc );
    mbedtls_ssl_init( &ext->sc );
    mbedtls_ssl_config_init( &ext->conf );
    mbedtls_ctr_drbg_init( &ext->ctr_drbg );
    mbedtls_entropy_init( &ext->entropy );

    //mbedtls_x509_crt_init( &ext->cacert );
}


static void ssl_free( SSLExt* ext )
{
    mbedtls_net_free( &ext->nc );
    mbedtls_ssl_free( &ext->sc );
    mbedtls_ssl_config_free( &ext->conf );
    mbedtls_ctr_drbg_free( &ext->ctr_drbg );
    mbedtls_entropy_free( &ext->entropy );
}


static void ssl_debug( void *ctx, int level,
                       const char *file, int line, const char *str )
{
    (void) level;
    fprintf( (FILE *) ctx, "%s:%04d: %s", file, line, str );
    fflush( (FILE *) ctx );
}


static int ssl_error( UThread* ut, const char* func, int err )
{
    return ur_error( ut, UR_ERR_INTERNAL, "%s: -0x%d", func, -err );
}


/*
  "tcps://host:port"
  "tcps://:port"
  "udps://host:port"
  "udps://:port"
*/
static int ssl_open( UThread* ut, const UPortDevice* pdev,
                     const UCell* from, int opt, UCell* res )
{
    NodeServ ns;
    SSLExt* ext;
    int ok;
    //int nowait = opt & UR_PORT_NOWAIT;
    mbedtls_net_context* nc;
    mbedtls_ssl_context* sc;
    mbedtls_ssl_config* conf;
    const char* pers = "ssl_client1";
    (void) opt;


    if( ! ur_is(from, UT_STRING) )
        return ur_error( ut, UR_ERR_TYPE, "socket open expected string" );

    ext = (SSLExt*) memAlloc( sizeof(SSLExt) );
    ext->se.addrlen = 0;
#ifdef _WIN32
    ext->se.event = WSA_INVALID_EVENT;
#endif

    //if( ur_is(from, UT_STRING) )
    {
        stringToNodeServ( ut, from, &ns );
        if( ! makeSockAddr( ut, &ext->se, &ns ) )
            goto fail;
    }

    if( ! ns.service )
    {
        ur_error( ut, UR_ERR_SCRIPT,
                  "Socket port requires hostname and/or port" );
        goto fail;
    }

    if( ! boron_requestAccess( ut, "Open socket %s", ns.service ) )
        goto fail;

    ssl_init( ext );
    nc   = &ext->nc;
    sc   = &ext->sc;
    conf = &ext->conf;

    ok = mbedtls_ctr_drbg_seed( &ext->ctr_drbg, mbedtls_entropy_func,
                                &ext->entropy,
                                (const unsigned char*) pers, strlen(pers) );
    if( ok != 0 )
    {
        ssl_error( ut, "mbedtls_ctr_drbg_seed", ok );
        goto failSSL;
    }

    ok = mbedtls_net_connect( nc, ns.node, ns.service,
                              (ns.socktype == SOCK_DGRAM)
                                ? MBEDTLS_NET_PROTO_UDP
                                : MBEDTLS_NET_PROTO_TCP );
    if( ok != 0 )
    {
        ssl_error( ut, "mbedtls_net_connect", ok );
        goto failSSL;
    }

    ok = mbedtls_ssl_config_defaults( conf, MBEDTLS_SSL_IS_CLIENT,
            (ns.socktype == SOCK_DGRAM) ? MBEDTLS_SSL_TRANSPORT_DATAGRAM
                                        : MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT );
    if( ok != 0 )
    {
        ssl_error( ut, "mbedtls_ssl_config_defaults", ok );
        goto failSSL;
    }

    mbedtls_ssl_conf_authmode( conf, MBEDTLS_SSL_VERIFY_NONE );
    //mbedtls_ssl_conf_ca_chain( conf, cacert, NULL );
    mbedtls_ssl_conf_rng( conf, mbedtls_ctr_drbg_random, &ext->ctr_drbg );
    mbedtls_ssl_conf_dbg( conf, ssl_debug, stdout );

    ok = mbedtls_ssl_setup( sc, conf );
    if( ok != 0 )
    {
        ssl_error( ut, "mbedtls_ssl_setup", ok );
        goto failSSL;
    }

    ok = mbedtls_ssl_set_hostname( sc, "Boron TLS Server 1" );
    if( ok != 0 )
    {
        ssl_error( ut, "mbedtls_ssl_set_hostname", ok );
        goto failSSL;
    }

    mbedtls_ssl_set_bio( sc, nc, mbedtls_net_send, mbedtls_net_recv, NULL );

    while( (ok = mbedtls_ssl_handshake( sc )) != 0 )
    {
        if( ok != MBEDTLS_ERR_SSL_WANT_READ &&
            ok != MBEDTLS_ERR_SSL_WANT_WRITE )
        {
            ssl_error( ut, "mbedtls_ssl_handshake", ok );
            goto failSSL;
        }
    }

    {
    UBuffer* pbuf;

    pbuf = boron_makePort( ut, pdev, ext, res );
    pbuf->TCP = (ns.socktype == SOCK_STREAM) ? 1 : 0;
    pbuf->FD  = nc->fd;
    //printf( "KR socket_open %d %d\n", pbuf->FD, pbuf->TCP );
    }
    return UR_OK;

failSSL:

    ssl_free( ext );

fail:

    memFree( ext );
    return UR_THROW;
}


static void ssl_close( UBuffer* pbuf )
{
    if( pbuf->FD > -1 )
    {
        ssl_free( ur_ptr(SSLExt, pbuf) );
        pbuf->FD = -1;
    }

    memFree( pbuf->ptr.v );
    //pbuf->ptr.v = 0;      // Done by port_destroy().
}


static void ssl_wait()
{
    // NOTE: Using the same sleep function as cfunc_sleep().
#ifdef _WIN32
    Sleep( 200 );
#else
    struct timespec ts;
    ts.tv_sec  = 0;
    ts.tv_nsec = 200 * 1000000;
    nanosleep( &ts, NULL );
#endif
}


static int ssl_read( UThread* ut, UBuffer* port, UCell* dest, int len )
{
    SSLExt* ext = ur_ptr(SSLExt, port);
    UBuffer* buf = ur_buffer( dest->series.buf );
    int n;

    if( port->FD > -1 && len )
    {
retry:
        n = mbedtls_ssl_read( &ext->sc, buf->ptr.b + buf->used, len );
        //printf( "KR ssl_read %d\n", n );
        if( n > 0 )
        {
            buf->used += n;
        }
        else if( n < 0 )
        {
            if( n == MBEDTLS_ERR_SSL_WANT_READ ||
                n == MBEDTLS_ERR_SSL_WANT_WRITE )
            {
                ssl_wait();
                goto retry;
            }

            // An error occured; the ssl context must not be used again.
            ssl_free( ext );
            port->FD = -1;

            return ur_error( ut, UR_ERR_ACCESS, "ssl_read %d", n );
        }
        else
        {
            ur_setId(dest, UT_NONE);
        }
    }
    return UR_OK;
}


extern int boron_sliceMem( UThread* ut, const UCell* cell, const void** ptr );


static int ssl_write( UThread* ut, UBuffer* port, const UCell* data )
{
    const void* buf;
    SSLExt* ext = ur_ptr(SSLExt, port);
    int len;
    int n;

    len = boron_sliceMem( ut, data, &buf );

    if( port->FD > -1 && len )
    {
retry:
        n = mbedtls_ssl_write( &ext->sc, buf, len );
        if( n < 0 )
        {
            if( n == MBEDTLS_ERR_SSL_WANT_READ ||
                n == MBEDTLS_ERR_SSL_WANT_WRITE )
            {
                ssl_wait();
                goto retry;
            }

            // An error occured; the ssl context must not be used again.
            ssl_free( ext );
            port->FD = -1;

            return ur_error( ut, UR_ERR_ACCESS, "ssl_write %d", n );
        }
        else if( n != len )
        {
            if( n > 0 && n < len )
            {
                buf = ((const char*) buf) + n;
                len -= n;
                goto retry;
            }
            return ur_error( ut, UR_ERR_ACCESS,
                             "ssl_write only sent %d of %d bytes", n, len );
        }
    }
    return UR_OK;
}


static int ssl_waitFD( UBuffer* port )
{
    return port->FD;
}


UPortDevice port_ssl =
{
    ssl_open, ssl_close, ssl_read, ssl_write, socket_seek,
    ssl_waitFD, 2048
};


/*EOF*/
