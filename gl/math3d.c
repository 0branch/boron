/*
  Copyright 2005-2011 Karl Robillard

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


#include "os_math.h"


enum eMatrixIndex
{
    k00 = 0,
    k01,
    k02,
    k03,
    k10,
    k11,
    k12,
    k13,
    k20,
    k21,
    k22,
    k23,
    k30,
    k31,
    k32,
    k33,

    kX = 12,
    kY,
    kZ
};


static float _identity[16] =
{
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};


void ur_loadIdentity( float* mat )
{
    memCpy( mat, _identity, 16 * sizeof(float) );
}


void ur_loadRotation( float* mat, const float* axis, float radians )
{
    float x, y, z;
    float c, s, t;

    x = axis[0];
    y = axis[1];
    z = axis[2];

    c = mathCosineF( radians );
    s = mathSineF( radians );
    t = 1.0f - c;

    mat[ 0 ] = t*x*x + c;
    mat[ 1 ] = t*x*y + s*z;
    mat[ 2 ] = t*x*z - s*y;
    mat[ 3 ] = 0.0f;

    mat[ 4 ] = t*x*y - s*z;
    mat[ 5 ] = t*y*y + c;
    mat[ 6 ] = t*y*z + s*x;
    mat[ 7 ] = 0.0f;

    mat[  8 ] = t*x*z + s*y;
    mat[  9 ] = t*y*z - s*x;
    mat[ 10 ] = t*z*z + c;
    mat[ 11 ] = 0.0f;

    mat[ 12 ] = 0.0f;
    mat[ 13 ] = 0.0f;
    mat[ 14 ] = 0.0f;
    mat[ 15 ] = 1.0f;
}


void ur_perspective( float* mat, float fovYDegrees, float aspect,
                     float near, float far )
{
    float xmax, ymax, depth; 

    ymax = near * tanf(fovYDegrees * UR_PI / 360.0f);
    xmax = ymax * aspect;
    depth = far - near;

    mat[0] = near / xmax;
    mat[1] = mat[2] = mat[3] = mat[4] = 0.0f;
    mat[5] = near / ymax;
    mat[6] = mat[7] = mat[8] = mat[9] = 0.0f;
    mat[10] = (-far - near) / depth;
    mat[11] = -1.0f;
    mat[14] = (-2.0f * near * far) / depth;
    mat[12] = mat[13] = mat[15] = 0.0f;
}


/*
  Result can be the same matrix as A, but not B.
*/
void ur_matrixMult( const float* a, const float* b, float* result )
{
#define A(col,row)  a[(col<<2)+row]
#define R(col,row)  result[(col<<2)+row]

    int i;
    for( i = 0; i < 4; i++ )
    {
        float ai0=A(0,i), ai1=A(1,i), ai2=A(2,i), ai3=A(3,i);
        R(0,i) = ai0 * b[k00] + ai1 * b[k01] + ai2 * b[k02] + ai3 * b[k03];
        R(1,i) = ai0 * b[k10] + ai1 * b[k11] + ai2 * b[k12] + ai3 * b[k13];
        R(2,i) = ai0 * b[k20] + ai1 * b[k21] + ai2 * b[k22] + ai3 * b[k23];
        R(3,i) = ai0 * b[k30] + ai1 * b[k31] + ai2 * b[k32] + ai3 * b[k33];
    }
}


void ur_transLocal( float* mat, float x, float y, float z )
{
    if( x )
    {
        mat[ kX ] += mat[ k00 ] * x;
        mat[ kY ] += mat[ k01 ] * x;
        mat[ kZ ] += mat[ k02 ] * x;
    }
    if( y )
    {
        mat[ kX ] += mat[ k10 ] * y;
        mat[ kY ] += mat[ k11 ] * y;
        mat[ kZ ] += mat[ k12 ] * y;
    }
    if( z )
    {
        mat[ kX ] += mat[ k20 ] * z;
        mat[ kY ] += mat[ k21 ] * z;
        mat[ kZ ] += mat[ k22 ] * z;
    }
}


/**
  Transpose 3x3 submatrix to negate rotation.
*/
void ur_matrixTranspose( float* mat, const float* a )
{
    mat[ k00 ] = a[ k00 ];
    mat[ k01 ] = a[ k10 ];
    mat[ k02 ] = a[ k20 ];
    mat[ k03 ] = a[ k03 ];

    mat[ k10 ] = a[ k01 ];
    mat[ k11 ] = a[ k11 ];
    mat[ k12 ] = a[ k21 ];
    mat[ k13 ] = a[ k13 ];

    mat[ k20 ] = a[ k02 ];
    mat[ k21 ] = a[ k12 ];
    mat[ k22 ] = a[ k22 ];
    mat[ k23 ] = a[ k23 ];
}


