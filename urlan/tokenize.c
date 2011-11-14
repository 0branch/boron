/*
  Copyright 2009-2011 Karl Robillard

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


#include <math.h>
#include "urlan.h"
#include "urlan_atoms.h"
#include "bignum.h"
#include "mem_util.h"
#include "os.h"

#ifdef _MSC_VER
#define inline  __inline
#endif

extern double ur_stringToDate( const char*, const char*, const char** );
extern UIndex ur_makeVector( UThread*, UAtom type, int size );

#define isDigit(v)     (('0' <= v) && (v <= '9'))


/**
  Converts string until end is reached or a non-digit is found.
  If pos is non-zero then it is set to point past the last valid character.
  If a non-digit is found then the number scanned up to that point is
  returned.
*/
int64_t str_toInt64( const char* start, const char* end, const char** pos )
{
    int cn;
    int64_t n = 0;
    const char* cp = start;

    if( cp != end )
    {
        if( (*cp == '+') || (*cp == '-') )
            ++cp;
        while( cp != end )
        {
            cn = *cp - '0';
            if( cn < 0 || cn > 9 )
                break;

            n *= 10;
            n += cn;
            ++cp;
        }
        if( *start == '-' )
            n = -n;
    }
    if( pos )
        *pos = cp;
    return n;
}


int64_t str_hexToInt64( const char* start, const char* end, const char** pos )
{
    int cn;
    int64_t n = 0;
    const char* cp = start;

    if( cp != end )
    {
        if( (*cp == '+') || (*cp == '-') )
            ++cp;
        while( cp != end )
        {
            cn = *cp;
            if( cn >= 'a' )
                cn = cn - 'a' + 10;
            else if( cn >= 'A' )
                cn = cn - 'A' + 10;
            else
                cn -= '0';
            if( cn < 0 || cn > 15 )
                break;

            n *= 16;
            n += cn;
            ++cp;
        }
        if( *start == '-' )
            n = -n;
    }
    if( pos )
        *pos = cp;
    return n;
}


/**
  Converts string until end is reached or a non-digit is found.
  If pos is non-zero then it is set to point past the last valid character.
  If a non-digit is found then the number scanned up to that point is
  returned.
*/
double str_toDouble( const char* start, const char* end, const char** pos )
{
    double cn;
    double n = 0.0;
    double dec = 0.0;
    const char* cp = start;

    if( cp != end )
    {
        if( (*cp == '+') || (*cp == '-') )
            ++cp;
        while( cp != end )
        {
            if( *cp == '.' )
            {
                if( dec )
                    break;              // Second decimal point.
                dec = 1.0;
            }
            else if( *cp == 'E' || *cp == 'e' )
            {
                double exp = 0.0;
                int expNeg = 0;

                ++cp;
                if( cp == end )
                    break;
                if( *cp++ == '-' )
                    expNeg = 1;

                while( cp != end )
                {
                    cn = (double) (*cp - '0');
                    if( cn < 0.0 || cn > 9.0 )
                        break;

                    exp = exp * 10.0 + cn;
                    ++cp;
                }

                n *= pow(10, expNeg ? -exp : exp);
                break;
            }
            else
            {
                cn = (double) (*cp - '0');
                if( cn < 0.0 || cn > 9.0 )
                    break;

                n = n * 10.0 + cn;
                if( dec )
                    dec *= 0.1;
            }
            ++cp;
        }

        if( *start == '-' )
            n = -n;
        if( dec )
            n *= dec;
    }
    if( pos )
        *pos = cp;
    return n;
}


#ifdef CONFIG_TIMECODE
extern const char* ur_stringToTimeCode( const char*, const char*, UCell* );

/*
  Returns non-zero if three colons are found.
*/
static int _isTimecode( const char* it, const char* end )
{
    int colons = 0;
    int ch;
    while( it != end )
    {
        ch = *it++;
        if( ch == ':' )
        {
            if( ++colons == 3 )
                return 1;
        }
        else if( ! isDigit(ch) )
            break;
    }
    return 0;
}
#endif


