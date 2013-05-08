/*
  Copyright 2010,2011 Karl Robillard

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
#include "os.h"


typedef struct
{
    UBuffer atomMap;
    UBuffer bufMap;
    UBuffer ctxAtoms;   // Temporary buffer for ur_ctxWordAtoms().
}
Serializer;


typedef struct
{
    UIndex bufN;
    //UIndex offset;
}
BufferIndex;


enum SeriesRange
{
    SERIES_ALL,
    SERIES_ITER,
    SERIES_SLICE
};


/*
  Add atom to map if it's not present.

  Returns atom index in serialized data.
*/
static int _mapAtom( Serializer* ser, UAtom atom )
{
    UBuffer* map = &ser->atomMap;
    UAtom* it  = map->ptr.u16;
    UAtom* end = it + map->used;

    for( ; it != end; ++it )
    {
        if( *it == atom )
            return it - map->ptr.u16;
    }

    ur_arrReserve( map, map->used + 1 );
    map->ptr.u16[ map->used++ ] = atom;
    return map->used - 1;
}


/*
  Add buffer to map if it's not present.

  Returns buffer index in serialized data.
*/
static int _mapBuffer( Serializer* ser, UIndex bufN )
{
#define mapBegin    ur_ptr(BufferIndex, map)
    UBuffer* map = &ser->bufMap;
    BufferIndex* it  = mapBegin;
    BufferIndex* end = it + map->used;

    for( ; it != end; ++it )
    {
        if( it->bufN == bufN )
            return it - mapBegin;
    }

    ur_arrExpand1( BufferIndex, map, it );
    it->bufN = bufN;
    //it->offset = 0;
    return it - mapBegin;
}


static inline uint32_t _zigZag32( int32_t n )
{
    return (n < 0) ? (((uint32_t)(-n)) << 1) - 1 : ((uint32_t) n) << 1;
}


static inline int32_t _undoZigZag32( uint32_t n )
{
    return (n & 1) ? -(n >> 1) - 1 : n >> 1;
}


static void _pokeU32( uint8_t* bp, uint32_t n )
{
    *bp++ = n >> 24;
    *bp++ = n >> 16;
    *bp++ = n >>  8;
    *bp   = n;
}


static void _pushU32( UBuffer* bin, uint32_t n )
{
    uint8_t* bp = bin->ptr.b + bin->used;

    *bp++ = n >> 24;
    *bp++ = n >> 16;
    *bp++ = n >>  8;
    *bp   = n;

    bin->used += 4;
}


static void _pushU64( UBuffer* bin, uint64_t* src )
{
#ifdef __BIG_ENDIAN__
    uint8_t* bp = bin->ptr.b + bin->used;
    uint8_t* sp = (uint8_t*) src;
    *bp++ = sp[7];
    *bp++ = sp[6];
    *bp++ = sp[5];
    *bp++ = sp[4];
    *bp++ = sp[3];
    *bp++ = sp[2];
    *bp++ = sp[1];
    *bp   = sp[0];
#else
    memCpy( bin->ptr.b + bin->used, src, 8 );
#endif
    bin->used += 8;
}


enum Pack32Count
{
    PACK_1   = 0,
    PACK_2   = 0x40,
    PACK_3   = 0x80,
    PACK_5   = 0xc0,
    PACK_ANY = 0xc0
};


static void _packU32( UBuffer* bin, uint32_t n )
{
    uint8_t* bp = bin->ptr.b + bin->used;
    if( n <= 0x3f )
    {
        *bp++ = n;
    }
    else
    {
        if( n <= 0x3fff )
        {
            *bp++ = PACK_2 | (n >> 8);
        }
        else
        {
            if( n <= 0x3fffff )
            {
                *bp++ = PACK_3 | (n >> 16);
            }
            else
            {
                *bp++ = PACK_5;
                *bp++ = n >> 24;
                *bp++ = n >> 16;
            }
            *bp++ = n >> 8;
        }
        *bp++ = n;
    }
    bin->used = bp - bin->ptr.b;
}


#define push8(N)    bin->ptr.b[ bin->used++ ] = N
#define pushU32(N)  _pushU32( bin, N )
#define packU32(N)  _packU32( bin, N )
#define packS32(N)  _packU32( bin, _zigZag32(N) )

