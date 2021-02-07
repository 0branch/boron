/*
  Copyright 2009-2019 Karl Robillard

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
#include "mem_util.h"
#include "env.h"
#include "os.h"

#ifdef _MSC_VER
#define inline  __inline
#endif

extern double ur_stringToDate(const uint8_t*,const uint8_t*,const uint8_t**);
extern int vector_append( UThread*, UBuffer*, const UCell* );

#define isDigit(v)     (('0' <= v) && (v <= '9'))


/**
  Converts string until end is reached or a non-digit is found.
  If pos is non-zero then it is set to point past the last valid character.
  If a non-digit is found then the number scanned up to that point is
  returned.
*/
int64_t str_toInt64( const uint8_t* start, const uint8_t* end,
                     const uint8_t** pos )
{
    int cn;
    int64_t n = 0;
    const uint8_t* cp = start;

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


int64_t str_hexToInt64( const uint8_t* start, const uint8_t* end,
                        const uint8_t** pos )
{
    int cn;
    int64_t n = 0;
    const uint8_t* cp = start;

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
double str_toDouble( const uint8_t* start, const uint8_t* end,
                     const uint8_t** pos )
{
#if 1
    char tmp[28];
    char* tpos;
    double n;
    int len = end - start;

    if( len > 26 )
        len = 26;
    memCpy( tmp, start, len );
    tmp[ len ] = '\0';
    n = strtod( tmp, &tpos );
    if( pos )
        *pos = start + (tpos - tmp);
    return n;
#else
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
#endif
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


double str_toTime( const uint8_t* start, const uint8_t* end,
                   const uint8_t** pos )
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


const uint8_t* str_toCoord( UCell* cell, const uint8_t* it, const uint8_t* end )
{
    int16_t* nit  = cell->coord.n;
    int16_t* nend = nit + UR_COORD_MAX;

    for( ; it != end; ++it )
    {
        if( *it == ' ' || *it == '\t' )
            continue;
        *nit++ = (int16_t) str_toInt64( it, end, &it );
        if( nit == nend || it == end || *it != ',' )
            break;
    }

    cell->coord.len = nit - cell->coord.n;
    if( cell->coord.len < 2 )
        cell->coord.len = 2;

    while( nit != nend )
        *nit++ = 0;

     return it;
}


const uint8_t* str_toVec3( UCell* cell, const uint8_t* it, const uint8_t* end )
{
    float* nit  = cell->vec3.xyz;
    float* nend = nit + 3;

    for( ; it != end; ++it )
    {
        if( *it == ' ' || *it == '\t' )
            continue;
        *nit++ = (float) str_toDouble( it, end, &it );
        if( nit == nend )
            break;
        if( (it == end) || *it != ',' )
            break;
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
uint8_t charset_word[32] = {
    0x00,0x00,0x00,0x00,0x42,0x6C,0xFF,0xF3,
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
    else if( c == '(' )
    {
        int nib;
        c = 0;
        while( it != end )
        {
            nib = *it++;
            if( ! IS_HEX(nib) )
                break;          // Skipping what should be ')' here.
            c = (c << 4) + hexNibble(nib);
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


extern int trim_indent_char( char* it, char* end );

/*
  Get range of mutli-line string to be automatically un-indented.
  Return non-zero if valid string found and marked with strStart & strEnd.
*/
static int _bracketNewline( const uint8_t* start, const uint8_t* end,
                            const uint8_t** strStart, const uint8_t** strEnd )
{
    const uint8_t* it = start;
    const uint8_t* lastLf;
    const int LF = 10;
    const int CR = 13;
    int len, count;

    // Must start with two or more left curly brackets followed by a newline.
    while( it != end && *it == '{' )
        ++it;
    len = it - start;
    if( it == end || len < 2 )
        return 0;
    if( *it == CR )
    {
        if( ++it == end )
            return 0;
    }
    if( *it != LF )
        return 0;
    lastLf = it++;
    start = it;

    // Scan for same number of right curly brackets followed by a newline.
    while( it != end )
    {
        if( *it == '}' )
        {
            for( count = 1, ++it; it != end && *it == '}'; ++count, ++it )
                ;
            if( count == len && it != end && (*it == LF || *it == CR) )
            {
                *strStart = start;
                *strEnd   = lastLf + 1;
                return len;
            }
        }

        if( *it == LF )
            lastLf = it;
        ++it;
    }
    return 0;
}


static void _errorToken( UThread* ut, const char* msg, int lines,
                         const uint8_t* start, const uint8_t* end )
{
    char buf[ 32 ];
    const uint8_t* it = start;
    int n;

    while( it != end )
    {
        n = *it++;
        if( IS_DELIM(n) )
            break;
    }
    n = it - start;
    if( n > 31 )
        n = 31;
    memCpy( buf, start, n );
    buf[ n ] = '\0';

    ur_error( ut, UR_ERR_SYNTAX, "%s \"%s\" (line %d)", msg, buf, lines );
}


// Return one-based number of newlines between it & end.
static int _lineCount( const uint8_t* it, const uint8_t* end )
{
    int count = 1;
    while( it != end )
    {
        if( *it++ == '\n' )
            ++count;
    }
    return count;
}


// Return non-zero if path is multi-type datatype! and cell was replaced.
static int _convertToDatatype( UCell* cell, int len )
{
    const UCell* it  = cell;
    const UCell* end = cell + len;
    for( ; it != end; ++it )
    {
        if( ! ur_is(it, UT_WORD) || ur_atom(it) >= UT_MAX )
            return 0;
    }
    ur_makeDatatype( cell, ur_atom(cell) );
    for( it = cell + 1; it != end; ++it )
        ur_datatypeAddType( cell, ur_atom(it) );
    return 1;
}


static UCell* _makeStringEnc( UThread* ut, UIndex blkN, const uint8_t* it,
                              const uint8_t* end, int enc )
{
    UIndex strN;
    UCell* cell;

    if( enc == UR_ENC_LATIN1 )
        strN = ur_makeStringLatin1( ut, it, end );
    else
        strN = ur_makeStringUtf8( ut, it, end );
    cell = ur_blkAppendNew( ur_buffer(blkN), UT_STRING );
    ur_setSeries( cell, strN, 0 );
    return cell;
}


enum TokenOps
{
    SKIP,   // "\t\r "  Whitespace
    NL,     //  \n      Newline
    COM_L,  //  ;       Line comment
    HASH,   //  #       UT_BINARY, UT_VECTOR, UT_WORD
    SLASH,  //  /       Block comment, UT_OPTION, UT_PATH
    STR,    //  "       UT_STRING
    STRB,   //  {       UT_STRING
    BLK,    // "[("     UT_BLOCK, UT_PAREN
    BLK_E,  // "])"     End block
    LIT,    //  '       UT_LITWORD, UT_LITPATH, UT_CHAR
    SIGN,   // "+-"     UT_INT, UT_DOUBLE, UT_VEC3, UT_COORD, UT_WORD
    ZERO,   // 0        Hex number, UT_INT, UT_DOUBLE
    DIGIT,  // 1-9      UT_INT, UT_DOUBLE, UT_VEC3, UT_COORD, UT_DATE, UT_TIME
    ALPHA,  // A-Z a-z  UT_SETWORD, UT_WORD
    FIL,    //  %       UT_FILE
    COLON,  //  :       UT_GETWORD
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
    SKIP,   ALPHA,  STR,    HASH,   ALPHA,  FIL,    ALPHA,  LIT,
//  (       )       *       +       ,       -       .       /
    BLK,    BLK_E,  ALPHA,  SIGN,   INV,    SIGN,   ALPHA,  SLASH,
//  0       1       2       3       4       5       6       7
    ZERO,   DIGIT,  DIGIT,  DIGIT,  DIGIT,  DIGIT,  DIGIT,  DIGIT,
//  8       9       :       ;       <       =       >       ?
    DIGIT,  DIGIT,  COLON,  COM_L,  ALPHA,  ALPHA,  ALPHA,  ALPHA,
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
    ALPHA,  ALPHA,  ALPHA,  STRB,   ALPHA,  ALPHA,  ALPHA
};


extern UAtom ur_internAtomUnlocked( UThread*, const char*, const char* );

/**
  \ingroup urlan_core

  Parse UTF-8 or Latin1 data into block.

  \param blkN           Index of initialized block buffer.
  \param inputEncoding  UR_ENC_UTF8 or UR_ENC_LATIN1
  \param start          Pointer to start of input data.
  \param end            Pointer to end of input data.

  \return UR_OK if all input successfully parsed, or UR_THROW on syntax error.
*/
UStatus ur_tokenizeB( UThread* ut, UIndex blkN, int inputEncoding,
                      const uint8_t* start, const uint8_t* end )
{
#define STACK   stack.ptr.i32
#define BLOCK   ur_buffer( STACK[stack.used - 1] )
#define CCP     (const char*)
    UBuffer stack;
    UBuffer* blk;
    UCell* cell;
    UAtom (*intern)( UThread*, const char*, const char* );
    const char* errorMsg;
    const uint8_t* token;
    const uint8_t* it = start;
    UIndex vectorN = UR_INVALID_BUF;
    UIndex vectorPos = 0;
    int mode;
    int tokenState = 0;
    int sol = 0;
    int ch;

#define CS_NEXT ((it == end) ? -1 : *it++)
#define TOK_END ((ch < 0) ? it : it-1)

#define syntaxError(msg) \
    errorMsg = msg; \
    goto error_msg

#define syntaxErrorT(msg) \
    errorMsg = msg; \
    goto error_token

    intern = ut->env->threadCount ? ur_internAtom : ur_internAtomUnlocked;

    ur_arrInit( &stack, sizeof(UIndex), 32 );
    ur_arrAppendInt32( &stack, blkN );

next:
    ch = CS_NEXT;
proc:
    while( ch > 0 )
    {
        if( ch > 126 )
            goto invalid_char;
        switch( firstCharOp[ ch ] )
        {
        case SKIP:
            goto next;

        case NL:
            sol = 1;
            tokenState = 0;
            goto next;

        case BLK:
        {
            UIndex newBlkN = ur_makeBlock( ut, 0 );
            cell = ur_blkAppendNew( BLOCK, (ch == '[') ? UT_BLOCK : UT_PAREN );
            ur_setSeries( cell, newBlkN, 0 );
            ur_arrAppendInt32( &stack, newBlkN );
            tokenState = 0;
        }
            goto next_sol;

        case BLK_E:
            if( vectorN )
            {
                vectorN = UR_INVALID_BUF;
                BLOCK->used = vectorPos;
                goto next;
            }
            if( stack.used == 1 )
            {
                ur_error( ut, UR_ERR_SYNTAX,
                          "End of block '%c' has no opening match (line %d)",
                          ch, _lineCount(start, it) );
                goto error;
            }
            --stack.used;
            tokenState = 0;
            sol = 0;
            goto next;

        case SIGN:
            token = it - 1;
            if( (ch = CS_NEXT) > 0 && isDigit(ch) )
                goto number;
            if( IS_WORD(ch) )
                goto word;
            goto push_word;

        case ZERO:
            token = it - 1;
            if( (ch = CS_NEXT) == 'x' )
            {
                token = it;
                while( (ch = CS_NEXT) > 0 && IS_HEX(ch) )
                    ;
                cell = ur_blkAppendNew( BLOCK, UT_INT );
                ur_int(cell) = str_hexToInt64( token, TOK_END, NULL );
                ur_setFlags( cell, UR_FLAG_INT_HEX );
                goto set_sol;
            }
            if( isDigit(ch) )
                goto number;
            goto num_dot;

        case DIGIT:
            token = it - 1;
number:
            while( (ch = CS_NEXT) > 0 && isDigit(ch) )
                ;
num_dot:
            mode = UT_INT;
            blk = BLOCK;

            if( ch=='.' )
            {
                mode = UT_DOUBLE;
                while( (ch = CS_NEXT) > 0 && isDigit(ch) )
                    ;
            }
            else if( ch == ':' )
            {
#ifdef CONFIG_TIMECODE
                if( _isTimecode( it, end ) )
                {
                    cell = ur_blkAppendNew( blk, UT_TIMECODE );
                    it = ur_stringToTimeCode( token, end, cell );
                    goto next_sol;
                }
#endif
                cell = ur_blkAppendNew( blk, UT_TIME );
                ur_double(cell) = str_toTime( token, end, &it );
                goto next_sol;
            }
            else if( ch == '-' )
            {
                const uint8_t* pos;
                cell = ur_blkAppendNew( blk, UT_DATE );
                ur_double(cell) = ur_stringToDate( token, end, &pos );
                it = pos;
                if( it == token )
                {
                    syntaxErrorT( "Invalid date" );
                }
                goto next_sol;
            }

            if( ch=='e' || ch=='E' )
            {
                mode = UT_DOUBLE;
                while( (ch = CS_NEXT) > 0 &&
                       (isDigit(ch) || ch == '+' || ch == '-') )
                    ;
            }

            if( mode == UT_DOUBLE )
            {
                if( ch == ',' )
                {
                    cell = ur_blkAppendNew( blk, UT_VEC3 );
                    it = str_toVec3( cell, token, end );
                    goto next_sol;
                }
                cell = ur_blkAppendNew( blk, UT_DOUBLE );
                ur_double(cell) = strtod( CCP token, NULL );
            }
            else
            {
                if( ch == ',' )
                {
                    cell = ur_blkAppendNew( blk, UT_COORD );
                    it = str_toCoord( cell, token, end );
                    goto next_sol;
                }
                cell = ur_blkAppendNew( blk, UT_INT );
                ur_int(cell) = str_toInt64( token, TOK_END, NULL );
            }

            if( vectorN )
            {
                UBuffer* vec = ur_buffer(vectorN);
                if( vec->used == 0 && vec->form == UR_VEC_I32 &&
                    ur_is(cell, UT_DOUBLE) )
                    vec->form = UR_VEC_F32;
                if( ! vector_append( ut, ur_buffer(vectorN), cell ) )
                    goto error;
                blk->used = vectorPos;
            }
            goto set_sol;

        case ALPHA:
            token = it - 1;
word:
            while( (ch = CS_NEXT) > 0 && IS_WORD(ch) )
                ;
push_word:
            if( tokenState == UT_GETWORD ||
                tokenState == UT_LITWORD ||
                tokenState == UT_OPTION )
            {
                mode = tokenState;
                tokenState = 0;
            }
            else if( ch == ':' )
                mode = UT_SETWORD;
            else
                mode = UT_WORD;
            blk = BLOCK;
            cell = ur_blkAppendNew( blk, mode );
            ur_setWordUnbound(cell, intern(ut, CCP token, CCP TOK_END));
            if( ch == ':' )
                ch = CS_NEXT;
            else if( ch == '/' )
                goto path;
            goto set_sol;

path:
            mode = blk->used - 1;       // Save start position of path.
path_seg:
            token = it;
            ch = CS_NEXT;
            if( isDigit(ch) || ch == '-' )
            {
                cell = ur_blkAppendNew( blk, UT_INT );
                ur_int(cell) = str_toInt64( token, end, &it );
                ch = CS_NEXT;
            }
            else
            {
                int wt = UT_WORD;
                if( ch == ':' )
                {
                    wt = UT_GETWORD;
                    ++token;
                }
                else if( ch == '\'' )
                {
                    wt = UT_LITWORD;
                    ++token;
                }
                else if( ! IS_WORD(ch) )
                {
                    --token;
                    syntaxErrorT( "Invalid path segment" );
                }
                while( (ch = CS_NEXT) > 0 && IS_WORD(ch) )
                    ;
                cell = ur_blkAppendNew( blk, wt );
                ur_setWordUnbound( cell, intern(ut, CCP token, CCP TOK_END) );
            }
            if( ch == '/' )
                goto path_seg;

            {
                int len;
                len = blk->used - mode;
                cell = blk->ptr.cell + mode;
                blk->used = mode + 1;
                if( ! _convertToDatatype( cell, len ) )
                {
                    // Move cells from code block to path buffer.
                    int pt = UT_PATH;
                    UIndex bufN = ur_makeBlock( ut, len );      // gc!
                    if( ur_is(cell, UT_LITWORD) )
                    {
                        cell->id.type = UT_WORD;
                        pt = UT_LITPATH;
                    }
                    else if( ch == ':' )
                    {
                        ch = CS_NEXT;
                        pt = UT_SETPATH;
                    }
                    ur_blkAppendCells( ur_buffer(bufN), cell, len );
                    ur_initSeries( cell, pt, bufN );
                }
            }
            goto set_sol;

        case STR:
            token = it;
            while( (ch = CS_NEXT) > 0 )
            {
                if( ch == '^' )
                {
                    if( CS_NEXT < 0 )
                        break;
                }
                if( ch == '"' )
                    goto push_string;
                if( ch == '\n' )
                    break;
            }
            goto str_term;

        case STRB:
            if( _bracketNewline(it - 1, end, &token, &it) )
            {
                UBuffer* str;
                cell = _makeStringEnc( ut, STACK[stack.used - 1],
                                       token, it, inputEncoding );
                str = ur_buffer( cell->series.buf );
                str->used -= trim_indent_char( str->ptr.c,
                                               str->ptr.c + str->used );
                while( *it != '\n' )
                    ++it;
                goto next_sol;
            }
            token = it;
            mode = 0;       // Nested depth.
            while( (ch = CS_NEXT) > 0 )
            {
                if( ch == '^' )
                {
                    if( CS_NEXT < 0 )
                        break;
                }
                else if( ch == '{' )
                {
                    ++mode;
                }
                else if( ch == '}' )
                {
                    if( mode )
                        --mode;
                    else
                        goto push_string;
                }
            }
str_term:
            syntaxError( "String not terminated" );
push_string:
            cell = _makeStringEnc( ut, STACK[stack.used - 1],
                                   token, TOK_END, inputEncoding );
            goto next_sol;

        case HASH:
            ch = CS_NEXT;
            blk = BLOCK;

            // Use previous value abutted to '#' as the form indicator.
            // The new vector value can be created in it's place.
            cell = NULL;
            if( it - start > 2 )
            {
                mode = it[-3];
                if( isDigit(mode) )
                    cell = blk->ptr.cell + blk->used - 1;
            }

            if( ch == '{' )
            {
                UBuffer* bin;
                const char* tend;

                token = it;
                while( (ch = CS_NEXT) > 0 && ch != '}' )
                    ;
                if( ch < 0 )
                    goto invalid_binary;

                if( cell )
                {
                    if( ! ur_is(cell, UT_INT) )
                        goto invalid_binary;
                    switch( ur_int(cell) )
                    {
                        case 2:
                            mode = UR_BENC_2;
                            break;
                        case 16:
                            mode = UR_BENC_16;
                            break;
                        case 64:
                            mode = UR_BENC_64;
                            break;
                        default:
                            goto invalid_binary;
                    }
                }
                else
                {
                    cell = ur_blkAppendNew( blk, UT_NONE );
                    mode = UR_BENC_16;
                }
                bin = ur_makeBinaryCell( ut, 0, cell );
                bin->form = mode;
                tend = (const char*) TOK_END;
                if( ur_binAppendBase( bin, CCP token, tend, mode ) == tend )
                    goto next_sol;
invalid_binary:
                syntaxError( "Invalid binary" );
            }
            else if( ch == '[' )
            {
                if( cell )
                {
                    mode = ur_is(cell, UT_WORD) ? ur_atom(cell) : 0;
                    if( mode < UR_VEC_I16 || mode > UR_VEC_F64 )
                    {
                        syntaxError( "Invalid vector form" );
                    }
                }
                else
                {
                    mode = UR_VEC_I32;
                    cell = ur_blkAppendNew( blk, UT_NONE );
                }
                ur_makeVectorCell( ut, mode, 0, cell );
                vectorN = cell->series.buf;
                vectorPos = blk->used;
                goto next_sol;
            }
            token = it - 1;
            goto push_word;

        case SLASH:
            ch = CS_NEXT;
            if( ch == '*' )
            {
                mode = 0;
                while( (ch = CS_NEXT) > 0 )
                {
                    if( ch == '/' && mode == '*' )
                        goto next;
                    mode = ch;
                }
                goto proc;
            }
            else if( ! IS_WORD(ch) )
            {
                token = it - 2;
                goto push_word;
            }
            tokenState = UT_OPTION;
            break;

        case FIL:
            token = it;
            ch = CS_NEXT;
            if( ch == '"' )
            {
                ++token;
                while( (ch = CS_NEXT) > 0 && ch != '"' )
                {
                    if( ch == '\n' )
                    {
                        syntaxError( "File has no closing quote" );
                    }
                }
            }
            else
            {
                do
                {
                    if( IS_DELIM( ch ) )
                        break;
                }
                while( (ch = CS_NEXT) > 0 );
            }
            {
            UIndex bufN;
            bufN = ur_makeStringUtf8( ut, token, TOK_END );     // gc!
            cell = ur_blkAppendNew( BLOCK, UT_FILE );
            ur_setSeries( cell, bufN, 0 );
            }
            if( token[-1] == '"' )
                ch = CS_NEXT;
            goto set_sol;

        case LIT:
            // TODO: Handle Latin1
            mode = ur_charUtf8ToUcs2( it, end, &it );
            if( mode >= 0 )
            {
                cell = ur_blkAppendNew( BLOCK, UT_CHAR );
                ur_char(cell) = mode;
                goto next_sol;
            }
            tokenState = UT_LITWORD;
            goto next;

        case COLON:
            tokenState = UT_GETWORD;
            goto next;

        case COM_L:
            while( (ch = CS_NEXT) && ch != '\n' )
                ;
            sol = 1;
            break;

        case INV:
            goto invalid_char;
        }
    }

    if( stack.used > 1 )
    {
        syntaxError( "Block or paren not closed" );
    }

    ur_arrFree( &stack );
    return UR_OK;

next_sol:
    ch = CS_NEXT;

set_sol:
    if( sol )
    {
        cell->id.flags |= UR_FLAG_SOL;
        sol = 0;
    }
    goto proc;

invalid_char:
    ur_error( ut, UR_ERR_SYNTAX, "Unprintable/Non-ASCII Input %d (line %d)",
              ch, _lineCount(start, it) );
    goto error;

error_msg:
    ur_error( ut, UR_ERR_SYNTAX, "%s (line %d)",
              errorMsg, _lineCount(start, it) );
    goto error;

error_token:
    _errorToken( ut, errorMsg, _lineCount(start, it), token, end );

error:
    ur_arrFree( &stack );
    return UR_THROW;
}


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
    UIndex hold;
    UIndex bufN;
    UStatus ok;

    ur_makeBlockCell( ut, UT_BLOCK, 0, res );
    bufN = res->series.buf;

    hold = ur_hold( bufN );
    ok = ur_tokenizeB( ut, bufN, UR_ENC_UTF8,
                       (const uint8_t*) it, (const uint8_t*) end );
    ur_release( hold );

    return ok ? bufN : 0;
}


//EOF
