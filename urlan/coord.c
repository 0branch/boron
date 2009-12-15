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
//----------------------------------------------------------------------------
// UT_COORD


#include "urlan.h"
#include "unset.h"


static int coord_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_BLOCK) )
    {
        UBlockIter bi;
        const UCell* cell;
        int16_t num;
        int len = 0;

        ur_setId(res, UT_COORD);
        res->coord.n[0] = 0;
        res->coord.n[1] = 0;

        ur_blkSlice( ut, &bi, from );
        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_WORD) )
            {
                cell = ur_wordCell( ut, bi.it );
                if( ! cell )
                    return UR_THROW;
            }
#if 0
            else if( ur_is(bi.it, UT_PATH) )
            {
                if( ! ur_pathCell( ut, bi.it, res ) )
                    return UR_THROW;
            }
#endif
            else
            {
                cell = bi.it;
            }

            if( ur_is(cell, UT_INT) )
                num = ur_int(cell);
            else if( ur_is(cell, UT_DECIMAL) )
                num = (int16_t) ur_decimal(cell);
            else
                break;

            res->coord.n[ len ] = num;
            if( ++len == UR_COORD_MAX )
                break;
        }

        res->coord.len = (len < 2) ? 2 : len;
        return UR_OK;
    }
    else if( ur_is(from, UT_COORD) )
    {
        *res = *from;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_SCRIPT, "make coord! expected block!/coord!" );
}


/*
int unset_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    (void) a;
    (void) b;
    (void) test;
    return 0;
}
*/


static
int coord_select( UThread* ut, const UCell* cell, UBlockIter* bi, UCell* res )
{
    int i;
    if( ur_is(bi->it, UT_INT) )
    {
        i = ur_int(bi->it) - 1;
        if( i > -1 && i < cell->coord.len )
        {
            ur_setId(res, UT_INT);
            ur_int(res) = cell->coord.n[ i ];
        }
        else
        {
            ur_setId(res, UT_NONE);
        }
        ++bi->it;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_SCRIPT, "coord select expected int!" );
}


static
void coord_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) ut;
    for( depth = 0; depth < cell->coord.len; ++depth )
    {
        if( depth )
            ur_strAppendChar( str, ',' );
        ur_strAppendInt( str, cell->coord.n[ depth ] ); 
    }
}


UDatatype dt_coord =
{
    "coord!",
    coord_make,             coord_make,             unset_copy,
    unset_compare,          coord_select,
    coord_toString,         coord_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


//EOF
