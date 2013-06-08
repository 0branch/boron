/*
  Copyright 2010,2013 Karl Robillard

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


static void ur_binAppendInt( UBuffer* bin, uint32_t n,
                             UAtom size, int bigEndian )
{
    uint8_t* bp;

    ur_binReserve( bin, bin->used + 4 );
    bp = bin->ptr.b + bin->used;

    switch( size )
    {
        case UR_ATOM_U8:
            *bp = n;
            bin->used += 1;
            break;

        case UR_ATOM_U16:
            if( bigEndian )
            {
                *bp++ = n >> 8;
                *bp   = n;
            }
            else
            {
                *bp++ = n;
                *bp   = n >> 8;
            }
            bin->used += 2;
            break;

        case UR_ATOM_U32:
            if( bigEndian )
            {
                *bp++ = n >> 24;
                *bp++ = n >> 16;
                *bp++ = n >> 8;
                *bp   = n;
            }
            else
            {
                *bp++ = n;
                *bp++ = n >> 8;
                *bp++ = n >> 16;
                *bp   = n >> 24;
            }
            bin->used += 4;
            break;
    }
}


static int binary_construct( UThread* ut, const UCell* blkC, const UCell* binC )
{
    UBlockIter bi;
    const UCell* cell;
    UBuffer* bin;
    int bigEndian = 0;
    UAtom size = UR_ATOM_U32;

    bin = ur_bufferSerM(binC);
    if( ! bin )
        return UR_THROW;

    ur_blkSlice( ut, &bi, blkC );
    ur_foreach( bi )
    {
        if( ur_is(bi.it, UT_WORD) )
        {
            switch( ur_atom(bi.it) )
            {
                case UR_ATOM_U8:
                case UR_ATOM_U16:
                case UR_ATOM_U32:
                    size = ur_atom(bi.it);
                    break;

                case UR_ATOM_BIG_ENDIAN:
                    bigEndian = 1;
                    break;

                case UR_ATOM_LITTLE_ENDIAN:
                    bigEndian = 0;
                    break;

                default:
                    if( ! (cell = ur_wordCell( ut, bi.it )) )
                        return UR_THROW;
                    if( ur_is(cell, UT_INT) )
                        ur_binAppendInt( bin, ur_int(cell), size, bigEndian );
                    else if( ur_is(cell, UT_BINARY) )
                        goto con_bin;
                    else if( ur_is(cell, UT_BLOCK) )
                        goto con_blk;
                    else
                        return errorType( "construct binary! expected int" );
            }
        }
        else if( ur_is(bi.it, UT_INT) )
        {
            ur_binAppendInt( bin, ur_int(bi.it), size, bigEndian );
        }
        else if( ur_is(bi.it, UT_BINARY) )
        {
            cell = bi.it;
con_bin:
            {
            UBinaryIter b2;
            ur_binSlice( ut, &b2, cell );
            if( b2.end > b2.it )
                ur_binAppendData( bin, b2.it, b2.end - b2.it );
            }
        }
        else if( ur_is(bi.it, UT_BLOCK) )
        {
            cell = bi.it;
con_blk:
            {
            UBlockIter b2;
            ur_blkSlice( ut, &b2, cell );
            ur_foreach( b2 )
            {
                if( ur_is(b2.it, UT_INT) )
                    ur_binAppendInt( bin, ur_int(b2.it), size, bigEndian );
            }
            }
        }
#if 0
        else
        {
            if( ! boron_eval1( ut, blkC, res ) )
                return UR_THROW;
        }
#endif
    }
    return UR_OK;
}


extern int string_append( UThread* ut, UBuffer* buf, const UCell* val );

static int sc_appendEval( UThread* ut, UBuffer* str, const UCell* val )
{
    if( ur_is(val, UT_WORD) )
    {
        if( ! (val = ur_wordCell( ut, val )) )
            return UR_THROW;
        if( ur_is(val, UT_NONE) )
            return UR_OK;
    }
    return string_append( ut, str, val );
}


/*
    Create output string from input with changes.

    The plan block is simply a list of search and replace values.
*/
static int string_construct( UThread* ut, UBuffer* str,
                             const UCell* inputC, const UCell* blkC )
{
#define sc_setBit(mem,n)    (mem[(n)>>3] |= 1<<((n)&7))
#define SC_BIT_COUNT    256
    USeriesIter in;
    UBlockIter bi;
    UBuffer cset;
    const UCell* begin;
    UIndex copyPos;
    int ch;
    int fp;


    ur_blkSlice( ut, &bi, blkC );
    begin = bi.it;
    if( (bi.end - bi.it) & 1 )
        --bi.end;

    // Create bitset of search value characters.
    ur_binInit( &cset, SC_BIT_COUNT / 8 );
    cset.used = SC_BIT_COUNT / 8;
    memSet( cset.ptr.b, 0, cset.used );
    for( ; bi.it != bi.end; bi.it += 2 )
    {
        if( ur_is(bi.it, UT_CHAR) )
        {
            ch = ur_int(bi.it);
        }
        else if( ur_is(bi.it, UT_STRING) )
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, bi.it );
            ch = ur_strChar( si.buf, si.it );
        }
        else
        {
            ch = -1;
        }

        if( ch >= 0 && ch < SC_BIT_COUNT )
            sc_setBit( cset.ptr.b, ch );
    }

    // Append to str with replacements.
    ur_seriesSlice( ut, &in, inputC );
    copyPos = in.it;
    while( (fp = ur_strFindChars(in.buf, in.it, in.end, cset.ptr.b, cset.used))
            > -1 )
    {
        ch = ur_strIsUcs2(in.buf) ? in.buf->ptr.u16[ fp ]
                                  : in.buf->ptr.b[ fp ];

        for( bi.it = begin; bi.it != bi.end; bi.it += 2 )
        {
            if( ur_is(bi.it, UT_CHAR) )
            {
                if( ch == ur_int(bi.it) )
                {
                    in.it = fp + 1;
match:
                    ur_strAppend( str, in.buf, copyPos, fp );
                    copyPos = in.it;
                    if( sc_appendEval( ut, str, bi.it + 1 ) != UR_OK )
                        return UR_THROW;
                    break;
                }
            }
            else if( ur_is(bi.it, UT_STRING) )
            {
                USeriesIter inA;
                USeriesIter si;
                int len;

                ur_seriesSlice( ut, &si, bi.it );
                len = si.end - si.it;

                inA.buf = in.buf;
                inA.it  = fp;
                inA.end = in.end;

                if( ur_strMatch( &inA, &si, 0 ) == len )
                {
                    in.it = fp + len;
                    goto match;
                }
            }
        }
        if( bi.it == bi.end )
            ++in.it;
    }
    ur_strAppend( str, in.buf, copyPos, in.end );

    ur_binFree( &cset );
    return UR_OK;
}


/*-cf-
    construct
        object  datatype!/binary!/string!
        plan    block!
    return: New value.
    group: data

    Make or append values with a detailed specification.
*/
CFUNC(cfunc_construct)
{
    if( ! ur_is(a2, UT_BLOCK) )
        return errorType( "construct expected block! plan" );

    switch( ur_type(a1) )
    {
        case UT_DATATYPE:
            if( ur_datatype(a1) == UT_BINARY )
            {
                ur_makeBinaryCell( ut, 0, res );
                goto con_binary;
            }
            break;

        case UT_BINARY:
            *res = *a1;
con_binary:
            return binary_construct( ut, a2, res );

        case UT_STRING:
        {
            const UBuffer* in = ur_bufferSer(a1);
            return string_construct( ut,
                    ur_makeStringCell( ut, in->form, in->used, res ),  // gc!
                    a1, a2 );
        }
    }
    return errorType( "construct expected binary!/string!" );
}


//EOF
