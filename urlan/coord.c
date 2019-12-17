/*
  Copyright 2009,2010,2012 Karl Robillard

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
#include "os.h"


static int coord_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_BLOCK) )
    {
        UBlockIt bi;
        const UCell* cell;
        int len = 0;

        ur_setId(res, UT_COORD);

        ur_blockIt( ut, &bi, from );
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
            {
                res->coord.n[ len ] = ur_int(cell);
                if( ++len == UR_COORD_MAX )
                    goto set_len;
            }
            else if( ur_is(cell, UT_DOUBLE) )
            {
                res->coord.n[ len ] = (int16_t) ur_double(cell);
                if( ++len == UR_COORD_MAX )
                    goto set_len;
            }
            else if( ur_is(cell, UT_COORD) )
            {
                int i;
                int blen = cell->coord.len;
                for( i = 0; i < blen; ++i )
                {
                    res->coord.n[ len ] = cell->coord.n[ i ];
                    if( ++len == UR_COORD_MAX )
                        goto set_len;
                }
            }
            else
                break;
        }

        while( len < 2 )
            res->coord.n[ len++ ] = 0;
set_len:
        res->coord.len = len;
        return UR_OK;
    }
    else if( ur_is(from, UT_COORD) )
    {
        *res = *from;
        return UR_OK;
    }
    else if( ur_is(from, UT_VEC3) )
    {
        int i;
        ur_initCoord(res, 3);
        for( i = 0; i < 3; ++i )
            res->coord.n[i] = (int16_t) from->vec3.xyz[i];
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "make coord! expected block!/coord!/vec3!" );
}


int coord_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) != ur_type(b) )
                break;
            // Fall through...

        case UR_COMPARE_SAME:
            if( a->coord.len == b->coord.len )
            {
                const int16_t* pa = a->coord.n;
                const int16_t* aend = pa + a->coord.len;
                const int16_t* pb = b->coord.n;
                while( pa != aend )
                {
                    if( *pa++ != *pb++ )
                        return 0;
                }
                return 1;
            }
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            if( ur_type(a) == ur_type(b) )
            {
                const int16_t* pa = a->coord.n;
                const int16_t* pb = b->coord.n;
                const int16_t* aend = pa +((a->coord.len < b->coord.len) ?
                                        a->coord.len : b->coord.len);
                while( pa != aend )
                {
                    if( *pa > *pb )
                        return 1;
                    if( *pa < *pb )
                        return -1;
                    ++pa;
                    ++pb;
                }
            }
            break;
    }
    return 0;
}


int coord_operate( UThread* ut, const UCell* a, const UCell* b, UCell* res,
                   int op )
{
    UCell tmp;
    const UCell* big;
    int len;
    int i;

    if( ur_is(a, UT_COORD) )
    {
        big = a;
        len = a->coord.len;
        if( ur_is(b, UT_INT) )
        {
            int16_t val = ur_int(b);
            for( i = 0; i < len; ++i )
                tmp.coord.n[ i ] = val;
            b = &tmp;
            goto compute;
        }
        else if( ur_is(b, UT_COORD) )
        {
            if( len > b->coord.len )
                len = b->coord.len;
            else
                big = b;
            goto compute;
        }
    }
    return ur_error( ut, UR_ERR_TYPE, "coord! operator exepected coord!" );

compute:

    ur_setId(res, UT_COORD);
    res->coord.len = big->coord.len;

#define coord_op(OP) res->coord.n[i] = a->coord.n[i] OP b->coord.n[i]

    switch( op )
    {
        case UR_OP_ADD:
            for( i = 0; i < len; ++i )
                coord_op( + );
            break;
        case UR_OP_SUB:
            for( i = 0; i < len; ++i )
                coord_op( - );
            break;
        case UR_OP_MUL:
            for( i = 0; i < len; ++i )
                coord_op( * );
            break;
        case UR_OP_DIV:
            for( i = 0; i < len; ++i )
            {
                if( b->coord.n[i] == 0 )
                    goto div_by_zero;
                coord_op( / );
            }
            break;
        case UR_OP_MOD:
            for( i = 0; i < len; ++i )
            {
                if( b->coord.n[i] == 0 )
                    goto div_by_zero;
                coord_op( % );
            }
            break;
        case UR_OP_AND:
            for( i = 0; i < len; ++i )
                coord_op( & );
            break;
        case UR_OP_OR:
            for( i = 0; i < len; ++i )
                coord_op( | );
            break;
        case UR_OP_XOR:
            for( i = 0; i < len; ++i )
                coord_op( ^ );
            break;
        default:
            return unset_operate( ut, a, b, res, op );
    }

    for( ; i < big->coord.len; ++i )
        res->coord.n[i] = big->coord.n[i];

    return UR_OK;

div_by_zero:

    return ur_error( ut, UR_ERR_SCRIPT, "coord! divide by zero" );
}


/* index is zero-based */
void coord_slice( const UCell* cell, int index, int count, UCell* res )
{
    if( (index < 0) || (index >= cell->coord.len - 1) )
    {
        ur_setId(res, UT_NONE);
    }
    else
    {
        ur_setId(res, UT_COORD);
        if( count < 2 )
            count = 2;
        else if( (index + count) > UR_COORD_MAX )
            count = UR_COORD_MAX - index;
        res->coord.len = count;
        memCpy( res->coord.n, cell->coord.n + index, sizeof(int16_t) * count );
    }
}


