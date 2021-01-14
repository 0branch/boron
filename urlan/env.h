#ifndef ENV_H
#define ENV_H
/*
  Copyright 2009,2010 Karl Robillard

  This file is part of the Urlan datatype system.

  Urlan is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Urlan is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Urlan.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "urlan.h"
#include "os.h"


#define LOCK_GLOBAL     mutexLock( env->mutex );
#define UNLOCK_GLOBAL   mutexUnlock( env->mutex );


struct UEnv
{
    OSMutex     mutex;
    UBuffer     sharedStore;
    UBuffer     atomNames;      // Protected by mutex.
    UBuffer     atomTable;      // Protected by mutex.
    uint16_t    typeCount;
    uint16_t    threadCount;    // Protected by mutex.
    uint32_t    threadSize;
    void (*threadFunc)( UThread*, enum UThreadMethod );
    UThread*    initialThread;
    const UDatatype* types[ UT_MAX ];
};


#endif  /*EOF*/
