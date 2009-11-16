
#include <math.h>


/* double trigFunc( double ) */

#define mathSine        sin
#define mathCosine      cos
#define mathTan         tan
#define mathASine       asin
#define mathACosine     acos
#define mathATan        atan
#define mathSqrt        sqrt

/* float trigFunc( float ) */

#define mathSineF(n)    (float) sin((double)(n))
#define mathCosineF(n)  (float) cos((double)(n))
#define mathSqrtF(n)    (float) sqrt((double)(n))

/* double mathMod( double, double ) */

#define mathMod         fmod


extern unsigned long randomSeed();
