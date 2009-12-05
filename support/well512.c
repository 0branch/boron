/*
 * WELL 512 Random number generator
 * See http://www.iro.umontreal.ca/~panneton/WELLRNG.html
 */

#ifdef __sun__
#include <inttypes.h>
#else
#include <stdint.h>
#endif

#define STATE_SIZE  16

static uint32_t wstate[ STATE_SIZE ];
static uint32_t wi = 0;


void init_genrand( uint32_t seed )
{
    int i;
    uint32_t prev;

    wi = 0;
    wstate[0] = seed;
    for( i = 1; i < STATE_SIZE; ++i )
    {
        prev = wstate[i-1];
        wstate[i] = (1812433253 * (prev ^ (prev >> 30)) + i);
    }
}


#if 0
void init_genrand16( const uint32_t* seed )
{
    uint32_t* dst = wstate;
    uint32_t* end = seed + STATE_SIZE;
    while( seed != end )
        *dst++ = *seed++;
}


void save_genrand16( uint32_t* dst )
{
    uint32_t* it  = wstate;
    uint32_t* end = it + STATE_SIZE;
    while( it != end )
        *dst++ = *it++;
}
#endif


#define MAT0(v,t)   (v^(v<<t))

/* generates a random number on [0,0xffffffff]-interval */
uint32_t genrand_int32()
{
    uint32_t a, b, c, z0;

    a = wstate[wi];
    c = wstate[(wi + 13) & 15];
    b = MAT0(a,16) ^ MAT0(c,15);

    c = wstate[(wi + 9) & 15];
    c ^= (c>>11);

    wstate[wi] = a = b ^ c;
    wi = (wi + 15) & 15;
    z0 = wstate[wi];
    wstate[wi] = MAT0(z0,2) ^ MAT0(b,18) ^ (c<<28) ^
                 (a ^ ((a << 5) & 0xDA442D24));

    return wstate[wi];
}


/* generates a random number on [0,1)-real-interval */
double genrand_real2()
{
    return genrand_int32() * (1.0 / 4294967296.0);  /* divided by 2^32 */
}


