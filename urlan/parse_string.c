/*
  Copyright 2005-2016 Karl Robillard

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


#include "urlan.h"
#include "urlan_atoms.h"
#include "mem_util.h"


#define REPEAT_ANY  0x7fffffff
#define bitIsSet(array,n)    (array[(n)>>3] & 1<<((n)&7))


#define PARSE_ERR   ut, UR_ERR_SCRIPT

enum StringParseException
{
    PARSE_EX_NONE,
    PARSE_EX_ERROR,
    PARSE_EX_BREAK
};


#define PIPE_BITS   64

typedef struct
{
    uint64_t pipe;
    uint32_t bitsFree;
}
BitPipe;


typedef struct
{
    UStatus (*eval)( UThread*, const UCell* );
    UBuffer* str;
    UIndex   inputBuf;
    UIndex   inputEnd;
    uint8_t  sliced;
    uint8_t  exception;
    uint8_t  matchCase;
    uint8_t  ucs2;
    uint8_t  bigEndian;
}
StringParser;


/*
  Return number of characters advanced.
*/
#define REPEAT_CHAR(T) \
static int _repeatChar_ ## T( const T* start, UIndex pos, UIndex inputEnd, \
        int limit, int c ) { \
    const T* it  = start + pos; \
    const T* end = start + inputEnd; \
    if( (limit != REPEAT_ANY) && (end > (it + limit)) ) \
        end = it + limit; \
    start = it; \
    while( it != end ) { \
        if( *it != c ) \
            break; \
        ++it; \
    } \
    return it - start; \
}

REPEAT_CHAR(uint8_t)
REPEAT_CHAR(uint16_t)


/*
  Return number of characters advanced.
*/
#define REPEAT_BITSET(T) \
static int _repeatBitset_ ## T( const UThread* ut, \
        const T* start, UIndex pos, UIndex inputEnd, \
        int limit, const UCell* binc ) { \
    const T* it  = start + pos; \
    const T* end = start + inputEnd; \
    int c; \
    const UBuffer* bin = ur_bufferSer(binc); \
    const uint8_t* bits = bin->ptr.b; \
    int maxC = bin->used * 8; \
    if( end > (it + limit) ) \
        end = it + limit; \
    start = it; \
    while( it != end ) { \
        c = *it; \
        if( c >= maxC ) \
            break; \
        if( ! bitIsSet( bits, c ) ) \
            break; \
        ++it; \
    } \
    return it - start; \
}

REPEAT_BITSET(uint8_t)
REPEAT_BITSET(uint16_t)


/*
  Return new pos or -1 if not found.
*/
#define SCAN_BITSET(T) \
static int _scanToBitset_ ## T( const UThread* ut, \
        const T* start, UIndex pos, UIndex inputEnd, \
        const UCell* binc ) { \
    const T* it  = start + pos; \
    const T* end = start + inputEnd; \
    int c; \
    const UBuffer* bin = ur_bufferSer(binc); \
    const uint8_t* bits = bin->ptr.b; \
    int maxC = bin->used * 8; \
    while( it != end ) { \
        c = *it; \
        if( c < maxC ) { \
            if( bitIsSet( bits, c ) ) \
                return it - start; \
        } \
        ++it; \
    } \
    return -1; \
}

SCAN_BITSET(uint8_t)
SCAN_BITSET(uint16_t)


/*
  bitCount must be 1 to PIPE_BITS-8.
  Returns zero if end of input reached.
*/
static const uint8_t* _pullBits( BitPipe* bp, uint32_t bitCount,
                                 const uint8_t* in, const uint8_t* inEnd,
                                 uint64_t* field )
{
    while( (PIPE_BITS - bp->bitsFree) < bitCount )
    {
        if( in == inEnd )
            return 0;
        bp->bitsFree -= 8;
        bp->pipe |= ((uint64_t) *in++) << bp->bitsFree;
        //++bp->byteCount;
    }
    *field = bp->pipe >> (PIPE_BITS - bitCount);
    bp->pipe <<= bitCount;
    bp->bitsFree += bitCount;
    return in;
}


