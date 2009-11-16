/*
  Copyright 2007-2009 Karl Robillard

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


#ifdef __cplusplus
extern "C" {
#endif

void    bignum_zero( UCell* );
void    bignum_seti( UCell*, int n );
void    bignum_setl( UCell*, int64_t n );
void    bignum_setd( UCell*, double n );
int64_t bignum_l( const UCell* );
double  bignum_d( const UCell* );
int     bignum_equal( const UCell*, const UCell* );
int     bignum_cmp( const UCell*, const UCell* );
void    bignum_negate( const UCell*, UCell* result );
void    bignum_add( const UCell*, const UCell*, UCell* result );
void    bignum_sub( const UCell*, const UCell*, UCell* result );
void    bignum_mul( const UCell*, const UCell*, UCell* result );

#ifdef __cplusplus
}
#endif


/*EOF*/
