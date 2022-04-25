/*
  Copyright 2005-2011 Karl Robillard

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


enum BlockParseException
{
    PARSE_EX_NONE,
    PARSE_EX_ERROR,
    PARSE_EX_BREAK
};


typedef struct
{
    UStatus (*eval)( UThread*, const UCell* );
    const UBuffer* blk;
    UIndex   inputBuf;
    UIndex   inputEnd;
    short    sliced;
    short    exception;
}
BlockParser;


/*
    Re-acquire buffer pointer & check if input modified.
*/
static const UBuffer* _acquireInput( UThread* ut, BlockParser* pe )
{
    const UBuffer* iblk = pe->blk = ur_bufferE( pe->inputBuf );
    if( pe->sliced )
    {
        // We have no way to track changes to the end of a slice,
        // so just make sure we remain in valid memery.
        if( iblk->used < pe->inputEnd )
            pe->inputEnd = iblk->used;
    }
    else
    {
        // Not sliced, track input end.
        if( iblk->used != pe->inputEnd )
            pe->inputEnd = iblk->used;
    }
    return iblk;
}


#define BLK_RULE_ERROR(msg) \
    ur_error( ut, UR_ERR_SCRIPT, msg ); \
    goto parse_err

#define CHECK_WORD(cell) \
    if( ! cell ) \
        goto parse_err;