static uint64_t _uintValue( const uint8_t* in, int bits, int bigEndian )
{
    uint64_t n = 0;
    if( bigEndian )
    {
        do
        {
            bits -= 8;
            n |= ((uint64_t) (*in++)) << bits;
        }
        while( bits );
    }
    else
    {
        int shift = 0;
        do
        {
            n |= ((uint64_t) (*in++)) << shift;
            shift += 8;
        }
        while( shift < bits );
    }
    return n;
}


static UIndex _parseBits( UThread* ut, StringParser* pe, UIndex pos,
                          const UCell* specC )
{
    UBlockIt bi;
    BitPipe bp;
    uint64_t field;
    uint32_t count;
    const UCell* setWord = 0;
    const uint8_t* in = pe->str->ptr.b;
    const uint8_t* inEnd = in + pe->inputEnd;

    in += pos;

    bp.pipe = 0;
    bp.bitsFree = PIPE_BITS;

    ur_blockIt( ut, &bi, specC );

match:

    ur_foreach( bi )
    {
        switch( ur_type(bi.it) )
        {
            case UT_INT:
                count = ur_int(bi.it);      // Bit count.
                if( count < 1 || count > PIPE_BITS )
                {
                    ur_error( PARSE_ERR, "bit-field size must be 1 to 64" );
                    goto failed;
                }
                if( count > (PIPE_BITS - 8) )
                {
                    const uint32_t halfPipe = PIPE_BITS / 2;
                    uint64_t high;
                    in = _pullBits( &bp, count - halfPipe, in, inEnd, &high );
                    if( ! in )
                        goto failed;
                    in = _pullBits( &bp, halfPipe, in, inEnd, &field );
                    if( ! in )
                        goto failed;
                    field |= high << halfPipe;
                }
                else
                {
                    in = _pullBits( &bp, count, in, inEnd, &field );
                    if( ! in )
                        goto failed;
                }
                goto set_field;

            case UT_WORD:
                switch( ur_atom(bi.it) )
                {
                case UR_ATOM_U8:
                    if( in == inEnd )
                        goto failed;
                    field = *in++;
                    goto set_field;

                case UR_ATOM_U16:
                    count = 2;
uint_count:         // Count is number of bytes.
                    if( (inEnd - in) < count )
                        goto failed;
                    field = _uintValue( in, count * 8, pe->bigEndian );
                    in += count;
                    goto set_field;

                case UR_ATOM_U32:
                    count = 4;
                    goto uint_count;

                case UR_ATOM_U64:
                    count = 8;
                    goto uint_count;

                case UR_ATOM_BIG_ENDIAN:
                    pe->bigEndian = 1;
                    break;

                case UR_ATOM_LITTLE_ENDIAN:
                    pe->bigEndian = 0;
                    break;
                }
                break;

            case UT_SETWORD:
                if( ! setWord )
                    setWord = bi.it;
                break;
        }
    }
    return in - pe->str->ptr.b;

set_field:

    if( setWord )
    {
        UCell* val;
        while( setWord != bi.it )
        {
            val = ur_wordCellM( ut, setWord++ );
            if( ! val )
                goto failed;
            ur_setId(val, UT_INT);
            ur_int(val) = field;
        }
        setWord = 0;
    }
    ++bi.it;
    goto match;

failed:

    return 0;
}


#define CHECK_WORD(cell) \
    if( ! cell ) \
        goto parse_err;

