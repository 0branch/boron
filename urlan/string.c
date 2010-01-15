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
/*
  UBuffer members:
    type        UT_STRING
    elemSize    1 or 2
    form        UR_ENC_*
    flags       Unused
    used        Number of characters used
    ptr.b/.u16  Character data
    ptr.i[-1]   Number of characters available

*/


#include "urlan.h"
#include "os.h"
#include "mem_util.h"


/** \defgroup dt_string Datatype String
  \ingroup urlan

  Strings are stored using the single word per character Latin-1 and UCS-2
  encodings (UR_ENC_LATIN1/UR_ENC_UCS2).

  UTF-8 (UR_ENC_UTF8) is only handled by ur_strAppend() and
  ur_makeStringUtf8() in order to bring UTF-8 strings into or out of the
  datatype system.

  @{
*/

/** \def ur_strFree()
  A string is a simple array.
*/

/**
  Generate and initialize a single string buffer.

  If you need multiple buffers then ur_genBuffers() should be used.

  The caller must create a UCell for this string in a held block before the
  next ur_recycle() or else it will be garbage collected.

  \param enc    Encoding type.
  \param size   Number of characters to reserve.

  \return  Buffer id of string.
*/
UIndex ur_makeString( UThread* ut, int enc, int size )
{
    UIndex bufN;
    ur_genBuffers( ut, 1, &bufN );
    ur_strInit( ur_buffer( bufN ), enc, size );
    return bufN;
}


/**
  Generate a single string and set cell to reference it.

  If you need multiple buffers then ur_genBuffers() should be used.

  \param enc    Encoding type.
  \param size   Number of characters to reserve.
  \param cell   Cell to initialize.

  \return  Pointer to string buffer.
*/
UBuffer* ur_makeStringCell( UThread* ut, int enc, int size, UCell* cell )
{
    UBuffer* buf;
    UIndex bufN;

    ur_genBuffers( ut, 1, &bufN );
    buf = ur_buffer( bufN );
    ur_strInit( buf, enc, size );

    ur_setId( cell, UT_STRING );
    ur_setSeries( cell, bufN, 0 );

    return buf;
}


/*
   Convert UTF-8 to UCS-2
*/
static void _makeString2( UBuffer* str, const uint8_t* it, const uint8_t* end )
{
    uint16_t  ch;
    uint16_t* out;

    //ur_arrReserve( str, str->used + (end  - it) );
    out = str->ptr.u16 + str->used;

    while( it != end )
    {
        ch = *it++;

        if( ch <= 0x7f )
        {
            *out++ = ch;
        }
        else if( ch >= 0xc2 && ch <= 0xdf )
        {
            if( it != end )
            {
                *out++ = ((ch & 0x1f) << 6) | (*it & 0x3f);
                ++it;
            }
        }
        else if( ch >= 0xe0 && ch <= 0xef )
        {
            if( (end - it) < 2 )
                break;
            *out++ = ((ch    & 0x0f) << 12) |
                     ((it[0] & 0x3f) <<  6) |
                      (it[1] & 0x3f);
            it += 2;
        }
        else if( ch >= 0xf0 && ch <= 0xf3 )
        {
            if( (end - it) < 3 )
                break;
            *out++ = 0;     // Only handle UCS-2
            it += 3;
        }
    }

    str->used = out - str->ptr.u16;
}


extern int ur_caretChar( const uint8_t* it, const uint8_t* end,
                         const uint8_t** pos );

