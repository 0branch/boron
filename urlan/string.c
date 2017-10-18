/*
  Copyright 2009-2013 Karl Robillard

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
    flags       UR_STRING_ENC_UP
    used        Number of characters used
    ptr.b/.u16  Character data
    ptr.i[-1]   Number of characters available

*/


#include "urlan.h"
#include "os.h"
#include "mem_util.h"

#include "ucs2_case.c"


// Emit inverted question mark for invalid Latin1 characters.
#define NOT_LATIN1_CHAR     0xBF

// Check encoding first since type will most commonly be UT_STRING.
#define IS_UCS2_STRING(buf) (ur_strIsUcs2(buf) && buf->type == UT_STRING)


/** \defgroup dt_string Datatype String
  \ingroup urlan

  Strings are stored using the single word per character Latin-1 and UCS-2
  encodings (UR_ENC_LATIN1/UR_ENC_UCS2).

  UTF-8 (UR_ENC_UTF8) is only handled by ur_strAppend() and
  ur_makeStringUtf8() in order to bring UTF-8 strings into or out of the
  datatype system.

  @{
*/

/** \def ur_cstr()
  Make null terminated UTF-8 string in binary buffer.

  This calls ur_cstring().

  \param strC   Valid UT_STRING or UT_FILE cell.
  \param bin    Initialized binary buffer to use.
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


extern int ur_caretChar( const uint8_t* it, const uint8_t* end,
                         const uint8_t** pos );

/**
  Generate and initialize a single string buffer from memory holding a
  Latin-1 string.

  This calls ur_makeString() internally.

  \param it     Start of Latin-1 data.
  \param end    End of Latin-1 data.

  \return  Buffer id of string.
*/
UIndex ur_makeStringLatin1( UThread* ut, const uint8_t* it, const uint8_t* end )
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
        if( ch == '^' && it != end )
        {
            // Filter caret escape sequence.
            ch = ur_caretChar( it, end, &it );
            if( ch >= 256 )
                goto make_ucs2;
        }
        *out++ = ch;
    }

    buf->used = out - buf->ptr.b;
    return n;

make_ucs2:
    {
    uint16_t* out2;

    // Abandon latin1 string to GC.
    n = ur_makeString( ut, UR_ENC_UCS2, len );
    buf = ur_buffer(n);
    out2 = buf->ptr.u16;
    it = start;
    while( it != end )
    {
        ch = *it++;
        if( ch == '^' && it != end )
            ch = ur_caretChar( it, end, &it );
        *out2++ = ch;
    }
    buf->used = out2 - buf->ptr.u16;
    }
    return n;
}


/*
   Return character count and highest value in a UTF-8 string.
*/
static int _statUtf8( const uint8_t* it, const uint8_t* end, int* highRes )
{
    int high = 0;
    int len = 0;
    int  ch;

    while( it != end )
    {
        ch = *it++;
        if( ch <= 0x7f )                // A single byte
        {
            if( ch == '^' )
            {
                // Filter caret escape sequence.
                if( it != end )
                    ch = ur_caretChar( it, end, &it );
            }
        }
        else if( (ch & 0xf0) == 0xf0 )
        {
            if( ch >= 0xfc )            // 6 bytes
            {
                if( (end - it) < 5 )
                    break;
                ch = ((ch    & 0x01) << 30) |
                     ((it[0] & 0x3f) << 24) |
                     ((it[1] & 0x3f) << 18);
                it += 2;
            }
            else if( ch >= 0xf8 )       // 5 bytes
            {
                if( (end - it) < 4 )
                    break;
                ch = ((ch    & 0x03) << 24) |
                     ((it[0] & 0x3f) << 18);
                it += 1;
            }
            else                        // 4 bytes
            {
                if( (end - it) < 3 )
                    break;
                ch = (ch & 0x07) << 18;
            }
            ch += ((it[0] & 0x3f) << 12) |
                  ((it[1] & 0x3f) <<  6) |
                   (it[2] & 0x3f);
            it += 3;
        }
        else if( ch >= 0xe0 )           // 3 bytes
        {
            if( (end - it) < 2 )
                break;
            ch = ((ch    & 0x0f) << 12) |
                 ((it[0] & 0x3f) <<  6) |
                  (it[1] & 0x3f);
            it += 2;
        }
        else                            // 2 bytes
        {
            if( it == end )
                break;
            ch = ((ch & 0x1f) << 6) | (*it & 0x3f);
            ++it;
        }

        ++len;
        if( high < ch )
            high = ch;
    }

    *highRes = high;
    return len;
}


