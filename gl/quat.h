#ifndef QUAT_H
#define QUAT_H
/*
  Normalized quaternions for UCells.
*/


#define UR_FLAGS_QUAT   3
#define ur_quatW(cell)  (cell)->id._pad0

float quat_w( const UCell* );
void quat_setW( UCell*, float w );
void quat_setIdentity( UCell* );
void quat_fromXYZ( UCell*, const float* xyz );
//void quat_fromAxis( UCell* q, float x, float y, float z, float angle );
void quat_fromEuler( UCell* q, float ax, float ay, float az );
//void quat_fromEuler2( UCell* q, float roll, float pitch, float yaw );
//void quat_fromSpherical( UCell*, float lat, float lon, float angle );
void quat_toMatrix( const UCell*, float* m );
void quat_mul( const UCell* A, const UCell* B, UCell* res );
void quat_slerp( const UCell* from, const UCell* to, float t, UCell* res );


#endif /*QUAT_H*/
