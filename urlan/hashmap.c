/*
  Copyright 2011-2016 Karl Robillard

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
/*
  UBuffer members:
    type        UT_HASHMAP
    elemSize    Unused
    form        Unused
    flags       Unused
    used        Number of slots in table
    ptr.v       Hash table
*/


#include "urlan.h"
#include "os.h"
#include "unset.h"

extern void block_markBuf( UThread*, UBuffer* );


typedef int32_t     MapIndex;   // LIMIT: 2,147,483,647 entries per hash-map.

typedef struct
{
    uint32_t key;               // Zero means entry is unused.
    MapIndex valueIndex;
    MapIndex next;              // Always odd or zero (end of list).
}
MapEntry;


#define FREE_VAL(buf)   *(buf->ptr.i32)
#define ENTRIES(buf)    (((MapEntry*) (buf->ptr.v)) + 1)

#define FREE_LIST_END   -1
#define MIN_MAP_SIZE    8
#define SCAN_RANGE      7


static inline int powerOfTwo( int size )
{
    int p2 = MIN_MAP_SIZE;
    while( p2 < size )
        p2 <<= 1;
    return p2;
}


void ur_mapAlloc( UBuffer* map, int size )
{
    if( size > 0 )
    {
        int p2 = powerOfTwo( size );
        int bsize = sizeof(MapEntry) + (p2 * sizeof(MapEntry) * 2);

        map->used  = p2;
        map->ptr.v = memAlloc( bsize );
        memSet( map->ptr.v, 0, bsize );
        FREE_VAL(map) = FREE_LIST_END;
    }
    else
    {
        map->used  = 0;
        map->ptr.v = 0;
    }
}


void ur_mapInit( UBuffer* map, int size )
{
    *((int32_t*) map) = 0;
    map->type = UT_HASHMAP;
    ur_mapAlloc( map, size );
}


/**
  Free hash-map data.

  map->ptr and map->used are set to zero.
*/
void ur_mapFree( UBuffer* map )
{
    if( map->ptr.v )
    {
        memFree( map->ptr.v );
        map->ptr.v = 0;
    }
    map->used = 0;
}


void ur_mapInsert( UBuffer* map, uint32_t hashKey, MapIndex valueIndex );

void ur_mapResize( UBuffer* map, int size )
{
    MapEntry* it;
    MapEntry* end;
    MapEntry* oldTable;
    int oldSize;


    if( map->used >= size )
        return;

    oldTable = ENTRIES(map);
    oldSize  = map->used;

    ur_mapAlloc( map, size );

    if( oldTable )
    {
        it  = oldTable;
        end = it + oldSize * 2;
        while( it != end )
        {
            if( it->key )
                ur_mapInsert( map, it->key, it->valueIndex );
            ++it;
        }
        memFree( oldTable - 1 );
    }
}


/**
  \param map        Initialized hash-map buffer.
  \param hashKey    Key to find.

  \return  Value index or -1 if not found.
*/
int ur_mapLookup( const UBuffer* map, uint32_t hashKey )
{
    MapEntry* table = ENTRIES(map);
    MapEntry* it;

    it = table + ((hashKey & (map->used - 1)) << 1);
    if( it->key )
    {
        while( 1 )
        {
            if( it->key == hashKey )
                return it->valueIndex;
            if( ! it->next )
                break;
            it = table + it->next;
        }
    }
    return -1;
}


/**
  If the hashKey already exists then the entry for that key is overwritten.

  \param map            Initialized hash-map buffer.
  \param hashKey
  \param valueIndex
*/
void ur_mapInsert( UBuffer* map, uint32_t hashKey, MapIndex valueIndex )
{
    MapEntry* table;
    MapEntry* it;
    MapEntry* end;
    MapEntry* head;
    MapEntry* tail;


    if( ! map->used )
        ur_mapResize( map, 1 );

    while( 1 )
    {
        table = ENTRIES(map);
        it = table + ((hashKey & (map->used - 1)) << 1);
        if( ! it->key )
            goto fill_entry;

        // Look for existing key.
        head = it;
        while( 1 )
        {
            if( it->key == hashKey )
            {
                it->valueIndex = valueIndex;
                return;
            }
            if( ! it->next )
            {
                tail = it;
                break;
            }
            it = table + it->next;
        }

        // Look for an unused list entry (odd entries) near head.
        it  = head + 1;
        end = table + (map->used << 1);
        if( end - it < SCAN_RANGE * 2 )
            it = table + 1;
        end = it + SCAN_RANGE * 2;

        while( it != end )
        {
            if( ! it->key )
            {
                tail->next = it - table;
                goto fill_entry;
            }
            it += 2;
        }

        // No empty entry found so increase map size.
        ur_mapResize( map, map->used * 2 );
    }

fill_entry:

    it->key = hashKey;
    it->valueIndex = valueIndex;
    it->next = 0;
}


