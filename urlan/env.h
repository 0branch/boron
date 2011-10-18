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


struct UEnv
{
    OSMutex     mutex;
    UBuffer     dataStore;
    UBuffer     atomNames;
    UBuffer     atomTable;
    UBuffer     ports;      // Boron UPortDevice (need way to extend UEnv).
    uint16_t    typeCount;
    uint16_t    _pad0;
    uint32_t    threadSize;
    void (*threadFunc)( UThread*, UThreadMethod );
    UThread*    threads;    // Protected by mutex.
    const UDatatype* types[ UT_MAX ];
};


#endif  /*EOF*/
