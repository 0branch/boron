/*
  Unit-length quaternions for UCells.

  x*x + y*y + z*z + w*w = 1.0
*/


#include <math.h>
#include "math3d.h"
#include "boron.h"


#define UR_FLAG_QUAT_NEGW   1
#define UR_FLAG_QUAT_ONEW   2

#define ur_x(cell)  (cell)->vec3.xyz[0]
#define ur_y(cell)  (cell)->vec3.xyz[1]
#define ur_z(cell)  (cell)->vec3.xyz[2]
#define ur_w(cell)  (cell)->id._pad0


#if 1
// Fast conversion from http://www.stereopsis.com/FPU.html

union FloatInt
{
    float f;
    uint32_t i;
};


#define quatWd  quat_w

float quat_w( const UCell* cell )
{
    union FloatInt conv;

    if( ur_flags( cell, UR_FLAG_QUAT_ONEW ) )
        return ur_flags( cell, UR_FLAG_QUAT_NEGW ) ? -1.0f : 1.0f;
    conv.i = 0x3F800000 | (ur_w(cell) << 7);
    return ur_flags( cell, UR_FLAG_QUAT_NEGW ) ? -(conv.f - 1.0f)
                                               :  (conv.f - 1.0f);
}

void quat_setW( UCell* cell, float w )
{
    union FloatInt conv;
#define CLR_SET(cell,clr,set)   cell->id.flags = (cell->id.flags & ~clr) | set
    if( w < 0.0f )
    {
        if( w <= -1.0f )
        {
            ur_setFlags( cell, UR_FLAG_QUAT_NEGW | UR_FLAG_QUAT_ONEW );
            return;
        }
        w = -w;
        CLR_SET( cell, UR_FLAG_QUAT_ONEW, UR_FLAG_QUAT_NEGW );
    }
    else if( w >= 1.0f )
    {
        CLR_SET( cell, UR_FLAG_QUAT_NEGW, UR_FLAG_QUAT_ONEW );
        return;
    }
    else
    {
        ur_clrFlags( cell, UR_FLAG_QUAT_NEGW | UR_FLAG_QUAT_ONEW );
    }
    conv.f = w + 1.0f;
    ur_w(cell) = (conv.i & 0x7fffff) >> 7;
}

#else

static double quatWd( const UCell* cell )
{

    double w = ((double) ur_w(cell)) / 65535.0;
    return ur_flags(cell, UR_FLAG_QUAT_NEGW) ? -w : w;
}


float quat_w( const UCell* cell )
{
    float w = ((float) ur_w(cell)) / 65535.0f;
    return ur_flags(cell, UR_FLAG_QUAT_NEGW) ? -w : w;
}


void quat_setW( UCell* cell, float w )
{
    if( w < 0.0f )
    {
        w = -w;
        ur_setFlags( cell, UR_FLAG_QUAT_NEGW );
    }
    else
        ur_clrFlags( cell, UR_FLAG_QUAT_NEGW );
    ur_w(cell) = (uint16_t) (w * 65535.0f);
}
#endif


void quat_setIdentity( UCell* q )
{
    q->vec3.xyz[0] = q->vec3.xyz[1] = q->vec3.xyz[2] = 0.0f;
    quat_setW( q, 1.0f );
}


/*
    Given x,y,z compute w.
*/
void quat_fromXYZ( UCell* q, const float* xyz )
{
    float x, y, z, w;
    float len;

    x = xyz[0];
    y = xyz[1];
    z = xyz[2];

    // Normalize input vector.
    len = sqrtf( x*x + y*y + z*z );
    if( len )
    {
        len = 1.0f / len;
        x *= len;
        y *= len;
        z *= len;
    }

    len = x*x + y*y + z*z;
    w = sqrtf( 1.0f - len );

#if 0
    // Normalize quaternion.
    len = sqrtf( w*w + len );
    if( len )
    {
        len = 1.0f / len;
        x *= len;
        y *= len;
        z *= len;
        w *= len;
    }
#endif

    ur_x(q) = x;
    ur_y(q) = y;
    ur_z(q) = z;
    quat_setW( q, w );
}


