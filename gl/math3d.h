#ifndef MATH3D_H
#define MATH3D_H
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


#ifdef __cplusplus
extern "C" {
#endif


#define UR_PI           3.14159265358979323846
#define degToRad(deg)   ((UR_PI/180.0)*(double)(deg))
#define radToDeg(rad)   ((180.0/UR_PI)*(double)(rad))


/* Matrix functions */

void ur_loadIdentity( float* );
void ur_loadRotation( float* mat, const float* axis, float radians );
void ur_perspective( float* mat, float fovYDegrees, float aspect,
                     float zNear, float zFar );
void ur_ortho( float* mat, float left, float right, float bottom, float top,
               float zNear, float zFar );
void ur_matrixMult( const float* a, const float* b, float* result );
void ur_transLocal( float* mat, float x, float y, float z );
void ur_matrixTranspose( float* mat, const float* a );
void ur_matrixInverse( float* mat, const float* a );


/* Vector functions */

float ur_distance( const float* vecA, const float* vecB );
void  ur_transform( float* pnt, const float* mat );
void  ur_transform3x3( float* pnt, const float* mat );
void  ur_reflect( const float* a, const float* b, float* result );
void  ur_lineToPoint( const float* a, const float* b, const float* pnt,
                      float* vec );
void  ur_normalize( float* vec );

#define ur_dot(a,b)  ((a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]))

#define ur_cross(a,b,res) \
    res[0] = (a[1] * b[2]) - (a[2] * b[1]); \
    res[1] = (a[2] * b[0]) - (a[0] * b[2]); \
    res[2] = (a[0] * b[1]) - (a[1] * b[0])

#define ur_lengthSq(a)  ((a[0] * a[0]) + (a[1] * a[1]) + (a[2] * a[2]))


#ifdef __cplusplus
}
#endif


#endif  /* MATH3D_H */
