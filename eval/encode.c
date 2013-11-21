/*
  Copyright 2013 Karl Robillard

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


#define URLENC_COMMON       1
#define URLENC_FUNC_ENCODE  urlenc_enc_u16
#define URLENC_FUNC_DECODE  urlenc_dec_u16
#define URLENC_T            uint16_t
#include "url_encoding.c"

#define URLENC_FUNC_ENCODE  urlenc_enc_u8
#define URLENC_FUNC_DECODE  urlenc_dec_u8
#define URLENC_T            uint8_t
#include "url_encoding.c"


static int url_encode( UThread* ut, const UCell* strC, UCell* res, int decode )
{
    USeriesIter si;
    UBuffer* nstr;
    int enc;
    int nlen;


    ur_seriesSlice( ut, &si, strC );
    enc = si.buf->form;

    nlen = si.end - si.it;
    nstr = ur_makeStringCell( ut, enc, decode ? nlen : nlen * 3, res );
    if( enc == UR_ENC_LATIN1 )
    {
        uint8_t* cp = si.buf->ptr.b;
        uint8_t* (*func)(const uint8_t*, const uint8_t*, uint8_t*) =
            decode ? urlenc_dec_u8 : urlenc_enc_u8;

        cp = func( cp + si.it, cp + si.end, nstr->ptr.b );
        nstr->used = cp - nstr->ptr.b;
    }
    else if( enc == UR_ENC_UCS2 )
    {
        uint16_t* cp = si.buf->ptr.u16;
        uint16_t* (*func)(const uint16_t*, const uint16_t*, uint16_t*) =
            decode ? urlenc_dec_u16 : urlenc_enc_u16;

        cp = func( cp + si.it, cp + si.end, nstr->ptr.u16 );
        nstr->used = cp - nstr->ptr.u16;
    }
    else
    {
        return ur_error( ut, UR_ERR_INTERNAL,
                         "encode 'url requires latin1 or ucs2 string" );
    }
    return UR_OK;
}


/*-cf-
    encode
        type    int!/word!   2, 16, 64, latin1, utf8, ucs2, url
        data    binary!/string!
        /bom    Prepend Unicode BOM for utf8 or ucs2 and return binary.
    return: String or binary with data converted to encoding type.
    group: data
    see: decode, encoding?

    When data is a string! then the type must be a word! and a new string
    is returned.

    If data is a binary! then the type must be an int! and the input value
    is returned with only the base indicator modified.
*/
CFUNC(cfunc_encode)
{
#define OPT_ENCODE_BOM  0x01
    static const uint8_t _bomUtf8[3] = { 0xef, 0xbb, 0xbf };
    const UCell* data = a2;
    int type = ur_type(data);

    if( ur_isStringType( type ) )
    {
        USeriesIter si;
        int enc;

        if( ! ur_is(a1, UT_WORD) )
            return errorType( "encode expected word! type" );

        switch( ur_atom(a1) )
        {
            case UR_ATOM_LATIN1:
                enc = UR_ENC_LATIN1;
                break;
            case UR_ATOM_UTF8:
                enc = UR_ENC_UTF8;
                break;
            case UR_ATOM_UCS2:
                enc = UR_ENC_UCS2;
                break;
            case UR_ATOM_URL:
                return url_encode( ut, data, res, 0 );
            default:
                return ur_error( ut, UR_ERR_TYPE,
                                 "encode passed invalid type '%s",
                                 ur_wordCStr( a1 ) );
        }

        if( CFUNC_OPTIONS & OPT_ENCODE_BOM )
        {
            UBuffer* bin = ur_makeBinaryCell( ut, 0, res );

            if( enc == UR_ENC_UTF8 )
            {
                ur_binAppendData( bin, _bomUtf8, 3 );
            }
            else if( enc == UR_ENC_UCS2 )
            {
                uint16_t bom = 0xfeff;
                ur_binAppendData( bin, (uint8_t*) &bom, 2 );
            }

            ur_seriesSlice( ut, &si, data );

            if( enc == si.buf->form )
            {
                ur_binAppendArray( bin, &si );
            }
            else
            {
                UBuffer tmp;
                ur_strInit( &tmp, enc, 0 );
                ur_strAppend( &tmp, si.buf, si.it, si.end );

                si.buf = &tmp;
                si.it  = 0;
                si.end = tmp.used;
                ur_binAppendArray( bin, &si );

                ur_strFree( &tmp );
            }
        }
        else
        {
            UBuffer* nstr = ur_makeStringCell( ut, enc, 0, res );
            ur_seriesSlice( ut, &si, data );
            ur_strAppend( nstr, si.buf, si.it, si.end );
        }
        return UR_OK;
    }
    else if( type == UT_BINARY )
    {
        UBuffer* bin;

        if( ! ur_is(a1, UT_INT) )
        {
bad_type:
            return errorType( "encode expected type 2, 16, or 64 for binary" );
        }

        if( ! (bin = ur_bufferSerM(data)) )
            return UR_THROW;
        switch( ur_int(a1) )
        {
            case 2:
                bin->form = UR_BENC_2;
                break;
            case 16:
                bin->form = UR_BENC_16;
                break;
            case 64:
                bin->form = UR_BENC_64;
                break;
            default:
                goto bad_type;
        }
        *res = *data;
        return UR_OK;
    }
    return errorType( "encode expected binary!/string! data" );
}


/*-cf-
    decode
        type    word!   url
        data    string!
    return: New string with data converted to encoding type.
    group: data
    see: encode, encoding?

    Undoes URL encoding.
*/
CFUNC(cfunc_decode)
{
    if( ur_atom(a1) == UR_ATOM_URL )
        return url_encode( ut, a2, res, 1 );
    return ur_error( ut, UR_ERR_SCRIPT, "decode expected 'url" );
}


/*-cf-
    encoding?
        data 
    return: Encoding type or none! if data is not a string!/binary!.
    group: data
    see: encode, decode

    A string! data value will return a word! (latin1, utf8, or ucs2).
    A binary! data value will return the base int! (2, 16, or 64).
*/
CFUNC(cfunc_encodingQ)
{
    static UAtom encAtoms[4] = {
        UR_ATOM_LATIN1, UR_ATOM_UTF8, UR_ATOM_UCS2, UT_UNSET
    };
    static char bencBase[4] = { 16, 2, 64, 16 };

    if( ur_isStringType( ur_type(a1) ) )
    {
        const UBuffer* buf = ur_bufferSer(a1);
        ur_setId(res, UT_WORD);
        ur_setWordUnbound(res, encAtoms[buf->form & 3] );
    }
    else if( ur_is(a1, UT_BINARY) )
    {
        const UBuffer* buf = ur_bufferSer(a1);
        ur_setId(res, UT_INT);
        ur_int(res) = bencBase[buf->form & 3];
    }
    else
        ur_setId(res, UT_NONE);
    return UR_OK;
}


/*EOF*/
