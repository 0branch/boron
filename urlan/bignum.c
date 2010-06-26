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

/** \defgroup dt_bignum Datatype Bignum
  \ingroup urlan

  64.48 fixed point numbers appropriate for storage in UCells.

  @{
*/

/** \file bignum.h
*/


#include <math.h>
#include "urlan.h"
#include "bignum.h"


// Word layout:  [f0 f1 f2 i0 i1 i2 i3] 

typedef uint16_t    Limb;
#define LIMBS       7
#define IPOS        3
#define BIG(c)      ((Limb*) c) + 1
#define isNeg(b)    (b[LIMBS - 1] & 0x8000)


#define OPT_32  1
#ifdef OPT_32
typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint16_t u16;
    uint32_t u32[ 3 ];
}
UCellBigNum;

#define BIGC(c)     (UCellBigNum*) c
#endif


/**
  Set bignum to zero.
*/
void bignum_zero( UCell* cell )
{
#ifdef OPT_32
    UCellBigNum* bc = BIGC(cell);
    bc->u16 = 0;
    bc->u32[0] = bc->u32[1] = bc->u32[2] = 0;
#else
    Limb* bn  = BIG(cell);
    Limb* end = bn + LIMBS;
    while( bn != end )
        *bn++ = 0;
#endif
}


/**
  Initialize bignum from an integer.
*/
void bignum_seti( UCell* cell, int n )
{
    Limb* b = BIG(cell);

    *b++ = 0;
    *b++ = 0;
    *b++ = 0;
    *b++ = (Limb) (n & 0xffff);
    *b++ = (Limb) ((n >> 16) & 0xffff);
    if( n < 0 )
    {
        *b++ = 0xffff;
        *b   = 0xffff;
    }
    else
    {
        *b++ = 0;
        *b   = 0;
    }
}


/**
  Initialize bignum from a 64-bit integer.
*/
void bignum_setl( UCell* cell, int64_t n )
{
    Limb* b = BIG(cell);

    *b++ = 0;
    *b++ = 0;
    *b++ = 0;
    *b++ = (Limb) (n & 0xffff);
    *b++ = (Limb) ((n >> 16) & 0xffff);
    *b++ = (Limb) ((n >> 32) & 0xffff);
    *b   = (Limb) ((n >> 48) & 0xffff);
}


/**
  Initialize bignum from a double.
*/
void bignum_setd( UCell* cell, double n )
{
    const double lm1 = 65536.0;
    const double lm2 = 65536.0 * 65536.0;
    const double lm3 = 65536.0 * 65536.0 * 65536.0;
    double d = (n < 0.0) ? -n : n;
    double e = floor( d / lm3 );

    if( e < 32767.0 )
    {
        Limb* bn = BIG(cell);

        bn[6] = (Limb)  e;          d -= bn[6] * lm3;
        bn[5] = (Limb) (d / lm2);   d -= bn[5] * lm2;
        bn[4] = (Limb) (d / lm1);   d -= bn[4] * lm1;
        bn[3] = (Limb)  d;          d -= bn[3];
        bn[2] = (Limb) (d * lm1);   d -= bn[2] / lm1;
        bn[1] = (Limb) (d * lm2);   d -= bn[1] / lm2;
        bn[0] = (Limb) (d * lm3);

        if( n < 0.0 )
            bignum_negate( cell, cell );
    }
    else
    {
        // Number too large.
        bignum_zero( cell );
    }
}


/**
  Convert the bignum to a 64-bit integer.
*/
int64_t bignum_l( const UCell* cell )
{
    uint64_t n;
    Limb* b = BIG(cell) + IPOS;
    n = b[0] + (((uint64_t) b[1]) << 16)
             + (((uint64_t) b[2]) << 32)
             + (((uint64_t) b[3]) << 48);
    return (int64_t) n;
}


/**
  Convert the bignum to a double.
*/
double bignum_d( const UCell* cell )
{
    double d = 0.0;
    double e = 1.0 / (65536.0 * 65536.0 * 65536.0);
    const Limb* b = BIG(cell);
    const Limb* end;

    if( isNeg(b) )
    {
        UCell pos;

        bignum_negate( cell, &pos );

        b = BIG(&pos);
        end = b + LIMBS;
        while( b != end )
        {
            d += e * *b++;
            e *= 65536.0;
        }
        return -d;
    }

    end = b + LIMBS;
    while( b != end )
    {
        d += e * *b++;
        e *= 65536.0;
    }
    return d;
}


/**
  Test if two bignums are the same.

  \return  Non-zero if the bignum cells are equal.
*/
int bignum_equal( const UCell* a, const UCell* b )
{
#ifdef OPT_32
    const UCellBigNum* ca = BIGC(a);
    const UCellBigNum* cb = BIGC(b);

    if( ca->u32[1] != cb->u32[1] )
        return 0;
    if( ca->u32[2] != cb->u32[2] )
        return 0;
    if( ca->u32[0] != cb->u32[0] )
        return 0;
    if( ca->u16 != cb->u16 )
        return 0;
#else
    const Limb* ba = BIG(a);
    const Limb* bb = BIG(b);
    int i;
    for( i = 0; i < LIMBS; ++i )
    {
        if( ba[i] != bb[i] )
            return 0;
    }
#endif
    return 1;
}