double str_toTime( const char* start, const char* end, const char** pos )
{
    double sec;
    int neg;

    if( *start == '-' ) 
    {
        ++start;
        neg = 1;
    }
    else
        neg = 0;

    sec = 3600.0 * str_toInt64( start, end, &start );
    if( *start == ':' )
    {
        ++start;
        sec += 60.0 * str_toInt64( start, end, &start );
        if( *start == ':' )
        {
            ++start;
            sec += str_toDouble( start, end, &start );
        }
    }
    if( pos )
        *pos = start;
    return neg ? -sec : sec;
}


const char* str_toCoord( UCell* cell, const char* it, const char* end )
{
    int16_t* nit  = cell->coord.n;
    int16_t* nend = nit + UR_COORD_MAX;

    while( it != end )
    {
        if( *it == ' ' || *it == '\t' )
        {
            ++it;
        }
        else
        {
            *nit++ = (int16_t) str_toInt64( it, end, &it );
            if( nit == nend || it == end || *it != ',' )
                break;
            ++it;
        }
    }

    cell->coord.len = nit - cell->coord.n;
    if( cell->coord.len < 2 )
        cell->coord.len = 2;

    while( nit != nend )
        *nit++ = 0;

     return it;
}


const char* str_toVec3( UCell* cell, const char* it, const char* end )
{
    float* nit  = cell->vec3.xyz;
    float* nend = nit + 3;

    while( it != end )
    {
        if( *it == ' ' || *it == '\t' )
        {
            ++it;
        }
        else
        {
            *nit++ = (float) str_toDouble( it, end, &it );
            if( nit == nend )
                break;
            if( (it == end) || *it != ',' )
                break;
            ++it;
        }
    }

    while( nit != nend )
        *nit++ = 0.0f;

     return it;
}


