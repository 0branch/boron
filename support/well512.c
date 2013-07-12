/*
 * WELL 512 Random number generator
 * See http://www.iro.umontreal.ca/~panneton/WELLRNG.html
 */


#include "well512.h"


void well512_init( Well512* ws, uint32_t seed )
{
    int i;
    uint32_t prev;
    uint32_t* wstate = ws->wstate;

    ws->wi = 0;
    wstate[0] = seed;
    for( i = 1; i < WELL512_STATE_SIZE; ++i )
    {
        prev = wstate[i-1];
        wstate[i] = (1812433253 * (prev ^ (prev >> 30)) + i);
    }
}


/*
  Generates a random number on [0,0xffffffff]-interval
*/
uint32_t well512_genU32( Well512* ws )
{
    uint32_t a, b, c, z0;
    uint32_t* wstate = ws->wstate;
    uint32_t wi = ws->wi;

#define MAT0(v,t)   (v^(v<<t))

    a = wstate[wi];
    c = wstate[(wi + 13) & 15];
    b = MAT0(a,16) ^ MAT0(c,15);

    c = wstate[(wi + 9) & 15];
    c ^= (c>>11);

    wstate[wi] = a = b ^ c;
    ws->wi = wi = (wi + 15) & 15;
    z0 = wstate[wi];
    wstate[wi] = MAT0(z0,2) ^ MAT0(b,18) ^ (c<<28) ^
                 (a ^ ((a << 5) & 0xDA442D24));

    return wstate[wi];
}