/**
  Compare two bignums.

  \return  1, 0, or -1 if a is greater than, equal to, or less than b.
*/
int bignum_cmp( const UCell* a, const UCell* b )
{
    const Limb* ba = BIG(a);
    const Limb* bb = BIG(b);
    int i;

    if( isNeg(ba) )
    {
        if( ! isNeg(bb) )
            return -1;
    }
    else if( isNeg(bb) )
    {
        return 1;
    }

    for( i = LIMBS - 1; i > -1; --i )
    {
        if( ba[i] > bb[i] )
            return 1;
        if( ba[i] < bb[i] )
            return -1;
    }
    return 0;
}


/**
  Convert to absolute value.
*/
void bignum_abs( UCell* cell )
{
    const Limb* bn = BIG(cell);
    if( isNeg(bn) )
        bignum_negate( cell, cell );
}


/**
  Negate the bignum.
  Cell and result may be the same.
*/
void bignum_negate( const UCell* cell, UCell* result )
{
    int i;
    uint32_t tmp;
    uint32_t carry = 1;
    const Limb* bn = BIG(cell);
    Limb* res = BIG(result);

    for( i = 0; carry && (i < LIMBS); ++i )
    {
        tmp = ((uint16_t) ~bn[i]) + carry;
        carry = tmp >> 16;
        res[i] = (Limb) (tmp & 0xffff);
    }

    while( i < LIMBS )
    {
        res[i] = ~bn[i];
        ++i;
    }
}


/**
  Get the sum of two bignums.
  A, b, & result may point to the same cell.
*/
void bignum_add( const UCell* a, const UCell* b, UCell* result )
{
    int i;
    uint32_t tmp;
    uint32_t carry = 0;
    const Limb* ba = BIG(a);
    const Limb* bb = BIG(b);
    Limb* res = BIG(result);

    for( i = 0; i < LIMBS; ++i )
    {
        tmp = ba[i] + bb[i] + carry;
        carry = tmp >> 16;
        res[i] = (Limb) (tmp & 0xffff);
    }
}


/**
  Get the difference between two bignums.
  A, b, & result may point to the same cell.
*/
void bignum_sub( const UCell* a, const UCell* b, UCell* result )
{
    UCell neg;
    bignum_negate( b, &neg );
    bignum_add( a, &neg, result );
}


/**
  Get the product of two bignums.
  A, b, & result may point to the same cell.
*/
void bignum_mul( const UCell* a, const UCell* b, UCell* result )
{
    UCell posA, posB;
    Limb prod[(LIMBS * 2) + 1];
    Limb* res;
    const Limb* ba = BIG(a);
    const Limb* bb = BIG(b);
    char sign = 0;
    uint32_t mul, tmp, carry;
    int i, j;

    //if( a == b )
    //    return square;

    if( isNeg(ba) )
    {
        sign = 1;
        bignum_negate( a, &posA );
        ba = BIG(&posA);
    }
    if( isNeg(bb) )
    {
        sign ^= 1;
        bignum_negate( b, &posB );
        bb = BIG(&posB);
    }

    // The high (overflow) limbs need not be initialized.
    for( i = 0; i < (LIMBS + 3); ++i )
        prod[i] = 0;

    res = prod;
    for( i = 0; i < LIMBS; ++i )
    {
        mul = ba[i];
        if( mul )
        {
            carry = 0;
            for( j = 0; j < LIMBS; ++j )
            {
                tmp = mul * bb[j] + carry;
                res[j] += tmp & 0xffff;
                carry = tmp >> 16;
            }
            res[j] += carry;
        }
        ++res;
    }

    res = BIG(result);
    for( i = 0; i < LIMBS; ++i )
        res[i] = prod[i + 3];
    if( sign )
        bignum_negate( result, result );
}


#ifdef UNIT_TEST
// gcc -DUNIT_TEST bignum.c -lm

#include <stdio.h>

int main()
{
    UCell ca, cb, cc;

#define A   &ca
#define B   &cb
#define C   &cc

#define print_d(op) \
    printf( "%f %c %f = %f\n", bignum_d(A), op, bignum_d(B), bignum_d(C) )

#define print_l(op) \
    printf( "%ld %c %ld = %ld\n", bignum_l(A), op, bignum_l(B), bignum_l(C) )

    bignum_zero( A );
    bignum_seti( B, 2 );
    bignum_add( A, B, C );
    print_d( '+' );

    bignum_seti( A, 1 );
    bignum_seti( B, -3 );
    bignum_add( A, B, C );
    print_d( '+' );

    bignum_setd( A, 500432.887 );
    bignum_seti( B, -3 );
    bignum_sub( A, B, C );
    print_d( '-' );
    print_l( '-' );

    return 0;
}
#endif


/** @} */

/*EOF*/