/* Whitespace: null space tab cr lf */
uint8_t charset_white[32] = {
    0x01,0x26,0x00,0x00,0x01,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

/* Delimiters: [](){}"; null space tab cr lf */
static uint8_t charset_delimiter[32] = {
    0x01,0x26,0x00,0x00,0x05,0x03,0x00,0x08,
    0x00,0x00,0x00,0x28,0x00,0x00,0x00,0x28,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

/* Hexidecimal: 0-9 a-f A-F */
uint8_t charset_hex[32] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x03,
    0x7E,0x00,0x00,0x00,0x7E,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

/* Word: !&*+-. 0-9 a-z A-Z <>?|=^_~ and all ascii >= 127 */
static uint8_t charset_word[32] = {
    0x00,0x00,0x00,0x00,0x42,0x6C,0xFF,0xF3,
    0xFE,0xFF,0xFF,0xD7,0xFF,0xFF,0xFF,0x57,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

/* Word: !&'*+-./: 0-9 a-z A-Z <>?|=^_~ and all ascii >= 127 */
static uint8_t charset_path[32] = {
  //0x00,0x00,0x00,0x00,0xC2,0x6C,0xFF,0xF3,
    0x00,0x00,0x00,0x00,0xC2,0xEC,0xFF,0xF7,    // With '/' & ':'
    0xFE,0xFF,0xFF,0xD7,0xFF,0xFF,0xFF,0x57,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

/* Base64 encoding: +/= 0-9 a-z A-Z */
uint8_t charset_base64[32] = {
    0x00,0x00,0x00,0x00,0x00,0x88,0xFF,0x23,
    0xFE,0xFF,0xFF,0x07,0xFE,0xFF,0xFF,0x07,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

#define ur_bitIsSet(mem,n)  (mem[(n)>>3] & 1<<((n)&7))
#define IS_HEX(v)       ur_bitIsSet(charset_hex,v)
#define IS_WHITE(v)     ur_bitIsSet(charset_white,v)
#define IS_DELIM(v)     ur_bitIsSet(charset_delimiter,v)
#define IS_WORD(v)      ur_bitIsSet(charset_word,v)
#define IS_PATH(v)      ur_bitIsSet(charset_path,v)
#define isAlpha(v)      ((v >= 'a' && v <= 'z') || (v >= 'A' && v <= 'Z'))


/*
  \param c  Valid hexidecimal character.

  \return Number from 0 to 15.
*/
static inline int hexNibble( int c )
{
    if( c <= '9' )
        return c - '0';
    else if( c <= 'F' )
        return c - 'A' + 10;
    else
        return c - 'a' + 10;
}


/*
  Convert caret escape sequence to a single character.

  \param it     UTF8 string following '^'.  Must be less than end.
  \param end    End of UTF8 string.
  \param pos    Return pointer to end of sequence.

  \return UCS2 character and set pos.
*/
int ur_caretChar( const uint8_t* it, const uint8_t* end, const uint8_t** pos )
{
    int c = *it++;
    if( IS_HEX(c) )
        c = hexNibble(c);
    else if( c == '-' )
        c = '\t';
    else if( c == '/' )
        c = '\n';
    else if( c == 'x' )
    {
        int nib;
        c = 0;
        while( it != end )
        {
            nib = *it;
            if( ! IS_HEX(nib) )
                break;
            c = (c << 4) + hexNibble(nib);
            ++it;
        }
    }
    *pos = it;
    return c;
}


/*
  Get a single UCS2 character from a UTF8 string and convert any Urlan
  escape sequence.

  \param it     Start of character string (past the single quote).
  \param end    End of character string buffer.
  \param pos    Return pointer to end of sequence (past the single quote).

  \return UCS2 character and set pos, or -1 if invalid.
*/
static int ur_charUtf8ToUcs2( const uint8_t* it, const uint8_t* end,
                              const uint8_t** pos )
{
    int c = *it++;
    if( c <= 0x7f )
    {
        if( c == '^' )
        {
            if( it == end )
                return -1;
            c = ur_caretChar( it, end, &it );
        }
    }
    else if( c >= 0xc2 && c <= 0xdf )
    {
        if( it == end )
            return -1;
        c = ((c & 0x1f) << 6) | (*it & 0x3f);
        ++it;
    }
    else if( c >= 0xe0 && c <= 0xef )
    {
        if( (end - it) < 2 )
            return -1;
        c = ((c & 0x0f) << 12) | ((it[0] & 0x3f) << 6) | (it[1] & 0x3f); 
        it += 2;
    }
    else
        return -1;
    if( (it == end) || (*it != '\'') )
        return -1;
    *pos = it + 1;
    return c;
}


/*
  Converts string to word!, path!, or datatype!.

  Returns cell added to blk.
*/
static UCell* _tokenizePath( UThread* ut, UBuffer* blk,
                             const char* it, const char* ew, const char* end,
                             int wtype )
{
#define find_char(a,b,ch)   (char*) find_uint8_t((uint8_t*)a, (uint8_t*)b, ch)
    int ch;
    UBuffer* path;
    UCell* cell;
    UCell* pc;
    UAtom atom;


    cell = ur_blkAppendNew( blk, UT_UNSET );       // Unset in case of GC.

    atom = ur_internAtom( ut, it, ew );
    if( atom < UT_MAX )
    {
        ur_makeDatatype( cell, atom );
        while( 1 )
        {
            it = ew + 1;
            ew = find_char( it, end, '/' );
            if( ! ew )
                ew = end;
            ch = *it;
            if( isAlpha(ch) )
            {
                atom = ur_internAtom( ut, it, ew );
                if( atom < UT_MAX )
                    ur_datatypeAddType( cell, atom );
            }
            if( ew == end )
                break;
        }
        return cell;
    }

    if( end[-1] == ':' )
    {
        --end;
        wtype = UT_SETPATH;
    }
    else if( wtype == UT_LITWORD )
    {
        wtype = UT_LITPATH;
    }
    else
    {
        if( wtype == UT_GETWORD )
            --it;   // Back-up to ':'
        wtype = UT_PATH;
    }
    path = ur_makeBlockCell( ut, wtype, 2, cell );

    while( 1 )
    {
        ch = *it;
        if( isDigit(ch) || (ch == '-') )
        {
            pc = ur_blkAppendNew( path, UT_INT );
            ur_int(pc) = (int32_t) str_toInt64( it, ew, 0 );
        }
        else if( IS_PATH(ch) )
        {
            if( ch == '\'' )
            {
                atom = UT_LITWORD;
                ++it;
            }
            else if( ch == ':' )
            {
                atom = UT_GETWORD;
                ++it;
            }
            else
                atom = UT_WORD;
            pc = ur_blkAppendNew( path, atom );
            ur_setWordUnbound( pc, ur_internAtom( ut, it, ew ) );
        }
        else
            break;

        if( ew == end )
            break;
        it = ew + 1;
        ew = find_char( it, end, '/' );
        if( ! ew )
            ew = end;
    }

    return cell;
}


#define SCAN_LOOP   for(;it != end;++it) { ch = *it;
#define SCAN_END    }


/*
   Skips over C-style block comments, which may be nested.

   \param it    Points to char after first '*'.
   \param end   End of string.

   Returns pointer to char past end of comment or zero if end reached.
*/
static const char* blockComment( const char* it, const char* end, int* lines )
{
    int ch;
    int nested = 0;
    int mode = 0;
    int lineCount = 0;


    SCAN_LOOP
        if( ch == '\n' )
        {
            ++lineCount;
            mode = 0;
        }
        else
        {
            switch( mode )
            {
                case 0:
                    if( ch == '*' )
                        mode = 1;
                    else if( ch == '/' )
                        mode = 2;
                    break;

                case 1:
                    if( ch == '/' )
                    {
                        if( ! nested )
                        {
                            *lines += lineCount;
                            return ++it;
                        }
                        --nested;
                    }
                    mode = 0;
                    break;

                case 2:
                    if( ch == '*' )
                        ++nested;
                    mode = 0;
                    break;
            }
        }
    SCAN_END

    *lines += lineCount;
    return 0;
}


static UCell* appendInt( UBuffer* blk, int64_t n, uint32_t max )
{
    UCell* cell;
    if( n > max || n < INT32_MIN )
    {
        cell = ur_blkAppendNew( blk, UT_BIGNUM );
        bignum_setl( cell, n );
    }
    else
    {
        cell = ur_blkAppendNew( blk, UT_INT );
        ur_int(cell) = (int32_t) n;
    }
    return cell;
}


enum TokenOps
{
    SKIP,   // "\t\r "  Whitespace
    NL,     //  \n      Newline
    COM_L,  //  ;       Line comment
    COM_B,  //  /       Block comment, UT_WORD
    BIN,    //  #       UT_BINARY, UT_VECTOR, UT_WORD
    STR,    // "\"{"    UT_STRING
    BLK,    // "[("     UT_BLOCK, UT_PAREN
    BLK_E,  // "])"     End block
    LIT,    //  '       UT_LITWORD, UT_LITPATH, UT_CHAR
    SIGN,   // "+-"     UT_INT, UT_DECIMAL, UT_VEC3, UT_COORD, UT_WORD
    ZERO,   // 0        Hex number, NUM
    NUM,    // 1-9      UT_INT, UT_DECIMAL, UT_VEC3, UT_COORD, UT_DATE, UT_TIME
    HEX,    //  $       UT_INT, UT_COORD
    FIL,    //  %       UT_FILE
    COLON,  //  :       UT_GETWORD
    ALPHA,  // A-Z a-z  UT_SETWORD, UT_WORD
    INV     //          Invalid character
};


static uint8_t firstCharOp[ 127 ] =
{
//  '\0'    SOH     STX     ETX     EOT     ENQ     ACK     '\a'
    INV,    INV,    INV,    INV,    INV,    INV,    INV,    SKIP,
//  '\b'    '\t'    '\n'    '\v'    '\f'    '\r'    SO      SI
    INV,    SKIP,   NL,     INV,    INV,    SKIP,   INV,    INV, 
//  DLE     DC1     DC2     DC3     DC4     NAK     SYN     ETB
    INV,    INV,    INV,    INV,    INV,    INV,    INV,    INV,
//  CAN     EM      SUB     ESC     FS      GS      RS      US
    INV,    INV,    INV,    INV,    INV,    INV,    INV,    INV,
//  ' '     !       "       #       $       %       &       ´       
    SKIP,   ALPHA,  STR,    BIN,    HEX,    FIL,    ALPHA,  LIT,
//  (       )       *       +       ,       -       .       /       
    BLK,    BLK_E,  ALPHA,  SIGN,   ALPHA,  SIGN,   ALPHA,  COM_B,
//  0       1       2       3       4       5       6       7       
    ZERO,   NUM,    NUM,    NUM,    NUM,    NUM,    NUM,    NUM,
//  8       9       :       ;       <       =       >       ?       
    NUM,    NUM,    COLON,  COM_L,  ALPHA,  ALPHA,  ALPHA,  ALPHA,
//  @       A       B       C       D       E       F       G 
    ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,
//  H       I       J       K       L       M       N       O
    ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,
//  P       Q       R       S       T       U       V       W 
    ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,
//  X       Y       Z       [       \       ]       ^       _
    ALPHA,  ALPHA,  ALPHA,  BLK,    ALPHA,  BLK_E,  ALPHA,  ALPHA,
//  `       a       b       c       d       e       f       g
    ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,
//  h       i       j       k       l       m       n       o
    ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,
//  p       q       r       s       t       u       v       w
    ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,  ALPHA,
//  x       y       z       {        |      }       ~       DEL
    ALPHA,  ALPHA,  ALPHA,  STR,    ALPHA,  ALPHA,  ALPHA
};


static const char* _errToken( UBuffer* bin, const char* it, const char* end )
{
    const char* start = it;
    int ch;

    SCAN_LOOP
        if( IS_DELIM(ch) )
            break;
    SCAN_END

    ch = it - start;
    ur_binReserve( bin, ch + 1 );
    memCpy( bin->ptr.c, start, ch );
    bin->ptr.c[ ch ] = '\0';

    return bin->ptr.c;
}


#define syntaxError(msg) \
    errorMsg = msg; \
    goto error_msg

#define syntaxErrorT(msg) \
    errorMsg = msg; \
    goto error_token


/**
  \ingroup urlan_core

  Convert a UTF-8 data string into a block.

  \note Currently, non-ASCII characters are only allowed inside
        string!/file! types and comments.

  \param it     Start of string.
  \param end    End of string.
  \param res    Cell to initialize as UT_BLOCK if non-zero is returned.

  \return Block buffer id.  If a syntax error is found, then an error is
  generated with ur_error() and zero is returned.
*/
UIndex ur_tokenize( UThread* ut, const char* it, const char* end, UCell* res )
{
#define STACK   stack.ptr.i
#define BLOCK   ur_buffer( STACK[ stack.used - 1 ] )
    UBuffer stack;
    UIndex hold;
    UIndex blkN;
    UCell* cell;
    const char* token;
    const char* errorMsg;
    int ch;
    int mode;
    int sol = 0;
    int lines = 0;


    blkN = ur_makeBlock( ut, 0 );
    hold = ur_hold( blkN );

    ur_arrInit( &stack, sizeof(UIndex), 32 );
    ur_arrAppendInt32( &stack, blkN );

start:

    SCAN_LOOP
        if( ch > 126 )
            goto invalid_char;

        switch( firstCharOp[ ch ] )
        {
            case SKIP:
                break;

            case NL:
newline:
                ++lines;
                sol = 1;
                break;

            case COM_L:
                SCAN_LOOP
                    if( ch == '\n' )
                        goto newline;
                SCAN_END
                goto finish;

            case COM_B:
                token = ++it;
                if( it == end || IS_DELIM(*it) )
                {
                    --token;
                    mode = UT_WORD;
                    goto word;
                }
                if( *it == '*' )
                {
                    it = blockComment( ++it, end, &lines );
                    if( it )
                        goto start;
                    goto finish;
                }
                SCAN_LOOP
                    if( ! IS_WORD( ch ) )
                        break;
                SCAN_END
                mode = UT_OPTION;
                goto word;

            case BIN:
                mode = UR_BENC_16;
binary:
                ++it;
                if( it == end )
                    goto invalid_bin;
                if( *it == '{' )
                {
                    const uint8_t* baseChars = (mode == UR_BENC_64) ?
                        charset_base64 : charset_hex;
                    ++it;
                    token = it;
                    SCAN_LOOP
                        if( ur_bitIsSet( baseChars, ch ) )
                        {
                            continue;
                        }
                        else if( IS_WHITE(ch) )
                        {
                            if( ch == '\n' )
                                ++lines;
                        }
                        else if( ch == '}' )
                        {
                            UBuffer* bin;
                            cell = ur_blkAppendNew( BLOCK, UT_NONE );
                            bin = ur_makeBinaryCell( ut, 0, cell );
                            bin->form = mode;
                            if( ur_binAppendBase( bin, token, it, mode ) != it )
                                goto invalid_bin;
                            ++it;
                            goto set_sol;
                        }
                        else
                        {
invalid_bin:
                            syntaxError( "Invalid binary" );
                        }
                    SCAN_END
                }
                else if( *it == '[' )
                {
                    UBuffer* buf;
                    UIndex tn;
                    token = 0;

                    tn = ur_makeVector( ut, UR_ATOM_I32, 0 );
                    cell = ur_blkAppendNew( BLOCK, UT_VECTOR );
                    buf = ur_buffer( tn );
                    ur_setSeries( cell, tn, 0 );

                    ++it;
                    SCAN_LOOP
                        if( IS_DELIM(ch) )
                        {
vector_white:
                            if( token )
                            {
                                if( ! buf->used )
                                {
                                    const char* cp = token;
                                    while( cp != it )
                                    {
                                        if( *cp++ == '.' )
                                        {
                                            buf->form = UR_ATOM_F32;
                                            break;
                                        }
                                    }
                                }
                                if( buf->form == UR_ATOM_I32 )
                                {
                                    ur_arrAppendInt32( buf, (int32_t)
                                        str_toInt64( token, it, 0 ) );
                                }
                                else
                                {
                                    ur_arrAppendFloat( buf, (float)
                                        str_toDouble( token, it, 0 ) );
                                }
                                token = 0;
                            }
                            if( ch == '\n' )
                            {
vector_newline:
                                ++lines;
                            }
                            else if( ch == ';' )
                            {
                                ++it;
                                SCAN_LOOP
                                    if( ch == '\n' )
                                        goto vector_newline;
                                SCAN_END
                                break;
                            }
                            else if( ch == ']' )
                            {
                                ++it;
                                goto set_sol;
                            }
                        }
                        else if( ch == '/' )
                        {
                            ++it;
                            if( it == end || *it != '*' )
                                break;
                            it = blockComment( ++it, end, &lines );
                            if( it )
                            {
                                --it;
                                goto vector_white;
                            }
                            break;
                        }
                        else if( ! token )
                        {
                            token = it;
                        }
                    SCAN_END
                    syntaxError( "Invalid vector" );
                }
                break;

            case STR:
                token = ++it;
                if( ch == '{' )
                {
                    int nested = 0;
                    SCAN_LOOP
                        if( ch == '^' )
                        {
                            if( ++it == end )
                                break;
                        }
                        else if( ch == '{' )
                        {
                            ++nested;
                        }
                        else if( ch == '}' )
                        {
                            if( nested )
                                --nested;
                            else
                                goto string_end;
                        }
                        else if( ch == '\n' )
                            ++lines;
                    SCAN_END
                }
                else // if( ch == '"' )
                {
                    SCAN_LOOP
                        if( ch == '^' )
                        {
                            if( ++it == end )
                                break;
                        }
                        if( ch == '"' )
                            goto string_end;
                        if( ch == '\n' )
                            break;
                    SCAN_END
                }
                syntaxError( "String not terminated" );
string_end:
                {
                UIndex newStrN = ur_makeStringUtf8( ut, (uint8_t*) token,
                                                        (uint8_t*) it );
                cell = ur_blkAppendNew( BLOCK, UT_STRING );
                ur_setSeries( cell, newStrN, 0 );
                }
                ++it;
                goto set_sol;

            case BLK:
            {
                UIndex newBlkN = ur_makeBlock( ut, 0 );
                cell = ur_blkAppendNew( BLOCK,
                                        (ch == '[') ? UT_BLOCK : UT_PAREN );
                ur_setSeries( cell, newBlkN, 0 );
                ur_arrAppendInt32( &stack, newBlkN );
                if( sol )
                {
                    cell->id.flags |= UR_FLAG_SOL;
                    sol = 0;
                }
            }
                break;

            case BLK_E:
                if( stack.used == 1 )
                {
                    ur_error( ut, UR_ERR_SYNTAX,
                            "End of block '%c' has no opening match (line %d)",
                            ch, lines + 1 );
                    goto error;
                }
                --stack.used;
                if( sol )
                    sol = 0;
                break;

            case LIT:
                token = ++it;
                {
                int c = ur_charUtf8ToUcs2( (const uint8_t*) token,
                                           (const uint8_t*) end,
                                           (const uint8_t**) &it );
                if( c >= 0 )
                {
                    cell = ur_blkAppendNew( BLOCK, UT_CHAR );
                    ur_int(cell) = c;
                    goto set_sol;
                }
                }
                if( it != end && isDigit(*it) )
                {
                    --token;
                    syntaxErrorT( "Invalid lit-word" );
                }
                mode = UT_LITWORD;
                goto alpha;

            case SIGN:
                token = it++;
                if( it == end || ! isDigit(*it) )
                {
                    mode = UT_WORD;
                    goto alpha;
                }
                goto number;

            case ZERO:
                token = it++;
                if( it != end && *it == 'x' )
                    goto hex_number;
                goto number;

            case NUM:
                token = it++;
number:
                SCAN_LOOP
                    if( ! isDigit(ch) )
                        break;
                SCAN_END

                {
                UBuffer* blk = BLOCK;
                switch( ch )
                {
                case '-':
                    cell = ur_blkAppendNew( blk, UT_DATE );
                    ur_decimal(cell) = ur_stringToDate( token, end, &it );
                    if( it == token )
                    {
                        syntaxErrorT( "Invalid date" );
                    }
                    break;
                case '.':   // UT_DECIMAL, UT_VEC3
                    cell = ur_blkAppendNew( blk, UT_DECIMAL );
                    ur_decimal(cell) = str_toDouble( token, end, &it );
                    if( (it != end) && (*it == ',') )
                    {
                        ur_type(cell) = UT_VEC3;
                        it = str_toVec3( cell, token, end );
                    }
                    break;
                case ':':
#ifdef CONFIG_TIMECODE
                    if( _isTimecode( it, end ) )
                    {
                        cell = ur_blkAppendNew( blk, UT_TIMECODE );
                        it = ur_stringToTimeCode( token, end, cell );
                        break;
                    }
#endif
                    cell = ur_blkAppendNew( blk, UT_TIME );
                    ur_decimal(cell) = str_toTime( token, end, &it );
                    break;
                case ',':
                    cell = ur_blkAppendNew( blk, UT_COORD );
                    it = str_toCoord( cell, token, end );
                    break;
                case '#':   // UT_BINARY
                    if( token[0] == '6' && token[1] == '4' )
                    {
                        mode = UR_BENC_64;
                        goto binary;
                    }
                    else if( token[0] == '2' && token[1] == '#' )
                    {
                        mode = UR_BENC_2;
                        goto binary;
                    }
                    else if( token[0] == '1' && token[1] == '6' )
                    {
                        mode = UR_BENC_16;
                        goto binary;
                    }
                    syntaxErrorT( "Invalid binary base" );
                default:
                    if( (it != end) && ! IS_DELIM(ch) )
                    {
                        syntaxErrorT( "Invalid int" );
                    }
                    cell = appendInt( blk, str_toInt64( token, end, &it ),
                                      INT32_MAX );
                    break;
                }
                }
                goto set_sol;

            case HEX:
hex_number:
                token = ++it;
                // Treating hex values as unsigned here ($ffffffff -> int!)
                cell = appendInt( BLOCK, str_hexToInt64( token, end, &it ),
                                  UINT32_MAX );
                if( it == token )
                {
                    --token;
                    if( *token == 'x' )
                        --token;
                    syntaxErrorT( "Invalid hexidecimal number" );
                }
                ur_setFlags( cell, UR_FLAG_INT_HEX );
                goto set_sol;

            case FIL:
                ++it;
                if( it != end && *it == '"' )
                {
                    token = ++it;
                    SCAN_LOOP
                        if( ch == '"' )
                            break;
                        if( ch == '\n' )
                        {
                            syntaxError( "File has no closing quote" );
                        }
                    SCAN_END
                }
                else
                {
                    token = it;
                    SCAN_LOOP
                        if( IS_DELIM( ch ) )
                            break;
                    SCAN_END
                }
                {
                UIndex newStrN = ur_makeStringUtf8( ut, (uint8_t*) token,
                                                        (uint8_t*) it );
                cell = ur_blkAppendNew( BLOCK, UT_FILE );
                ur_setSeries( cell, newStrN, 0 );
                if( ch == '"' )
                    ++it;
                }
                goto set_sol;

            case COLON:
                token = ++it;
                if( it != end && isDigit(*it) )
                {
                    --token;
                    syntaxErrorT( "Invalid get-word" );
                }
                mode = UT_GETWORD;
                goto alpha;

            case ALPHA:
                token = it++;
                mode = UT_WORD;
alpha:
                SCAN_LOOP
                    if( ! IS_WORD( ch ) )
                        break;
                SCAN_END
                if( ch == '/' )
                {
                    const char* endw = it;
                    SCAN_LOOP
                        if( ! IS_PATH( ch ) )
                            break;
                    SCAN_END
                    cell = _tokenizePath( ut, BLOCK, token, endw, it, mode );
                }
                else if( ch == ':' )
                {
                    if( mode != UT_WORD )
                    {
                        --token;
                        syntaxErrorT( "Unexpected colon" );
                    }
                    cell = ur_blkAppendNew( BLOCK, UT_SETWORD );
                    ur_setWordUnbound( cell, ur_internAtom( ut, token, it ) );
                    ++it;
                }
                else
                {
word:
                    cell = ur_blkAppendNew( BLOCK, mode );
                    ur_setWordUnbound( cell, ur_internAtom( ut, token, it ) );
                }
                goto set_sol;

            /*
            case UNEX:
                ur_error( ut, UR_ERR_SYNTAX,
                          "Unexpected character %c (line %d)", ch, lines + 1 );
                goto error;
            */

            case INV:
                goto invalid_char;
        }
    SCAN_END

finish:

    if( stack.used > 1 )
    {
        syntaxError( "Block or paren not closed" );
    }

    //ur_bindDefault( ut, blkN );   // Assumes languge?

cleanup:

    ur_release( hold );
    ur_arrFree( &stack );
    if( blkN )
    {
        ur_setId( res, UT_BLOCK );
        ur_setSeries( res, blkN, 0 );
    }
    return blkN;

set_sol:

    if( sol )
    {
        cell->id.flags |= UR_FLAG_SOL;
        sol = 0;
    }
    goto start;

invalid_char:

    ur_error( ut, UR_ERR_SYNTAX,
              "Unprintable/Non-ASCII Input %d (line %d)", ch, lines + 1 );
    goto error;

error_msg:

    ur_error( ut, UR_ERR_SYNTAX, "%s (line %d)", errorMsg, lines + 1 );
    goto error;

error_token:

    {
    UBuffer etok;
    ur_binInit( &etok, 0 );
    ur_error( ut, UR_ERR_SYNTAX, "%s %s (line %d)",
              errorMsg, _errToken( &etok, token, end ), lines + 1 );
    ur_binFree( &etok );
    }

error:

    blkN = 0;
    goto cleanup;
}


//EOF