/**
  Generate and initialize a single string buffer from memory holding a
  UTF-8 string.

  This calls ur_makeString() internally.

  \param it     Start of UTF-8 data.
  \param end    End of UTF-8 data.

  \return  Buffer id of string.
*/
UIndex ur_makeStringUtf8( UThread* ut, const uint8_t* it, const uint8_t* end )
{
    int len = end - it;
    int ch;
    uint8_t* out;
    const uint8_t* start = it;
    UIndex n = ur_makeString( ut, UR_ENC_LATIN1, len );
    UBuffer* buf = ur_buffer(n);

    out = buf->ptr.b;

    while( it != end )
    {
        ch = *it++;
        if( ch > 0x7f )
        {
            if( ch <= 0xdf )
            {
                // If the UTF-8 value is in the Latin-1 range then
                // stay with an 8-bit string.
                ch = ((ch & 0x1f) << 6) | (*it & 0x3f);
                if( ch < 256 )
                {
                    ++it;
                    goto output_char;
                }
            }

            // Abandon latin1 string to GC.
            n = ur_makeString( ut, UR_ENC_UCS2, len );
            _makeString2( ur_buffer(n), start, end );
            return n;
        }
        else if( ch == '^' )
        {
            // Filter caret escape sequence.
            if( it != end )
                ch = ur_caretChar( it, end, &it );
        }
output_char:
        *out++ = ch;
    }

    //memCpy( buf->ptr.c, it, len );

    buf->used = out - buf->ptr.b;
    return n;
}


/**
  Initialize buffer to type UT_STRING.

  \param enc    Encoding type.
  \param size   Number of characters to reserve.
*/
void ur_strInit( UBuffer* buf, int enc, int size )
{
    ur_arrInit( buf, (enc == UR_ENC_UCS2) ? 2 : 1, size );
    buf->type = UT_STRING;
    buf->form = enc;
}


/*
   Returns number of characters copied.
*/
static int copyAsciiToUtf16( uint16_t* dest, const char* src, int srcLen )
{
    const char* end = src + srcLen;
    while( src != end )
        *dest++ = *src++;
    return srcLen;
}


/*
   Returns number of characters copied.
*/
int copyLatin1ToUtf8( uint8_t* dest, const uint8_t* src, int srcLen )
{
    // TODO: Prevent overflow of dest.
    const uint8_t* dStart = dest;
    const uint8_t* end = src + srcLen;
    uint8_t c;

    while( src != end )
    {
        c = *src++;
        if( c > 127 )
        {
            *dest++ = 0xC0 | (c >> 6);
            c = 0x80 | (c & 0x3f);
        }
        *dest++ = c;
    }
    return dest - dStart;
}


/*
   Returns number of characters copied.
*/
int copyUtf8ToLatin1( uint8_t* dest, const uint8_t* src, int srcLen )
{
    const uint8_t* dStart = dest;
    const uint8_t* end = src + srcLen;
    uint16_t c;

    while( src != end )
    {
        c = *src++;
        if( c > 0x7f )
        {
            if( c <= 0xdf )
            {
                c = ((c & 0x1f) << 6) | (*src & 0x3f);
                if( c < 256 )
                {
                    // The UTF-8 value is in the Latin-1 range.
                    ++src;
                    goto output_char;
                }
            }
            c = 0xBF;   // Not a Latin1 character; emit inverted question mark.
        }
output_char:
        *dest++ = (uint8_t) c;
    }
    return dest - dStart;
}


/*
   Returns number of characters copied.
*/
int copyUcs2ToLatin1( uint8_t* dest, const uint16_t* src, int srcLen )
{
    const uint16_t* end = src + srcLen;
    uint16_t c;

    while( src != end )
    {
        c = *src++;
        if( c > 255 )
            c = 0xBF;   // Not a Latin1 character; emit inverted question mark.
        *dest++ = (uint8_t) c;
    }
    return srcLen;
}


/*
   Returns number of characters copied.
*/
int copyUcs2ToUtf8( uint8_t* dest, const uint16_t* src, int srcLen )
{
    // TODO: Prevent overflow of dest.
    const uint8_t* dStart;
    const uint16_t* end;
    uint16_t c;

    dStart = dest;
    end = src + srcLen;

    while( src != end )
    {
        c = *src++;
        if( c > 127 )
        {
            if( c > 0x07ff )
            {
                *dest++ = 0xE0 | (c >> 12);
                *dest++ = 0x80 | ((c >> 6) & 0x3f);
                c = 0x80 | (c & 0x3f);
            }
            else
            {
                *dest++ = 0xC0 | (c >> 6);
                c = 0x80 | (c & 0x3f);
            }
        }
        *dest++ = (uint8_t) c;
    }
    return dest - dStart;
}