/*
   Convert UTF-8 to UCS-2.

   The caller must ensure that all characters are in the UCS-2 range.
*/
static void _convertString2( UBuffer* str, const uint8_t* it, const uint8_t* end )
{
    uint16_t  ch;
    uint16_t* out = str->ptr.u16 + str->used;

    while( it != end )
    {
        ch = *it++;
        if( ch <= 0x7f )
        {
            if( ch == '^' && it != end )
                ch = ur_caretChar( it, end, &it );
            *out++ = ch;
        }
        else if( ch >= 0xc2 && ch <= 0xdf )
        {
            if( it == end )
                break;
            *out++ = ((ch & 0x1f) << 6) | (*it & 0x3f);
            ++it;
        }
        else if( ch >= 0xe0 )
        {
            if( (end - it) < 2 )
                break;
            *out++ = ((ch & 0x0f) << 12) | ((it[0] & 0x3f) << 6) | (it[1] & 0x3f);
            it += 2;
        }
    }

    str->used = out - str->ptr.u16;
}


static uint8_t* _emitUtf8( uint8_t* out, int c )
{
    if( c > 127 )
    {
        if( c < 0x800 )
        {
            *out++ = 0xC0 | (c >> 6);
            c = 0x80 | (c & 0x3f);
        }
        else if( c < 0x10000 )
        {
            *out++ = 0xE0 | (c >> 12);
            *out++ = 0x80 | ((c >> 6) & 0x3f);
            c = 0x80 | (c & 0x3f);
        }
        else if( c < 0x200000 )
        {
            *out++ = 0xF0 | (c >> 18);
            *out++ = 0x80 | ((c >> 12) & 0x3f);
            *out++ = 0x80 | ((c >> 6) & 0x3f);
            c = 0x80 | (c & 0x3f);
        }
        else
        {
            // Outside Unicode range (0x0 to 0x10FFFF).
            *out++ = 0xC0 | (NOT_LATIN1_CHAR >> 6);
            c = 0x80 | (NOT_LATIN1_CHAR & 0x3f);
        }
    }
    *out++ = (uint8_t) c;
    return out;
}


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
    UBuffer* buf;
    uint8_t* out;
    int ch;
    int clen;
    int high;
    UIndex bufN;

    clen = _statUtf8( it, end, &high );
    if( high < 256 )
    {
        bufN = ur_makeString( ut, UR_ENC_LATIN1, clen );
        buf = ur_buffer(bufN);
        out = buf->ptr.b;

        while( it != end )
        {
            ch = *it++;
            if( ch > 0x7f )
            {
                if( it == end )
                    break;                          // Drop incomplete multi-byte char.
                ch = ((ch & 0x1f) << 6) | (*it & 0x3f);
                ++it;
            }
            else if( ch == '^' && it != end )
            {
                ch = ur_caretChar( it, end, &it );  // Filter caret escape sequence.
            }
            *out++ = ch;
        }
        buf->used = out - buf->ptr.b;
    }
    else if( high < 0x10000 )
    {
        bufN = ur_makeString( ut, UR_ENC_UCS2, clen );
        _convertString2( ur_buffer(bufN), it, end );
    }
    else
    {
        bufN = ur_makeString( ut, UR_ENC_UTF8, end - it );
        buf = ur_buffer(bufN);
        out = buf->ptr.b;

        while( it != end )
        {
            ch = *it++;
            if( ch == '^' && it != end )
            {
                ch = ur_caretChar( it, end, &it );  // Filter caret escape sequence.
                out = _emitUtf8( out, ch );
            }
            else
            {
                *out++ = ch;
            }
        }
        buf->used = out - buf->ptr.b;
    }

    return bufN;
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
int copyLatin1ToUcs2( uint16_t* dest, const uint8_t* src, int srcLen )
{
    const uint8_t* end = src + srcLen;
    while( src != end )
        *dest++ = *src++;
    return srcLen;
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
            c = NOT_LATIN1_CHAR;
        }
