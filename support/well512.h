#ifndef WELL512_H
#define WELL512_H

#ifdef __sun__
#include <inttypes.h>
#else
#include <stdint.h>
#endif

#define WELL512_STATE_SIZE  16

typedef struct
{
    uint32_t wi;
    uint32_t wstate[ WELL512_STATE_SIZE ];
}
Well512;


#ifdef __cplusplus
extern "C" {
#endif

void well512_init( Well512* ws, uint32_t seed );
uint32_t well512_genU32( Well512* ws );

#ifdef __cplusplus
}
#endif


/*
   Generate a random number on [0,1)-real-interval.
   (uint32_t divided by 2^32)
*/
#define well512_genReal(ws)    (well512_genU32(ws) * (1.0 / 4294967296.0))


#endif /* WELL512_H */