/**
  Append a single UCS2 character to a string.
*/
void ur_strAppendChar( UBuffer* str, int uc )
{
    ur_arrReserve( str, str->used + 1 );
    if( str->form == UR_ENC_UCS2 )
        str->ptr.u16[ str->used ] = uc;
    else
        str->ptr.c[ str->used ] = uc;
    ++str->used;
}


/**
  Append a null-terminated ASCII string to a string buffer.
*/
void ur_strAppendCStr( UBuffer* str, const char* cstr )
{
    int len = strLen( cstr );

    ur_arrReserve( str, str->used + len );
    if( str->form == UR_ENC_UCS2 )
        copyAsciiToUtf16( str->ptr.u16 + str->used, cstr, len );
    else
        memCpy( str->ptr.b + str->used, cstr, len );
    str->used += len;
}


static char _hexDigits[] = "0123456789ABCDEF";  //GHIJKLMNOPQRSTUVWXYZ";

#define INT_TO_STR(T) \
    T* cp; \
    if( n < 0 ) { \
        *str++ = '-'; \
        n = -n; \
    } \
    cp = str; \
    do { \
        *cp++ = _hexDigits[ n % 10 ]; \
    } while( (n /= 10) > 0 ); \
    reverse_ ## T( str, cp ); \
    return cp;

#if __WORDSIZE == 64
#define int_to_uint8_t  int64_to_uint8_t
#define int_to_uint16_t int64_to_uint16_t
#else
static uint8_t* int_to_uint8_t( int n, uint8_t* str )
{
    INT_TO_STR(uint8_t)
}

static uint16_t* int_to_uint16_t( int n, uint16_t* str )
{
    INT_TO_STR(uint16_t)
}
#endif

static uint8_t* int64_to_uint8_t( int64_t n, uint8_t* str )
{
    INT_TO_STR(uint8_t)
}

static uint16_t* int64_to_uint16_t( int64_t n, uint16_t* str )
{
    INT_TO_STR(uint16_t)
}


/**
  Append an integer to a string.
*/
void ur_strAppendInt( UBuffer* str, int32_t n )
{
    ur_arrReserve( str, str->used + 12 );
    if( str->form == UR_ENC_UCS2 )
    {
        uint16_t* cp = int_to_uint16_t( n, str->ptr.u16 + str->used );
        str->used = cp - str->ptr.u16;
    }
    else
    {
        uint8_t* cp = int_to_uint8_t( n, str->ptr.b + str->used );
        str->used = cp - str->ptr.b;
    }
}


/**
  Append an 64-bit integer to a string.
*/
void ur_strAppendInt64( UBuffer* str, int64_t n )
{
    ur_arrReserve( str, str->used + 21 );
    if( str->form == UR_ENC_UCS2 )
    {
        uint16_t* cp = int64_to_uint16_t( n, str->ptr.u16 + str->used );
        str->used = cp - str->ptr.u16;
    }
    else
    {
        uint8_t* cp = int64_to_uint8_t( n, str->ptr.b + str->used );
        str->used = cp - str->ptr.b;
    }
}


#define HEX_TO_STR(T) \
static T* hex_to_ ## T( T* cp, uint32_t n, uint32_t hi ) { \
    T* start; \
    *cp++ = '$'; \
    start = cp; \
    do { \
        *cp++ = _hexDigits[ n & 15 ]; \
    } while( (n >>= 4) > 0 ); \
    while( hi ) { \
        *cp++ = _hexDigits[ hi & 15 ]; \
        hi >>= 4; } \
    reverse_ ## T( start, cp ); \
    return cp; \
}

HEX_TO_STR(uint8_t)
HEX_TO_STR(uint16_t)

/**
  Append a hexidecimal integer to a string.
*/
void ur_strAppendHex( UBuffer* str, uint32_t n, uint32_t hi )
{
    ur_arrReserve( str, str->used + (hi ? 18 : 10) );
    if( str->form == UR_ENC_UCS2 )
    {
        uint16_t* cp = hex_to_uint16_t( str->ptr.u16 + str->used, n, hi );
        str->used = cp - str->ptr.u16;
    }
    else
    {
        uint8_t* cp = hex_to_uint8_t( str->ptr.b + str->used, n, hi );
        str->used = cp - str->ptr.b;
    }
}