static void _serializeBlock( Serializer* ser, UBuffer* bin, const UBuffer* blk )
{
    UBlockIter bi;

    bi.it  = blk->ptr.cell;
    bi.end = bi.it + blk->used;

    ur_foreach( bi ) 
    {
        int type = ur_type(bi.it);

        ur_binReserve( bin, bin->used + 12 );
        push8( type | ur_flags(bi.it, UR_FLAG_SOL) );

        switch( type )
        {
        default:
        case UT_UNSET:
            break;

        case UT_DATATYPE:
            push8( ur_datatype(bi.it) );
            if( ur_datatype(bi.it) == UT_TYPEMASK )
            {
                pushU32( bi.it->datatype.mask0 );  // High bits often set.
                packU32( bi.it->datatype.mask1 );  // High bits seldom set.
            }
            break;

        //case UT_NONE:

        case UT_LOGIC:
        case UT_CHAR:
            packU32( ur_int(bi.it) );
            break;

        case UT_INT:
            // UR_FLAG_INT_HEX
            packS32( ur_int(bi.it) );
            break;

        case UT_DECIMAL:
        case UT_BIGNUM:
        case UT_TIME:
        case UT_DATE:
            _pushU64( bin, (uint64_t*) &ur_decimal(bi.it) );
            break;

        case UT_COORD:
            push8( bi.it->coord.len );
        {
            const int16_t* np = bi.it->coord.n;
            const int16_t* nend = np + bi.it->coord.len;
            while( np != nend )
                packS32( *np++ );
        }
            break;

        case UT_VEC3:
        {
            uint32_t* np = (uint32_t*) bi.it->vec3.xyz;
            packU32( np[0] );
            packU32( np[1] );
            packU32( np[2] );
        }
            break;

#ifdef CONFIG_TIMECODE
        case UT_TIMECODE:
            push8( ur_flags(bi.it, UR_FLAG_TIMECODE_DF) );
        {
            const int16_t* np = bi.it->coord.n;
            const int16_t* nend = np + 4;
            while( np != nend )
                packS32( *np++ );
        }
            break;
#endif

        case UT_WORD:
        case UT_LITWORD:
        case UT_SETWORD:
        case UT_GETWORD:
        case UT_OPTION:
            //printf( "KR word.ctx %d\n", bi.it->word.ctx );
            switch( ur_binding(bi.it) )
            {
                case UR_BIND_THREAD:
                case UR_BIND_ENV:
                    // Avoiding global contexts (BUF_THREAD_CTX) for now.
                    if( bi.it->word.ctx > 1 || bi.it->word.ctx < -1 )
                    {
                        push8( UR_BIND_THREAD /*ur_binding(bi.it)*/ );
                        packU32( _mapBuffer( ser, bi.it->word.ctx ) );
                        packU32( bi.it->word.index );
                        break;
                    }
                    // Fall through...

                default:
                    push8( UR_BIND_UNBOUND );
                    break;
            }
            packU32( _mapAtom( ser, ur_atom(bi.it) ) );
            break;

        case UT_BINARY:
        case UT_BITSET:
        case UT_STRING:
        case UT_FILE:
        case UT_VECTOR:
        case UT_BLOCK:
        case UT_PAREN:
        case UT_PATH:
        case UT_LITPATH:
        case UT_SETPATH:
            packU32( _mapBuffer( ser, bi.it->series.buf ) );
            if( ur_isSliced( bi.it ) )
            {
                push8( SERIES_SLICE );
                packU32( bi.it->series.it );
                packU32( bi.it->series.end );
            }
            else if( bi.it->series.it > 0 )
            {
                push8( SERIES_ITER );
                packU32( bi.it->series.it );
            }
            else
            {
                push8( SERIES_ALL );
            }
            break;

        case UT_CONTEXT:
            packU32( _mapBuffer( ser, bi.it->context.buf ) );
            break;

        case UT_ERROR:
            break;
        }
    }
}