output_char:
        *dest++ = (uint8_t) c;
    }
    return dest - dStart;
}


/*
   Returns number of characters copied.
*/
/*
  UTF-8 Encoding (UCS2 range)

  Unicode Value  Byte 1    Byte 2    Byte 3
  -------------  --------  --------  --------
  0x0000-0x007f  0xxxxxxx
  0x0080-0x07ff  110xxxxx  10xxxxxx
  0x0800-0xffff  1110xxxx  10xxxxxx  10xxxxxx
*/
int copyUtf8ToUcs2( uint16_t* dest, const uint8_t* src, int srcLen )
{
    const uint16_t* dStart = dest;
    const uint8_t* end = src + srcLen;
    uint16_t c;

    while( src != end )
    {
        c = *src++;
        if( c > 0x7f )
        {
            if( (c & 0xe0) == 0xc0 )
            {
                c = (c & 0x1f) << 6 | (src[0] & 0x3f);
                ++src;
            }
            else if( (c & 0xf0) == 0xe0 )
            {
                c = (c & 0x0f) << 12 | (src[0] & 0x3f) << 6 | (src[1] & 0x3f);
                src += 2;
            }
            else if( (c & 0xc0) == 0x80 )
                continue;
            else
                c = NOT_LATIN1_CHAR;
        }
        *dest++ = c;
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
            c = NOT_LATIN1_CHAR;
        *dest++ = (uint8_t) c;
    }
    return srcLen;
}


/*
   Returns number of characters copied.

   \param dest      UTF8 output.  Must have room for three bytes.
   \param src       UCS2 input.
*/
int copyUcs2ToUtf8( uint8_t* dest, const uint16_t* src, int srcLen )
{
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
    switch( str->form )
    {
        case UR_ENC_LATIN1:
            ur_arrReserve( str, str->used + 1 );
            str->ptr.b[ str->used ] = (uc > 255) ? NOT_LATIN1_CHAR : uc;
            ++str->used;
            break;

        case UR_ENC_UTF8:
        {
            uint16_t n = uc;
            ur_arrReserve( str, str->used + 3 );
            str->used += copyUcs2ToUtf8( str->ptr.b + str->used, &n, 1 );
        }
            break;

        case UR_ENC_UCS2:
            ur_arrReserve( str, str->used + 1 );
            str->ptr.u16[ str->used ] = uc;
            ++str->used;
            break;
    }
}


/**
  Append a null-terminated UTF-8 string to a string buffer.
*/
void ur_strAppendCStr( UBuffer* str, const char* cstr )
{
    int len = strLen( cstr );

    ur_arrReserve( str, str->used + len );
    switch( str->form )
    {
        case UR_ENC_LATIN1:
            len = copyUtf8ToLatin1(str->ptr.b + str->used, (uint8_t*)cstr, len);
            break;
        case UR_ENC_UTF8:
            memCpy( str->ptr.c + str->used, cstr, len );
            break;
        case UR_ENC_UCS2:
            len = copyUtf8ToUcs2(str->ptr.u16 + str->used, (uint8_t*)cstr, len);
            break;
    }
    str->used += len;
}


char _hexDigits[] = "0123456789ABCDEF";  //GHIJKLMNOPQRSTUVWXYZ";

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
    T* start = cp; \
    do { \
        *cp++ = _hexDigits[ n & 15 ]; \
    } while( (n >>= 4) > 0 ); \
    if( hi ) { \
        while( cp != (start + 8) ) \
            *cp++ = '0'; \
        do { \
            *cp++ = _hexDigits[ hi & 15 ]; \
            hi >>= 4; \
        } while( hi ); \
    } \
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


extern int fpconv_dtoa(double d, char* dest);
extern int fpconv_ftoa(double d, char* dest);

/**
  Append a double to a string.
*/
void ur_strAppendDouble( UBuffer* str, double n )
{
#define DBL_CHARS   30
    int len;

    ur_arrReserve( str, str->used + DBL_CHARS );
    if( str->form == UR_ENC_UCS2 )
    {
        char tmp[ DBL_CHARS ];
        len = fpconv_dtoa( n, tmp );
        str->used += copyLatin1ToUcs2( str->ptr.u16 + str->used,
                                       (uint8_t*) tmp, len );
    }
    else
    {
        len = fpconv_dtoa( n, str->ptr.c + str->used );
        str->used += len;
    }
}