/**
  Negates rotation and translation.
*/
void ur_matrixInverse( float* mat, const float* a )
{
    ur_matrixTranspose( mat, a );

    // Negate translation.

    mat[ kX ] = - (a[kX] * a[k00]) - (a[kY] * a[k01]) - (a[kZ] * a[k02]);
    mat[ kY ] = - (a[kX] * a[k10]) - (a[kY] * a[k11]) - (a[kZ] * a[k12]);
    mat[ kZ ] = - (a[kX] * a[k20]) - (a[kY] * a[k21]) - (a[kZ] * a[k22]);
    mat[ k33 ] = a[ k33 ];
}


/*--------------------------------------------------------------------------*/


/**
  Returns the distance from vecA to vecB.
*/
float ur_distance( const float* vecA, const float* vecB )
{
    float dx, dy, dz;

    dx = vecB[0] - vecA[0];
    dy = vecB[1] - vecA[1];
    dz = vecB[2] - vecA[2];

    return mathSqrtF( dx * dx + dy * dy + dz * dz );
}


/**
  Applies the entire matrix to this point.
*/
void ur_transform( float* pnt, const float* mat )
{
    float ox, oy, oz;

    ox = pnt[0];
    oy = pnt[1];
    oz = pnt[2];

    pnt[0] = mat[ k00 ] * ox + mat[ k10 ] * oy + mat[ k20 ] * oz + mat[ k30 ];
    pnt[1] = mat[ k01 ] * ox + mat[ k11 ] * oy + mat[ k21 ] * oz + mat[ k31 ];
    pnt[2] = mat[ k02 ] * ox + mat[ k12 ] * oy + mat[ k22 ] * oz + mat[ k32 ];
}


/**
  Applies the upper 3x3 portion of the matrix to this point.
*/
void ur_transform3x3( float* pnt, const float* mat )
{
    float ox, oy, oz;

    ox = pnt[0];
    oy = pnt[1];
    oz = pnt[2];

    pnt[0] = mat[ k00 ] * ox + mat[ k10 ] * oy + mat[ k20 ] * oz;
    pnt[1] = mat[ k01 ] * ox + mat[ k11 ] * oy + mat[ k21 ] * oz;
    pnt[2] = mat[ k02 ] * ox + mat[ k12 ] * oy + mat[ k22 ] * oz;
}


/**
  Result can be the same as either a or b.
*/
void ur_reflect( const float* a, const float* b, float* result )
{
    float dot2 = ur_dot(a, b) * 2.0f;

    result[0] = a[0] - (b[0] * dot2);
    result[1] = a[1] - (b[1] * dot2);
    result[2] = a[2] - (b[2] * dot2);
}


/**
  Returns shortest vector from line a-b to point.

  \param a    First line endpoint.
  \param b    Second line endpoint.
  \param pnt
  \param vec  Vector result.  This pointer may be the same as a, b, or pnt.
*/
void ur_lineToPoint( const float* a, const float* b, const float* pnt,
                     float* vec )
{
    float dir[3];
    float ft;

    dir[0] = b[0] - a[0];
    dir[1] = b[1] - a[1];
    dir[2] = b[2] - a[2];

    vec[0] = pnt[0] - a[0];
    vec[1] = pnt[1] - a[1];
    vec[2] = pnt[2] - a[2];

    ft = ur_dot( vec, dir );
    if( ft > 0.0 )
    {
        float rlen = ur_lengthSq( dir );
        if( rlen < ft )
        {
            vec[0] -= dir[0];
            vec[1] -= dir[1];
            vec[2] -= dir[2];
        }
        else
        {
            ft /= rlen;
            vec[0] -= ft * dir[0];
            vec[1] -= ft * dir[1];
            vec[2] -= ft * dir[2];
        }
    }
}


void ur_normalize( float* vec )
{
    float x, y, z;
    float len;

    x = vec[0];
    y = vec[1];
    z = vec[2];

    len = mathSqrtF( x * x + y * y + z * z );
    if( len )
    {
        len = 1.0f / len;
        vec[0] = x * len;
        vec[1] = y * len;
        vec[2] = z * len;
    }
}


/*--------------------------------------------------------------------------*/


#if 0
/**
  Returns zero if invalid values were found.
*/
int ur_loadVectorBlock( float* vec, int count, UCell* blkV )
{
#define VEND    if(vec == vend) break

    OBlock* blk = ur_block(blkV);
    UCell* it  = blk->ptr.cells + blkV->series.it;
    UCell* end = blk->ptr.cells + blk->used;
    float* vend = vec + count;
    int ok = 1;

    while( it != end )
    {
        if( ur_is(it, UT_INT) )
        {
            *vec++ = (float) ur_int(it);
            VEND;
        }
        else if( ur_is(it, UT_DECIMAL) )
        {
            *vec++ = (float) ur_decimal(it);
            VEND;
        }
#if 0
        else if( ur_is(it, UT_VEC3) )
        {
            *vec++ = it->vec3.x;
            VEND;
            *vec++ = it->vec3.y;
            VEND;
            *vec++ = it->vec3.z;
            VEND;
        }
#endif
        else
        {
            ok = 0;
        }

        ++it;
    }

    return ok;
}
#endif