#if 0
/*
    Angle arguments are radians.
*/
void quat_fromAxis( UCell* q, float x, float y, float z, float angle )
{
    float sinA, len, w;

    angle *= 0.5;
    sinA = sinf( angle );

    x *= sinA;
    y *= sinA;
    z *= sinA;
    w  = cosf( angle );

    // Normalize quaternion.
    len = sqrtf( w*w + x*x + y*y + z*z );
    if( len )
    {
        len = 1.0f / len;
        x *= len;
        y *= len;
        z *= len;
        w *= len;
    }

    ur_x(q) = x;
    ur_y(q) = y;
    ur_z(q) = z;
    quat_setW( q, w );
}
#endif


/*
    Angle arguments are radians.
*/
void quat_fromEuler( UCell* q, float x, float y, float z )
{
    float half, w, len;
    float cx, cy, cz;
    float sx, sy, sz;
    float cc, cs, sc, ss;

    half = x * 0.5;
    cx = cosf( half );
    sx = sinf( half );

    half = y * 0.5;
    cy = cosf( half );
    sy = sinf( half );

    half = z * 0.5;
    cz = cosf( half );
    sz = sinf( half );

    cc = cx * cz;
    cs = cx * sz;
    sc = sx * cz;
    ss = sx * sz;

    x = (cy * sc) - (sy * cs);
    y = (cy * ss) + (sy * cc);
    z = (cy * cs) - (sy * sc);
    w = (cy * cc) + (sy * ss);

    // Normalize quaternion.
    len = sqrtf( w*w + x*x + y*y + z*z );
    if( len )
    {
        len = 1.0f / len;
        x *= len;
        y *= len;
        z *= len;
        w *= len;
    }

    ur_x(q) = x;
    ur_y(q) = y;
    ur_z(q) = z;
    quat_setW( q, w );
}


#if 0
void quat_fromEuler2( UCell* q, float roll, float pitch, float yaw )
{
    float cr, cp, cy, sr, sp, sy, cpcy, spsy;

    roll *= 0.5f;
    cr = cosf( roll );
    sr = sinf( roll );

    pitch *= 0.5f;
    cp = cosf( pitch );
    sp = sinf( pitch );

    yaw *= 0.5f;
    cy = cosf( yaw );
    sy = sinf( yaw );

    cpcy = cp * cy;
    spsy = sp * sy;

    quat_setW( q, cr * cpcy + sr * spsy );
    ur_x(q) = sr * cpcy - cr * spsy;
    ur_y(q) = cr * sp * cy + sr * cp * sy;
    ur_z(q) = cr * cp * sy - sr * sp * cy;
}


/*
    Angle arguments are radians.
*/
void quat_fromSpherical( UCell* q, float lat, float lon, float angle )
{
    double sinA;
    double sin_lat;

    angle *= 0.5;
    sinA = sin( angle );
    sin_lat = sin( lat );

    ur_x(q) = (float) (sinA * cos( lat ) * sin( lon ));
    ur_y(q) = (float) (sinA * sin_lat);
    ur_z(q) = (float) (sinA * sin_lat * cos( lon ));
    quat_setW( q, cosf( angle ) );
}
#endif


