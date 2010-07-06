/*
  Copyright 2009,2010 Karl Robillard

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
// UT_VECTOR


#include "urlan.h"
#include "urlan_atoms.h"
#include "unset.h"
#include "os.h"


UIndex ur_makeVector( UThread* ut, UAtom type, int size )
{
    UBuffer* buf;
    UIndex bufN;
    int esize;

    switch( type )
    {
        case UR_ATOM_I16:
        case UR_ATOM_U16:
            esize = 2;
            break;

        case UR_ATOM_I32:
        case UR_ATOM_U32:
        case UR_ATOM_F32:
            esize = 4;
            break;

        case UR_ATOM_F64:
            esize = 8;
            break;

        default:
            return UR_INVALID_BUF;
    }

    ur_genBuffers( ut, 1, &bufN );
    buf = ur_buffer( bufN );

    ur_arrInit( buf, esize, size );
    buf->type = UT_VECTOR;
    buf->form = type;

    return bufN;
}


void ur_vecAppend( UBuffer* dest, const UBuffer* src, UIndex start, UIndex end )
{
    int len = end - start;
    if( len > 0 )
    {
        if( dest->form == src->form )
        {
            ur_arrReserve( dest, dest->used + len );
            switch( dest->form )
            {
            case UR_ATOM_I16:
            case UR_ATOM_U16:
                memCpy( dest->ptr.u16 + dest->used, src->ptr.u16 + start,
                        len * sizeof(uint16_t) );
                break;

            case UR_ATOM_I32:
            case UR_ATOM_U32:
            case UR_ATOM_F32:
                memCpy( dest->ptr.u32 + dest->used, src->ptr.u32 + start,
                        len * sizeof(uint32_t) );
                break;

            case UR_ATOM_F64:
                memCpy( dest->ptr.d + dest->used, src->ptr.d + start,
                        len * sizeof(double) );
                break;
            }
            dest->used += len;
        }
    }
}


int vector_make( UThread* ut, const UCell* from, UCell* res )
{
    UAtom atom;
    UIndex bufN;

    if( ! ur_is(from, UT_WORD) )
        return ur_error( ut, UR_ERR_TYPE, "make vector! expected word!" );
    atom = ur_atom(from);

    bufN = ur_makeVector( ut, atom, 0 );
    if( bufN == UR_INVALID_BUF )
        return ur_error( ut, UR_ERR_SCRIPT, "Invalid vector! type %s",
                         ur_atomCStr( ut, atom ) );

    ur_setId(res, UT_VECTOR);
    ur_setSeries(res, bufN, 0);
    return UR_OK;
}


void vector_copy( UThread* ut, const UCell* from, UCell* res )
{
    USeriesIter si;
    UIndex n;

    ur_seriesSlice( ut, &si, from );
    n = ur_makeVector( ut, si.buf->form, 0 );       // Invalidates si.buf.
    ur_vecAppend( ur_buffer(n), ur_bufferSer(from), si.it, si.end );

    ur_setId( res, UT_VECTOR );
    ur_setSeries( res, n, 0 );
}


int vector_convert( UThread* ut, const UCell* from, UCell* res )
{
    UBuffer* buf;
    UIndex bufN;

    if( ur_is(from, UT_INT) )
    {
        bufN = ur_makeVector( ut, UR_ATOM_I32, 1 );
        buf = ur_buffer( bufN );
        buf->ptr.i[ 0 ] = ur_int(from);
init:
        buf->used = 1;
        ur_setId(res, UT_VECTOR);
        ur_setSeries(res, bufN, 0);
        return UR_OK;
    }
    else if( ur_is(from, UT_DECIMAL) )
    {
        bufN = ur_makeVector( ut, UR_ATOM_F32, 1 );
        buf = ur_buffer( bufN );
        buf->ptr.f[ 0 ] = ur_decimal(from);
        goto init;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "convert vector! expected int!/decimal!" );
}


void vector_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    USeriesIter si;
    (void) depth;

    ur_seriesSlice( ut, &si, cell );

    if( (si.buf->form != UR_ATOM_I32) && (si.buf->form != UR_ATOM_F32) )
        ur_strAppendCStr( str, ur_atomCStr( ut, si.buf->form ) );
    ur_strAppendCStr( str, "#[" );

    switch( si.buf->form )
    {
        case UR_ATOM_I16:
            ur_foreach( si )
            {
                ur_strAppendInt( str, si.buf->ptr.i16[ si.it ] ); 
                ur_strAppendChar( str, ' ' );
            }
            break;

        case UR_ATOM_U16:
            ur_foreach( si )
            {
                ur_strAppendInt( str, si.buf->ptr.u16[ si.it ] ); 
                ur_strAppendChar( str, ' ' );
            }
            break;

        case UR_ATOM_I32:
        case UR_ATOM_U32:
            ur_foreach( si )
            {
                ur_strAppendInt( str, si.buf->ptr.i[ si.it ] ); 
                ur_strAppendChar( str, ' ' );
            }
            break;

        case UR_ATOM_F32:
            ur_foreach( si )
            {
                ur_strAppendDouble( str, si.buf->ptr.f[ si.it ] ); 
                ur_strAppendChar( str, ' ' );
            }
            break;

        case UR_ATOM_F64:
            ur_foreach( si )
            {
                ur_strAppendDouble( str, si.buf->ptr.d[ si.it ] ); 
                ur_strAppendChar( str, ' ' );
            }
            break;
    }

    if( ur_strChar( str, -1 ) == ' ' )
        --str->used;
    ur_strAppendChar( str, ']' );
}


/*
void vector_toText( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) depth;
}
*/


