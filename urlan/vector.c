/*
  Copyright 2009-2012 Karl Robillard

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
#include "mem_util.h"


static int ur_vecFormElemSize( UAtom form )
{
    // NOTE: UrlanVectorType will not be atom values in the future.
    switch( form )
    {
        case UR_ATOM_I16:
        case UR_ATOM_U16:
            return 2;

        case UR_ATOM_I32:
        case UR_ATOM_U32:
        case UR_ATOM_F32:
            return 4;

        case UR_ATOM_F64:
            return 8;

        default:
            return 0;
    }
}


/**
  Initialize buffer to type UT_VECTOR.

  \param type       Normally an UrlanVectorType (UT_VEC_I32, UR_VEC_F32, etc.).
  \param elemSize   Byte size of each element.  May be zero if type is an
                    UrlanVectorType.
  \param size       Number of elements to reserve.
*/
void ur_vecInit( UBuffer* buf, int type, int elemSize, int size )
{
    if( ! elemSize )
        elemSize = ur_vecFormElemSize( type );

    ur_arrInit( buf, elemSize, size );
    buf->type = UT_VECTOR;
    buf->form = type;
}


/**
  Generate a single vector buffer.

  If you need multiple buffers then ur_genBuffers() should be used.

  The caller must create a UCell for this block in a held block before the
  next ur_recycle() or else it will be garbage collected.

  \param type   Element type (UT_VEC_U16, UT_VEC_I32, UT_VEC_F32, etc.).
  \param size   Number of elements to reserve.

  \return  Buffer id of block.
*/
UIndex ur_makeVector( UThread* ut, enum UrlanVectorType type, int size )
{
    UIndex bufN;
    UBuffer* buf = ur_genBuffers( ut, 1, &bufN );
    ur_vecInit( buf, type, ur_vecFormElemSize( type ), size );
    return bufN;
}


/**
  Generate a single vector and set cell to reference it.

  \param type   Element type (UT_VEC_U16, UT_VEC_I32, UT_VEC_F32, etc.).
  \param size   Number of elements to reserve.
  \param cell   Cell to initialize.

  \return  Pointer to vector buffer.
*/
UBuffer* ur_makeVectorCell( UThread* ut, enum UrlanVectorType type, int size,
                            UCell* cell )
{
    UBuffer* buf;
    UIndex bufN;

    buf = ur_genBuffers( ut, 1, &bufN );
    ur_vecInit( buf, type, ur_vecFormElemSize( type ), size );

    ur_initSeries( cell, UT_VECTOR, bufN );

    return buf;
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
            case UR_VEC_I16:
            case UR_VEC_U16:
                memCpy( dest->ptr.u16 + dest->used, src->ptr.u16 + start,
                        len * sizeof(uint16_t) );
                break;

            case UR_VEC_I32:
            case UR_VEC_U32:
            case UR_VEC_F32:
                memCpy( dest->ptr.u32 + dest->used, src->ptr.u32 + start,
                        len * sizeof(uint32_t) );
                break;

            case UR_VEC_F64:
                memCpy( dest->ptr.d + dest->used, src->ptr.d + start,
                        len * sizeof(double) );
                break;
            }
            dest->used += len;
        }
    }
}


static int vector_make( UThread* ut, const UCell* from, UCell* res )
{
    UAtom atom;
    int esize;

    if( ! ur_is(from, UT_WORD) )
        return ur_error( ut, UR_ERR_TYPE, "make vector! expected word!" );

    // NOTE: UrlanVectorType will not be atom values in the future.
    atom = ur_atom(from);
    esize = ur_vecFormElemSize( atom );
    if( ! esize )
        return ur_error( ut, UR_ERR_SCRIPT, "Invalid vector! type %s",
                         ur_atomCStr( ut, atom ) );

    ur_makeVectorCell( ut, atom, 0, res );
    return UR_OK;
}


void vector_copy( UThread* ut, const UCell* from, UCell* res )
{
    USeriesIter si;
    UBuffer* buf;

    ur_seriesSlice( ut, &si, from );
    buf = ur_makeVectorCell( ut, si.buf->form, 0, res ); // Invalidates si.buf.
    ur_vecAppend( buf, ur_bufferSer(from), si.it, si.end );
}