/**
  \param map        Initialized hash-map buffer.
  \param hashKey

  \return valueIndex removed or -1 if key was not found.
*/
int ur_mapRemove( UBuffer* map, uint32_t hashKey )
{
    MapEntry* it;
    MapEntry* np;
    MapEntry* table;


    if( ! map->used )
        return -1;

    table = ENTRIES(map);
    it = table + ((hashKey & (map->used - 1)) << 1);
    if( it->key )
    {
        if( it->key == hashKey )
        {
            // Save valueIndex since it may be overwritten below.
            int index = it->valueIndex;
            if( it->next )
            {
                // Transfer list node to head entry.
                np = table + it->next;
                *it = *np;
                np->key = 0;
            }
            else
            {
                it->key = 0;
            }
            return index;
        }
        else
        {
            while( it->next )
            {
                np = it;
                it = table + it->next;
                if( it->key == hashKey )
                {
                    it->key = 0;
                    np->next = it->next;
                    return it->valueIndex;
                }
            }
        }
    }
    return -1;
}


//----------------------------------------------------------------------------

/*
   FIXME: This implementation assumes there will be no hash collisions.

   NOTE: ur_bufferSerM is used to get map UBuffer since it works on
         series.buf and checks ur_isShared() for us.  The map and value
         buffers must always both be in the shared environment or not.
*/

#define ur_hashMapBuf(c)    (c)->series.buf
#define ur_hashValBuf(c)    (c)->series.it

#define hashmap_badKeyError(cell) \
    ur_error(ut,UR_ERR_TYPE,"Invalid hash-map! key (%s)", \
             ur_atomCStr(ut,ur_type(cell)))


#define HASH_DATA(func,DT) \
static uint32_t func( const DT* it, const DT* end, uint32_t hash ) { \
    uint32_t c; \
    while( it != end ) { \
        c = *it++; \
        hash = (33 * hash) + 720 + c; \
    } \
    return hash; \
}


HASH_DATA( ur_hashData, uint8_t )
HASH_DATA( ur_hashData16, uint16_t )


uint32_t ur_hashCell( UThread* ut, const UCell* val )
{
    const uint8_t* a;
    const uint8_t* b;

    switch( ur_type(val) )
    {
        case UT_UNSET:
        case UT_DATATYPE:
        case UT_NONE:
        case UT_LOGIC:
            return 0;

        case UT_CHAR:
        case UT_INT:
            a = (uint8_t*) &ur_int(val);
            b = a + sizeof(int64_t);
            goto hash_mem;

        case UT_DOUBLE:
        case UT_TIME:
        case UT_DATE:
            a = (uint8_t*) &ur_double(val);
            b = a + sizeof(double);
            goto hash_mem;

        case UT_COORD:
            a = (uint8_t*) val->coord.n;
            b = a + (sizeof(int16_t) * val->coord.len);
            goto hash_mem;

        case UT_VEC3:
            a = (uint8_t*) val->vec3.xyz;
            b = a + (sizeof(float) * 3);
            goto hash_mem;

        case UT_TIMECODE:
            return 0;

        case UT_WORD:
        case UT_LITWORD:
        case UT_SETWORD:
        case UT_GETWORD:
        case UT_OPTION:
        {
            a = (uint8_t*) ur_atomCStr( ut, ur_atom(val) );
            for( b = a; *b; ++b )
                ;
        }
            goto hash_mem;

        case UT_BINARY:
        {
            UBinaryIter bi;
            ur_binSlice( ut, &bi, val );
            a = bi.it;
            b = bi.end;
        }
            goto hash_mem;

        case UT_STRING:
        case UT_FILE:
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, val );
            if( ur_strIsUcs2( si.buf ) )
            {
                return ur_hashData16( si.buf->ptr.u16 + si.it,
                                      si.buf->ptr.u16 + si.end, ur_type(val) );
            }
            a = si.buf->ptr.b + si.it;
            b = si.buf->ptr.b + si.end;
        }
            goto hash_mem;

        case UT_VECTOR:

        case UT_BLOCK:
        case UT_PAREN:
        case UT_PATH:
        case UT_LITPATH:
        case UT_SETPATH:

        case UT_BITSET:
        case UT_CONTEXT:
        case UT_HASHMAP:
        case UT_ERROR:
        default:
            return 0;
    }
    return 0;

hash_mem:

    return ur_hashData( a, b, ur_type(val) );
}


