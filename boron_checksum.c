/*
  Copyright 2009 Karl Robillard

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
#include <support/sha1.c>


/*
  CRC-16 Checksum - DDCMP and IBM Bisync

  x^16 + x^15 + x^2 + 1
*/

#define CRC16_POLYNOMIAL    0xa001
#define CRC16_INITIAL       0

uint16_t checksum_crc16( uint8_t* data, int byteCount )
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


/*-cf-
    checksum
        data    binary!/string!
        /crc16
        /sha1
    return: int!/binary!

    Computes sha1 checksum by default.
*/
CFUNC(cfunc_checksum)
{
#define OPT_CHECKSUM_CRC16  1
#define OPT_CHECKSUM_SHA1   2
    USeriesIter si;
    uint8_t* it;
    uint8_t* end;
    int type = ur_type(a1);

    if( (type == UT_BINARY) || ur_isStringType( type ) )
    {
        ur_seriesSlice( ut, &si, a1 );

        it  = si.buf->ptr.b + si.it;
        end = si.buf->ptr.b + si.end;

        if( CFUNC_OPTIONS & OPT_CHECKSUM_CRC16 )
        {
            ur_setId(res, UT_INT);
            ur_int(res) = checksum_crc16( it, end - it );
            return UR_OK;
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
            return UR_OK;
        }
    }
    return ur_error( ut, UR_ERR_TYPE, "checksum expected binary!/string!" );
}


//EOF