/**
  Append a float to a string.

  This emits fewer significant digits than ur_strAppendDouble().
*/
void ur_strAppendFloat( UBuffer* str, float n )
{
#define FLT_CHARS   18
    int len;

    ur_arrReserve( str, str->used + FLT_CHARS );
    if( str->form == UR_ENC_UCS2 )
    {
        char tmp[ FLT_CHARS ];
        len = fpconv_ftoa( n, tmp );
        str->used += copyLatin1ToUcs2( str->ptr.u16 + str->used,
                                       (uint8_t*) tmp, len );
    }
    else
    {
        len = fpconv_ftoa( n, str->ptr.c + str->used );
        str->used += len;
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


static void convertLatin1ToUcs2( UBuffer* str )
{
    UBuffer tmp;

    ur_strInit( &tmp, UR_ENC_UCS2, ur_testAvail(str) );
    copyLatin1ToUcs2( tmp.ptr.u16, str->ptr.b, str->used );
    tmp.used = str->used;

    ur_arrFree( str );
    *str = tmp;
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

redo:
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
                if( str->flags & UR_STRING_ENC_UP )
                {
                    convertLatin1ToUcs2( str );
                    goto redo;
                }
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
                copyLatin1ToUcs2( dest, strB->ptr.b + itB, usedB );
                break;
            case UR_ENC_UTF8:
                usedB = copyUtf8ToUcs2( dest, strB->ptr.b + itB, usedB );
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


extern void base2_encodeByte( const uint8_t n, char* out );
extern void base64_encodeTriplet( const uint8_t* in, int len, char* out );

static inline int nibbleToChar( int n )
{
    return (n < 10) ? n + '0' : n + 'A' - 10;
}

/**
  Append binary data as text of the specified encoding.
*/
void ur_strAppendBinary( UBuffer* str, const uint8_t* it, const uint8_t* end,
                         enum UrlanBinaryEncoding enc )
{
    int c;
    switch( enc )
    {
        default:
        case UR_BENC_16:
            while( it != end )
            {
                c = *it++;
                ur_strAppendChar( str, nibbleToChar(c >> 4) );
                ur_strAppendChar( str, nibbleToChar(c & 0x0f) );
            }
            break;

        case UR_BENC_2:
        {
            char buf[10];
            int used = (it != end);
            buf[8] = ' ';
            buf[9] = '\0';
            while( it != end )
            {
                base2_encodeByte( *it++, buf );
                ur_strAppendCStr( str, buf );
            }
            if( used )
                --str->used;
        }
            break;

        case UR_BENC_64:
        {
            char buf[5];
            buf[4] = '\0';
            for( c = end - it; c > 0; it += 3, c -= 3 )
            {
                base64_encodeTriplet( it, c, buf );
                ur_strAppendCStr( str, buf );
            }
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
  Test if all characters are ASCII.

  \param str    Valid string buffer.

  \return Non-zero if all characters are ASCII.
*/
int ur_strIsAscii( const UBuffer* str )
{
    switch( str->form )
    {
        case UR_ENC_LATIN1:
        case UR_ENC_UTF8:
        {
            const uint8_t* it  = str->ptr.b;
            const uint8_t* end = it + str->used;
            while( it != end )
            {
                if( *it++ > 0x7f )
                    return 0;
            }
        }
            break;

        case UR_ENC_UCS2:
        {
            const uint16_t* it  = str->ptr.u16;
            const uint16_t* end = it + str->used;
            while( it != end )
            {
                if( *it++ > 0x7f )
                    return 0;
            }
        }
            break;
    }
    return 1;
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
            uint8_t* convert = 0;
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
                            if( ! convert )
                                convert = (uint8_t*) it - 1;
                            ++it;
                            continue;
                        }
                    }
                    return;
                }
            }

            if( convert )
            {
                //printf( "KR flatten %d\n", (int) (convert - str->ptr.b) );
                str->used = (convert - str->ptr.b) +
                            copyUtf8ToLatin1(convert, convert, end - convert);
            }
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


/*
  Returns pointer to val or zero if val not found.
*/
#define FIND_LC(T) \
const T* find_lc_ ## T( const T* it, const T* end, T val ) { \
    while( it != end ) { \
        if( ur_charLowercase(*it) == val ) \
            return it; \
        ++it; \
    } \
    return 0; \
}

FIND_LC(uint8_t)
FIND_LC(uint16_t)


/*
  Returns pointer to val or zero if val not found.
*/
#define FIND_LC_LAST(T) \
const T* find_lc_last_ ## T( const T* it, const T* end, T val ) { \
    while( it != end ) { \
        --end; \
        if( ur_charLowercase(*end) == val ) \
            return end; \
    } \
    return 0; \
}

FIND_LC_LAST(uint8_t)
FIND_LC_LAST(uint16_t)


/**
  Find the first instance of a character in a string.

  \param str    Valid string/binary buffer.
  \param start  Start index in str.
  \param end    Ending index in str.
  \param ch     Character to look for.
  \param opt    Mask of UrlanFindOption (UR_FIND_CASE, UR_FIND_LAST).

  \return First index where character is found or -1 if not found.
*/
UIndex ur_strFindChar( const UBuffer* str, UIndex start, UIndex end, int ch,
                       int opt )
{
    int matchCase = (opt & UR_FIND_CASE) || (ch < 'A');

    if( IS_UCS2_STRING(str) )
    {
        const uint16_t* (*func)( const uint16_t*, const uint16_t*, uint16_t );
        const uint16_t* it;

        if( matchCase )
        {
            func = (opt & UR_FIND_LAST) ? find_last_uint16_t
                                        : find_uint16_t;
        }
        else
        {
            ch = ur_charLowercase( ch );
            func = (opt & UR_FIND_LAST) ? find_lc_last_uint16_t
                                        : find_lc_uint16_t;
        }
        it = func( str->ptr.u16 + start, str->ptr.u16 + end, ch );
        if( it )
            return it - str->ptr.u16;
    }
    else
    {
        const uint8_t* (*func)( const uint8_t*, const uint8_t*, uint8_t );
        const uint8_t* it;

        if( matchCase )
        {
            func = (opt & UR_FIND_LAST) ? find_last_uint8_t
                                        : find_uint8_t;
        }
        else
        {
            ch = ur_charLowercase( ch );
            func = (opt & UR_FIND_LAST) ? find_lc_last_uint8_t
                                        : find_lc_uint8_t;
        }
        it = func( str->ptr.b + start, str->ptr.b + end, ch );
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
                        const uint8_t* charSet, int len )
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
  Find the last character of a set in a string.

  \param str        Valid string buffer.
  \param start      Start index in str.
  \param end        Ending index in str.
  \param charSet    Bitset of characters to look for.
  \param len        Byte length of charSet.

  \return Last index where any characters in charSet are found or -1 if
          none are found.
*/
UIndex ur_strFindCharsRev( const UBuffer* str, UIndex start, UIndex end,
                           const uint8_t* charSet, int len )
{
    if( ur_strIsUcs2(str) )
    {
        const uint16_t* it;
        it = find_last_charset_uint16_t( str->ptr.u16 + start,
                                         str->ptr.u16 + end, charSet, len );
        if( it )
            return it - str->ptr.u16;
    }
    else
    {
        const uint8_t* it;
        it = find_last_charset_uint8_t( str->ptr.b + start,
                                        str->ptr.b + end, charSet, len );
        if( it )
            return it - str->ptr.b;
    }
    return -1;
}


/*
  Returns first occurance of pattern or 0 if it is not found.
*/
#define FIND_PATTERN_IC(N,T,P) \
const T* find_pattern_ic_ ## N( const T* it, const T* end, \
        const P* pit, const P* pend ) { \
    int pfirst = ur_charLowercase(*pit++); \
    while( it != end ) { \
        if( ur_charLowercase(*it) == pfirst ) { \
            const T* in = it + 1; \
            const P* p  = pit; \
            while( p != pend && in != end ) { \
                if( ur_charLowercase(*in) != ur_charLowercase(*p) ) \
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

FIND_PATTERN_IC(8,uint8_t,uint8_t)
FIND_PATTERN_IC(16,uint16_t,uint16_t)
FIND_PATTERN_IC(8_16,uint8_t,uint16_t)
FIND_PATTERN_IC(16_8,uint16_t,uint8_t)


/**
  Find string in another string or binary series.

  \param ai         String/binary to search.
  \param bi         Pattern to look for.
  \param matchCase  If non-zero, compare character cases.

  \return Index of pattern in string or -1 if not found.
*/
UIndex ur_strFind( const USeriesIter* ai, const USeriesIter* bi,
                   int matchCase )
{
    const UBuffer* bufA = ai->buf;
    const UBuffer* bufB = bi->buf;
    int ci = 0;

    if( IS_UCS2_STRING(bufA) )
        ci += 1;
    if( IS_UCS2_STRING(bufB) )
        ci += 2;

    switch( ci )
    {
        case 0:
        {
            const uint8_t* (*func)( const uint8_t*, const uint8_t*,
                                    const uint8_t*, const uint8_t* );
            const uint8_t* found;

            func = matchCase ? find_pattern_8 : find_pattern_ic_8;
            found = func( bufA->ptr.b + ai->it,
                          bufA->ptr.b + ai->end,
                          bufB->ptr.b + bi->it,
                          bufB->ptr.b + bi->end );
            return found ? found - bufA->ptr.b : -1;
        }
        case 1:
        {
            const uint16_t* (*func)( const uint16_t*, const uint16_t*,
                                     const uint8_t*, const uint8_t* );
            const uint16_t* found;

            func = matchCase ? find_pattern_16_8 : find_pattern_ic_16_8;
            found = func( bufA->ptr.u16 + ai->it,
                          bufA->ptr.u16 + ai->end,
                          bufB->ptr.b + bi->it,
                          bufB->ptr.b + bi->end );
            return found ? found - bufA->ptr.u16 : -1;
        }
        case 2:
        {
            const uint8_t* (*func)( const uint8_t*, const uint8_t*,
                                    const uint16_t*, const uint16_t* );
            const uint8_t* found;

            func = matchCase ? find_pattern_8_16 : find_pattern_ic_8_16;
            found = func( bufA->ptr.b + ai->it,
                          bufA->ptr.b + ai->end,
                          bufB->ptr.u16 + bi->it,
                          bufB->ptr.u16 + bi->end );
            return found ? found - bufA->ptr.b : -1;
        }
        case 3:
        {
            const uint16_t* (*func)( const uint16_t*, const uint16_t*,
                                     const uint16_t*, const uint16_t* );
            const uint16_t* found;

            func = matchCase ? find_pattern_16 : find_pattern_ic_16;
            found = func( bufA->ptr.u16 + ai->it,
                          bufA->ptr.u16 + ai->end,
                          bufB->ptr.u16 + bi->it,
                          bufB->ptr.u16 + bi->end );
            return found ? found - bufA->ptr.u16 : -1;
        }
    }
    return -1;
}


/**
  Find last string in another string or binary series.

  \param ai         String/binary to search.
  \param bi         Pattern to look for.
  \param matchCase  If non-zero, compare character cases.

  \return Index of pattern in string or -1 if not found.
*/
UIndex ur_strFindRev( const USeriesIter* ai, const USeriesIter* bi,
                      int matchCase )
{
    USeriesIter ci = *ai;
    UIndex lpos = -1;
    UIndex pos;

    // TODO: Implement reverse search.

    while( 1 )
    {
        pos = ur_strFind( &ci, bi, matchCase );
        if( pos < 0 )
            break;
        lpos = pos;
        ci.it = pos + 1;
    }
    return lpos;
}


/*
  Returns pointer in pattern at the end of the matching elements.
*/
#define MATCH_PATTERN_IC(N,T,P) \
const P* match_pattern_ic_ ## N( const T* it, const T* end, \
        const P* pit, const P* pend ) { \
    while( pit != pend ) { \
        if( it == end ) \
            return pit; \
        if( ur_charLowercase(*it) != ur_charLowercase(*pit) ) \
            return pit; \
        ++it; \
        ++pit; \
    } \
    return pit; \
}

MATCH_PATTERN_IC(8,uint8_t,uint8_t)
MATCH_PATTERN_IC(16,uint16_t,uint16_t)
MATCH_PATTERN_IC(8_16,uint8_t,uint16_t)
MATCH_PATTERN_IC(16_8,uint16_t,uint8_t)


/**
  Compare characters in two string or binary series.

  \param ai         String/binary slice A.
  \param bi         String/binary slice B.
  \param matchCase  If non-zero, compare character cases.

  \return Number of characters which match in strings.
*/
UIndex ur_strMatch( const USeriesIter* ai, const USeriesIter* bi,
                    int matchCase )
{
    const UBuffer* bufA = ai->buf;
    const UBuffer* bufB = bi->buf;
    int ci = 0;

    if( IS_UCS2_STRING(bufA) )
        ci += 1;
    if( IS_UCS2_STRING(bufB) )
        ci += 2;

    switch( ci )
    {
        case 0:
        {
            const uint8_t* (*func)( const uint8_t*, const uint8_t*,
                                    const uint8_t*, const uint8_t* );
            const uint8_t* pstart = bufB->ptr.b + bi->it;

            func = matchCase ? match_pattern_8 : match_pattern_ic_8;
            return func( bufA->ptr.b + ai->it,
                         bufA->ptr.b + ai->end,
                         pstart, bufB->ptr.b + bi->end ) - pstart;
        }
        case 1:
        {
            const uint8_t* (*func)( const uint16_t*, const uint16_t*,
                                    const uint8_t*, const uint8_t* );
            const uint8_t* pstart = bufB->ptr.b + bi->it;

            func = matchCase ? match_pattern_16_8 : match_pattern_ic_16_8;
            return func( bufA->ptr.u16 + ai->it,
                         bufA->ptr.u16 + ai->end,
                         pstart, bufB->ptr.b + bi->end ) - pstart;
        }
        case 2:
        {
            const uint16_t* (*func)( const uint8_t*, const uint8_t*,
                                     const uint16_t*, const uint16_t* );
            const uint16_t* pstart = bufB->ptr.u16 + bi->it;

            func = matchCase ? match_pattern_8_16 : match_pattern_ic_8_16;
            return func( bufA->ptr.b + ai->it,
                         bufA->ptr.b + ai->end,
                         pstart, bufB->ptr.u16 + bi->end ) - pstart;
        }
        case 3:
        {
            const uint16_t* (*func)( const uint16_t*, const uint16_t*,
                                     const uint16_t*, const uint16_t* );
            const uint16_t* pstart = bufB->ptr.u16 + bi->it;

            func = matchCase ? match_pattern_16 : match_pattern_ic_16;
            return func( bufA->ptr.u16 + ai->it,
                         bufA->ptr.u16 + ai->end,
                         pstart, bufB->ptr.u16 + bi->end ) - pstart;
        }
    }
    return 0;
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


/**
  Make null terminated UTF-8 string in binary buffer.

  \param str    Valid string buffer.
  \param bin    Initialized binary buffer.  The contents are replaced with
                the C string.
  \param start  Start position in str.
  \param end    End position in str.  A negative number is the same as
                str->used.

  \return Pointer to C string in bin.
*/
char* ur_cstring( const UBuffer* str, UBuffer* bin, UIndex start, UIndex end )
{
    int len = str->used;

    if( (end > -1) && (end < len) )
        len = end;
    len -= start;
    if( len > 0 )
    {
        ur_binReserve( bin, (len * 2) + 1 );

        switch( str->form )
        {
            case UR_ENC_LATIN1:
                // TODO: Prevent overflow of dest.
                len = copyLatin1ToUtf8( bin->ptr.b, str->ptr.b + start, len );
                break;

            case UR_ENC_UTF8:
                memCpy( bin->ptr.b, str->ptr.b + start, len ); 
                break;

            case UR_ENC_UCS2:
                // TODO: Prevent overflow of dest.
                len = copyUcs2ToUtf8( bin->ptr.b, str->ptr.u16 + start, len );
                break;
        }
    }
    else
    {
        ur_binReserve( bin, 1 );
        len = 0;
    }

    bin->used = len;
    bin->ptr.c[ len ] = '\0';
    return bin->ptr.c;
}


/** @} */


//EOF
