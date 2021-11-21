/*
  Copyright 2009-2010,2013 Karl Robillard

  This file is part of the Boron programming language.

  Boron is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Boron is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Boron.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <time.h>
#include "boron.h"
#include "boron_internal.h"


#define genrand_int32()     well512_genU32( &BT->rand )
#define genrand_real2()     well512_genReal( &BT->rand )


static unsigned long _clockSeed()
{
    unsigned long seed;
    seed  = time(NULL);
    seed += clock();
    return seed;
}


/**
    Seed the thread RNG.

    \ingroup boron
*/
void boron_randomSeed( UThread* ut, uint32_t seed )
{
    well512_init( &BT->rand, seed );
}


/**
    Get the next number from the thread RNG.

    \return Value from 0 to 0xffffffff.
    \ingroup boron
*/
uint32_t boron_random( UThread* ut )
{
    return genrand_int32();
}


/*-cf-
    random
        data    logic!/int!/double!/coord!/vec3! or series.
        /seed   Use data as generator seed.
    return: Random number, series position, or seed if /seed option used.
    group: data

    If data is a number, then a number from 1 through data will be returned.

    A call to random/seed must be done before random values will be generated.
    If seed data is is not an int! then a clock-based seed is used.
*/
CFUNC_PUB(cfunc_random)
{
#define OPT_RANDOM_SEED     1
    int type = ur_type(a1);

    if( CFUNC_OPTIONS & OPT_RANDOM_SEED )
    {
        ur_setId(res, UT_INT);
        ur_int(res) = (type == UT_INT) ? ur_int(a1) : (int32_t) _clockSeed();
        well512_init( &BT->rand, (uint32_t) ur_int(res) );
        return UR_OK;
    }

    if( ur_isSeriesType(type) )
    {
        int len = boron_seriesEnd( ut, a1 );
        *res = *a1;
        if( len > 0 )
            res->series.it += genrand_int32() % (len - a1->series.it);
        return UR_OK;
    }

    switch( type )
    {
        case UT_LOGIC:
            ur_setId(res, UT_LOGIC);
            ur_logic(res) = genrand_int32() & 1;
            break;

        case UT_INT:
            if( ur_int(a1) == 0 )
            {
                *res = *a1;
            }
            else
            {
                ur_setId(res, UT_INT);
                ur_int(res) = (genrand_int32() % ur_int(a1)) + 1;
            }
            break;

        case UT_DOUBLE:
            ur_setId(res, UT_DOUBLE);
            ur_double(res) = ur_double(a1) * genrand_real2();
            break;

        case UT_COORD:
        {
            int i, n;
            ur_setId(res, UT_COORD);
            res->coord.len = a1->coord.len;
            for( i = 0; i < a1->coord.len; ++i )
            {
                n = a1->coord.n[ i ];
                res->coord.n[ i ] = n ? (genrand_int32() % n) + 1 : 0;
            }
        }
            break;

        case UT_VEC3:
        {
            int i;
            float n;
            ur_setId(res, UT_VEC3);
            for( i = 0; i < 3; ++i )
            {
                n = a1->vec3.xyz[ i ];
                res->vec3.xyz[ i ] = n ? genrand_real2() * n : 0.0f;
            }
        }
            break;

        default:
            return ur_error( ut, UR_ERR_TYPE, "random does not handle %s",
                             ur_atomCStr( ut, type ) );
    }
    return UR_OK;
}


//EOF
