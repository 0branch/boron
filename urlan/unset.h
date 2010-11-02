/*
  Copyright 2009 Karl Robillard

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


extern int  unset_make( UThread*, const UCell* from, UCell* res );
extern void unset_copy( UThread*, const UCell* from, UCell* res );
extern int  unset_compare( UThread*, const UCell* a, const UCell* b, int mode );
extern int  unset_operate( UThread*, const UCell*, const UCell*, UCell*, int );
extern const UCell*
            unset_select( UThread*, const UCell* cell, const UCell* sel,
                          UCell* tmp );
extern int  unset_fromString( UThread*, const UBuffer* str, UCell* res );
extern void unset_toString( UThread*, const UCell* cell, UBuffer* str,
                            int depth );
extern void unset_mark( UThread*, UCell* cell );
extern void unset_destroy( UBuffer* buf );
extern void unset_toShared( UCell* cell );
extern void unset_bind( UThread*, UCell* cell, const UBindTarget* bt );

#define unset_toText    unset_toString
#define unset_recycle   0
#define unset_markBuf   0


//EOF