/*
  Returns pointer to end of number in cp.
*/
static char* _doubleToStr( double n, char* cp )
{
    strPrint( cp, "%f", n );
    while( *cp != '\0' )
        ++cp;
    while( cp[-1] == '0' && cp[-2] != '.' )
        --cp;
    return cp;
}


/**
  Append a double to a string.
*/
void ur_strAppendDouble( UBuffer* str, double n )
{
#define DBL_CHARS   18
    char* cp;

    ur_arrReserve( str, str->used + DBL_CHARS );
    if( str->form == UR_ENC_UCS2 )
    {
        char tmp[ DBL_CHARS ];
        cp = _doubleToStr( n, tmp );
        str->used += copyAsciiToUtf16( str->ptr.u16 + str->used, tmp, cp-tmp );
    }
    else
    {
        cp = _doubleToStr( n, str->ptr.c + str->used );
        str->used = cp - str->ptr.c;
    }
}


#if 0
#define INDENT_LEN      depth
#define ADD_INDENT(cp)  *cp++ = '\t'
#else
#define INDENT_LEN      depth*4
#define ADD_INDENT(cp) \
    *cp++ = ' '; \
    *cp++ = ' '; \
    *cp++ = ' '; \
    *cp++ = ' '
#endif

/**
  Append tabs to a string.

  \param depth  Number of tabs to append.
*/
void ur_strAppendIndent( UBuffer* str, int depth )
{
    int len = INDENT_LEN;

    // Also do '\n'?
    ur_arrReserve( str, str->used + len );
    if( str->form == UR_ENC_UCS2 )
    {
        uint16_t* cp = str->ptr.u16 + str->used;
        str->used += len;
        while( depth-- )
        {
            ADD_INDENT(cp);
        }
    }
    else
    {
        char* cp = str->ptr.c + str->used;
        str->used += len;
        while( depth-- )
        {
            ADD_INDENT(cp);
        }
    }
}


/**
  Append another string buffer to this string.

  \param str    Destination string.
  \param strB   String to append.
  \param itB    Start character of strB.
  \param endB   End character of strB.
*/
void ur_strAppend( UBuffer* str, const UBuffer* strB, UIndex itB, UIndex endB )
{
    int usedB;

    if( endB > strB->used )
        endB = strB->used;
    if( itB >= endB )
        return;
    usedB = endB - itB;

    switch( str->form )
    {
        case UR_ENC_LATIN1:
        {
            uint8_t* dest;
            ur_arrReserve( str, str->used + usedB );
            dest = str->ptr.b + str->used;
            switch( strB->form )
            {
            case UR_ENC_LATIN1:
                memCpy( dest, strB->ptr.c + itB, usedB );
                str->used += usedB;
                break;
            case UR_ENC_UTF8:
                str->used += copyUtf8ToLatin1( dest, strB->ptr.b + itB, usedB );
                break;
            case UR_ENC_UCS2:
                str->used += copyUcs2ToLatin1( dest, strB->ptr.u16+itB, usedB );
                break;
            }
        }
            break;

        case UR_ENC_UTF8:
        {
            uint8_t* dest;
            switch( strB->form )
            {
            case UR_ENC_LATIN1:
                ur_arrReserve( str, str->used + (usedB * 2) );
                dest = str->ptr.b + str->used;
                str->used += copyLatin1ToUtf8( dest, strB->ptr.b + itB, usedB );
                break;
            case UR_ENC_UTF8:
                ur_arrReserve( str, str->used + usedB );
                dest = str->ptr.b + str->used;
                memCpy( dest, strB->ptr.b + itB, usedB );
                str->used += usedB;
                break;
            case UR_ENC_UCS2:
                ur_arrReserve( str, str->used + usedB*2 );
                dest = str->ptr.b + str->used;
                str->used += copyUcs2ToUtf8( dest, strB->ptr.u16 + itB, usedB );
                break;
            }
        }
            break;

        case UR_ENC_UCS2:
        {
            uint16_t* dest;
            ur_arrReserve( str, str->used + usedB );
            dest = str->ptr.u16 + str->used;
            switch( strB->form )
            {
            case UR_ENC_LATIN1:
            case UR_ENC_UTF8:       // FIXME: Need copyUtf8ToUcs2().
                copyAsciiToUtf16( dest, strB->ptr.c + itB, usedB );
                break;
            case UR_ENC_UCS2:
                memCpy( dest, strB->ptr.u16 + itB, usedB * 2 );
                break;
            }
            str->used += usedB;
        }
            break;
    }
}


