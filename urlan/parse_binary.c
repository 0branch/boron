/*
  Copyright 2009 Karl Robillard

  This file is part of the Urlan datatype system.

  Urlan is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Urlan is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Urlan.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "os.h"
#include "urlan.h"
#include "urlan_atoms.h"
#include "mem_util.h"


//#define bitIsSet(array,n)    (array[(n)>>3] & 1<<((n)&7))


#define PARSE_ERR   ut, UR_ERR_SCRIPT

enum BinaryParseException
{
    PARSE_EX_NONE,
    PARSE_EX_ERROR,
    PARSE_EX_BREAK
};


#define PIPE_BITS   32

typedef struct
{
    uint32_t pipe;
    uint32_t bitsFree;
    //uint32_t byteCount;
}
BitPipe;


typedef struct
{
    int (*eval)( UThread*, const UCell* );
    UIndex   inputBufN;
    UIndex   inputEnd;
    uint8_t  sliced;
    uint8_t  exception;
    uint8_t  bigEndian;
    BitPipe  bp;
}
BinaryParser;


/*
  bitCount must be 1 to 24.
  Returns zero if end of input reached.
*/
static uint8_t* pullBits( BinaryParser* pe, uint32_t bitCount,
                          uint8_t* in, uint8_t* inEnd, uint32_t* field )
{
    while( (PIPE_BITS - pe->bp.bitsFree) < bitCount )
    {
        if( in == inEnd )
            return 0;
        pe->bp.bitsFree -= 8;
        pe->bp.pipe |= ((uint32_t) *in++) << pe->bp.bitsFree;
        //++pe->bp.byteCount;
    }
    *field = pe->bp.pipe >> (PIPE_BITS - bitCount);
    pe->bp.pipe <<= bitCount;
    pe->bp.bitsFree += bitCount;
    return in;
}


#define CHECK_WORD(cell) \
    if( ! cell ) \
        goto parse_err;