void vector_pick( const UBuffer* buf, UIndex n, UCell* res )
{
    if( n > -1 && n < buf->used )
    {
        switch( buf->form )
        {
            case UR_ATOM_I16:
                ur_setId(res, UT_INT);
                ur_int(res) = buf->ptr.i16[ n ];
                break;

            case UR_ATOM_U16:
                ur_setId(res, UT_INT);
                ur_int(res) = buf->ptr.u16[ n ];
                break;

            case UR_ATOM_I32:
            case UR_ATOM_U32:
                ur_setId(res, UT_INT);
                ur_int(res) = buf->ptr.i[ n ];
                break;

            case UR_ATOM_F32:
                ur_setId(res, UT_DECIMAL);
                ur_decimal(res) = buf->ptr.f[ n ];
                break;

            case UR_ATOM_F64:
                ur_setId(res, UT_DECIMAL);
                ur_decimal(res) = buf->ptr.d[ n ];
                break;
        } 
    }
    else
        ur_setId(res, UT_NONE);
}


/*
  Returns number of floats set in fv.
*/
int vector_pickFloatV( const UBuffer* buf, UIndex n, float* fv, int count )
{
    if( (buf->used - n) < count )
        count = buf->used - n;

    switch( buf->form )
    {
        case UR_ATOM_I16:
        case UR_ATOM_U16:
        {
            int16_t* it  = buf->ptr.i16 + n;
            int16_t* end = it + count;
            while( it != end )
                *fv++ = (float) *it++;
        }
            break;

        case UR_ATOM_I32:
        case UR_ATOM_U32:
        {
            int32_t* it  = buf->ptr.i + n;
            int32_t* end = it + count;
            while( it != end )
                *fv++ = (float) *it++;
        }
            break;

        case UR_ATOM_F32:
        {
            float* it  = buf->ptr.f + n;
            float* end = it + count;
            while( it != end )
                *fv++ = *it++;
        }
            break;

        case UR_ATOM_F64:
        {
            double* it  = buf->ptr.d + n;
            double* end = it + count;
            while( it != end )
                *fv++ = (float) *it++;
        }
            break;
    }
    return count;
}


static void vector_pokeInt( UBuffer* buf, UIndex n, int32_t val )
{
    switch( buf->form )
    {
        case UR_ATOM_I16:
        case UR_ATOM_U16:
            buf->ptr.i16[ n ] = val;
            break;

        case UR_ATOM_I32:
        case UR_ATOM_U32:
            buf->ptr.i[ n ] = val;
            break;

        case UR_ATOM_F32:
            buf->ptr.f[ n ] = (float) val;
            break;

        case UR_ATOM_F64:
            buf->ptr.d[ n ] = (double) val;
            break;
    }
}


static void vector_pokeDouble( UBuffer* buf, UIndex n, double val )
{
    switch( buf->form )
    {
        case UR_ATOM_I16:
        case UR_ATOM_U16:
            buf->ptr.i16[ n ] = (int) val;
            break;

        case UR_ATOM_I32:
        case UR_ATOM_U32:
            buf->ptr.i[ n ] = (int) val;
            break;

        case UR_ATOM_F32:
            buf->ptr.f[ n ] = (float) val;
            break;

        case UR_ATOM_F64:
            buf->ptr.d[ n ] = val;
            break;
    }
}


static void vector_pokeFloatV( UBuffer* buf, UIndex n,
                               const float* fv, int count )
{
    if( (buf->used - n) < count )
        count = buf->used - n;

    switch( buf->form )
    {
        case UR_ATOM_I16:
        case UR_ATOM_U16:
        {
            int16_t* it  = buf->ptr.i16 + n;
            int16_t* end = it + count;
            while( it != end )
                *it++ = (int16_t) *fv++;
        }
            break;

        case UR_ATOM_I32:
        case UR_ATOM_U32:
        {
            int32_t* it  = buf->ptr.i + n;
            int32_t* end = it + count;
            while( it != end )
                *it++ = (int32_t) *fv++;
        }
            break;

        case UR_ATOM_F32:
        {
            float* it  = buf->ptr.f + n;
            float* end = it + count;
            while( it != end )
                *it++ = *fv++;
        }
            break;

        case UR_ATOM_F64:
        {
            double* it  = buf->ptr.d + n;
            double* end = it + count;
            while( it != end )
                *it++ = (double) *fv++;
        }
            break;
    }
}


