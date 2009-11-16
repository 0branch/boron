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


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>


#ifdef TRACK_MALLOC
typedef struct OListNode OListNode;

struct OListNode
{
    OListNode*  prev;
    OListNode*  next;
};

typedef struct
{
    OListNode   head;
    OListNode   tail;
}
OList;

static void _listAppend( OListNode* node, OListNode* after )
{
    node->prev = after;
    node->next = after->next;
    after->next->prev = node;
    after->next = node;
}

static void _listRemove( OListNode* node )
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = node->next = 0;
}


typedef struct
{
    OListNode link;
    void* ptr;
    size_t size;
}
MallocRecord;


static OList _list = { {0,&_list.tail}, {&_list.head,0} };


void* memAlloc( size_t size )
{
    MallocRecord* rec;
    void* ptr = malloc( size );

    if( ptr )
    {
        // Add ptr to list.

        rec = (MallocRecord*) malloc( sizeof(MallocRecord) );
        assert( rec );
        rec->ptr  = ptr;
        rec->size = size;
        _listAppend( &rec->link, &_list.head );
    }

    return ptr;
}


static MallocRecord* memFindRecord( void* ptr )
{
    OListNode* it = _list.head.next;
    while( it->next )
    {
        if( ((MallocRecord*) it)->ptr == ptr )
            return (MallocRecord*) it;
        it = it->next;
    }
    return 0;
}


void* memRealloc( void* ptr, size_t size )
{
    MallocRecord* rec = memFindRecord( ptr );
    if( rec )
    {
        rec->ptr = realloc( ptr, size );
        assert( rec->ptr );
        rec->size = size;
        return rec->ptr;
    }
    else
    {
        printf( "memRealloc - %p was not allocted with memAlloc!\n", ptr );
        return realloc( ptr, size );
    }
}


void memFree( void* ptr )
{
    MallocRecord* rec = memFindRecord( ptr );
    if( rec )
    {
        // Remove ptr from list.
        free( rec->ptr );
        _listRemove( &rec->link );
        free( rec );
    }
    else
    {
        printf( "memFree - %p was not allocted with memAlloc!\n", ptr );
    }
}


void memReport( int verbose )
{
    size_t total;
    int count;
    OListNode* it;
    MallocRecord* rec;

    printf( "memReport:\n" );

    count = total = 0;
    it = _list.head.next;
    while( it->next )
    {
        rec = (MallocRecord*) it;

        total += rec->size;
        ++count;

        if( verbose )
            printf( "  %d bytes at %p\n", (int) rec->size, rec->ptr );

        it = it->next;
    }

    printf( "  %d allocations\n  %d bytes\n", count, (int) total );
}
#endif


//EOF
