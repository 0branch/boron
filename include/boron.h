#ifndef BORON_H
#define BORON_H
/*
  Copyright 2009-2015 Karl Robillard

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


#include "urlan.h"


#define BORON_VERSION_STR  "2.0.4"
#define BORON_VERSION      0x020004


enum BoronDataType
{
    UT_FUNC = UT_BI_COUNT,
    UT_CFUNC,
    UT_AFUNC,
    UT_PORT,
    UT_BORON_COUNT
};


enum BoronWordBindings
{
    BOR_BIND_FUNC = UR_BIND_USER,
    BOR_BIND_OPTION,
    BOR_BIND_OPTION_ARG
};


typedef UStatus (*BoronCFunc)(UThread*,UCell*,UCell*);
#define CFUNC(name)     static UStatus name( UThread* ut, UCell* a1, UCell* res )
#define CFUNC_PUB(name) UStatus name( UThread* ut, UCell* a1, UCell* res )
#define CFUNC_OPTIONS       a1[-1].id.ext
#define CFUNC_OPT_ARG(opt)  (a1 + ((uint8_t*)a1)[-opt])


typedef struct
{
    UCellId id;
    UIndex  avail;      // End cell is pos + avail.
    const UCell* pos;
}
UCellCFuncEval;

#define boron_evalPos(a1)   ((UCellCFuncEval*) a1)->pos
#define boron_evalAvail(a1) ((UCellCFuncEval*) a1)->avail


enum UserAccess
{
    UR_ACCESS_DENY,
    UR_ACCESS_ALLOW,
    UR_ACCESS_ALWAYS
};


enum PortForm
{
    UR_PORT_SIMPLE,     // buf->ptr.v is UPortDevice*.
    UR_PORT_EXT,        // buf->ptr.v is UPortDevice**.
};


enum PortOpenOptions
{
    UR_PORT_READ    = 0x01,
    UR_PORT_WRITE   = 0x02,
    UR_PORT_NEW     = 0x04,
    UR_PORT_NOWAIT  = 0x08
};


enum PortSeek
{
    UR_PORT_HEAD,
    UR_PORT_TAIL,
    UR_PORT_SKIP
};


#ifdef _WIN32
#define UR_PORT_HANDLE  0x7fffffff
#endif

typedef struct UPortDevice  UPortDevice;

struct UPortDevice
{
    int  (*open) ( UThread*, const UPortDevice*, const UCell* from, int opt,
                   UCell* res );
    void (*close)( UBuffer* );
    int  (*read) ( UThread*, UBuffer*, UCell*, int len );
    int  (*write)( UThread*, UBuffer*, const UCell* );
    int  (*seek) ( UThread*, UBuffer*, UCell*, int where );
#ifdef _WIN32
    int (*waitFD)( UBuffer*, void** );
#else
    int (*waitFD)( UBuffer* );
#endif
    int  defaultReadLen;
};


#ifdef __cplusplus
extern "C" {
#endif

UEnvParameters* boron_envParam( UEnvParameters* );
UThread* boron_makeEnv( UEnvParameters* );
void     boron_freeEnv( UThread* );
UStatus  boron_defineCFunc( UThread*, UIndex ctxN, const BoronCFunc* funcs,
                            const char* spec, int slen );
void     boron_overrideCFunc( UThread*, const char* name, BoronCFunc func );
void     boron_addPortDevice( UThread*, const UPortDevice*, UAtom name );
UBuffer* boron_makePort( UThread*, const UPortDevice*, void* ext, UCell* res );
void     boron_setAccessFunc( UThread*, int (*func)( UThread*, const char* ) );
UStatus  boron_requestAccess( UThread*, const char* msg, ... );
void     boron_bindDefault( UThread*, UIndex blkN );
UStatus  boron_load( UThread*, const char* file, UCell* res );
const UCell*
         boron_eval1(UThread*, const UCell* it, const UCell* end, UCell* res);
UStatus  boron_doBlock( UThread* ut, const UCell* blkC, UCell* res );
UCell*   boron_reduceBlock( UThread* ut, const UCell* blkC, UCell* res );
UCell*   boron_evalUtf8( UThread*, const char* script, int len );
void     boron_reset( UThread* );
UStatus  boron_throwWord( UThread*, UAtom atom, UIndex stackPos );
int      boron_catchWord( UThread*, UAtom atom );
char*    boron_cstr( UThread*, const UCell* strC, UBuffer* bin );
char*    boron_cpath( UThread*, const UCell* strC, UBuffer* bin );
UBuffer* boron_tempBinary( const UThread* );
UStatus  boron_badArg( UThread*, UIndex atom, int argN );

#ifdef __cplusplus
}
#endif


#endif  /*EOF*/