/*
  Returns zero if matching rule not found or exception occured.
*/
static const UCell* _parseStr( UThread* ut, StringParser* pe,
                               const UCell* rit, const UCell* rend,
                               UIndex* spos )
{
    const UCell* tval;
    int32_t repMin;
    int32_t repMax;
    UBuffer* istr = pe->str;
    UIndex pos = *spos;


match:

    while( rit != rend )
    {
        switch( ur_type(rit) )
        {
            case UT_WORD:
                switch( ur_atom(rit) )
                {
                case UR_ATOM_OPT:
                    ++rit;
                    repMin = 0;
                    repMax = 1;
                    goto repeat;

                case UR_ATOM_ANY:
                    ++rit;
                    repMin = 0;
                    repMax = REPEAT_ANY;
                    goto repeat;

                case UR_ATOM_SOME:
                    ++rit;
                    repMin = 1;
                    repMax = REPEAT_ANY;
                    goto repeat;

                case UR_ATOM_BREAK:
                    pe->exception = PARSE_EX_BREAK;
                    *spos = pos;
                    return 0;

                case UR_ATOM_BAR:
                    goto complete;

                case UR_ATOM_TO:
                case UR_ATOM_THRU:
                {
                    UAtom ratom = ur_atom(rit);

                    ++rit;
                    if( rit == rend )
                        return 0;

                    if( ur_is(rit, UT_WORD) )
                    {
                        tval = ur_wordCell( ut, rit );
                        CHECK_WORD(tval);
                    }
                    else
                    {
                        tval = rit;
                    }

                    switch( ur_type(tval) )
                    {
                        case UT_CHAR:
                            pos = ur_strFindChar( istr, pos, pe->inputEnd,
                                                  ur_int(tval), pe->matchCase );
                            if( pos < 0 )
                                goto failed;
                            if( ratom == UR_ATOM_THRU )
                                ++pos;
                            break;

                        case UT_BINARY:
                        case UT_STRING:
                        {
                            USeriesIter si;
                            USeriesIter sp;

                            si.buf = istr;
                            si.it  = pos;
                            si.end = pe->inputEnd;

                            ur_seriesSlice( ut, &sp, tval );
                            pos = ur_strFind( &si, &sp, pe->matchCase );
                            if( pos < 0 )
                                goto failed;
                            if( ratom == UR_ATOM_THRU )
                                pos += sp.end - sp.it;
                        }
                            break;

                        case UT_BITSET:
                            pos = pe->ucs2 ?
                                _scanToBitset_uint16_t( ut, istr->ptr.u16,
                                            pos, pe->inputEnd, tval ) :
                                _scanToBitset_uint8_t( ut, istr->ptr.b,
                                            pos, pe->inputEnd, tval );
                            if( pos < 0 )
                                goto failed;
                            if( ratom == UR_ATOM_THRU )
                                ++pos;
                            break;

                        default:
                            ur_error( PARSE_ERR, "to/thru does not handle %s",
                                      ur_atomCStr( ut, ur_type(tval) ) );
                            goto parse_err;
                    }
                    ++rit;
                }
                    break;

                case UR_ATOM_SKIP:
                    repMin = 1;
skip:
                    if( (pos + repMin) > pe->inputEnd )
                        goto failed;
                    pos += repMin;
                    ++rit;
                    break;

                case UR_ATOM_PLACE:
                    ++rit;
                    if( (rit != rend) && ur_is(rit, UT_WORD) )
                    {
                        tval = ur_wordCell( ut, rit++ );
                        CHECK_WORD(tval);
                        if( ur_is(tval, UT_STRING) )
                        {
                            pos = tval->series.it;
                            break;
                        }
                    }
                    ur_error( PARSE_ERR, "place expected series word" );
                    goto parse_err;

                case UR_ATOM_BITS:
                    if( pe->ucs2 )
                    {
                        ur_error( PARSE_ERR, "bits does not support UCS2" );
                        goto parse_err;
                    }

                    ++rit;
                    if( (rit != rend) && ur_is(rit, UT_BLOCK) )
                    {
                        pos = _parseBits( ut, pe, pos, rit++ );
                        if( ! pos )
                            goto parse_err;
                        break;
                    }
                    ur_error( PARSE_ERR, "bits expected block!" );
                    goto parse_err;

                //case UR_ATOM_COPY:

                default:
                    tval = ur_wordCell( ut, rit );
                    CHECK_WORD(tval);

                    switch( ur_type(tval) )
                    {
                        case UT_CHAR:
                            goto match_char;
                        case UT_INT:
                            repMin = ur_int(tval);
                            goto set_repeat;
                        case UT_STRING:
                            goto match_string;
                        case UT_BLOCK:
                            goto match_block;
                        case UT_BITSET:
                            goto match_bitset;
                    }
                    ur_error( PARSE_ERR,
                              "parse expected char!/block!/bitset!/string!" );
                    goto parse_err;
                }
                break;

            case UT_SETWORD:
            {
                UCell* cell = ur_wordCellM( ut, rit++ );
                if( ! cell )
                    goto parse_err;
                ur_setId( cell, istr->type );
                ur_setSlice( cell, pe->inputBuf, pos, pe->inputEnd );
            }
                break;

            case UT_GETWORD:
            {
                UCell* cell = ur_wordCellM( ut, rit++ );
                if( ! cell )
                    goto parse_err;
                if( cell->series.buf == pe->inputBuf )
                    cell->series.end = pos;
            }
                break;

            case UT_INT:
                repMin = ur_int(rit);
set_repeat:
                ++rit;
                if( rit == rend )
                    return 0;

                if( ur_is(rit, UT_INT) )
                {
                    repMax = ur_int(rit);
                    ++rit;
                }
                else if( ur_is(rit, UT_WORD) && ur_atom(rit) == UR_ATOM_SKIP )
                {
                    goto skip;
                }
                else
                {
                    repMax = repMin;
                }
                goto repeat;

            case UT_CHAR:
                tval = rit;
match_char:
                if( pos >= pe->inputEnd )
                    goto failed;
                if( (pe->ucs2 ? istr->ptr.u16[ pos ] :
                                istr->ptr.b[ pos ]) != ur_int(tval) )
                    goto failed;
                ++rit;
                ++pos;
                break;

            case UT_BLOCK:
                tval = rit;
match_block:
                {
                UBlockIt bi;
                UIndex rblkN = tval->series.buf;
                ur_blockIt( ut, &bi, tval );
                tval = _parseStr( ut, pe, bi.it, bi.end, &pos );
                istr = pe->str;
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
                }
                ++rit;
                break;

            case UT_PAREN:
                if( UR_OK != pe->eval( ut, rit ) )
                    goto parse_err;

                /* Re-acquire pointer & check if input modified. */
                istr = pe->str = ur_buffer( pe->inputBuf );
                if( pe->sliced )
                {
                    // We have no way to track changes to the end of a slice,
                    // so just make sure we remain in valid memery.
                    if( istr->used < pe->inputEnd )
                        pe->inputEnd = istr->used;
                }
                else
                {
                    // Not sliced, track input end.
                    if( istr->used != pe->inputEnd )
                        pe->inputEnd = istr->used;
                }

                ++rit;
                break;

            case UT_BINARY:
            case UT_STRING:
                tval = rit;
match_string:
            {
                USeriesIter si;
                USeriesIter sp;
                int plen;

                si.buf = istr;
                si.it  = pos;
                si.end = pe->inputEnd;

                ur_seriesSlice( ut, &sp, tval );
                plen = sp.end - sp.it;

                if( plen && (ur_strMatch( &si, &sp, pe->matchCase ) == plen) )
                {
                    pos += plen;
                    ++rit;
                }
                else
                    goto failed;
            }
                break;

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

            default:
                ur_error( PARSE_ERR, "Invalid parse rule value (%s)",
                          ur_atomCStr( ut, ur_type(rit) ) );
                goto parse_err;
        }
    }

complete:

    *spos = pos;
    return rit;

repeat:

    /* Repeat rit for repMin to repMax times. */

    if( rit == rend )
    {
        ur_error( PARSE_ERR, "Unexpected end of parse rule" );
        goto parse_err;
    }
    else
    {
        int count;

        if( ur_is(rit, UT_WORD) )
        {
            tval = ur_wordCell( ut, rit );
            CHECK_WORD(tval);
        }
        else
        {
            tval = rit;
        }

        switch( ur_type(tval) )
        {
            case UT_CHAR:
                count = pe->ucs2 ?
                    _repeatChar_uint16_t( istr->ptr.u16, pos, pe->inputEnd,
                                          repMax, ur_int(tval) ) :
                    _repeatChar_uint8_t( istr->ptr.b, pos, pe->inputEnd,
                                         repMax, ur_int(tval) );
                pos += count;
                break;

            case UT_BINARY:
            case UT_STRING:
            {
                USeriesIter si;
                USeriesIter sp;
                int plen;

                si.buf = istr;
                si.it  = pos;
                si.end = pe->inputEnd;

                ur_seriesSlice( ut, &sp, tval );
                plen = sp.end - sp.it;

                count = 0;
                if( plen )
                {
                    while( count < repMax )
                    {
                        if( ur_strMatch( &si, &sp, pe->matchCase ) != plen )
                            break;
                        si.it += plen;
                        ++count;
                    }
                    pos = si.it;
                }
            }
                break;

            case UT_BITSET:
                count = pe->ucs2 ?
                    _repeatBitset_uint16_t( ut, istr->ptr.u16,
                                            pos, pe->inputEnd, repMax, tval ) :
                    _repeatBitset_uint8_t( ut, istr->ptr.b,
                                           pos, pe->inputEnd, repMax, tval );
                pos += count;
                break;

            case UT_BLOCK:
            {
                UBlockIt bli;
                ur_blockIt( ut, &bli, tval );

                count = 0;

                while( count < repMax )
                {
                    if( pos >= pe->inputEnd )
                        break;
                    if( ! _parseStr( ut, pe, bli.it, bli.end, &pos ) )
                    {
                        if( pe->exception == PARSE_EX_ERROR )
                        {
                            ur_appendTrace( ut, tval->series.buf, 0 );
                            return 0;
                        }
                        if( pe->exception == PARSE_EX_BREAK )
                        {
                            pe->exception = PARSE_EX_NONE;
                            ++count;
                        }
                        break;
                    }
                    ++count;
                }
                istr = pe->str;
            }
                break;

            default:
                ur_error( PARSE_ERR, "Invalid parse rule" );
                goto parse_err;
        }

        if( count < repMin )
            goto failed;
        ++rit;
    }
    goto match;

failed:

    // Goto next rule; search for '|'.

    for( ; rit != rend; ++rit )
    {
        // It would be faster to check atom first.  This test will fail more
        // often, but the word.atom is not currently guaranteed to be in
        // intialized memory, causing memory checker errors. 
        if( ur_is(rit, UT_WORD) && (ur_atom(rit) == UR_ATOM_BAR) )
        {
            ++rit;
            pos = *spos;
            goto match;
        }
    }
    return 0;

parse_err:

    pe->exception = PARSE_EX_ERROR;
    return 0;
}