/* index is zero-based */
int coord_poke( UThread* ut, UCell* cell, int index, const UCell* src )
{
    if( (index < 0) || (index >= cell->coord.len) )
        return ur_error( ut, UR_ERR_SCRIPT, "poke coord! index out of range" );

    if( ur_is(src, UT_INT) || ur_is(src, UT_CHAR) )
    {
        cell->coord.n[ index ] = ur_int(src);
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "poke coord! expected char!/int!" );
}


/* index is zero-based */
void coord_pick( const UCell* cell, int index, UCell* res )
{
    if( (index < 0) || (index >= cell->coord.len) )
    {
        ur_setId(res, UT_NONE);
    }
    else
    {
        ur_setId(res, UT_INT);
        ur_int(res) = cell->coord.n[ index ];
    }
}


static
const UCell* coord_select( UThread* ut, const UCell* cell, const UCell* sel,
                           UCell* tmp )
{
    if( ur_is(sel, UT_INT) )
    {
        coord_pick( cell, ur_int(sel) - 1, tmp );
        return tmp;
    }
    ur_error( ut, UR_ERR_SCRIPT, "coord select expected int!" );
    return 0;
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
    coord_compare,          coord_operate,          coord_select,
    coord_toString,         coord_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


#ifdef CONFIG_TIMECODE
//----------------------------------------------------------------------------
// UT_TIMECODE


static int timecode_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_TIMECODE) )
    {
        *res = *from;
        return UR_OK;
    }
    if( coord_make( ut, from, res ) )
    {
        int i;
        for( i = res->coord.len; i < 4; ++i )
            res->coord.n[i] = 0;
        ur_type(res) = UT_TIMECODE;
        return UR_OK;
    }
    return UR_THROW;
}


static
void timecode_toString( UThread* ut, const UCell* cell, UBuffer* str,
                        int depth )
{
    char buf[ 14 ];
    char* cp = buf;
    int i = 0;
    int n;
    (void) ut;
    (void) depth;

    while( 1 )
    {
        n = cell->coord.n[i];
        *cp++ = '0' + (n / 10);
        *cp++ = '0' + (n % 10);
        if( ++i == 4 )
            break;
        *cp++ = ':';
    }
    if( cell->coord.flags & UR_FLAG_TIMECODE_DF )
        *cp++ = 'D';
    *cp = '\0';

    ur_strAppendCStr( str, buf );
}


UDatatype dt_timecode =
{
    "timecode!",
    timecode_make,          timecode_make,          unset_copy,
    unset_compare,          unset_operate,          coord_select,
    timecode_toString,      timecode_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


#define isDigit(v)     (('0' <= v) && (v <= '9'))

const char* ur_stringToTimeCode( const char* it, const char* end, UCell* cell )
{
    int c, i, n;
    int16_t* elem = cell->coord.n;

    cell->coord.len = 4;

    if( it != end )
    {
        c = *it;
        if( ! isDigit( c ) )        // Skip any sign (currently ignored).
            ++it;
    }

    for( c = i = n = 0; it != end; ++it )
    {
        c = *it;
        if( isDigit( c ) )
        {
            n = (n * 10) + (c - '0');
        }
        else
        {
            elem[i] = n;
            n = 0;
            if( ++i == 4 || c != ':' )
                break;
        }
    }
    for( ; i < 4; ++i )
    {
        elem[i] = n;
        n = 0;
    }

    if( it != end )
    {
        c = *it;
        if( c == 'D' || c == 'd' )
        {
            cell->coord.flags |= UR_FLAG_TIMECODE_DF;
            ++it;
        }
        else if( c == 'N' || c == 'n' )
        {
            ++it;
        }
    }
    return it;
}
#endif


//EOF