/*
  Returns zero if matching rule not found or exception occured.
*/
static const UCell* _parseBin( UThread* ut, BinaryParser* pe,
                               const UCell* rit, const UCell* rend,
                               UIndex* spos )
{
    const UCell* set = 0;
    const UCell* tval;
    uint32_t bitCount;
    uint32_t field;
    UBuffer* ibin  = ur_buffer( pe->inputBufN );
    uint8_t* in    = ibin->ptr.b + *spos;
    uint8_t* inEnd = ibin->ptr.b + pe->inputEnd;


match:

    while( rit != rend )
    {
        switch( ur_type(rit) )
        {
            case UT_INT:
                bitCount = ur_int(rit);
                if( bitCount < 1 || bitCount > 32 )
                {
                    ur_error( PARSE_ERR, "bit-field size must be 1 to 32" );
                    goto parse_err;
                }
                if( bitCount > 24 )
                {
                    uint32_t high;
                    in = pullBits( pe, bitCount - 16, in, inEnd, &high );
                    if( ! in )
                        goto failed;
                    in = pullBits( pe, 16, in, inEnd, &field );
                    if( ! in )
                        goto failed;
                    field |= high << 16;
                }
                else
                {
                    in = pullBits( pe, bitCount, in, inEnd, &field );
                    if( ! in )
                        goto failed;
                }
                goto set_field;

            case UT_WORD:
                switch( ur_atom(rit) )
                {
                case UR_ATOM_U8:
                    if( in == inEnd )
                        goto failed;
                    field = *in++;
                    goto set_field;

                case UR_ATOM_U16:
                    if( (inEnd - in) < 2 )
                        goto failed;
                    if( pe->bigEndian )
                        field = (in[0] << 8) | in[1];
                    else
                        field = (in[1] << 8) | in[0];
                    in += 2;
                    goto set_field;

                case UR_ATOM_U32:
                    if( (inEnd - in) < 4 )
                        goto failed;
                    if( pe->bigEndian )
                        field = (in[0] << 24) | (in[1] << 16) |
                                (in[2] <<  8) |  in[3];
                    else
                        field = (in[3] << 24) | (in[2] << 16) |
                                (in[1] <<  8) |  in[0];
                    in += 4;
                    goto set_field;

                case UR_ATOM_SKIP:
                    ++rit;
                    ++in;
                    break;
#if 0
                case UR_ATOM_MARK:
                    break;

                case UR_ATOM_PLACE:
                    ++rit;
                    if( (rit != rend) && ur_is(rit, UT_WORD) )
                    {
                        tval = ur_wordCell( ut, rit++ );
                        CHECK_WORD(tval);
                        if( ur_is(tval, UT_BINARY) )
                        {
                            pos = tval->series.it;
                            break;
                        }
                    }
                    ur_error( PARSE_ERR, "place expected series word" );
                    goto parse_err;
#endif
                case UR_ATOM_COPY:      // copy  dest   size
                                        //       word!  int!/word!
                    ++rit;
                    if( (rit != rend) && ur_is(rit, UT_WORD) )
                    {
                        UCell* res = ur_wordCellM( ut, rit );
                        CHECK_WORD(res);
                        if( ++rit != rend )
                        {
                            tval = rit++;
                            if( ur_is(tval, UT_WORD) )
                            {
                                tval = ur_wordCell( ut, tval );
                                CHECK_WORD(tval);
                            }
                            if( ur_is(tval, UT_INT) )
                            {
                                UBuffer* cb;
                                int size = ur_int(tval);
                                cb = ur_makeBinaryCell( ut, size, res );
                                cb->used = size;
                                memCpy( cb->ptr.b, in, size );
                                in += size;
                                break;
                            }
                        }
                        ur_error( PARSE_ERR, "copy expected int! count" );
                        goto parse_err;
                    }
                    ur_error( PARSE_ERR, "copy expected word! destination" );
                    goto parse_err;

                case UR_ATOM_BIG_ENDIAN:
                    ++rit;
                    pe->bigEndian = 1;
                    break;

                case UR_ATOM_LITTLE_ENDIAN:
                    ++rit;
                    pe->bigEndian = 0;
                    break;

                default:
                    tval = ur_wordCell( ut, rit );
                    CHECK_WORD(tval);

                    if( ur_is(tval, UT_CHAR) )
                        goto match_char;
                    else if( ur_is(tval, UT_STRING) )
                        goto match_string;
                    else if( ur_is(tval, UT_BLOCK) )
                        goto match_block;
                    /*
                    else if( ur_is(tval, UT_BITSET) )
                        goto match_bitset;
                    */
                    else
                    {
                        ur_error( PARSE_ERR,
                                "parse expected char!/string!/block!" );
                        goto parse_err;
                    }
                    break;
                }
                break;

            case UT_SETWORD:
                set = rit++;
                while( (rit != rend) && ur_is(rit, UT_SETWORD) )
                    ++rit;
                break;
#if 0
            case UT_GETWORD:
                break;

            case UT_INT:
                repMin = ur_int(rit);

                ++rit;
                if( rit == rend )
                    return 0;

                if( ur_is(rit, UT_INT) )
                {
                    repMax = ur_int(rit);
                    ++rit;
                }
                else
                {
                    repMax = repMin;
                }
                goto repeat;
#endif
            case UT_CHAR:
match_char:
                if( *in != ur_int(rit) )
                    goto failed;
                ++in;
                ++rit;
                break;

            case UT_BLOCK:
                tval = rit;
match_block:
            {
                UBlockIter bi;
                UIndex pos = in - ibin->ptr.b;
                UIndex rblkN = tval->series.buf;
                ur_blkSlice( ut, &bi, tval );
                tval = _parseBin( ut, pe, bi.it, bi.end, &pos );
                ibin = ur_buffer( pe->inputBufN );
                if( ! tval )
                {
                    if( pe->exception == PARSE_EX_ERROR )
                    {
                        ur_appendTrace( ut, rblkN, 0 );
                        return 0;
                    }
                    if( pe->exception == PARSE_EX_BREAK )
                        pe->exception = PARSE_EX_NONE;
                    else
                        goto failed;
                }
                in    = ibin->ptr.b + pos;
                inEnd = ibin->ptr.b + pe->inputEnd;
                ++rit;
            }
                break;

            case UT_PAREN:
            {
                UIndex pos = in - ibin->ptr.b;

                if( UR_OK != pe->eval( ut, rit ) )
                    goto parse_err;

                /* Re-acquire pointer & check if input modified. */
                ibin = ur_buffer( pe->inputBufN );
                if( pe->sliced )
                {
                    // We have no way to track changes to the end of a slice,
                    // so just make sure we remain in valid memery.
                    if( ibin->used < pe->inputEnd )
                        pe->inputEnd = ibin->used;
                }
                else
                {
                    // Not sliced, track input end.
                    if( ibin->used != pe->inputEnd )
                        pe->inputEnd = ibin->used;
                }
                in    = ibin->ptr.b + pos;
                inEnd = ibin->ptr.b + pe->inputEnd;
                ++rit;
            }
                break;

            case UT_STRING:
                tval = rit;
match_string:
            {
                UBinaryIter bi;
                int size;

                ur_binSlice( ut, &bi, tval );
                if( ur_strIsUcs2(bi.buf) )
                    goto bad_enc;
                size = bi.end - bi.it;
                if( size > (inEnd - in) )
                    goto failed;
                if( match_pattern_uint8_t(in, inEnd, bi.it, bi.end) == bi.end )
                {
                    in += size;
                    ++rit;
                }
                else
                    goto failed;
            }
                break;
#if 0
            case UT_BITSET:
                tval = rit;
match_bitset:
            if( pos >= pe->inputEnd )
                goto failed;
            {
                const UBuffer* bin = ur_bufferSer( tval );
                int c = istr->ptr.c[ pos ];
                if( bitIsSet( bin->ptr.b, c ) )
                {
                    ++rit;
                    ++pos;
                }
                else
                    goto failed;
            }
                break;
#endif
            default:
                ur_error( PARSE_ERR, "invalid parse value" );
                             //orDatatypeName( ur_type(rit) ) );
                goto parse_err;
        }
    }

//complete:

    *spos = in - ibin->ptr.b;
    return rit;

set_field:

    if( set )
    {
        UCell* val;
        while( set != rit )
        {
            val = ur_wordCellM( ut, set++ );
            CHECK_WORD(val);
            ur_setId(val, UT_INT);
            ur_int(val) = field;
        }
        set = 0;
    }
    ++rit;
    goto match;

failed:

    *spos = in - ibin->ptr.b;
    return 0;

bad_enc:

    ur_error( ut, UR_ERR_INTERNAL,
              "parse binary does not handle UCS2 strings" );
    //goto parse_err;

parse_err:

    pe->exception = PARSE_EX_ERROR;
    return 0;
}


