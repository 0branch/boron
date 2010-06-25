/*
  Copyright 2009-2010 Karl Robillard

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


#if CONFIG_COMPRESS == 2
#include <bzlib.h>
#else
#include <zlib.h>
#endif


/*-cf-
    compress
        data    string!/binary!
    return: binary!
*/
CFUNC(cfunc_compress)
{
#define BZ_LEN(l)   l + (l / 99) + 600
    if( ur_is(a1, UT_STRING) || ur_is(a1, UT_BINARY) )
    {
        USeriesIter si;
        UBuffer* bin;
        char* inp;      // Would be const except for bz2 interface.
        unsigned int len;
#ifdef ZLIB_H
        uLongf blen;
#else
        unsigned int blen;
#endif

        ur_seriesSlice( ut, &si, a1 );
        if( ur_is(a1, UT_STRING) && ur_strIsUcs2(si.buf) )
        {
            inp = (char*) (si.buf->ptr.u16 + si.it);
            len = (si.end - si.it) * 2;
        }
        else
        {
            inp = si.buf->ptr.c + si.it;
            len = si.end - si.it;
        }

        blen = len + (len / 99) + 600;
        bin = ur_makeBinaryCell( ut, blen + 8, res );   // "latin-1\n"

        if( ur_is(a1, UT_STRING) )
        {
            switch( si.buf->form )
            {
                case UR_ENC_LATIN1:
                    memCpy( bin->ptr.c, "latin-1\n", bin->used = 8 );
                    break;
                case UR_ENC_UTF8:
                    memCpy( bin->ptr.c, "utf-8\n", bin->used = 6 );
                    break;
                case UR_ENC_UCS2:
                {
                    uint16_t n = 1;
                    memCpy( bin->ptr.c,
                            (((char*)&n)[0] == 1) ? "ucs-2le\n" : "ucs-2be\n",
                            bin->used = 8 );
                }
                    break;
            }
        }

        if( len > 0 )
        {
            int ret;
#ifdef ZLIB_H
            ret = compress( bin->ptr.b + bin->used, &blen, (Bytef*) inp, len );
            if( ret == Z_BUF_ERROR )
#else
            ret = BZ2_bzBuffToBuffCompress( bin->ptr.c + bin->used, &blen,
                                            inp, len, 3, 0, 0 );
            if( ret == BZ_OUTBUFF_FULL )
#endif
                return ur_error( ut, UR_ERR_ACCESS, "compress buffer full" );
            bin->used += blen;
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "compress expected string!/binary!" );
}


/*
   Decompress data into binary buffer.

   \param data      Compressed data.
   \param len       Byte length of compressed data.
   \param binOut    Binary buffer for output.

   \return  Non-zero if successful.
*/
int ur_decompress( const void* data, int len, UBuffer* binOut )
{
#ifdef ZLIB_H
#define BUF_LOW     32
    z_stream strm;
    int ok;

    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in  = (uint8_t*) data;
    strm.avail_in = len;

    ok = inflateInit( &strm );
    if( ok != Z_OK )
    {
        //fprintf( stderr, "zlib inflateInit failure (%d)", ok );
        return 0;
    }

    ur_binReserve( binOut, (len < BUF_LOW) ? BUF_LOW : len );
    strm.next_out  = binOut->ptr.b;
    strm.avail_out = ur_avail(binOut);

    do
    {
        if( strm.avail_out < BUF_LOW )
        {
            ur_binReserve( binOut, ur_avail(binOut) + (2 * BUF_LOW) );
            strm.next_out  = binOut->ptr.b + strm.total_out;
            strm.avail_out = ur_avail(binOut) - strm.total_out;
        }

        ok = inflate( &strm, Z_NO_FLUSH );
        binOut->used = strm.total_out;
    }
    while( ok == Z_OK );

    inflateEnd( &strm );

    return (ok == Z_STREAM_END) ? 1 : 0;
#else
#define BUF_LOW     32
    bz_stream strm;
    int ok;

    strm.bzalloc = 0;
    strm.bzfree  = 0;
    strm.opaque  = 0;

    ok = BZ2_bzDecompressInit( &strm, 0, 0 );
    if( ok != BZ_OK )
    {
        //fprintf( stderr, "BZ2_bzDecompressInit failure (%d)", ok );
        return 0;
    }

    strm.next_in  = (char*) data;
    strm.avail_in = len;

    ur_binReserve( binOut, (len < BUF_LOW) ? BUF_LOW : len );
    strm.next_out  = binOut->ptr.c;
    strm.avail_out = ur_avail(binOut);

    do
    {
        if( strm.avail_out < BUF_LOW )
        {
            ur_binReserve( binOut, ur_avail(binOut) + (2 * BUF_LOW) );
            strm.next_out  = binOut->ptr.c + strm.total_out_lo32;
            strm.avail_out = ur_avail(binOut) - strm.total_out_lo32;
        }

        ok = BZ2_bzDecompress( &strm );
        binOut->used = strm.total_out_lo32;
    }
    while( ok == BZ_OK );

    BZ2_bzDecompressEnd( &strm );

    return (ok == BZ_STREAM_END) ? 1 : 0;
#endif
}


/*
  Returns 'it' at end of pat, or zero if string does not match pat.
*/
static const uint8_t* str_match( const uint8_t* it, const uint8_t* end,
                                 const char* pat )
{
    int ch;
    while( it != end )
    {
        ch = *pat++;
        if( ch == '\0' )
            return it;
        if( ch != *it++ )
            break;
    }
    return 0;
}


/*-cf-
    decompress
        data    binary!
    return: string!/binary!
*/
CFUNC(cfunc_decompress)
{
    UBinaryIter bi;
    UBuffer* out;
    const uint8_t* start;

    if( ! ur_is(a1, UT_BINARY) )
        return ur_error( ut, UR_ERR_TYPE, "decompress expected binary!" );
    ur_binSlice( ut, &bi, a1 );

    out = ur_makeBinaryCell( ut, 0, res );

    if( (start = str_match( bi.it, bi.end, "latin-1\n" )) ||
        (start = str_match( bi.it, bi.end, "utf-8\n" )) )
    {
        ur_type(res) = UT_STRING;
        ur_binToStr( out, (*start == 'l') ? UR_ENC_LATIN1 : UR_ENC_UTF8 );
    }
    else if( (start = str_match( bi.it, bi.end, "ucs-2le\n" )) ||
             (start = str_match( bi.it, bi.end, "ucs-2be\n" )) )
    {
        return ur_error( ut, UR_ERR_INTERNAL,
                         "FIXME: decompress ucs-2 not implemented" );
    }
    else
    {
        start = bi.it;
    }

    if( ! ur_decompress( start, bi.end - start, out ) )
        return ur_error( ut, UR_ERR_INTERNAL, "decompress failed" );

    return UR_OK;
}


//EOF
