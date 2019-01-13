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

static const BoronCFunc myFuncs[] = { printHello };
static const char myFuncSpecs[] = "hello n";

int main()
{
    UEnvParameters param;
    UThread* ut = boron_makeEnv( boron_envParam(&param) );  // Startup.
    if( ! ut )
        return 255;

    boron_defineCFunc( ut, UR_MAIN_CONTEXT, myFuncs, myFuncSpecs,
                       sizeof(myFuncSpecs)-1 );     // Add our cfunc!.
    boron_evalUtf8( ut, "hello 3", -1 );            // Invoke it.
    boron_freeEnv( ut );                            // Cleanup.
    return 0;
}