/** \defgroup urlan_dsl  Domain Languages
  \ingroup urlan
  These are small, special purpose evaluators.
*/

/**
  \ingroup urlan_dsl

  Parse a string or binary using the parse language.

  \note UR_ENC_UTF8 strings are accepted but multi-byte characters are not
        handled.

  \param str        Input to parse, which must be in thread storage.
  \param start      Starting character in input.
  \param end        Ending character in input.
  \param parsePos   Index in input where parse ended.
  \param ruleBlk    Rules in the parse language.  This block must be held
                    and remain unchanged during the parsing.
  \param eval       Evaluator callback to do paren values in rule.
                    The callback must return UR_OK/UR_THROW.
  \param matchCase  Use character case when comparing strings.

  \return UR_OK or UR_THROW.
*/
UStatus ur_parseString( UThread* ut, UBuffer* str, UIndex start, UIndex end,
                        UIndex* parsePos, const UBuffer* ruleBlk,
                        UStatus (*eval)(UThread*, const UCell*), int matchCase )
{
    StringParser p;

    p.eval = eval;
    p.str  = str;
    p.inputBuf  = str - ut->dataStore.ptr.buf;  // TODO: Don't access dataStore
    p.inputEnd  = end;
    p.sliced    = (end != str->used);
    p.exception = PARSE_EX_NONE;
    p.matchCase = matchCase ? UR_FIND_CASE : 0;
    p.ucs2      = (str->type == UT_STRING) && ur_strIsUcs2(str);
    p.bigEndian = 0;

    *parsePos = start;
    _parseStr( ut, &p, ruleBlk->ptr.cell,
                       ruleBlk->ptr.cell + ruleBlk->used, parsePos );
    return (p.exception == PARSE_EX_ERROR) ? UR_THROW : UR_OK;
}


/*EOF*/