int vector_convert( UThread* ut, const UCell* from, UCell* res )
{
    UBuffer* buf;

    if( ur_is(from, UT_INT) )
    {
        buf = ur_makeVectorCell( ut, UR_VEC_I32, 1, res );
        buf->ptr.i[ 0 ] = ur_int(from);
init:
        buf->used = 1;
        return UR_OK;
    }
    else if( ur_is(from, UT_DECIMAL) )
    {
        buf = ur_makeVectorCell( ut, UR_VEC_F32, 1, res );
        buf->ptr.f[ 0 ] = ur_decimal(from);
        goto init;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "convert vector! expected int!/decimal!" );
}


int vector_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_SAME:
            return ((a->series.buf == b->series.buf) &&
                    (a->series.it == b->series.it) &&
                    (a->series.end == b->series.end));

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) != ur_type(b) )
                break;
            if( (a->series.buf == b->series.buf) &&
                (a->series.it == b->series.it) &&
                (a->series.end == b->series.end) )
                return 1;
            {
            USeriesIter ai;
            USeriesIter bi;
            int size;

            ur_seriesSlice( ut, &ai, a );
            ur_seriesSlice( ut, &bi, b );
            size = ai.end - ai.it;

            if( (size == (bi.end - bi.it)) && (ai.buf->form == bi.buf->form) )
            {
                if( memcmp( ai.buf->ptr.b + (ai.it * ai.buf->elemSize),
                            bi.buf->ptr.b + (bi.it * bi.buf->elemSize),
                           size * ai.buf->elemSize ) == 0 )
                    return 1;
            }
            }
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            break;
    }
    return 0;
}


void vector_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    USeriesIter si;
    (void) depth;

    ur_seriesSlice( ut, &si, cell );

    if( (si.buf->form != UR_VEC_I32) && (si.buf->form != UR_VEC_F32) )
        ur_strAppendCStr( str, ur_atomCStr( ut, si.buf->form ) );
    ur_strAppendCStr( str, "#[" );

    switch( si.buf->form )
    {
        case UR_VEC_I16:
            ur_foreach( si )
            {
                ur_strAppendInt( str, si.buf->ptr.i16[ si.it ] ); 
                ur_strAppendChar( str, ' ' );
            }
            break;

        case UR_VEC_U16:
            ur_foreach( si )
            {
                ur_strAppendInt( str, si.buf->ptr.u16[ si.it ] ); 
                ur_strAppendChar( str, ' ' );
            }
            break;

        case UR_VEC_I32:
        case UR_VEC_U32:
            ur_foreach( si )
            {
                ur_strAppendInt( str, si.buf->ptr.i[ si.it ] ); 
                ur_strAppendChar( str, ' ' );
            }
            break;

        case UR_VEC_F32:
            ur_foreach( si )
            {
                ur_strAppendFloat( str, si.buf->ptr.f[ si.it ] );
                ur_strAppendChar( str, ' ' );
            }
            break;

        case UR_VEC_F64:
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
            case UR_VEC_I16:
                ur_setId(res, UT_INT);
                ur_int(res) = buf->ptr.i16[ n ];
                break;

            case UR_VEC_U16:
                ur_setId(res, UT_INT);
                ur_int(res) = buf->ptr.u16[ n ];
                break;

            case UR_VEC_I32:
            case UR_VEC_U32:
                ur_setId(res, UT_INT);
                ur_int(res) = buf->ptr.i[ n ];
                break;

            case UR_VEC_F32:
                ur_setId(res, UT_DECIMAL);
                ur_decimal(res) = buf->ptr.f[ n ];
                break;

            case UR_VEC_F64:
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
        case UR_VEC_I16:
        case UR_VEC_U16:
        {
            int16_t* it  = buf->ptr.i16 + n;
            int16_t* end = it + count;
            while( it != end )
                *fv++ = (float) *it++;
        }
            break;

        case UR_VEC_I32:
        case UR_VEC_U32:
        {
            int32_t* it  = buf->ptr.i + n;
            int32_t* end = it + count;
            while( it != end )
                *fv++ = (float) *it++;
        }
            break;

        case UR_VEC_F32:
        {
            float* it  = buf->ptr.f + n;
            float* end = it + count;
            while( it != end )
                *fv++ = *it++;
        }
            break;

        case UR_VEC_F64:
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
        case UR_VEC_I16:
        case UR_VEC_U16:
            buf->ptr.i16[ n ] = val;
            break;

        case UR_VEC_I32:
        case UR_VEC_U32:
            buf->ptr.i[ n ] = val;
            break;

        case UR_VEC_F32:
            buf->ptr.f[ n ] = (float) val;
            break;

        case UR_VEC_F64:
            buf->ptr.d[ n ] = (double) val;
            break;
    }
}


