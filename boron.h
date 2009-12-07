#ifndef BORON_H
#define BORON_H
/*
  Copyright 2009 Karl Robillard

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


enum BoronDataType
{
    UT_FUNC = UT_BI_COUNT,
    UT_CFUNC
};


enum BoronWordBindings
{
    UR_BIND_FUNC = UR_BIND_USER,
    UR_BIND_OPTION
    //UR_BIND_OBJECT,
    //UR_BIND_PLUG
};


#define CFUNC(name)     static int name( UThread* ut, UCell* a1, UCell* res )
#define CFUNC_PUB(name) int name( UThread* ut, UCell* a1, UCell* res )
#define CFUNC_OPTIONS   a1[-1].series.end


#ifdef __cplusplus
extern "C" {
#endif

UThread* boron_makeEnv();
void     boron_freeEnv( UThread* );
void     boron_addCFunc( UThread*, int (*func)(UThread*,UCell*,UCell*),
                         const char* sig );
void     boron_bindDefault( UThread*, UIndex blkN );
int      boron_doCStr( UThread*, const char* cmd, int len );
UCell*   boron_result( UThread* );
UCell*   boron_exception( UThread* );
void     boron_reset( UThread* );
int      boron_throwWord( UThread*, UAtom atom );
char*    boron_cstr( UThread*, const UCell* strC, UBuffer* bin );
char*    boron_cpath( UThread*, const UCell* strC, UBuffer* bin );

#ifdef __cplusplus
}
#endif


#endif  /*EOF*/