/**
  Terminate with null character so buffer can be used as a C string.
  Str->used is not changed.
*/
void ur_strTermNull( UBuffer* str )
{
    ur_arrReserve( str, str->used + 1 );
    if( ur_strIsUcs2(str) )
        str->ptr.u16[ str->used ] = '\0';
    else
        str->ptr.c[ str->used ] = '\0';
}


/**
  Convert a UTF-8 or UCS-2 string buffer to Latin-1 if possible.

  \param str    Valid string buffer.
*/
void ur_strFlatten( UBuffer* str )
{
    switch( str->form )
    {
        case UR_ENC_UTF8:
        {
            int ch;
            int convert = 0;
            const uint8_t* it  = str->ptr.b;
            const uint8_t* end = it + str->used;
            while( it != end )
            {
                ch = *it++;
                if( ch > 0x7f )
                {
                    if( ch <= 0xdf && (it != end) )
                    {
                        ch = ((ch & 0x1f) << 6) | (*it & 0x3f);
                        if( ch < 256 )
                        {
                            ++it;
                            convert = 1;
                            continue;
                        }
                    }
                    return;
                }
            }

            if( convert )
                str->used = copyUtf8ToLatin1(str->ptr.b, str->ptr.b, str->used);

            str->form = UR_ENC_LATIN1;
        }
            break;

        case UR_ENC_UCS2:
        {
            uint8_t* bp;
            const uint16_t* it  = str->ptr.u16;
            const uint16_t* end = it + str->used;

            while( it != end )
            {
                if( *it++ > 255 )
                    return;
            }

            bp = str->ptr.b;
            it = str->ptr.u16;
            while( it != end )
                *bp++ = (uint8_t) *it++;

            str->elemSize = 1;
            str->form = UR_ENC_LATIN1;
            if( str->used )
                ur_avail(str) *= 2;
        }
            break;
    }
}


/**
  Convert characters of string slice to lowercase.

  \param buf    Pointer to valid string buffer.
  \param start  Start position.
  \param send   Slice end position.
*/
void ur_strLowercase( UBuffer* buf, UIndex start, UIndex send )
{
    switch( buf->form )
    {
        case UR_ENC_LATIN1:
        {
            uint8_t* it  = buf->ptr.b + start;
            uint8_t* end = buf->ptr.b + send;
            while( it != end )
            {
                *it = ur_charLowercase( *it );
                ++it;
            }
        }
            break;

        case UR_ENC_UCS2:
        {
            uint16_t* it  = buf->ptr.u16 + start;
            uint16_t* end = buf->ptr.u16 + send;
            while( it != end )
            {
                *it = ur_charLowercase( *it );
                ++it;
            }
        }
            break;
    }
}


/**
  Convert characters of string slice to uppercase.

  \param buf    Pointer to valid string buffer.
  \param start  Start position.
  \param send   Slice end position.
*/
void ur_strUppercase( UBuffer* buf, UIndex start, UIndex send )
{
    switch( buf->form )
    {
        case UR_ENC_LATIN1:
        {
            uint8_t* it  = buf->ptr.b + start;
            uint8_t* end = buf->ptr.b + send;
            while( it != end )
            {
                *it = ur_charUppercase( *it );
                ++it;
            }
        }
            break;

        case UR_ENC_UCS2:
        {
            uint16_t* it  = buf->ptr.u16 + start;
            uint16_t* end = buf->ptr.u16 + send;
            while( it != end )
            {
                *it = ur_charUppercase( *it );
                ++it;
            }
        }
            break;
    }
}