/**
  \ingroup urlan_dsl

  Parse a binary data format.

  \param bin        Input to parse, which must be in thread storage.
  \param start      Starting byte in input.
  \param end        Ending byte in input.
  \param parsePos   Index in input where parse ended.
  \param ruleBlk    Rules in the parse language.  This block must be held
                    and remain unchanged during the parsing.
  \param eval       Evaluator callback to do paren values in rule.
                    The callback must return UR_OK/UR_THROW.

  \return UR_OK or UR_THROW.
*/
int ur_parseBinary( UThread* ut, UBuffer* bin, UIndex start, UIndex end,
                    UIndex* parsePos, const UBuffer* ruleBlk,
                    int (*eval)( UThread*, const UCell* ) )
{
    BinaryParser p;

    p.eval = eval;
    p.inputBufN = bin - ut->dataStore.ptr.buf;  // TODO: Don't access dataStore
    p.inputEnd  = end;
    p.sliced    = (end != bin->used);
    p.exception = PARSE_EX_NONE;
    p.bigEndian = 0;
    p.bp.pipe = 0;
    p.bp.bitsFree = PIPE_BITS;
    //p.bp.byteCount = 0;

    *parsePos = start;
    _parseBin( ut, &p, ruleBlk->ptr.cell,
                       ruleBlk->ptr.cell + ruleBlk->used, parsePos );
    return (p.exception == PARSE_EX_ERROR) ? UR_THROW : UR_OK;
}


/*EOF*/
