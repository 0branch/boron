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


#include "quickSortIndex.h"


#define OPT_SORT_CASE   0x01
#define OPT_SORT_GROUP  0x02
#define OPT_SORT_FIELD  0x04


struct CompareField
{
    UThread* ut;
    UBlockIter fb;
    uint32_t opt;
};


static const UCell* _blkCell( const UBuffer* blk, UIndex pos )
{
    if( pos >= 0 && pos < blk->used )
        return blk->ptr.cell + pos;
    return 0;
}


static int _binByte( const UBuffer* bin, UIndex pos )
{
    if( pos >= 0 && pos < bin->used )
        return bin->ptr.b[ pos ];
    return -1;
}


static int _compareField( struct CompareField* cf,
                          const UCell* a, const UCell* b )
{
    const UBuffer* bufA;
    const UBuffer* bufB;
    const UCell* it;
    int i;
    int ca;
    int type = ur_type(a);

    if( type != ur_type(b) )
        return 0;
    if( ! ((1 << type) &
           (1<<UT_BINARY | 1<<UT_STRING | 1<<UT_FILE |
            1<<UT_BLOCK | 1<<UT_CONTEXT)) )
        return 0;

    bufA = ur_bufferSeries( cf->ut, a );
    bufB = ur_bufferSeries( cf->ut, b );

    switch( type )
    {
        case UT_BINARY:
        case UT_STRING:
        case UT_FILE:
            for( it = cf->fb.it; it != cf->fb.end; ++it )
            {
                if( ur_is(it, UT_INT) )
                {
                    // Convert from one-based index but allow negative pos.
                    i = ur_int(it);
                    if( i > 1 )
                        --i;

                    if( type == UT_BINARY )
                    {
                        ca = _binByte( bufA, i + a->series.it );
                        i  = _binByte( bufB, i + b->series.it );
                    }
                    else
                    {
                        ca = ur_strChar( bufA, i + a->series.it );
                        i  = ur_strChar( bufB, i + b->series.it );
                        if( ! (cf->opt & OPT_SORT_CASE) )
                        {
                            ca = ur_charLowercase( ca );
                            i  = ur_charLowercase( i );
                        }
                    }

                    if( ca > i )
                        return 1;
                    if( ca < i )
                        return -1;
                }
            }
            break;

        case UT_BLOCK:
        {
            const UCell* valA;
            const UCell* valB;

            for( it = cf->fb.it; it != cf->fb.end; ++it )
            {
                if( ur_is(it, UT_INT) )
                {
                    i = ur_int(it) - 1;

                    valA = _blkCell( bufA, i + a->series.it );
                    valB = _blkCell( bufB, i + b->series.it );

                    if( (i = ur_compare( cf->ut, valA, valB )) )
                        return i;
                }
            }
        }
            break;

        case UT_CONTEXT:
            for( it = cf->fb.it; it != cf->fb.end; ++it )
            {
                if( ur_is(it, UT_WORD) )
                {
                    if( (i = ur_ctxLookup( bufA, ur_atom(it) )) < 0 )
                        return 0;
                    a = ur_ctxCell( bufA, i );

                    if( (i = ur_ctxLookup( bufB, ur_atom(it) )) < 0 )
                        return 0;
                    b = ur_ctxCell( bufB, i );

                    if( (i = ur_compare( cf->ut, a, b )) )
                        return i;
                }
            }
            break;
    }
    return 0;
}


/*-cf-
    sort
        set         series
        /case       Use case-sensitive comparison with string types.
        /group      Compare groups of elements by first value in group.
            size    int!
        /field      Sort on selected words of context elements.
            which   block!
    return: New series with sorted elements.
    group: series
*/
CFUNC(cfunc_sort)
{
    int type = ur_type(a1);

    if( ur_isBlockType(type) )
    {
        QuickSortIndex qs;
        struct CompareField fld;
        UBlockIter bi;
        UBuffer* blk;
        uint32_t* ip;
        uint32_t* iend;
        int group;
        int len;
        int indexLen;

        ur_blkSlice( ut, &bi, a1 );
        len = bi.end - bi.it;

        // Make invalidates bi.buf.
        blk = ur_makeBlockCell( ut, type, len, res );

        fld.opt = CFUNC_OPTIONS;
        if( fld.opt & OPT_SORT_GROUP )
        {
            group = ur_int(a2);
            if( group < 1 )
                group = 1;
            indexLen = len / group;
        }
        else
        {
            group = 1;
            indexLen = len;
        }

        qs.index    = ((uint32_t*) (blk->ptr.cell + len)) - indexLen;
        qs.data     = (uint8_t*) bi.it;
        qs.elemSize = sizeof(UCell);

        if( fld.opt & OPT_SORT_FIELD )
        {
            fld.ut = ut;
            ur_blkSlice( ut, &fld.fb, a3 );

            qs.user     = (void*) &fld;
            qs.compare  = (QuickSortFunc) _compareField;
        }
        else
        {
            qs.user     = (void*) ut;
            qs.compare  = (QuickSortFunc) ((fld.opt & OPT_SORT_CASE) ?
                                ur_compareCase : ur_compare);
        }

        ip = qs.index;
        iend = ip + quickSortIndex( &qs, 0, len, group );

        len = qs.elemSize * group;
        while( ip != iend )
        {
            memCpy( blk->ptr.cell + blk->used, bi.it + *ip, len );
            blk->used += group;
            ++ip;
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_INTERNAL, "FIXME: sort only supports block!" );
}


/*EOF*/