static void vector_pokeDouble( UBuffer* buf, UIndex n, double val )
{
    switch( buf->form )
    {
        case UR_VEC_I16:
        case UR_VEC_U16:
            buf->ptr.i16[ n ] = (int) val;
            break;

        case UR_VEC_I32:
        case UR_VEC_U32:
            buf->ptr.i[ n ] = (int) val;
            break;

        case UR_VEC_F32:
            buf->ptr.f[ n ] = (float) val;
            break;

        case UR_VEC_F64:
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
        case UR_VEC_I16:
        case UR_VEC_U16:
        {
            int16_t* it  = buf->ptr.i16 + n;
            int16_t* end = it + count;
            while( it != end )
                *it++ = (int16_t) *fv++;
        }
            break;

        case UR_VEC_I32:
        case UR_VEC_U32:
        {
            int32_t* it  = buf->ptr.i + n;
            int32_t* end = it + count;
            while( it != end )
                *it++ = (int32_t) *fv++;
        }
            break;

        case UR_VEC_F32:
        {
            float* it  = buf->ptr.f + n;
            float* end = it + count;
            while( it != end )
                *it++ = *fv++;
        }
            break;

        case UR_VEC_F64:
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
const UCell* vector_select( UThread* ut, const UCell* cell, const UCell* sel,
                            UCell* tmp )
{
    if( ur_is(sel, UT_INT) )
    {
        const UBuffer* buf = ur_bufferSer(cell);
        vector_pick( buf, cell->series.it + ur_int(sel) - 1, tmp );
        return tmp;
    }
    ur_error( ut, UR_ERR_SCRIPT, "vector select expected int!" );
    return 0;
}


int vector_append( UThread* ut, UBuffer* buf, const UCell* val )
{
    switch( ur_type(val) )
    {
        case UT_CHAR:
        case UT_INT:
            ur_arrReserve( buf, buf->used + 1 );
            vector_pokeInt( buf, buf->used++, ur_int(val) );
            break;

        case UT_DECIMAL:
            ur_arrReserve( buf, buf->used + 1 );
            vector_pokeDouble( buf, buf->used++, ur_decimal(val) );
            break;

        case UT_VEC3:
            ur_arrReserve( buf, buf->used + 3 );
            buf->used += 3;
            vector_pokeFloatV( buf, buf->used - 3, val->vec3.xyz, 3 );
            break;

        case UT_BINARY:
        {
            UBuffer src;
            USeriesIter si;

            ur_seriesSlice( ut, &si, val );

            // Append binary data using ur_vecAppend() and dummy vector! src.
            // TODO: Handle si.it when it's not aligned to elemSize.
            ur_vecInit( &src, buf->form, 0, 0 );
            src.ptr.v = si.buf->ptr.b + si.it;
            ur_vecAppend( buf, &src, 0, (si.end - si.it) / buf->elemSize );
        }
            break;

        case UT_VECTOR:
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, val );
            ur_vecAppend( buf, si.buf, si.it, si.end );
        }
            break;

        case UT_BLOCK:
        {
            UBlockIter bi;
            ur_blkSlice( ut, &bi, val );
            ur_foreach( bi )
            {
                if( ! vector_append( ut, buf, bi.it ) )
                    return UR_THROW;
            }
        }
            break;

        default:
            return ur_error( ut, UR_ERR_TYPE,
                 "append vector! expected char!/int!/decimal!/vec3!/binary!/vector!" );
    }
    return UR_OK;
}


int vector_insert( UThread* ut, UBuffer* buf, UIndex index,
                   const UCell* val, UIndex part )
{
    (void) index;
    (void) part;

    switch( ur_type(val) )
    {
        case UT_CHAR:
        case UT_INT:
            ur_arrReserve( buf, buf->used + 1 );
            vector_pokeInt( buf, buf->used++, ur_int(val) );
            break;

        case UT_DECIMAL:
            ur_arrReserve( buf, buf->used + 1 );
            vector_pokeDouble( buf, buf->used++, ur_decimal(val) );
            break;

        case UT_VEC3:
            ur_arrReserve( buf, buf->used + 3 );
            buf->used += 3;
            vector_pokeFloatV( buf, buf->used - 3, val->vec3.xyz, 3 );
            break;

        case UT_VECTOR:
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, val );
            ur_vecAppend( buf, si.buf, si.it, si.end );
        }
            break;
