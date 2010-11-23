/* Quick Sort of Index */


//#include <stdio.h>
#include "quickSortIndex.h"


//#define DEBUG_SORT  1

#define INSERT_SIZE 10
#define isGT(a,b)   (qs->compare(qs->user, a, b) > 0)
#define isLT(a,b)   (qs->compare(qs->user, a, b) < 0)
#define VALUE(n)    (data + (index[n] * qs->elemSize))

#define swap(a,b) \
    tmp = index[a]; \
    index[a] = index[b]; \
    index[b] = tmp


static void qsortIndex( const QuickSortIndex* qs, uint32_t l, uint32_t r )
{
    uint8_t* pivot;
    uint8_t* data = qs->data;
    uint32_t* index = qs->index;
    uint32_t i, j;
    uint32_t p;
    uint32_t tmp;

#ifdef DEBUG_SORT
    printf( "qsortIndex [" );
    for( i = l; i <= r; ++i )
        printf( " %d", index[i] );
    printf( "]\n" );
#endif

    if( (l + INSERT_SIZE) > r )
    {
#ifdef DEBUG_SORT
        printf( "insertion\n" );
#endif
        // Insertion sorting on small series.
        for( p = l + 1; p <= r; p++ )
        {
            tmp = index[p];
            pivot = data + (tmp * qs->elemSize);
            for( j = p; j > l; j-- )
            {
                if( isGT( pivot, VALUE(j - 1) ) )
                    break;
                index[j] = index[j - 1];
            }
            index[j] = tmp;
        }
    }
    else
    {
        // Select pivot using median-of-three.
        p = (l + r) / 2;
#ifdef DEBUG_SORT
        printf( "median %d %d %d\n", l, p, r );
#endif
        if( isGT( VALUE(l), VALUE(p) ) )
        {
            swap( l, p );
        }
        if( isGT( VALUE(l), VALUE(r) ) )
        {
            swap( l, r );
        }
        if( isGT( VALUE(p), VALUE(r) ) )
        {
            swap( p, r );
        }

        // Move pivot to end.
        swap( p, r );
        pivot = VALUE(r);

        i = l;
        j = r - 1;
        while( 1 )
        {
            while( i < j && isLT( VALUE(i), pivot ) )
                ++i;
            while( i < j && isGT( VALUE(j), pivot ) )
                --j;
            if( i >= j )
                break;
            swap( i, j );
            ++i;
        }

        // Restore pivot.
        if( isGT( VALUE(i), pivot ) )
        {
            swap( i, r );
        }

        if( l < i )
            qsortIndex( qs, l, i - 1 );
        if( r > i )
            qsortIndex( qs, i + 1, r );
    }
}


/*
  Sort an index of elements (rather than the elements themselves).

  The QuickSortIndex struct must be filled by the caller.

  The index must point to an array of which is large enough to hold
  ((end - begin) / stride) uint32_t values.

  The compare callback must return 1, 0, or -1 if the first argument is
  greater than, equal to, or less than the second argument.

  \param begin   First index.
  \param end     Ending index (not included in the sort).
  \param stride  Index increment.

  \return  Number of indicies set and sorted in qs->index.
*/
int quickSortIndex( const QuickSortIndex* qs, uint32_t begin, uint32_t end,
                    uint32_t stride )
{
    uint32_t icount;
    uint32_t* ip = qs->index;

    icount = (end - begin) / stride;
    if( icount < 2 )
    {
        if( icount == 1 )
            *ip = begin;
        else
            icount = 0;
    }
    else
    {
        for( ; begin < end; begin += stride )
            *ip++ = begin;
        qsortIndex( qs, 0, icount - 1 ); 
    }
    return icount;
}


/*EOF*/
