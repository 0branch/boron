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
    UBlockIt fb;
    uint32_t opt;
};


/*
static int _binByte( const UBuffer* bin, UIndex pos )
{
    if( pos >= 0 && pos < bin->used )
        return bin->ptr.b[ pos ];
    return -1;
}
*/


static int _compareField( struct CompareField* cf,
                          const UCell* a, const UCell* b )
{
    UThread* ut = cf->ut;
    const UCell* it;
    int i;
    int rev;
    int type = ur_type(a);

    if( type != ur_type(b) )
        return 0;

    switch( type )
    {
#if 0
        case UT_BINARY:
        case UT_STRING:
        case UT_FILE:
        {
            const UBuffer* bufA = ur_bufferSer( a );
            const UBuffer* bufB = ur_bufferSer( b );
            int ca;

            for( it = cf->fb.it; it != cf->fb.end; ++it )
            {
                if( ur_is(it, UT_INT) )
                {
                    // Convert from one-based index but allow negative
                    // position to skip from end.
                    i = ur_int(it);
                    if( i > 0 )
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
        }
            break;
#endif
        case UT_BLOCK:
        {
            USeriesIter siA;
            USeriesIter siB;
            int pos;

            ur_seriesSlice( ut, &siA, a );
            ur_seriesSlice( ut, &siB, b );

            for( it = cf->fb.it; it != cf->fb.end; )
            {
                if( ur_is(it, UT_INT) )
                {
                    // Convert from one-based index but allow negative
                    // position to pick from end.
                    i = ur_int(it);
                    if( i > 0 )
                        --i;

                    if( (++it != cf->fb.end) && ur_is(it, UT_OPTION) )
                    {
                        ++it;
                        rev = 1;
                    }
                    else
                    {
                        rev = 0;
                    }

#define SORT_BLK_CELL(res, si, index) \
    pos = index + ((index < 0) ? si.end : si.it); \
    if( pos < si.it || pos >= si.end ) \
        return 0; \
    res = si.buf->ptr.cell + pos;

                    SORT_BLK_CELL( a, siA, i );
                    SORT_BLK_CELL( b, siB, i );

                    if( (i = ur_compare( ut, a, b )) )
                        return rev ? -i : i;
                }
                else
                {
                    ++it;
                }
            }
        }
            break;

        case UT_CONTEXT:
        {
            const UBuffer* bufA = ur_bufferSer( a );
            const UBuffer* bufB = ur_bufferSer( b );
            UAtom word;

            for( it = cf->fb.it; it != cf->fb.end; )
            {
                if( ur_is(it, UT_WORD) )
                {
                    word = ur_atom(it);

                    if( (++it != cf->fb.end) && ur_is(it, UT_OPTION) )
                    {
                        ++it;
                        rev = 1;
                    }
                    else
                    {
                        rev = 0;
                    }

                    if( (i = ur_ctxLookup( bufA, word )) < 0 )
                        return 0;
                    a = ur_ctxCell( bufA, i );

                    if( (i = ur_ctxLookup( bufB, word )) < 0 )
                        return 0;
                    b = ur_ctxCell( bufB, i );

                    if( (i = ur_compare( ut, a, b )) )
                        return rev ? -i : i;
                }
                else
                {
                    ++it;
                }
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
        /field      Sort on specified context words or block indices.
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
        UBlockIt bi;
        UBuffer* blk;
        uint32_t* ip;
        uint32_t* iend;
        int group;
        int len;
        int indexLen;

        ur_blockIt( ut, &bi, a1 );
        len = bi.end - bi.it;

        fld.opt = CFUNC_OPTIONS;
        if( fld.opt & OPT_SORT_GROUP )
        {
            group = ur_int(CFUNC_OPT_ARG(2));
            if( group < 1 )
                group = 1;
            indexLen = len / group;
            len = indexLen * group;     // Remove any partial group.
        }
        else
        {
            group = 1;
            indexLen = len;
        }

        blk = ur_makeBlockCell( ut, type, len, res );   // gc!

        qs.index    = ((uint32_t*) (blk->ptr.cell + len)) - indexLen;
        qs.data     = (uint8_t*) bi.it;
        qs.elemSize = sizeof(UCell);

        if( fld.opt & OPT_SORT_FIELD )
        {
            fld.ut = ut;
            ur_blockIt( ut, &fld.fb, CFUNC_OPT_ARG(3) );

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