void quat_toMatrix( const UCell* q, float* m, int initialize )
{
    float txy, txz, tyz, twx, twy, twz;
    float txx, tyy, tzz;
    float t2;
    float w = quat_w( q );


    t2  = ur_z(q) * 2.0f;
    txz = ur_x(q) * t2;
    tyz = ur_y(q) * t2;
    twz = w * t2;
    tzz = ur_z(q) * t2;

    t2  = ur_y(q) * 2.0f;
    txy = ur_x(q) * t2;
    tyy = ur_y(q) * t2;
    twy = w * t2;

    t2  = ur_x(q) * 2.0f;
    txx = ur_x(q) * t2;
    twx = w * t2;


    m[0]  = 1.0f - (tyy + tzz);
    m[1]  = txy + twz;
    m[2]  = txz - twy;

    m[4]  = txy - twz;
    m[5]  = 1.0f - (txx + tzz);
    m[6]  = tyz + twx;

    m[8]  = txz + twy;
    m[9]  = tyz - twx;
    m[10] = 1.0f - (txx + tyy);


    if( initialize )
    {
        m[3] = m[7] = m[11] = m[12] = m[13] = m[14] = 0.0f;
        m[15] = 1.0f;
    }
}


/*
    \param res  Result quaternion.  May be the same as A or B.
*/
void quat_mul( const UCell* A, const UCell* B, UCell* res )
{
    float a, b, c, d, e, f, g, h;
    float wA, wB;

    wA = quat_w( A );
    wB = quat_w( B );

    a = (wA      + ur_x(A)) * (wB      + ur_x(B));
    b = (ur_z(A) - ur_y(A)) * (ur_y(B) - ur_z(B));
    c = (wA      - ur_x(A)) * (ur_y(B) + ur_z(B));
    d = (ur_y(A) + ur_z(A)) * (wB      - ur_x(B));
    e = (ur_x(A) + ur_z(A)) * (ur_x(B) + ur_y(B));
    f = (ur_x(A) - ur_z(A)) * (ur_x(B) - ur_y(B));
    g = (wA      + ur_y(A)) * (wB      - ur_z(B));
    h = (wA      - ur_y(A)) * (wB      + ur_z(B));

    quat_setW( res, b + (-e - f + g + h) * 0.5f );
    ur_x(res) = a - ( e + f + g + h) * 0.5f;
    ur_y(res) = c + ( e - f + g - h) * 0.5f;
    ur_z(res) = d + ( e - f - g + h) * 0.5f;
}


/*
    \param t    Time from 0.0 to 1.0.
    \param res  Result quaternion.  May be the same as from or to.
*/
void quat_slerp( const UCell* from, const UCell* to, float t, UCell* res )
{
    // Nick Bobick's code.

    const double DELTA = 0.0001;
    double fw, tw;
    double to1[4];
    double cosom, scale0, scale1;


    fw = quatWd( from );
    tw = quatWd( to );

    // calc cosine
    cosom = ur_x(from) * ur_x(to) +
            ur_y(from) * ur_y(to) +
            ur_z(from) * ur_z(to) + fw * tw;

    // adjust signs (if necessary)
    if( cosom < 0.0 )
    {
        cosom = -cosom;
        to1[0] = - ur_x(to);
        to1[1] = - ur_y(to);
        to1[2] = - ur_z(to);
        to1[3] = - tw;
    }
    else
    {
        to1[0] = ur_x(to);
        to1[1] = ur_y(to);
        to1[2] = ur_z(to);
        to1[3] = tw;
    }


    // calculate coefficients

    if( (1.0 - cosom) > DELTA )
    {
        // standard case (slerp)
        double omega = acos(cosom);
        double sinom = sin(omega);
        scale0 = sin((1.0 - t) * omega) / sinom;
        scale1 = sin(t * omega) / sinom;
    }
    else
    {
        // "from" and "to" quaternions are very close 
        //  ... so we can do a linear interpolation
        scale0 = 1.0 - t;
        scale1 = t;
    }

    // calculate final values
    ur_x(res) = (float) (scale0 * ur_x(from) + scale1 * to1[0]);
    ur_y(res) = (float) (scale0 * ur_y(from) + scale1 * to1[1]);
    ur_z(res) = (float) (scale0 * ur_z(from) + scale1 * to1[2]);
    quat_setW( res, scale0 * fw + scale1 * to1[3] );
}


//EOF