static const float* _cellToVec( const UCell* cell, float* tmp )
{
    if( ur_is(cell, UT_VEC3) )
        return cell->vec3.xyz;
    if( ur_is(cell, UT_COORD) )
    {
        tmp[0] = (float) cell->coord.n[0];
        tmp[1] = (float) cell->coord.n[1];
        tmp[2] = (float) cell->coord.n[2];
        return tmp;
    }
    return 0;
}


/*-cf-
    dot
        a   coord!/vec3!
        b   coord!/vec3!
    return: Dot product of vectors.
    group: math
*/
CFUNC( cfunc_dot )
{
    float tmp[6];
    const float* va;
    const float* vb;

    if( (va = _cellToVec( a1, tmp )) &&
        (vb = _cellToVec( a1 + 1, tmp + 3 )) )
    {
        ur_setId(res, UT_DECIMAL);
        ur_decimal(res) = (double) ur_dot( va, vb );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "dot expected coord!/vec3!" );
}


/*-cf-
    cross
        a   coord!/vec3!
        b   coord!/vec3!
    return: Cross product of vectors.
    group: math
*/
CFUNC( cfunc_cross )
{
    float tmp[6];
    const float* va;
    const float* vb;

    if( (va = _cellToVec( a1, tmp )) &&
        (vb = _cellToVec( a1 + 1, tmp + 3 )) )
    {
        ur_setId(res, UT_VEC3);
        ur_cross( va, vb, res->vec3.xyz );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "cross expected coord!/vec3!" );
}


/*-cf-
    normalize
        vec coord!/vec3!
    return: Normalized vector.
    group: math
*/
CFUNC( cfunc_normalize )
{
    float tmp[3];
    const float* va;

    if( (va = _cellToVec( a1, tmp )) )
    {
        ur_setId(res, UT_VEC3);
        memCpy( res->vec3.xyz, va, sizeof(float) * 3 );
        ur_normalize( res->vec3.xyz );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "normalize expected coord!/vec3!" );
}


/*-cf-
    project-point
        a   vec3!
        b   vec3!
        pnt vec3!
    return: Point projected onto line a-b.
    group: math
*/
CFUNC( cfunc_project_point )
{
    const UCell* b = a1 + 1;

    if( ur_is(a1, UT_VEC3) && ur_is(b, UT_VEC3) && ur_is(a1+2, UT_VEC3) )
    {
        const float* pnt = a1[2].vec3.xyz;
        float* rp = res->vec3.xyz;

        ur_setId(res, UT_VEC3);
        ur_lineToPoint( a1->vec3.xyz, b->vec3.xyz, pnt, rp );
        rp[0] = pnt[0] - rp[0];
        rp[1] = pnt[1] - rp[1];
        rp[2] = pnt[2] - rp[2];
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "project-point expected vec3!" );
}


#if 0
/*_cf-
    set-stride
        vector  vector!
        n       int!
    return: vector
*/
CFUNC( cfunc_set_stride )
{
    const UCell* a2 = a1 + 1;
    if( ur_is(a1, UT_VECTOR) && ur_is(a2, UT_INT) )
    {
        ur_vectorStride(prev) = ur_int(a1);
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "set-stride expected vector! int!" );
}


/*_cf-
    set-stride
        vector  vector!
        map     vector!
    return: vector
*/
CFUNC( uc_remap )
{
    const UCell* a2 = a1 + 1;
    if( ur_is(a1, UT_VECTOR) && ur_is(a2, UT_VECTOR) )
    {
        int32_t* vit;
        int32_t* vend;
        int32_t* map;
        int32_t* mend;
        uint32_t mcount;
        uint32_t n;

        if( (ur_vectorDT(prev) != UT_INT) || (ur_vectorDT(tos) != UT_INT) )
            goto bad_dt;

        ur_binaryMem( ut, prev, (uint8_t**) &vit, (uint8_t**) &vend );
        ur_binaryMem( ut, tos,  (uint8_t**) &map, (uint8_t**) &mend );
        mcount = mend - map;

        while( vit != vend )
        {
            n = *vit;
            if( n < mcount )
                *vit = map[ n ];
            ++vit;
        }

        UR_S_DROP;
    }
    return;

bad_dt:

    return ur_error( ut, UR_ERR_TYPE, "remap expected two integer vector!" );
}
#endif


/* EOF */