#ifdef __BIG_ENDIAN__
static void _binAppendSwap2( UBuffer* bin, uint16_t* it, int count )
{
    uint8_t* dest;
    uint16_t* end = it + count;
    int size = count * 2;

    ur_binReserve( bin, bin->used + size );
    dest = bin->ptr.b + bin->used;
    bin->used += size;
    while( it != end )
    {
        *dest++ = *it >> 8;
        *dest++ = *it++;
    }
}


static void _binAppendSwap4( UBuffer* bin, uint32_t* it, int count )
{
    uint8_t* dest;
    uint32_t* end = it + count;
    int size = count * 4;

    ur_binReserve( bin, bin->used + size );
    dest = bin->ptr.b + bin->used;
    bin->used += size;
    while( it != end )
    {
        *dest++ = *it >> 24;
        *dest++ = *it >> 16;
        *dest++ = *it >> 8;
        *dest++ = *it++;
    }
}


static void _memCpySwap2(uint8_t* dest, const uint8_t* src, uint32_t elemCount)
{
    while( elemCount-- )
    {
        *dest++ = src[1];
        *dest++ = src[0];
        src += 2;
    }
}


static void _memCpySwap4(uint8_t* dest, const uint8_t* src, uint32_t elemCount)
{
    while( elemCount-- )
    {
        *dest++ = src[3];
        *dest++ = src[2];
        *dest++ = src[1];
        *dest++ = src[0];
        src += 4;
    }
}
#endif


/**
  Serialize block.

  \param  blkN  Index to valid block buffer.
  \param  res   Cell to be set to new output binary.

  \return UR_OK/UR_THROW
*/
int ur_serialize( UThread* ut, UIndex blkN, UCell* res )
{
    Serializer ser;
    UBuffer* bin;
    int ok = UR_OK;

    ur_arrInit( &ser.atomMap, sizeof(UAtom), 0 );
    ur_arrInit( &ser.bufMap, sizeof(BufferIndex), 0 );
    ur_arrInit( &ser.ctxAtoms, sizeof(UAtom), 0 );

    bin = ur_makeBinaryCell( ut, 256, res );
    ur_binAppendData( bin, (const uint8_t*) "BOR1", 4 );
    _pushU32( bin, 0 );     // Reserve atoms offset.
    _pushU32( bin, 0 );     // Reserve buffer count.
    _mapBuffer( &ser, blkN );

    {
        const BufferIndex* it;
        const UBuffer* buf;
        int i;

        for( i = 0; i < ser.bufMap.used; ++i )
        {
            it = ((BufferIndex*) ser.bufMap.ptr.v) + i;
            buf = ur_bufferE( it->bufN );

            ur_binReserve( bin, bin->used + 7 );

            switch( buf->type )
            {
            case UT_BINARY:
            case UT_BITSET:
                push8( buf->type );
                packU32( buf->used );
                if( buf->used )
                    ur_binAppendData( bin, buf->ptr.b, buf->used );
                break;

            case UT_STRING:
            case UT_FILE:
            case UT_VECTOR:
                push8( buf->type );
                push8( buf->form );
                packU32( buf->used );
                if( buf->used )
                {
#ifdef __BIG_ENDIAN__
                    switch( buf->elemSize )
                    {
                        case 2:
                            _binAppendSwap2( bin, buf->ptr.u16, buf->used );
                            break;
                        case 4:
                            _binAppendSwap4( bin, buf->ptr.u32, buf->used );
                            break;
                        default:
                            ur_binAppendData( bin, buf->ptr.b,
                                              buf->elemSize * buf->used );
                            break;
                    }
#else
                    ur_binAppendData( bin, buf->ptr.b,
                                      buf->elemSize * buf->used );
#endif
                }
                break;

            case UT_BLOCK:
            case UT_PAREN:
            case UT_PATH:
            case UT_LITPATH:
            case UT_SETPATH:
                push8( buf->type );
                packU32( buf->used );
                if( buf->used )
                    _serializeBlock( &ser, bin, buf );
                break;

            case UT_CONTEXT:
                push8( buf->type );
                packU32( buf->used );
                if( buf->used )
                {
                    // Words
                    int ai;
                    ur_binReserve( bin, bin->used + (buf->used * 3) );
                    ur_arrReserve( &ser.ctxAtoms, buf->used );
                    ur_ctxWordAtoms( buf, ser.ctxAtoms.ptr.u16 );
                    for( ai = 0; ai < buf->used; ++ai )
                        packU32( _mapAtom( &ser, ser.ctxAtoms.ptr.u16[ai] ) );

                    // Values
                    _serializeBlock( &ser, bin, buf );
                }
                break;

            default:
                ok = ur_error( ut, UR_ERR_SCRIPT,
                        "Invalid serialized buffer type (%d)", buf->type );
                goto cleanup;
            }
        }
    }

    if( ser.atomMap.used )
    {
        const UAtom* it  = ser.atomMap.ptr.u16;
        const UAtom* end = it + ser.atomMap.used;
        const char* str;
#define replaceLast(B,C)  B->ptr.b[ B->used - 1 ] = C

        _pokeU32( bin->ptr.b + 4, bin->used );      // Atoms offset.
        while( it != end )
        {
            str = ur_atomCStr( ut, *it++ );
            ur_binAppendData( bin, (const uint8_t*) str, strLen(str) + 1 );
            replaceLast( bin, ' ' );
        }
        replaceLast( bin, '\0' );
    }

    _pokeU32( bin->ptr.b + 8, ser.bufMap.used );    // Buffer count.

cleanup:

    ur_arrFree( &ser.atomMap );
    ur_arrFree( &ser.bufMap );
    ur_arrFree( &ser.ctxAtoms );
    return ok;
}


