/*
  Copyright 2009 Karl Robillard

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


extern void     init_genrand( uint32_t s );
extern uint32_t genrand_int32();
extern double   genrand_real2();


static unsigned long _clockSeed()
{
    unsigned long seed;
    seed  = time(NULL);
    seed += clock();
    return seed;
}


/*-cf-
    random
        data    logic!/int!/decimal!/coord! or series.
        /seed   Use data as generator seed.
    return: Random number, series position, or none! if /seed option used.

    If data is a number, then a number from 1 through data will be returned.

    A call to random/seed must be done before random values will be generated.
    If seed data is is not an int! then a clock-based seed is used.
*/
CFUNC(cfunc_random)
{
#define OPT_RANDOM_SEED     1
    int type = ur_type(a1);

    if( CFUNC_OPTIONS & OPT_RANDOM_SEED )
    {
        init_genrand( (type == UT_INT) ? (uint32_t) ur_int(a1) : _clockSeed() );
        ur_setId(res, UT_NONE);
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
            ur_int(res) = genrand_int32() & 1;
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

        case UT_DECIMAL:
            ur_setId(res, UT_DECIMAL);
            ur_decimal(res) = ur_decimal(a1) * genrand_real2();
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

        default:
            return ur_error( ut, UR_ERR_TYPE, "random does not handle %s",
                             ur_atomCStr( ut, type ) );
    }
    return UR_OK;
}


//EOF