void vector_poke( UBuffer* buf, UIndex n, const UCell* val )
{
    if( n > -1 && n < buf->used )
    {
        if( ur_is(val, UT_DECIMAL) )
            vector_pokeDouble( buf, n, ur_decimal(val) );
        else if( ur_is(val, UT_CHAR) || ur_is(val, UT_INT) )
            vector_pokeInt( buf, n, ur_int(val) );
        else if( ur_is(val, UT_VEC3) )
            vector_pokeFloatV( buf, n, val->vec3.xyz, 3 );
    }
}


static
int vector_select( UThread* ut, const UCell* cell, UBlockIter* bi, UCell* res )
{
    if( ur_is(bi->it, UT_INT) )
    {
        const UBuffer* buf = ur_bufferSer(cell);
        vector_pick( buf, cell->series.it + ur_int(bi->it) - 1, res );
        ++bi->it;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_SCRIPT, "vector select expected int!" );
}


int vector_append( UThread* ut, UBuffer* buf, const UCell* val )
{
    int vt = ur_type(val);
    if( (vt == UT_CHAR) || (vt == UT_INT) )
    {
        ur_arrReserve( buf, buf->used + 1 );
        vector_pokeInt( buf, buf->used++, ur_int(val) );
        return UR_OK;
    }
    else if( vt == UT_DECIMAL )
    {
        ur_arrReserve( buf, buf->used + 1 );
        vector_pokeDouble( buf, buf->used++, ur_decimal(val) );
        return UR_OK;
    }
    else if( vt == UT_VECTOR )
    {
        USeriesIter si;
        ur_seriesSlice( ut, &si, val );
        ur_vecAppend( buf, si.buf, si.it, si.end );
        return UR_OK;
    }
    else if( vt == UT_VEC3 )
    {
        ur_arrReserve( buf, buf->used + 3 );
        buf->used += 3;
        vector_pokeFloatV( buf, buf->used - 3, val->vec3.xyz, 3 );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                 "append vector! expected char!/int!/decimal!/vec3!/vector!" );
}


int vector_change( UThread* ut, USeriesIterM* si, const UCell* val,
                   UIndex part )
{
    (void) ut;
    (void) si;
    (void) val;
    (void) part;
#if 0
    int type = ur_type(val);
    if( type == UT_CHAR || type == UT_INT )
    {
        UBuffer* buf = si->buf;
        if( si->it == buf->used )
            ur_binReserve( buf, ++buf->used );
        buf->ptr.b[ si->it++ ] = ur_int(val);
        if( part > 1 )
            ur_binErase( buf, si->it, part - 1 );
        return UR_OK;
    }
    else if( type == UT_BINARY )
    {
        UBinaryIter ri;
        UBuffer* buf;
        UIndex newUsed;
        int slen;

        ur_binSlice( ut, &ri, val );
        slen = ri.end - ri.it;
        if( slen > 0 )
        {
            buf = si->buf;
            if( part > 0 )
            {
                if( part < slen )
                    ur_binExpand( buf, si->it, slen - part );
                else if( part > slen )
                    ur_binErase( buf, si->it, part - slen );
                newUsed = buf->used;
            }
            else
            {
                newUsed = si->it + slen;
                if( newUsed < buf->used )
                    newUsed = buf->used;
            }

            // TODO: Handle overwritting self when buf is val.

            buf->used = si->it;
            ur_binAppendData( buf, ri.it, slen );
            si->it = buf->used;
            buf->used = newUsed;
        }
        return UR_OK;
    }
#endif
    return ur_error( ut, UR_ERR_TYPE,
                     "change vector! expected char!/int!/decimal!/vector!" );
}


void vector_remove( UThread* ut, USeriesIterM* si, UIndex part )
{
    (void) ut;
    ur_arrErase( si->buf, si->it, (part > 0) ? part : 1 );
}


int vector_find( UThread* ut, const USeriesIter* si, const UCell* val, int opt )
{
    (void) ut;
    (void) si;
    (void) val;
    (void) opt;
#if 0
    const uint8_t* it;
    const UBuffer* buf = si->buf;
    int vt = ur_type(val);

    if( (vt == UT_CHAR) || (vt == UT_INT) )
    {
        it = buf->ptr.b;
        if( opt & UR_FIND_LAST )
            it = find_last_uint8_t( it + si->it, it + si->end, ur_int(val) );
        else
            it = find_uint8_t( it + si->it, it + si->end, ur_int(val) );
        if( it )
            return it - buf->ptr.b;
    }
    else if( vt == UT_DECIMAL )
    {
    }
#endif
    return -1;
}


extern void binary_mark( UThread* ut, UCell* cell );
extern void binary_toShared( UCell* cell );


USeriesType dt_vector =
{
    {
    "vector!",
    vector_make,            vector_convert,         vector_copy,
    unset_compare,          unset_operate,          vector_select,
    vector_toString,        vector_toString,
    unset_recycle,          binary_mark,            ur_arrFree,
    unset_markBuf,          binary_toShared,        unset_bind
    },
    vector_pick,            vector_poke,            vector_append,
    vector_change,          vector_remove,          vector_find
};


//EOF
