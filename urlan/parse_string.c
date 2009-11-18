/*
  Copyright 2005-2009 Karl Robillard

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


typedef char   UChar;


#define UString UBuffer
#define bitIsSet(array,n)    (array[(n)>>3] & 1<<((n)&7))


#define PARSE_ERR   ut, UR_ERR_SCRIPT

enum StringParseException
{
    PARSE_EX_NONE,
    PARSE_EX_ERROR,
    PARSE_EX_BREAK
};


typedef struct
{
    int (*eval)( UThread*, const UCell* );
    UString* str;
    UIndex   inputBuf;
    UIndex   inputEnd;
    int      sliced;
    int      exception;
    int      matchCase;
}
StringParser;


/*
  Returns index in str where pattern is found or -1 if not found.
*/
static UIndex _findString( const UString* str, UIndex itN, UIndex endN,
                           const uint8_t* pit, const uint8_t* pend,
                           int matchCase )
{
    const uint8_t* it  = str->ptr.b + itN;
    const uint8_t* end = str->ptr.b + endN;
    (void) matchCase;
    it = find_pattern_uint8_t( it, end, pit, pend );
    if( ! it )
        return -1;
    return it - str->ptr.b;
}


#ifdef _MSC_VER
#define inline  __inline
#endif

static inline int _toLower( int c )
{
    if( (c >= 'A') && (c <= 'Z') )
        c += 'a' - 'A';
    return c;
}


/*
  Returns index in strA where match is complete or zero if strings do not
  match.
*/
static UIndex _matchString( const UString* strA, UIndex iA, UIndex endA,
                            const uint8_t* pit, const uint8_t* pend,
                            int matchCase )
{
    const uint8_t* it  = strA->ptr.b + iA;
    const uint8_t* end = strA->ptr.b + endA;

    if( matchCase )
    {
        const uint8_t* pf;
        pf = match_pattern_uint8_t( it, end, pit, pend );
        if( pf == pend )
            return iA + (pend - pit);
        return 0;
    }
    else
    {
        while( pit != pend )
        {
            if( it == end )
                return 0;
            if( _toLower(*it) != _toLower(*pit) )
                return 0;
            ++it;
            ++pit;
        }
        return it - strA->ptr.b;
    }
}


static int _repeatChar( UString* input, UIndex pos, UIndex inputEnd,
                        int limit, int c )
{
    UChar* start;
    UChar* it;
    UChar* end;

    it  = input->ptr.c;
    end = it + inputEnd;
    it += pos;
    if( end > (it + limit) )
        end = it + limit;

    start = it;
    while( it != end )
    {
        if( *it != c )
            break;
        ++it;
    }
    return it - start;
}


/*
  Return number of characters advanced.
*/
static int _repeatBitset( UThread* ut,
                          UString* input, UIndex pos, UIndex inputEnd,
                          int limit, const UCell* patc )
{
    UChar* start;
    UChar* it;
    UChar* end;
    int c;
    const UBuffer* bin = ur_bufferSer(patc);
    const uint8_t* bits = bin->ptr.b;
    int maxC = bin->used * 8;

    it  = input->ptr.c;
    end = it + inputEnd;
    it += pos;
    if( end > (it + limit) )
        end = it + limit;

    start = it;
    while( it != end )
    {
        c = *it;
        if( c >= maxC )
            break;
        if( ! bitIsSet( bits, c ) )
            break;
        ++it;
    }
    return it - start;
}


/*
  Return new pos or -1 if not found.
*/
static int _scanToBitset( UThread* ut,
                          UString* input, UIndex pos, UIndex inputEnd,
                          const UCell* binc )
{
    UChar* start;
    UChar* it;
    UChar* end;
    int c;
    const UBuffer* bin = ur_bufferSer(binc);
    const uint8_t* bits = bin->ptr.b;
    int maxC = bin->used * 8;

    it  = input->ptr.c + pos;
    end = input->ptr.c + inputEnd;

    start = it;
    while( it != end )
    {
        c = *it;
        if( c < maxC )
        {
            if( bitIsSet( bits, c ) )
                return it - input->ptr.c;
        }
        ++it;
    }
    return -1;
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
    UString* istr = pe->str;
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
                    repMax = 0x7fffffff;
                    goto repeat;

                case UR_ATOM_SOME:
                    ++rit;
                    repMin = 1;
                    repMax = 0x7fffffff;
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
                        {
                            int c = ur_int(tval);
                            UChar* cp  = istr->ptr.c + pos;
                            UChar* end = istr->ptr.c + pe->inputEnd;
                            while( cp != end )
                            {
                                if( *cp == c )
                                    break;
                                ++cp;
                            }
                            if( cp == end )
                                goto failed;
                            pos = cp - istr->ptr.c;
                            if( ratom == UR_ATOM_THRU )
                                ++pos;
                        }
                            break;

                        case UT_STRING:
                        {
                            UBinaryIter bi;

                            ur_binSlice( ut, &bi, tval );
                            if( bi.buf->form == UR_ENC_UCS2 )
                                goto bad_enc;
                            pos = _findString( istr, pos, pe->inputEnd,
                                               bi.it, bi.end, pe->matchCase );
                            if( pos < 0 )
                                goto failed;
                            if( ratom == UR_ATOM_THRU )
                                pos += bi.end - bi.it;
                        }
                            break;

                        case UT_BITSET:
                        {
                            int n;
                            n = _scanToBitset( ut, istr, pos, pe->inputEnd,
                                               tval );
                            if( n < 0 )
                                goto failed;
                            pos = n;
                            if( ratom == UR_ATOM_THRU )
                                ++pos;
                        }
                            break;

                        case UT_BLOCK:
                            // TODO
                            ur_error( PARSE_ERR,
                                      "to/thru block! not implemented" );
                            goto parse_err;
                    }
                    ++rit;
                }
                    break;

                case UR_ATOM_SKIP:
                    // TODO - int! skip
                    //if( pos >= istr->used )
                    //    return 0;
                    ++rit;
                    ++pos;
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

                //case UR_ATOM_COPY:

                default:
                    tval = ur_wordCell( ut, rit );
                    CHECK_WORD(tval);

                    if( ur_is(tval, UT_CHAR) )
                        goto match_char;
                    else if( ur_is(tval, UT_STRING) )
                        goto match_string;
                    else if( ur_is(tval, UT_BLOCK) )
                        goto match_block;
                    else if( ur_is(tval, UT_BITSET) )
                        goto match_bitset;
                    else
                    {
                        ur_error( PARSE_ERR,
                              "parse expected char!/block!/bitset!/string!" );
                        goto parse_err;
                    }
                    break;
                }
                break;

            case UT_SETWORD:
            {
                UCell* cell = ur_wordCellM( ut, rit++ );
                if( ! cell )
                    goto parse_err;
                ur_setId( cell, UT_STRING );
                ur_setSlice( cell, pe->inputBuf, pos, pe->inputEnd );
            }
                break;

            case UT_GETWORD:
            {
                UCell* cell = ur_wordCellM( ut, rit++ );
                if( ! cell )
                    goto parse_err;
                if( ur_is(cell, UT_STRING) &&
                    (cell->series.buf == pe->inputBuf) )
                    cell->series.end = pos;
            }
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

            case UT_CHAR:
                tval = rit;
