/*
  Copyright 2010 Karl Robillard

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


/*-cf-
    construct
        object  datatype!/binary!
        plan    block!

    Make or append values with a detailed specification.
*/
CFUNC(cfunc_construct)
{
    if( ! ur_is(a2, UT_BLOCK) )
        return errorType( "construct expected block! plan" );

    if( ur_is(a1, UT_DATATYPE) )
    {
        if( ur_datatype(a1) == UT_BINARY )
        {
            ur_makeBinaryCell( ut, 0, res );
            goto con_binary;
        }
    }
    else if( ur_is(a1, UT_BINARY) )
    {
        *res = *a1;
con_binary:
        return binary_construct( ut, a2, res );
    }
    return errorType( "construct expected binary!" );
}


//EOF
