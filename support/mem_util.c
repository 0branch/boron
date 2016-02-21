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


#include <stdint.h>


/*
  Returns pointer to val or zero if val not found.
*/
#define FIND(T) \
const T* find_ ## T( const T* it, const T* end, T val ) { \
    while( it != end ) { \
        if( *it == val ) \
            return it; \
        ++it; \
    } \
    return 0; \
}

FIND(uint8_t)
FIND(uint16_t)
FIND(uint32_t)


/*
  Returns pointer to val or zero if val not found.
*/
#define FIND_LAST(T) \
const T* find_last_ ## T( const T* it, const T* end, T val ) { \
    while( it != end ) { \
        --end; \
        if( *end == val ) \
            return end; \
    } \
    return 0; \
}

FIND_LAST(uint8_t)
FIND_LAST(uint16_t)
FIND_LAST(uint32_t)


/*
  Returns first occurance of any character in cset or 0 if none are found.
  csetLen is the number of bytes in cset.
*/
#define FIND_CHARSET(T) \
const T* find_charset_ ## T( const T* it, const T* end, \
    const uint8_t* cset, int csetLen ) { \
    T n; \
    int index; \
    while( it != end ) { \
        n = *it; \
        index = n >> 3; \
        if( (index < csetLen) && (cset[index] & (1 << (n & 7))) ) \
            return it; \
        ++it; \
    } \
    return 0; \
}

FIND_CHARSET(uint8_t)
FIND_CHARSET(uint16_t)


/*
  Returns last occurance of any character in cset or 0 if none are found.
  csetLen is the number of bytes in cset.
*/
#define FIND_LAST_CHARSET(T) \
const T* find_last_charset_ ## T( const T* it, const T* end, \
    const uint8_t* cset, int csetLen ) { \
    T n; \
    int index; \
    while( it != end ) { \
        --end; \
        n = *end; \
        index = n >> 3; \
        if( (index < csetLen) && (cset[index] & (1 << (n & 7))) ) \
            return end; \
    } \
    return 0; \
}

FIND_LAST_CHARSET(uint8_t)
FIND_LAST_CHARSET(uint16_t)


/*
  Returns first occurance of pattern or 0 if it is not found.

  TODO: Look at using a faster search algorithm such as Boyer-Moore or grep
        source.
*/
#define FIND_PATTERN(N,T,P) \
const T* find_pattern_ ## N( const T* it, const T* end, \
        const P* pit, const P* pend ) { \
    int pfirst = *pit++; \
    while( it != end ) { \
        if( *it == pfirst ) { \
            const T* in = it + 1; \
            const P* p  = pit; \
            while( p != pend && in != end ) { \
                if( *in != *p ) \
                    break; \
                ++in; \
                ++p; \
            } \
            if( p == pend ) \
                return it; \
        } \
        ++it; \
    } \
    return 0; \
}

FIND_PATTERN(8,uint8_t,uint8_t)
FIND_PATTERN(16,uint16_t,uint16_t)
FIND_PATTERN(8_16,uint8_t,uint16_t)
FIND_PATTERN(16_8,uint16_t,uint8_t)


/*
  Returns pointer in pattern at the end of the matching elements.
*/
#define MATCH_PATTERN(N,T,P) \
const P* match_pattern_ ## N( const T* it, const T* end, \
        const P* pit, const P* pend ) { \
    while( pit != pend ) { \
        if( it == end ) \
            return pit; \
        if( *it != *pit ) \
            return pit; \
        ++it; \
        ++pit; \
    } \
    return pit; \
}

MATCH_PATTERN(8,uint8_t,uint8_t)
MATCH_PATTERN(16,uint16_t,uint16_t)
MATCH_PATTERN(8_16,uint8_t,uint16_t)
MATCH_PATTERN(16_8,uint16_t,uint8_t)


#define REVERSE(T) \
void reverse_ ## T( T* it, T* end ) { \
    T tmp; \
    while( it < end ) { \
        tmp = *it; \
        --end; \
        *it++ = *end; \
        *end = tmp; \
    } \
}

REVERSE(uint8_t)
REVERSE(uint16_t)
REVERSE(uint32_t)


#define COMPARE(T) \
int compare_ ## T( const T* it, const T* end, const T* itB, const T* endB ) { \
    int lenA = end - it; \
    int lenB = endB - itB; \
    while( it < end && itB < endB ) { \
        if( *it > *itB ) \
            return 1; \
        if( *it < *itB ) \
            return -1; \
        ++it; \
        ++itB; \
    } \
    if( lenA > lenB ) \
        return 1; \
    if( lenA < lenB ) \
        return -1; \
    return 0; \
}

COMPARE(uint8_t)
COMPARE(uint16_t)


/*EOF*/