/*--------------------------------------------------------------------------*/


typedef struct
{
    const uint8_t* it;
    const uint8_t* end;
}
BinaryIter;


static uint32_t _pullU32( BinaryIter* bi )
{
    uint32_t n;
    n = (bi->it[0] << 24) | (bi->it[1] << 16) | (bi->it[2] << 8) | bi->it[3];
    bi->it += 4;
    return n;
}


static void _pullU64( BinaryIter* bi, uint64_t* dest )
{
#ifdef __BIG_ENDIAN__
    const uint8_t* bp = bi->it;
    uint8_t* dp = (uint8_t*) dest;
    *dp++ = bp[7];
    *dp++ = bp[6];
    *dp++ = bp[5];
    *dp++ = bp[4];
    *dp++ = bp[3];
    *dp++ = bp[2];
    *dp++ = bp[1];
    *dp   = bp[0];
#else
    memCpy( dest, bi->it, 8 );
#endif
    bi->it += 8;
}


static uint32_t _unpackU32( BinaryIter* bi )
{
    uint32_t n = bi->it[0];
    int pack = n & PACK_ANY;
    if( pack )
    {
        if( pack == PACK_2 )
        {
            n = ((n & 0x3f) << 8) | bi->it[1];
            bi->it += 2;
        }
        else if( pack == PACK_3 )
        {
            n = ((n & 0x3f) << 16) | (bi->it[1] << 8) | bi->it[2];
            bi->it += 3;
        }
        else
        {
            n = (bi->it[1] << 24) | (bi->it[2] << 16) |
                (bi->it[3] <<  8) |  bi->it[4];
            bi->it += 5;
        }
    }
    else
    {
        ++bi->it;
    }
    return n;
}


#define pull8(N)        N = *bi->it++
#define pullU32(N)      N = _pullU32(bi)
#define unpackU32(N)    N = _unpackU32(bi)
#define unpackS32(N)    N = _undoZigZag32( _unpackU32(bi) )