/**
  Convert UCS2 character to lowercase.
*/
int ur_charLowercase( int c )
{
    if( c >= 'A' )
    {
        if( c <= 'Z' )
            return c + 32;
        if( c >= 0x00C0 )
        {
            if( (c <= 0x00DE) && (c != 0x00D7) )
                return c + 32;
            if( c >= 0x0391 )
            {
                if( (c <= 0x03AB) && (c != 0x03A2) )
                    return c + 32;
                if( (c >= 0x0410) && (c <= 0x042F) )
                    return c + 32;
                if( (c >= 0x0531) && (c <= 0x0556) )
                    return c + 48;
                // TODO: Implement full lookup table.
            }
        }
    }
    return c;
}


/**
  Convert UCS2 character to uppercase.
*/
int ur_charUppercase( int c )
{
    if( c >= 'a' )
    {
        if( c <= 'z' )
            return c - 32;
        if( c >= 0x00E0 )
        {
            if( (c <= 0x00FE) && (c != 0x00F7) )
                return c - 32;
            if( c >= 0x03B1 )
            {
                if( (c <= 0x03CB) && (c != 0x03C2) )
                    return c - 32;
                if( (c >= 0x0430) && (c <= 0x044F) )
                    return c - 32;
                if( (c >= 0x0561) && (c <= 0x0586) )
                    return c - 48;
                // TODO: Implement full lookup table.
            }
        }
    }
    return c;
}


/**
  Find the first instance of a character in a string.

  \param str    Valid string buffer.
  \param start  Start index in str.
  \param end    Ending index in str.
  \param ch     Bitset of characters to look for.

  \return First index where character is found or -1 if not found.
*/
UIndex ur_strFindChar( const UBuffer* str, UIndex start, UIndex end, int ch )
{
    if( ur_strIsUcs2(str) )
    {
        const uint16_t* it;
        it = find_uint16_t( str->ptr.u16 + start, str->ptr.u16 + end, ch );
        if( it )
            return it - str->ptr.u16;
    }
    else
    {
        const uint8_t* it;
        it = find_uint8_t( str->ptr.b + start, str->ptr.b + end, ch );
        if( it )
            return it - str->ptr.b;
    }
    return -1;
}


/**
  Find the first character of a set in a string.

  \param str        Valid string buffer.
  \param start      Start index in str.
  \param end        Ending index in str.
  \param charSet    Bitset of characters to look for.
  \param len        Byte length of charSet.

  \return First index where any characters in charSet are found or -1 if
          none are found.
*/
UIndex ur_strFindChars( const UBuffer* str, UIndex start, UIndex end,
                        uint8_t* charSet, int len )
{
    if( ur_strIsUcs2(str) )
    {
        const uint16_t* it;
        it = find_charset_uint16_t( str->ptr.u16 + start, str->ptr.u16 + end,
                                    charSet, len );
        if( it )
            return it - str->ptr.u16;
    }
    else
    {
        const uint8_t* it;
        it = find_charset_uint8_t( str->ptr.b + start, str->ptr.b + end,
                                   charSet, len );
        if( it )
            return it - str->ptr.b;
    }
    return -1;
}


/**
  Return the character at a given position.

  If the str->form is UR_ENC_UTF8, then the return value will be the byte at
  pos, not the UCS2 character.

  \param str    Valid string buffer.
  \param pos    Character index.  Pass negative numbers to index from the end
                (e.g. -1 will return the last character).

  \return UCS2 value or -1 if pos is out of range.
*/
int ur_strChar( const UBuffer* str, UIndex pos )
{
    if( pos < 0 )
        pos += str->used;
    if( pos >= 0 && pos < str->used )
    {
        return ur_strIsUcs2(str) ? str->ptr.u16[ pos ]
                                 : str->ptr.b[ pos ];
    }
    return -1;
}


/** @} */


//EOF
