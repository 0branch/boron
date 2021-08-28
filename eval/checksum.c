/*
  Copyright 2009,2010 Karl Robillard

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


#define SHA1HANDSOFF    1
#include <sha1.c>


/*
  CRC-16 Checksum - DDCMP and IBM Bisync

  x^16 + x^15 + x^2 + 1
*/

#define CRC16_POLYNOMIAL    0xa001
#define CRC16_INITIAL       0

uint16_t checksum_crc16( const uint8_t* data, int byteCount )
{
    uint16_t crc, bit;
    int i;

    crc = CRC16_INITIAL;
    while( byteCount-- > 0 )
    {
        for( i = 1; i <= 0x80; i <<= 1 )
        {
            bit = ((*data) & i) ? 0x8000 : 0;
            if( crc & 1 )
                bit ^= 0x8000;
            crc >>= 1;
            if( bit )
                crc ^= CRC16_POLYNOMIAL;
        }
        ++data;
    }
    return crc;
}


/*
  crc32_next and checksum_crc32 generated with universal_crc by Danjel McGougan
  and tweaked by hand.  See http://mcgougan.se/universal_crc/

  universal_crc -b 32 -p 0x04c11db7 -i 0xffffffff
  CRC of the string "123456789" is 0x0376e6e7
*/
static inline uint32_t crc32_next( uint32_t crc, uint8_t data )
{
    uint32_t eor;
    unsigned int i = 8;

    crc ^= (uint32_t)data << 24;
    do {
        /* This might produce branchless code */
        eor = crc & 0x80000000 ? 0x4c11db7 : 0;
        crc <<= 1;
        crc ^= eor;
    } while (--i);

    return crc;
}


uint32_t checksum_crc32( const uint8_t* data, int byteCount )
{
    uint32_t crc = 0xffffffff;
    if( byteCount ) do {
        crc = crc32_next( crc, *data++ );
    } while( --byteCount );
    return crc;
}


#define CHECKSUM_BUF_SIZE    4*1024


static void file_crc32( FILE* fp, UBuffer* buf, UCell* res )
{
    uint8_t* data;
    size_t n;
    uint32_t crc = 0xffffffff;

    while( 1 )
    {
        n = fread( buf->ptr.b, 1, CHECKSUM_BUF_SIZE, fp );
        if( n < 1 )
            break;
        data = buf->ptr.b;
        do {
            crc = crc32_next( crc, *data++ );
        } while( --n );
    }

    ur_setId(res, UT_INT);
    ur_setFlags(res, UR_FLAG_INT_HEX);
    ur_int(res) = crc;
}


static void file_sha1( UThread* ut, FILE* fp, UBuffer* buf, UCell* res )
{
    SHA1_CTX context;
    UBuffer* sum;
    size_t n;

    sum = ur_makeBinaryCell( ut, 20, res );
    sum->used = 20;

    SHA1_Init( &context );
    while( 1 )
    {
        n = fread( buf->ptr.b, 1, CHECKSUM_BUF_SIZE, fp );
        if( n < 1 )
            break;
        SHA1_Update( &context, buf->ptr.b, n );
    }
    SHA1_Final( &context, sum->ptr.b );
}


/*-cf-
    checksum
        data    binary!/string!/file!
        /sha1
        /crc16  IBM Bisync, USB
        /crc32  IEEE 802.3, MPEG-2
    return: int!/binary!
    group: data

    Computes sha1 checksum by default.
*/
CFUNC(cfunc_checksum)
{
#define OPT_CHECKSUM_SHA1   1
#define OPT_CHECKSUM_CRC16  2
#define OPT_CHECKSUM_CRC32  4
    int type = ur_type(a1);

    if( (type == UT_BINARY) || (type == UT_STRING) )
    {
        USeriesIter si;
        uint8_t* it;
        uint8_t* end;

        ur_seriesSlice( ut, &si, a1 );

        if( (type == UT_STRING) && ur_strIsUcs2(si.buf) )
            return ur_error( ut, UR_ERR_TYPE,
                             "checksum does not handle ucs2 strings" );

        it  = si.buf->ptr.b + si.it;
        end = si.buf->ptr.b + si.end;

        type = CFUNC_OPTIONS;
        if( type > OPT_CHECKSUM_SHA1 )
        {
            if( type & OPT_CHECKSUM_CRC32 )
                ur_int(res) = checksum_crc32( it, end - it );
            else
                ur_int(res) = checksum_crc16( it, end - it );
            ur_setId(res, UT_INT);
            ur_setFlags(res, UR_FLAG_INT_HEX);
        }
        else
        {
            SHA1_CTX context;
            UBuffer* bin;

            bin = ur_makeBinaryCell( ut, 20, res );
            bin->used = 20;

            SHA1_Init( &context );
            SHA1_Update( &context, it, end - it );
            SHA1_Final( &context, bin->ptr.b );
        }
        return UR_OK;
    }
    else if( type == UT_FILE )
    {
        const char* filename;
        FILE* fp;
        UBuffer* tmp;

        filename = boron_cpath( ut, a1, 0 );
        fp = fopen( filename, "rb" );
        if( ! fp )
        {
            return ur_error( ut, UR_ERR_ACCESS,
                             "could not open file %s", filename );
        }

        tmp = &BT->tbin;        // Same buffer as filename.
        ur_binReserve( tmp, CHECKSUM_BUF_SIZE );

        if( CFUNC_OPTIONS & OPT_CHECKSUM_CRC32 )
            file_crc32( fp, tmp, res );
        else
            file_sha1( ut, fp, tmp, res );

        fclose( fp );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "checksum expected binary!/string!/file!" );
}


//EOF