#if 0
        case UT_BLOCK:
        {
            UBlockIter bi;
            ur_blkSlice( ut, &bi, val );
            ur_foreach( bi )
            {
                if( ! vector_insert( ut, buf, 0, bi.it, part ) )
                    return UR_THROW;
            }
        }
            break;
#endif
        default:
            return ur_error( ut, UR_ERR_TYPE,
                 "insert vector! expected char!/int!/decimal!/vec3!/vector!" );
    }
    return UR_OK;
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


void vector_reverse( const USeriesIterM* si )
{
    const UBuffer* buf = si->buf;
    switch( buf->elemSize )
    {
        case 2:
            reverse_uint16_t( buf->ptr.u16 + si->it, buf->ptr.u16 + si->end );
            break;
        case 4:
            reverse_uint32_t( buf->ptr.u32 + si->it, buf->ptr.u32 + si->end );
            break;
        case 8:
            assert( 0 && buf->elemSize );
            break;
    }
}


static int intValue( const UCell* cell, uint32_t* n )
{
    int type = ur_type(cell);
    if( (type == UT_INT) || (type == UT_CHAR) )
    {
        *n = ur_int(cell);
        return 1;
    }
    if( type == UT_DECIMAL )
    {
        *n = (uint32_t) ur_decimal(cell);
        return 1;
    }
    return 0;
}


/*
static int floatValue( const UCell* cell, double* n )
{
    int type = ur_type(cell);
    if( (type == UT_INT) || (type == UT_CHAR) )
    {
        *n = (double) ur_int(cell);
        return 1;
    }
    if( type == UT_DECIMAL )
    {
        *n = ur_decimal(cell);
        return 1;
    }
    return 0;
}
*/


int vector_find( UThread* ut, const USeriesIter* si, const UCell* val, int opt )
{
    const UBuffer* buf = si->buf;
    (void) ut;

    switch( buf->form )
    {
        case UR_VEC_I16:
        case UR_VEC_U16:
        {
            const uint16_t* it = buf->ptr.u16;
            uint32_t n;
            if( ! intValue( val, &n ) )
                return -1;
            if( opt & UR_FIND_LAST )
                it = find_last_uint16_t( it + si->it, it + si->end, n );
            else
                it = find_uint16_t( it + si->it, it + si->end, n );
            if( it )
                return it - buf->ptr.u16;
        }
            break;

        case UR_VEC_I32:
        case UR_VEC_U32:
        {
            const uint32_t* it = buf->ptr.u32;
            uint32_t n;
            if( ! intValue( val, &n ) )
                return -1;
            if( opt & UR_FIND_LAST )
                it = find_last_uint32_t( it + si->it, it + si->end, n );
            else
                it = find_uint32_t( it + si->it, it + si->end, n );
            if( it )
                return it - buf->ptr.u32;
        }
            break;
#if 0
        // Need to comapare floats with tolerance?
        case UR_VEC_F32:
        {
            const float* it = buf->ptr.f;
            double n;
            if( ! floatValue( val, &n ) )
                return -1;
            if( opt & UR_FIND_LAST )
                it = find_last_float( it + si->it, it + si->end, n );
            else
                it = find_float( it + si->it, it + si->end, n );
            if( it )
                return it - buf->ptr.f;
        }
            break;

        case UR_VEC_F64:
        {
            const double* it = buf->ptr.d;
            double n;
            if( ! floatValue( val, &n ) )
                return -1;
            if( opt & UR_FIND_LAST )
                it = find_last_double( it + si->it, it + si->end, n );
            else
                it = find_double( it + si->it, it + si->end, n );
            if( it )
                return it - buf->ptr.d;
        }
            break;
#endif
    }
    return -1;
}


extern void binary_mark( UThread* ut, UCell* cell );
extern void binary_toShared( UCell* cell );


USeriesType dt_vector =
{
    {
    "vector!",
    vector_make,            vector_convert,         vector_copy,
    vector_compare,         unset_operate,          vector_select,
    vector_toString,        vector_toString,
    unset_recycle,          binary_mark,            ur_arrFree,
    unset_markBuf,          binary_toShared,        unset_bind
    },
    vector_pick,            vector_poke,            vector_append,
    vector_insert,          vector_change,          vector_remove,
    vector_reverse,         vector_find
};


//EOF
