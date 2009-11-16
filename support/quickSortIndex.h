#ifndef QUICKSORTINDEX_H
#define QUICKSORTINDEX_H


#include <stdint.h>


typedef struct QuickSortIndex   QuickSortIndex;
typedef int (*QuickSortFunc)( void* user, void* a, void* b );

struct QuickSortIndex
{
    uint32_t* index;
    uint8_t*  user;
    uint8_t*  data;
    uint32_t  elemSize;
    QuickSortFunc compare;
};


extern int quickSortIndex( const QuickSortIndex*,
                           uint32_t first, uint32_t last, uint32_t stride );


#endif