/*
  Returns non-zero if successful
*/
static int _unserializeBlock( UAtom* atoms, UIndex* ids,
                              BinaryIter* bi, UBuffer* blk )
{
    UCell* cell = blk->ptr.cell;
    int n;
    int type;
    int i;

    for( i = 0; i < blk->used; ++i, ++cell )
    {
        if( bi->it >= bi->end )
            return 0;

        n = *bi->it++;
        type = n & 0x7f;
        if( type > UT_ERROR )
            return 0;

        ur_setId( cell, type );
        if( n & 0x80 )
            ur_setFlags( cell, UR_FLAG_SOL );

        switch( type )
        {
        case UT_UNSET:
            break;

        case UT_DATATYPE:
            pull8( ur_datatype(cell) );
            if( ur_datatype(cell) == UT_TYPEMASK )
            {
                pullU32( cell->datatype.mask0 );
                unpackU32( cell->datatype.mask1 );
            }
            break;

        case UT_NONE:
            break;

        case UT_LOGIC:
        case UT_CHAR:
            unpackU32( ur_int(cell) );
            break;
            
        case UT_INT:
            unpackS32( ur_int(cell) );
            break;

        case UT_DECIMAL:
        case UT_BIGNUM:
        case UT_TIME:
        case UT_DATE:
            _pullU64( bi, (uint64_t*) &ur_decimal(cell) );
            break;

        case UT_COORD:
            pull8( cell->coord.len );
        {
            int16_t* np = cell->coord.n;
            int16_t* nend = np + cell->coord.len;
            while( np != nend )
                unpackS32( *np++ );
        }
            break;

        case UT_VEC3:
        {
            uint32_t* np = (uint32_t*) cell->vec3.xyz;
            unpackU32( np[0] );
            unpackU32( np[1] );
            unpackU32( np[2] );
        }
            break;

#ifdef CONFIG_TIMECODE
        case UT_TIMECODE:
            pull8( n );
            if( n )
                ur_setFlags(cell, UR_FLAG_TIMECODE_DF );
        {
            int16_t* np = cell->coord.n;
            int16_t* nend = np + 4;
            while( np != nend )
                unpackS32( *np++ );
        }
            break;
#endif

        case UT_WORD:
        case UT_LITWORD:
        case UT_SETWORD:
        case UT_GETWORD:
        case UT_OPTION:
            pull8( n );
            if( n == UR_BIND_THREAD )
            {
                ur_binding(cell) = UR_BIND_THREAD;
                unpackU32( n );
                cell->word.ctx = ids[ n ];
                unpackU32( cell->word.index );
            }
            else
            {
                ur_binding(cell) = UR_BIND_UNBOUND;
                cell->word.ctx = UR_INVALID_BUF;
            }
            unpackU32( n );
            ur_atom(cell) = atoms[ n ];
            break;

        case UT_BINARY:
        case UT_BITSET:
        case UT_STRING:
        case UT_FILE:
        case UT_VECTOR:
        case UT_BLOCK:
        case UT_PAREN:
        case UT_PATH:
        case UT_LITPATH:
        case UT_SETPATH:
            unpackU32( n );
            ur_setSeries( cell, ids[ n ], 0 );

            pull8( n );
            if( n > SERIES_ALL )
            {
                if( n > SERIES_SLICE )
                    return 0;
                unpackU32( cell->series.it );
                if( n == SERIES_SLICE )
                    unpackU32( cell->series.end );
            }
            break;

        case UT_CONTEXT:
            unpackU32( n );
            ur_setSeries( cell, ids[ n ], 0 );
            break;

        case UT_ERROR:
            break;

        default:
            return 0;
        }
    }
    return 1;
}


int ur_serializedHeader( const uint8_t* data, int len )
{
    if( len > 12 )
    {
        return data[0] == 'B' && data[1] == 'O' && data[2] == 'R' &&
               data[3] == '1' && data[12] == UT_BLOCK;
    }
    return 0;
}


