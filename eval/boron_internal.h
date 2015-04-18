#ifndef BORON_INTERNAL_H
#define BORON_INTERNAL_H


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
    UCell* args;
    UIndex funcBuf;
}
LocalFrame;


// UCellFuncOpt is stored on the data stack just before function arguments.
typedef struct
{
    uint8_t  type;          // UT_LOGIC (For UR_BIND_OPTION result).
    uint8_t  flags;
    uint16_t optionMask;    // UCellId _pad0
    uint8_t  optionJump[ MAX_OPT ];
    uint16_t jumpIt;
    uint16_t jumpEnd;
}
UCellFuncOpt;


typedef struct BoronThread
{
    UThread ut;
    int (*requestAccess)( UThread*, const char* );
    UCell*  evalData;
    UCell*  tos;
    UCell*  eos;
    LocalFrame* bof;
    LocalFrame* tof;
    LocalFrame* eof;
    UIndex  holdData;
    UIndex  dstackN;
    UIndex  fstackN;
    UIndex  tempN;
    UCellFuncOpt fo;
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