match_char:
                if( (pos >= pe->inputEnd) ||
                    (istr->ptr.c[ pos ] != ur_int(tval)) )
                    goto failed;
                ++rit;
                ++pos;
                break;

            case UT_BLOCK:
                tval = rit;
match_block:
                {
                UBlockIter bi;
                UIndex rblkN = tval->series.buf;
                ur_blkSlice( ut, &bi, tval );
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

            case UT_STRING:
                tval = rit;
match_string:
            {
                UBinaryIter bi;
                ur_binSlice( ut, &bi, tval );
                if( bi.buf->form == UR_ENC_UCS2 )
                    goto bad_enc;
                pos = _matchString( istr, pos, pe->inputEnd,
                                    bi.it, bi.end, pe->matchCase );
                if( pos > 0 )
                    ++rit;
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
                ur_error( PARSE_ERR, "invalid parse value" );
                             //orDatatypeName( ur_type(rit) ) );
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
        ur_error( PARSE_ERR, "Enexpected end of parse rule" );
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
                count = _repeatChar( istr, pos, pe->inputEnd,
                                     repMax, ur_int(tval) );
                pos += count;
                break;

            case UT_STRING:
            {
                int p2;
                UBinaryIter bi;
                ur_binSlice( ut, &bi, tval );
                if( bi.buf->form == UR_ENC_UCS2 )
                    goto bad_enc;

                count = 0;

                while( count < repMax )
                {
                    p2 = _matchString( istr, pos, pe->inputEnd,
                                       bi.it, bi.end, pe->matchCase );
                    if( p2 < 1 )
                        break;
                    pos = p2;
                    ++count;
                }
            }
                break;

            case UT_BITSET:
                count = _repeatBitset( ut, istr, pos, pe->inputEnd,
                                       repMax, tval );
                pos += count;
                break;

            case UT_BLOCK:
            {
                UBlockIter bli;
                ur_blkSlice( ut, &bli, tval );

                count = 0;

                while( count < repMax )
                {
                    if( pos == pe->inputEnd )
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

bad_enc:

    ur_error( ut, UR_ERR_INTERNAL, "ur_strParse() does not handle UCS2" );

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

  Parse a string using the parse language.

  \note Currently only UR_ENC_LATIN1 strings are handled.

  \param str        Input to parse, which must be in thread storage.
  \param start      Starting character in input.
  \param end        Ending character in input.
  \param parsePos   Index in input where parse ended.
  \param ruleBlk    Rules in the parse language.  This block must be held
                    and remain unchanged during the parsing.
  \param eval       Evaluator callback to do paren values in rule.
                    The callback must return UR_OK/UR_THROW.

  \return UR_OK or UR_THROW.
*/
int ur_parseString( UThread* ut, UBuffer* str, UIndex start, UIndex end,
                    UIndex* parsePos, const UBuffer* ruleBlk,
                    int (*eval)( UThread*, const UCell* ) )
{
    StringParser p;

    if( str->form == UR_ENC_UCS2 )
    {
        ur_error( ut, UR_ERR_INTERNAL, "ur_strParse() does not handle UCS2" );
        return start;
    }

    p.eval = eval;
    p.str  = str;
    p.inputBuf  = str - ut->dataStore.ptr.buf;  // TODO: Don't access dataStore
    p.inputEnd  = end;
    p.sliced    = (end != str->used);
    p.exception = PARSE_EX_NONE;
    p.matchCase = 1;

    *parsePos = start;
    _parseStr( ut, &p, ruleBlk->ptr.cell,
                       ruleBlk->ptr.cell + ruleBlk->used, parsePos );
    return (p.exception == PARSE_EX_ERROR) ? UR_THROW : UR_OK;
}


/*EOF*/
