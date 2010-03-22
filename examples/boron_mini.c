#include <stdio.h>
#include "boron.h"


CFUNC(printHello)
{
    int i;
    int count = ur_is(a1, UT_INT) ? ur_int(a1) : 1;
    (void) ut;

    for( i = 0; i < count; ++i )
        printf( "Hello World\n" );

    ur_setId(res, UT_UNSET);
    return UR_OK;
}


int main()
{
    UThread* ut = boron_makeEnv( 0, 0 );            // Startup.
    if( ! ut )
        return -1;

    boron_addCFunc( ut, printHello, "hello n" );    // Add our cfunc!.
    boron_doCStr( ut, "hello 3", -1 );              // Invoke it.
    boron_freeEnv( ut );                            // Cleanup.
    return 0;
}