static UBuffer* _makeHashMap( UThread* ut, int size, UCell* res )
{
    UIndex bufN[2];
    UBuffer* map;

    map = ur_genBuffers( ut, 2, bufN );
    ur_mapInit( map, size );
    ur_blkInit( ur_buffer( bufN[1] ), UT_BLOCK, size * 2 );

    ur_setId( res, UT_HASHMAP );
    ur_hashMapBuf(res) = bufN[0];
    ur_hashValBuf(res) = bufN[1];

    return map;
}


UStatus hashmap_insert( UThread* ut, const UCell* mapC, const UCell* keyC,
                        const UCell* valueC )
{
    UBuffer* map;
    UBuffer* blk;
    uint32_t key;
    int i;
    MapIndex fn;

    map = ur_bufferSerM(mapC);
    if( ! map )
        return UR_THROW;

    key = ur_hashCell( ut, keyC );
    if( ! key )
        return hashmap_badKeyError(keyC);

    blk = ur_buffer( ur_hashValBuf(mapC) );
    i = ur_mapLookup( map, key );
    if( i > -1 )
    {
        UCell* cell = blk->ptr.cell + (i << 1);
        *cell++ = *keyC;
        *cell   = *valueC;
    }
    else
    {
        fn = FREE_VAL(map);
        if( fn == FREE_LIST_END )
        {
            ur_mapInsert( map, key, blk->used >> 1 );
            ur_blkPush( blk, keyC );
            ur_blkPush( blk, valueC );
        }
        else
        {
            UCell* cell = blk->ptr.cell + (fn << 1);
            FREE_VAL(map) = ur_int(cell);
            *cell++ = *keyC;
            *cell   = *valueC;
        }
    }
    return UR_OK;
}


static int hashmap_make( UThread* ut, const UCell* from, UCell* res )
{
    int n;
    if( ur_is(from, UT_INT) )
    {
        n = ur_int(from);
        _makeHashMap( ut, (n < 0) ? 0 : n, res );
        return UR_OK;
    }
    if( ur_is(from, UT_BLOCK) )
    {
        UBlockIt bi;
        UBuffer* map;
        UBuffer* blk;
        uint32_t key;
        uint16_t index = 0;

        ur_blockIt( ut, &bi, from );

        n = bi.end - bi.it;
        if( n & 1 )
            --bi.end;
        map = _makeHashMap( ut, n / 2, res );   // gc!
        blk = ur_buffer( ur_hashValBuf(res) );

        ur_foreach( bi )
        {
            key = ur_hashCell( ut, bi.it );
            if( ! key )
                return hashmap_badKeyError(bi.it);

            ur_mapInsert( map, key, index++ );
            ur_blkPush( blk, bi.it++ );
            ur_blkPush( blk, bi.it );
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "make hash-map! expected int!/block!" );
}


void hashmap_copy( UThread* ut, const UCell* from, UCell* res )
{
    UBuffer* buf2;
    const UBuffer* buf1;

    buf2 = _makeHashMap( ut, 0, res );          // gc!
    buf1 = ur_bufferE( ur_hashMapBuf(from) );
    ur_mapResize( buf2, buf1->used );           // Does unwanted memSet.
    FREE_VAL(buf2) = FREE_VAL(buf1);
    memCpy( ENTRIES(buf2), ENTRIES(buf1), buf1->used * sizeof(MapEntry) * 2 );

    buf2 = ur_buffer( ur_hashValBuf(res) );
    buf1 = ur_bufferE( ur_hashValBuf(from) );
    ur_blkAppendCells( buf2, buf1->ptr.cell, buf1->used );
}


int hashmap_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_SAME:
            if( ur_hashMapBuf(a) == ur_hashMapBuf(b) )
                return 1;
            break;

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) != ur_type(b) )
                break;
            if( ur_hashMapBuf(a) == ur_hashMapBuf(b) )
                return 1;
            // TODO: Compare keys & values.
            break;
    }
    return 0;
}


const UCell* hashmap_select( UThread* ut, const UCell* cell, const UCell* sel,
                             UCell* tmp )
{
    const UBuffer* map = ur_bufferE( ur_hashMapBuf(cell) );
    uint32_t key;
    int idx;

    if( map->used )
    {
        key = ur_hashCell( ut, sel );
        if( ! key )
        {
            hashmap_badKeyError(sel);
            return 0;
        }
        idx = ur_mapLookup( map, key );
        if( idx > -1 )
        {
            map = ur_bufferE( ur_hashValBuf(cell) );
            idx = (idx << 1) + 1;
            assert( idx < map->used );
            return map->ptr.cell + idx;
        }
    }

    ur_setId( tmp, UT_NONE );
    return tmp;
}


