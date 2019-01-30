/*
  Copyright 2009,2011 Karl Robillard

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


#define KEEP_CASE   1
#define MAX_WORD_LEN    64
#define LOWERCASE(c)    if(c >= 'A' && c <= 'Z') c -= 'A' - 'a'


typedef struct
{
    uint32_t hash;
    uint16_t nameIndex;     // Index into atomNames.ptr.c
    uint16_t nameLen;

    uint16_t head;
    uint16_t chain;
}
AtomRec;


/**
  \ingroup urlan_core

  Get name of atom.

  \param atom  Valid atom.

  \return Null terminated string.
*/
const char* ur_atomCStr( UThread* ut, UAtom atom /*, int* plen*/ )
{
    UEnv* env = ut->env;
    AtomRec* rec = ((AtomRec*) env->atomTable.ptr.v) + atom;
    //if( plen )
    //    *plen = rec->nameLen;
    return env->atomNames.ptr.c + rec->nameIndex;
}


uint32_t ur_hash( const uint8_t* str, const uint8_t* end )
{
    int c;
    uint32_t hashval = 0;

    while( str != end )
    {
        c = *str++;
        LOWERCASE( c );
        hashval = (33 * hashval) + 720 + c;
    }

    return hashval;
}


/*
  Example atom hash table with five used and eight avail slots.
  The names some, wordH, & wordP all hash to the same slot and must be chained.

      hash       name  | head chain
      -----------------|-----------
  0 [ 0x3D24461D word1 |  2         ]
  1 [ 0x01D81D74 some  |       3    ]
  2 [ 0x01D88B98 this  |            ]
  3 [ 0x3D244654 wordH |       4    ]
  4 [ 0x3D24465C wordP |  1         ]
    [                  |  0         ]
    [                  |            ]
    [                  |            ]
*/

#define HASH_INSERT( atoms, avail, table, node, hash, index ) \
    node = table + (hash % avail); \
    if( node->head == 0xffff ) \
        node->head = index; \
    else { \
        node = table + node->head; \
        while( node->chain != 0xffff ) \
            node = table + node->chain; \
        node->chain = index; \
    }


static void _rebuildAtomHash( UBuffer* buf )
{
    AtomRec* table;
    AtomRec* node;
    AtomRec* it;
    AtomRec* end;
    UIndex avail;

    table = ur_ptr(AtomRec, buf);
    avail = ur_avail(buf);

    // Clear lookup table.

    it  = table;
    end = table + avail;
    while( it != end )
    {
        it->head  = 0xffff;
        it->chain = 0xffff;
        ++it;
    }

    // Re-insert existing entries.

    it  = table;
    end = table + buf->used;
    while( it != end )
    {
        HASH_INSERT( buf, avail, table, node, it->hash, it - table )
        ++it;
    }
}


/*
  This must be called inside LOCK_GLOBAL/UNLOCK_GLOBAL.

  \param ut     If non-zero, then ur_error() is called when the atom tables
                are full.

  \return UR_INVALID_ATOM if atom tables are full.
*/
static UAtom _internAtom( UThread* ut, UBuffer* atoms, UBuffer* names,
                          const uint8_t* str, const uint8_t* end )
{
    uint8_t* cp;
    const uint8_t* it;
    const uint8_t* sp;
    int c, d;
    int len;
    uint32_t hash;
    UIndex   avail;
    AtomRec* table;
    AtomRec* node;

#if 0
    uint8_t rep[32];
    cp = rep;
    sp = str;
    while( sp != end )
        *cp++ = *sp++;
    *cp = '\0';
    printf( "KR intern {%s}\n", rep );
#endif

    len = end - str;
    if( len > MAX_WORD_LEN )        /* LIMIT: Maximum word length */
    {
        len = MAX_WORD_LEN;
        end = str + len;
    }
    hash = ur_hash( str, end );

    table = ur_ptr(AtomRec, atoms);
    avail = ur_avail(atoms);

    node = table + (hash % avail);
    if( node->head == 0xffff )
    {
        node->head = atoms->used;
    }
    else
    {
        node = table + node->head;
        while( 1 )
        {
            if( node->nameLen == len )
            {
                sp = names->ptr.b + node->nameIndex;
                it = str;
                while( it != end )
                {
#ifdef KEEP_CASE
                    c = *sp++;
                    d = *it++;
                    if( c == d )
                        continue;
                    LOWERCASE( c );
                    LOWERCASE( d );
                    if( c != d )
                        break;
#else
                    if( *sp++ != *it++ )
                        break;
#endif
                }
                if( it == end )
                    goto done;
            }

            if( node->chain == 0xffff )
            {
                node->chain = atoms->used;
                break;
            }
            node = table + node->chain;
        }
    }

    // Nope, add new atom.

    if( atoms->used == avail )
    {
        // Atom table size is fixed so read only access does not need to be
        // locked.  When the table is full, we are finished.
        if( ut )
            ur_error( ut, UR_ERR_INTERNAL, "Atom table is full" );
        return UR_INVALID_ATOM;
    }
    node = table + atoms->used;
    ++atoms->used;

    node->hash      = hash;
    node->nameIndex = names->used;
    node->nameLen   = len;

#if 1
    if( (names->used + len + 1) > ur_avail(names) )
    {
        if( ut )
            ur_error( ut, UR_ERR_INTERNAL, "Atom name buffer is full" );
        return UR_INVALID_ATOM;
    }
#else
    ur_arrayReserve( names, sizeof(char), names->used + len + 1 );
#endif

    cp = names->ptr.b + names->used;
    names->used += len + 1;
    while( str != end )
    {
#ifdef KEEP_CASE
        *cp++ = *str++;
#else
        c = *str++;
        LOWERCASE( c );
        *cp++ = c;
#endif
    }
    *cp = '\0';

done:

    return node - table;
}


#ifdef DEBUG
void dumpAtoms( UThread* ut )
{
    UEnv* env = ut->env;

    LOCK_GLOBAL
    {
    const char* names = env->atomNames.ptr.c;
    AtomRec* table = (AtomRec*) env->atomTable.ptr.v;
    AtomRec* it  = table;
    AtomRec* end = table + env->atomTable.used;

#if __WORDSIZE == 64
#define OFFINT  "%4ld"
#else
#define OFFINT  "%4d"
#endif

    while( it != end )
    {
        dprint( OFFINT " %08x %5d %5d %s\n", it - table, it->hash,
                it->head, it->chain,
                names + it->nameIndex );
        ++it;
    }
    }
    UNLOCK_GLOBAL
}
#endif


/*EOF*/
