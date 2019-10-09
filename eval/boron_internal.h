#ifndef BORON_INTERNAL_H
#define BORON_INTERNAL_H


#include "env.h"

#ifdef CONFIG_RANDOM
#include "well512.h"
#endif
#ifdef CONFIG_ASSEMBLE
#include <jit/jit.h>
#endif


#define MAX_OPT     8       // LIMIT: 8 options per func/cfunc.
#define OPT_BITS(c) (c)->id._pad0

#define PORT_SITE(dev,pbuf,portC) \
    UBuffer* pbuf = ur_buffer( portC->port.buf ); \
    UPortDevice* dev = (pbuf->form == UR_PORT_SIMPLE) ? \
        (UPortDevice*) pbuf->ptr.v : \
        (pbuf->ptr.v ? *((UPortDevice**) pbuf->ptr.v) : 0)


typedef struct
{
    UEnv    env;
    UBuffer ports;
    UStatus (*funcRead)( UThread*, UCell*, UCell* );
    UAtom   compileAtoms[5];
}
BoronEnv;

#define BENV      ((BoronEnv*) ut->env)


typedef struct BoronThread
{
    UThread thread;
    UBuffer tbin;           // Temporary binary buffer.
    int (*requestAccess)( UThread*, const char* );
    UCell*  stackLimit;
    UBuffer frames;         // Function body & locals stack position.
    UCell   optionCell;
#ifdef CONFIG_RANDOM
    Well512 rand;
#endif
#ifdef CONFIG_ASSEMBLE
    jit_context_t jit;
    UAtomEntry* insTable;
#endif
}
BoronThread;

#define BT      ((BoronThread*) ut)
#define RESULT  (BT->evalData + BT_RESULT)


extern UIndex boron_seriesEnd( UThread* ut, const UCell* cell );


#endif  // BORON_INTERNAL_H