void hashmap_mark( UThread* ut, UCell* cell )
{
    UIndex n;

    n = ur_hashMapBuf(cell);
    if( ! ur_isShared(n) )
        ur_markBuffer( ut, n );

    n = ur_hashValBuf(cell);
    if( ! ur_isShared(n) )
    {
        if( ur_markBuffer( ut, n ) )
            block_markBuf( ut, ur_buffer(n) );
    }
}


void hashmap_toShared( UCell* cell )
{
    ur_hashMapBuf(cell) = -ur_hashMapBuf(cell);
    ur_hashValBuf(cell) = -ur_hashValBuf(cell);
}


UStatus hashmap_remove( UThread* ut, const UCell* mapC, const UCell* keyC )
{
    UBuffer* map;
    UBuffer* blk;
    UCell* cell;
    uint32_t key;
    int i;

    map = ur_bufferSerM(mapC);
    if( ! map )
        return UR_THROW;

    key = ur_hashCell( ut, keyC );
    if( ! key )
        return hashmap_badKeyError(keyC);

    i = ur_mapRemove( map, key );
    if( i > -1 )
    {
        // Unset key and point the head of the free list to it.

        blk = ur_buffer( ur_hashValBuf(mapC) );
        cell = blk->ptr.cell + (i << 1);
        ur_setId( cell, UT_UNSET );
        ur_int(cell) = FREE_VAL(map);
        FREE_VAL(map) = i;

        ur_setId( cell + 1, UT_NONE );      // Set value to none!.
    }
    return UR_OK;
}


void hashmap_clear( UThread* ut, UCell* mapC )
{
    if( ! ur_isShared( ur_hashMapBuf(mapC) ) )
    {
        ur_mapFree( ur_buffer( ur_hashMapBuf(mapC) ) );
        ur_blkFree( ur_buffer( ur_hashValBuf(mapC) ) );
    }
}


#define ur_blkReserve   ur_arrReserve

/*
  Append hash-map values to block.
*/
void hashmap_values( UThread* ut, const UCell* mapC, UBuffer* blk )
{
    UBlockIter bi;

    bi.buf = ur_bufferE( ur_hashValBuf(mapC) );
    bi.it  = bi.buf->ptr.cell;
    bi.end = bi.it + bi.buf->used;

    ur_blkReserve( blk, blk->used + bi.buf->used );
    for( ; bi.it != bi.end; bi.it += 2 )
    {
        if( ! ur_is(bi.it, UT_UNSET) )
            blk->ptr.cell[ blk->used++ ] = bi.it[1];
    }
}


static void hashmap_print( UThread* ut, const UCell* mapC, UBuffer* str,
                           int depth )
{
    UBlockIter bi;
    const UCell* val;
    const UDatatype** dt = ut->types;

    bi.buf = ur_bufferE( ur_hashValBuf(mapC) );
    bi.it  = bi.buf->ptr.cell;
    bi.end = bi.it + bi.buf->used;

    for( ; bi.it != bi.end; bi.it += 2 )
    {
        if( ! ur_is(bi.it, UT_UNSET) )
        {
            ur_strAppendIndent( str, depth );
            dt[ ur_type(bi.it) ]->toString( ut, bi.it, str, depth );
            ur_strAppendChar( str, ' ' );
            val = bi.it + 1;
            dt[ ur_type(val) ]->toString( ut, val, str, depth );
            ur_strAppendChar( str, '\n' );
        }
    }
}


/*
  If depth is -1 then the make words and braces will be omitted.
*/
void hashmap_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    if( depth < 0 )
    {
        hashmap_print( ut, cell, str, 0 );
    }
    else
    {
        ur_strAppendCStr( str, "make hash-map! [\n" );
        hashmap_print( ut, cell, str, depth + 1 );
        ur_strAppendIndent( str, depth );
        ur_strAppendCStr( str, "]" );
    }
}


void hashmap_toText( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    hashmap_print( ut, cell, str, depth );
}


//----------------------------------------------------------------------------


void ur_mapInitV( UThread* ut, UBuffer* map, const UBuffer* valueBlk )
{
    UIndex size = valueBlk->used & ~1;
    const UCell* it  = valueBlk->ptr.cell;
    const UCell* end = it + size;
    uint32_t key;
    uint16_t index = 0;

    ur_mapInit( map, size >> 1 );

    for( ; it != end; it += 2 )
    {
        key = ur_hashCell( ut, it );
        if( key )
            ur_mapInsert( map, key, index++ );
    }
}


UDatatype dt_hashmap =
{
    "hash-map!",
    hashmap_make,           hashmap_make,           hashmap_copy,
    hashmap_compare,        unset_operate,          hashmap_select,
    hashmap_toString,       hashmap_toText,
    unset_recycle,          hashmap_mark,           ur_mapFree,
    unset_markBuf,          hashmap_toShared,       unset_bind
};


//EOF
