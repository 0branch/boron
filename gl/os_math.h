
#include <math.h>

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

/* double trigFunc( double ) */

#define mathSine        sin
#define mathCosine      cos
#define mathTan         tan
#define mathASine       asin
#define mathACosine     acos
#define mathATan        atan
#define mathSqrt        sqrt

/* float trigFunc( float ) */

#if defined(_WIN32) || defined(__sun__)
#define mathSineF(n)    (float) sin((double)(n))
#define mathCosineF(n)  (float) cos((double)(n))
#define mathSqrtF(n)    (float) sqrt((double)(n))
#else
#define mathSineF       sinf
#define mathCosineF     cosf
#define mathSqrtF       sqrtf
#endif

/* double mathMod( double, double ) */

#define mathMod         fmod


//extern unsigned long randomSeed();