/*
    Returns zero if matching rule not found or exception occured.
*/
static const UCell* _parseBlock( UThread* ut, BlockParser* pe,
                                 const UCell* rit, const UCell* rend,
                                 UIndex* spos )
{
    const UCell* tval;
    int32_t repMin;
    int32_t repMax;
    UAtom atom;
    const UBuffer* iblk = pe->blk;
    UIndex pos = *spos;


match:

    while( rit != rend )
    {
        switch( ur_type(rit) )
        {
            case UT_WORD:
                atom = ur_atom(rit);

                if( atom < UT_BI_COUNT )
                {
                    // Datatype
                    if( pos >= pe->inputEnd )
                        goto failed;
                    tval = iblk->ptr.cell + pos;
                    if( ur_type(tval) != atom )
                    {
                        /*
                        if( atom == UT_NUMBER )
                        {
                            if( ur_is(tval,UT_INT) || ur_is(tval,UT_DECIMAL) )
                                goto type_matched;
                        }
                        */
                        goto failed;
                    }
//type_matched:
                    ++rit;
                    ++pos;
                }
                else switch( atom )
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
                        const UCell* ci;
                        const UCell* ce;
                        UAtom ratom = ur_atom(rit);

                        ++rit;
                        if( rit == rend )
                            return 0;

                        ci = iblk->ptr.cell + pos;
                        ce = iblk->ptr.cell + pe->inputEnd;

                        if( ur_is(rit, UT_WORD) )
                        {
                            if( ur_atom(rit) < UT_BI_COUNT )
                            {
                                atom = ur_atom(rit);
                                while( ci != ce )
                                {
                                    if( ur_type(ci) == atom )
                                        break;
                                    ++ci;
                                }
                                if( ci == ce )
                                    goto failed;
                                pos = ci - iblk->ptr.cell;
                                if( ratom == UR_ATOM_THRU )
                                    ++pos;
                                ++rit;
                                break;
                            }
                            else
                            {
                                tval = ur_wordCell( ut, rit );
                                CHECK_WORD( tval )
                            }
                        }
                        else
                        {
                            tval = rit;
                        }


                        if( ur_is(tval, UT_BLOCK) )
                        {
                            // TODO: If block then all values must match.
                            BLK_RULE_ERROR( "to/thru block! not implemented" );
                        }
                        else
                        {
                            while( ci != ce )
                            {
                                if( ur_equal(ut, ci, tval) )
                                    break;
                                ++ci;
                            }
                            if( ci == ce )
                                goto failed;
                            pos = ci - iblk->ptr.cell;
                            if( ratom == UR_ATOM_THRU )
                                ++pos;
                        }
                        ++rit;
                    }
                        break;

                    case UR_ATOM_INTO:
                        ++rit;
                        if( rit == rend || ! ur_is(rit, UT_BLOCK) )
                        {
                            BLK_RULE_ERROR( "parse into expected block" );
                        }
                        tval = iblk->ptr.cell + pos;
                        if( ! ur_isBlockType(ur_type(tval)) )
                            goto failed;
                        if( ur_isShared( tval->series.buf ) )
                            goto failed;
                    {
                        BlockParser ip;
                        UBlockIt bi;
                        UIndex parsePos = 0;

                        ip.eval = pe->eval;
                        ip.blk  = ur_bufferSer( tval );
                        ip.inputBuf  = tval->series.buf;
                        ip.inputEnd  = ip.blk->used;
                        ip.sliced    = 0;
                        ip.exception = PARSE_EX_NONE;

                        ur_blockIt( ut, &bi, rit );

                        tval = _parseBlock( ut, &ip, bi.it, bi.end, &parsePos );
                        iblk = _acquireInput( ut, pe );
                        if( ! tval )
                        {
                            if( ip.exception == PARSE_EX_ERROR )
                            {
                                pe->exception = PARSE_EX_ERROR;
                                ur_appendTrace( ut, rit->series.buf, 0 );
                                return 0;
                            }
                            if( ip.exception != PARSE_EX_BREAK )
                                goto failed;
                        }
                    }
                        ++rit;
                        ++pos;
                        break;

                    case UR_ATOM_SKIP:
                        repMin = 1;
skip:
                        if( (pos + repMin) > pe->inputEnd )
                            goto failed;
                        pos += repMin;
                        ++rit;
                        break;

                    case UR_ATOM_SET:
                        ++rit;
                        if( rit == rend )
                            goto unexpected_end;
                        if( ! ur_is(rit, UT_WORD) )
                        {
                            BLK_RULE_ERROR( "parse set expected word" );
                        }
                        if( pos >= pe->inputEnd )
                            goto failed;
                        {
                        UCell* cell = ur_wordCellM( ut, rit );
                        CHECK_WORD( cell )
                        *cell = iblk->ptr.cell[ pos ];
                        }
                        ++rit;
                        break;

                    case UR_ATOM_PLACE:
                        ++rit;
                        if( (rit != rend) && ur_is(rit, UT_WORD) )
                        {
                            tval = ur_wordCell( ut, rit++ );
                            CHECK_WORD( tval )
                            if( ur_is(tval, UT_BLOCK) )
                            {
                                pos = tval->series.it;
                                break;
                            }
                        }
                        BLK_RULE_ERROR( "place expected series word" );

                    //case UR_ATOM_COPY:

                    default:
                    {
                        tval = ur_wordCell( ut, rit );
                        CHECK_WORD( tval )

                        if( ur_is(tval, UT_BLOCK) )
                        {
                            goto match_block;
                        }
                        else if( ur_is(tval, UT_INT) )
                        {
                            repMin = ur_int(tval);
                            goto set_repeat;
                        }
                        else
                        {
                            BLK_RULE_ERROR( "parse expected block" );
                        }
                    }
                        break;
                }
                break;

            case UT_SETWORD:
            {
                UCell* cell = ur_wordCellM( ut, rit );
                CHECK_WORD( cell )
                ++rit;

                ur_setId( cell, UT_BLOCK );
                ur_setSlice( cell, pe->inputBuf, pos, pe->inputEnd );
            }
                break;

            case UT_GETWORD:
            {
                UCell* cell = ur_wordCellM( ut, rit );
                CHECK_WORD( cell )
                ++rit;

                if( ur_is(cell, UT_BLOCK) &&
                    (cell->series.buf == pe->inputBuf) )
                    cell->series.end = pos;
            }
                break;

            case UT_LITWORD:
                if( pos >= pe->inputEnd )
                    goto failed;
                tval = iblk->ptr.cell + pos;
                if( (ur_is(tval, UT_WORD) || ur_is(tval, UT_LITWORD))
                    && (ur_atom(tval) == ur_atom(rit)) )
                {
                    ++rit;
                    ++pos;
                }
                else
                    goto failed;
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

            case UT_DATATYPE:
                if( pos >= pe->inputEnd )
                    goto failed;
                if( ! ur_isDatatype( iblk->ptr.cell + pos, rit ) )
                    goto failed;
                ++rit;
                ++pos;
                break;

            case UT_CHAR:
            case UT_BINARY:
            case UT_STRING:
            case UT_FILE:
                if( pos >= pe->inputEnd )
                    goto failed;
                if( ! ur_equal( ut, iblk->ptr.cell + pos, rit ) )
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
                tval = _parseBlock( ut, pe, bi.it, bi.end, &pos );
                iblk = pe->blk;
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
                iblk = _acquireInput( ut, pe );
                ++rit;
                break;

            default:
                ur_error( ut, UR_ERR_SCRIPT, "Invalid parse rule value (%s)",
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
unexpected_end:
        BLK_RULE_ERROR( "Unexpected end of parse rule" );
    }
    else
    {
        int count;

        if( ur_is(rit, UT_WORD) )
        {
            tval = ur_wordCell( ut, rit );
            CHECK_WORD( tval )
        }
        else
        {
            tval = rit;
        }

        switch( ur_type(tval) )
        {
            case UT_DATATYPE:
                // If repMax would pass end of input limit repMax.
                count = pe->inputEnd - pos;
                if( count < repMax )
                    repMax = count;
                count = 0;

                atom = ur_datatype(tval);
                while( count < repMax )
                {
                    tval = iblk->ptr.cell + pos;
                    if( ur_type(tval) != atom )     // Test mask?
                        break;
                    ++pos;
                    ++count;
                }
                break;

            case UT_LITWORD:
                // If repMax would pass end of input limit repMax.
                count = pe->inputEnd - pos;
                if( count < repMax )
                    repMax = count;
                count = 0;

                atom = ur_atom(tval);
                while( count < repMax )
                {
                    tval = iblk->ptr.cell + pos;
                    if( ! (ur_is(tval, UT_WORD) || ur_is(tval, UT_LITWORD)) )
                        break;
                    if( ur_atom(tval) != atom )
                        break;
                    ++pos;
                    ++count;
                }
                break;

            case UT_BLOCK:
            {
                UBlockIt bi;
                ur_blockIt( ut, &bi, tval );

                count = 0;

                while( count < repMax )
                {
                    if( pos >= pe->inputEnd )
                        break;
                    if( ! _parseBlock( ut, pe, bi.it, bi.end, &pos ) )
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
                if( pe->exception == PARSE_EX_ERROR )
                    return 0;
                iblk = pe->blk;
            }
                break;

            default:
                BLK_RULE_ERROR( "Invalid parse rule" );
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


/**
  \ingroup urlan_dsl

  Parse a block using the parse language.

  \param blk        Input to parse, which must be in thread storage.
  \param start      Starting cell in input.
  \param end        Ending cell in input.
  \param parsePos   Index in input where parse ended.
  \param ruleBlk    Rules in the parse language.  This block must be held
                    and remain unchanged during the parsing.
  \param eval       Evaluator callback to do paren values in rule.
                    The callback must return UR_OK/UR_THROW.

  \return UR_OK or UR_THROW.
*/
UStatus ur_parseBlock( UThread* ut, UBuffer* blk, UIndex start, UIndex end,
                       UIndex* parsePos, const UBuffer* ruleBlk,
                       UStatus (*eval)(UThread*, const UCell*) )
{
    BlockParser p;

    p.eval = eval;
    p.blk  = blk;
    p.inputBuf  = blk - ut->dataStore.ptr.buf;  // TODO: Don't access dataStore
    p.inputEnd  = end;
    p.sliced    = (end != blk->used);
    p.exception = PARSE_EX_NONE;

    *parsePos = start;
    _parseBlock( ut, &p, ruleBlk->ptr.cell,
                         ruleBlk->ptr.cell + ruleBlk->used, parsePos );
    return (p.exception == PARSE_EX_ERROR) ? UR_THROW : UR_OK;
}


/*EOF*/