/*
  Unserialize binary.

  \param  start     Pointer to serialized binary.
  \param  end       Pointer to end of binary.
  \param  res       Cell to be set to new output block.

  \return UR_OK/UR_THROW
*/
int ur_unserialize( UThread* ut, const uint8_t* start, const uint8_t* end,
                    UCell* res )
{
    BinaryIter bi;
    UBuffer atoms;
    UBuffer ids;
    UBuffer* buf;
    int i;
    int n;
    int type;
    int used;
    int ok = UR_OK;


    if( ! ur_serializedHeader( start, end - start ) )
        return ur_error( ut, UR_ERR_SCRIPT, "Invalid serialized data header" );

    bi.it  = start + 4;
    bi.end = end;
    n = _pullU32(&bi);
    ur_arrInit( &atoms, sizeof(UAtom), n );
    if( n )
    {
        start += n;
        ur_internAtoms( ut, (const char*) start, atoms.ptr.u16 ); 
        bi.end = start;
    }

    n = _pullU32(&bi);
    ur_arrInit( &ids, sizeof(UIndex), n );
    ur_genBuffers( ut, n, ids.ptr.i );

    for( i = 0; i < n; ++i )
    {
        if( bi.it >= bi.end )
        {
            ur_error( ut, UR_ERR_SCRIPT, "Unexpected end of serialized data" );
            goto fail;
        }

        type = *bi.it++;

        switch( type )
        {
        case UT_BINARY:
        case UT_BITSET:
            used = _unpackU32(&bi);

            buf = ur_buffer( ids.ptr.i[ i ] );
            ur_binInit( buf, used );
            if( type != UT_BINARY )
                buf->type = type;
            if( used )
            {
                ur_binAppendData( buf, bi.it, used );
                bi.it += used;
            }
            break;

        case UT_STRING:
        case UT_FILE:
        {
            int form = *bi.it++;
            used = _unpackU32(&bi);

            buf = ur_buffer( ids.ptr.i[ i ] );
            ur_strInit( buf, form, used );
            if( used )
            {
                buf->used = used;
                used *= buf->elemSize;
                memCpy( buf->ptr.v, bi.it, used );
                bi.it += used;
            }
        }
            break;

        case UT_VECTOR:
        {
            int form = *bi.it++;
            used = _unpackU32(&bi);

            buf = ur_buffer( ids.ptr.i[ i ] );
            ur_vecInit( buf, form, 0, used );
            if( used )
            {
                buf->used = used;
#ifdef __BIG_ENDIAN__
                switch( buf->elemSize )
                {
                    case 2:
                        _memCpySwap2( buf->ptr.b, bi.it, used );
                        used *= 2;
                        break;
                    case 4:
                        _memCpySwap4( buf->ptr.b, bi.it, used );
                        used *= 4;
                        break;
                    default:
                        used *= buf->elemSize;
                        memCpy( buf->ptr.v, bi.it, used );
                        break;
                }
#else
                used *= buf->elemSize;
                memCpy( buf->ptr.v, bi.it, used );
#endif
                bi.it += used;
            }
        }
            break;

        case UT_BLOCK:
        case UT_PAREN:
        case UT_PATH:
        case UT_LITPATH:
        case UT_SETPATH:
            used = _unpackU32(&bi);

            buf = ur_buffer( ids.ptr.i[ i ] );
            ur_blkInit( buf, type, used );
            if( used )
            {
unser_block:
                buf->used = used;
                if( ! _unserializeBlock( atoms.ptr.u16, ids.ptr.i, &bi, buf ) )
                {
                    buf->used = 0;
                    ur_error( ut, UR_ERR_SCRIPT, "Invalid serialized block" );
                    goto fail;
                }
            }
            break;

        case UT_CONTEXT:
            used = _unpackU32(&bi);

            buf = ur_buffer( ids.ptr.i[ i ] );
            ur_ctxInit( buf, used );
            if( used )
            {
#define ENTRIES(buf)    ((UAtomEntry*) (buf->ptr.cell + ur_avail(buf)))
                int ai;
                uint32_t an;
                UAtomEntry* ent = ENTRIES(buf);

                for( ai = 0; ai < used; ++ai, ++ent )
                {
                    an = _unpackU32(&bi);
                    ent->atom  = atoms.ptr.u16[ an ];
                    ent->index = ai;
                }

                ur_ctxSort( buf );
                goto unser_block;
            }
            break;

        default:
            ur_error( ut, UR_ERR_SCRIPT,
                      "Invalid serialized buffer type (%d)", type );
            goto fail;
        }
    }

    ur_setId( res, UT_BLOCK );
    ur_setSeries( res, ids.ptr.i[0], 0 );
    goto cleanup;

fail:

    // Initialize any unset buffers to something.
    for( ; i < n; ++i )
        ur_binInit( ur_buffer( ids.ptr.i[ i ] ), 0 );
    ok = UR_THROW;

cleanup:

    ur_arrFree( &atoms );
    ur_arrFree( &ids );
    return ok;
}


//EOF
