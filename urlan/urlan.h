#ifndef URLAN_H
#define URLAN_H
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


#include <stdint.h>


#define UR_VERSION_STR  "0.1.0"
#define UR_VERSION      0x000100


enum UrlanDataType
{
    UT_UNSET,
    UT_DATATYPE,
    UT_NONE,
    UT_LOGIC,
    UT_CHAR,
    UT_INT,
    UT_DECIMAL,
    UT_BIGNUM,
    UT_TIME,
    UT_DATE,
    UT_COORD,
    UT_VEC3,
                /* The following reference buffers. */
    UT_WORD,
    UT_LITWORD,
    UT_SETWORD,
    UT_GETWORD,
    UT_OPTION,
                /* Series */
    UT_BINARY,
    UT_BITSET,
    UT_STRING,
    UT_FILE,
    UT_VECTOR,
    UT_BLOCK,
    UT_PAREN,
    UT_PATH,
    UT_LITPATH,
    UT_SETPATH,
                /* Other */
    UT_CONTEXT,
    UT_ERROR,

    UT_BI_COUNT,
    UT_MAX      = 64,
    UT_TYPEMASK = 99,
    UT_REFERENCE_BUF = UT_WORD,
};


/* Cell flags */

#define UR_FLAG_INT_HEX         0x01
#define UR_FLAG_PRINT_RECURSION 0x40
#define UR_FLAG_SOL             0x80


enum UrlanWordBindings
{
    UR_BIND_UNBOUND = 0,    /* ur_setId zeros flags so this is default. */
    UR_BIND_THREAD,
    UR_BIND_ENV,
    UR_BIND_USER
};


enum UrlanStringEncoding
{
    UR_ENC_LATIN1,
    UR_ENC_UTF8,
    UR_ENC_UCS2,
    UR_ENC_COUNT
};


enum UrlanReturnCode
{
    UR_THROW = 0,
    UR_OK    = 1
};


enum UrlanErrorType
{
    UR_ERR_TYPE,            /**< Invalid argument/parameter datatype. */ 
    UR_ERR_SCRIPT,          /**< General script evaluation error. */
    UR_ERR_SYNTAX,          /**< Syntax error. */
    UR_ERR_ACCESS,          /**< Problem accessing external resources. */
    UR_ERR_INTERNAL,        /**< Fatal internal problem. */
    UR_ERR_COUNT
};


#define UR_INVALID_BUF  0
#define UR_INVALID_HOLD -1


typedef int32_t     UIndex;
typedef uint16_t    UAtom;


typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint16_t _pad0;
}
UCellId;


typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint8_t  n;
    uint8_t  bitCount;
    uint32_t mask0;     /* LIMIT: Maximum of 96 datatypes. */
    uint32_t mask1;
    uint32_t mask2;
}
UCellDatatype;


typedef struct
{
    UCellId  id;
    int32_t  i;
    double   d;         /* On 8 byte boundary. */
}
UCellNumber;


#define UR_COORD_MAX    6

typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint16_t len;
    int16_t  n[ UR_COORD_MAX ];
}
UCellCoord;


typedef struct
{
    UCellId id;
    float num[3];
}
UCellVec3;


typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint8_t  binding;
    uint8_t  _pad0;
    UIndex   ctx;       /* Same location as UCellSeries buf. */
    uint16_t index;     /* LIMIT: Words per context. */
    UAtom    atom;
    UAtom    sel[2];
}
UCellWord;


typedef struct
{
    UCellId  id;
    UIndex   buf;       /* Buffer index.  Same location as UCellWord ctx. */
    UIndex   it;        /* Element iterator, slice start */
    UIndex   end;       /* Slice end */
}
UCellSeries;


typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint16_t exType;
    UIndex   messageStr;
    UIndex   traceBlk;
    UIndex   _pad0;
}
UCellError;


typedef union
{
    UCellId       id;
    UCellDatatype datatype;
    UCellWord     word;
    UCellNumber   number;
    UCellCoord    coord;
    UCellVec3     vec3;
    UCellSeries   series;
    UCellSeries   context;
    UCellSeries   port;
    UCellError    error;
}
UCell;


typedef struct UBuffer  UBuffer;

struct UBuffer
{
    uint8_t     type;
    uint8_t     elemSize;   // For array buffers.
    uint8_t     form;       // Encoding, etc.
    uint8_t     flags;
    UIndex      used;       // Number of elements (for series).
    union
    {
        UBuffer*    buf;    //!< Buffers
        UCell*      cell;   //!< Cells
        char*       c;      //!< chars
        uint8_t*    b;      //!< bytes
        int16_t*    i16;    //!< int16_t
        uint16_t*   u16;    //!< uint16_t
        int32_t*    i;      //!< int32_t
        uint32_t*   u32;    //!< uint32_t
        double*     d;      //!< doubles
        float*      f;      //!< floats
        void*       v;      //!< Other
    } ptr;
};


typedef struct UEnv         UEnv;
typedef struct UThread      UThread;
typedef struct UDatatype    UDatatype;


typedef enum
{
    UR_THREAD_INIT,
    UR_THREAD_FREE,
    UR_THREAD_FREEZE
}
UThreadMethod;


struct UThread
{
    UBuffer     dataStore;
    UBuffer     holds;
    UBuffer     gcBits;
    int32_t     freeBufCount;
    UIndex      freeBufList;
    UEnv*       env;
    UThread*    nextThread;
    UDatatype** types;
    const UCell* (*wordCell)( UThread*, const UCell* );
    UCell* (*wordCellM)( UThread*, const UCell* );
};


typedef struct
{
    const UBuffer* buf;
    UIndex it;
    UIndex end;
}
USeriesIter;

typedef struct
{
    UBuffer* buf;
    UIndex it;
    UIndex end;
}
USeriesIterM;

typedef struct
{
    const UBuffer* buf;
    const uint8_t* it;
    const uint8_t* end;
}
UBinaryIter;

typedef struct
{
    UBuffer* buf;
    uint8_t* it;
    uint8_t* end;
}
UBinaryIterM;


typedef struct
{
    const UBuffer* buf;
    const UCell* it;
    const UCell* end;
}
UBlockIter;

typedef struct
{
    UBuffer* buf;
    UCell* it;
    UCell* end;
}
UBlockIterM;


typedef struct
{
    const UBuffer* ctx;
    UIndex ctxN;
    int bindType;
}
UBindTarget;


enum UrlanCompareTest
{
    UR_COMPARE_SAME,
    UR_COMPARE_EQUAL,
    UR_COMPARE_ORDER
};

enum UrlanRecyclePhase
{
    UR_RECYCLE_MARK,
    UR_RECYCLE_SWEEP
};

struct UDatatype
{
    const char* name;

    int  (*make)      ( UThread*, const UCell* from, UCell* res );
    void (*copy)      ( UThread*, const UCell* from, UCell* res );
    int  (*compare)   ( UThread*, const UCell* a, const UCell* b, int test );
    int  (*select)    ( UThread*, const UCell* cell, UBlockIter* bi,UCell* res);
    void (*toString)  ( UThread*, const UCell* cell, UBuffer* str, int depth );
    void (*toText)    ( UThread*, const UCell* cell, UBuffer* str, int depth );

    void (*recycle)   ( UThread*, int phase );
    void (*mark)      ( UThread*, UCell* cell );
    void (*destroy)   ( UBuffer* buf );
    void (*markBuf)   ( UThread*, UBuffer* buf );
    void (*toShared)  ( UCell* cell );

    void (*bind)      ( UThread*, UCell* cell, const UBindTarget* bt );
};

 
enum UrlanFindOption
{
    UR_FIND_LAST = 1
};

typedef struct
{
    UDatatype dt;
    void (*pick)      ( const UBuffer* buf, UIndex n, UCell* res );
    void (*poke)      ( UBuffer* buf, UIndex n, const UCell* val );
    int  (*append)    ( UThread*, UBuffer* buf, const UCell* val );
    int  (*change)    ( UThread*, USeriesIterM* si, const UCell* val,
                        UIndex part );
    void (*remove)    ( UThread*, USeriesIterM* si, UIndex part );
    int  (*find)      ( UThread*, const UCell* ser, const UCell* val, int opt );
}
USeriesType;


#ifdef __cplusplus
extern "C" {
#endif

UThread* ur_makeEnv( UDatatype* dt, int dtCount,
                     unsigned int thrSize,
                     void (*thrMethod)(UThread*,UThreadMethod) );
void     ur_freeEnv( UThread* );
void     ur_freezeEnv( UThread* );
int      ur_datatypeCount( UThread* );
UAtom    ur_internAtom( UThread*, const char* it, const char* end );
UAtom*   ur_internAtoms( UThread*, const char* words, UAtom* atoms );
const char* ur_atomCStr( UThread*, UAtom atom );
void     ur_genBuffers( UThread*, int count, UIndex* index );
void     ur_destroyBuffer( UThread*, UBuffer* );
UIndex   ur_holdBuffer( UThread*, UIndex bufN );
void     ur_releaseBuffer( UThread*, UIndex hold );
void     ur_recycle( UThread* );
int      ur_markBuffer( UThread*, UIndex bufN );
int      ur_error( UThread*, int errorType, const char* fmt, ... );
UBuffer* ur_errorBlock( UThread* );
UBuffer* ur_threadContext( UThread* );
UBuffer* ur_envContext( UThread* );
void     ur_appendTrace( UThread*, UIndex blkN, UIndex it );
UIndex   ur_tokenize( UThread*, const char* it, const char* end, UCell* res );
void     ur_toStr( UThread*, const UCell* cell, UBuffer* str, int depth );
void     ur_toText( UThread*, const UCell* cell, UBuffer* str );
const UCell* ur_wordCell( UThread*, const UCell* cell );
UCell*   ur_wordCellM( UThread*, const UCell* cell );
int      ur_setWord( UThread*, const UCell* wordCell, const UCell* src );
const UBuffer* ur_bufferEnv( UThread*, UIndex n );
const UBuffer* ur_bufferSeries( UThread*, const UCell* cell );
UBuffer* ur_bufferSeriesM( UThread*, const UCell* cell );
void     ur_seriesSlice( UThread*, USeriesIter* si, const UCell* cell );
int      ur_seriesSliceM( UThread*, USeriesIterM* si, const UCell* cell );
void     ur_bind( UThread*, UBuffer* blk, const UBuffer* ctx, int bindType );
void     ur_bindCells( UThread*, UCell* it, UCell* end, const UBindTarget* bt );
int      ur_isTrue( const UCell* cell );
int      ur_same( UThread*, const UCell* a, const UCell* b );
int      ur_equal( UThread*, const UCell* a, const UCell* b );
int      ur_compare( UThread*, const UCell* a, const UCell* b );
double   ur_now();

void     ur_makeDatatype( UCell* cell, int type );
int      ur_isDatatype( const UCell* cell, const UCell* datatype );
void     ur_datatypeAddType( UCell* cell, int type );

int      ur_charLowercase( int c );
int      ur_charUppercase( int c );

UIndex   ur_makeBinary( UThread*, int size );
UBuffer* ur_makeBinaryCell( UThread*, int size, UCell* cell );
void     ur_binInit( UBuffer*, int size );
void     ur_binReserve( UBuffer*, int size );
void     ur_binExpand( UBuffer*, int index, int count );
void     ur_binErase( UBuffer*, int start, int count );
void     ur_binAppendData( UBuffer*, const uint8_t* data, int len );
void     ur_binFree( UBuffer* );
void     ur_binSlice( UThread*, UBinaryIter*, const UCell* cell );
int      ur_binSliceM( UThread*, UBinaryIterM*, const UCell* cell );
const char* ur_binAppendHex( UBuffer* buf, const char* it, const char* end );

UIndex   ur_makeString( UThread*, int enc, int size );
UBuffer* ur_makeStringCell( UThread*, int enc, int size, UCell* cell );
UIndex   ur_makeStringUtf8( UThread*, const uint8_t* it, const uint8_t* end );
void     ur_strInit( UBuffer*, int enc, int size );
void     ur_strAppendCStr( UBuffer*, const char* );
void     ur_strAppendChar( UBuffer*, int );
void     ur_strAppendInt( UBuffer*, int32_t );
void     ur_strAppendInt64( UBuffer*, int64_t );
void     ur_strAppendHex( UBuffer*, uint32_t n, uint32_t hi );
void     ur_strAppendDouble( UBuffer*, double );
void     ur_strAppendIndent( UBuffer*, int depth );
void     ur_strAppend( UBuffer*, const UBuffer* strB, UIndex itB, UIndex endB );
void     ur_strTermNull( UBuffer* );
int      ur_strLowercase( UThread*, const UCell* cell );
int      ur_strUppercase( UThread*, const UCell* cell );
UIndex   ur_strFindChar( const UBuffer*, UIndex start, UIndex end, int ch );
UIndex   ur_strFindChars( const UBuffer*, UIndex start, UIndex end,
                          uint8_t* charSet, int len );
#define  ur_strFree ur_arrFree
#define  ur_strIsUcs2(buf)  ((buf)->form == UR_ENC_UCS2)

UIndex   ur_makeBlock( UThread*, int size );
UBuffer* ur_makeBlockCell( UThread*, int type, int size, UCell* cell );
UIndex   ur_blkClone( UThread*, UIndex blkN );
void     ur_blkInit( UBuffer*, int type, int size );
UCell*   ur_blkAppendNew( UBuffer*, int type );
void     ur_blkAppendCells( UBuffer*, const UCell* cells, int count );
void     ur_blkPush( UBuffer*, const UCell* cell );
UCell*   ur_blkPop( UBuffer* );
void     ur_blkSlice( UThread*, UBlockIter*, const UCell* cell );
int      ur_blkSliceM( UThread*, UBlockIterM*, const UCell* cell );
#define  ur_blkFree ur_arrFree

int      ur_pathCell( UThread*, const UCell* pc, UCell* res );
int      ur_setPath( UThread*, const UCell* path, const UCell* src );

UIndex   ur_makeContext( UThread*, int size );
UBuffer* ur_makeContextCell( UThread*, int size, UCell* cell );
UBuffer* ur_ctxClone( UThread*, const UBuffer* src, UCell* cell );
void     ur_ctxInit( UBuffer*, int size );
void     ur_ctxReserve( UBuffer*, int size );
void     ur_ctxFree( UBuffer* );
void     ur_ctxSetWords( UBuffer*, const UCell* it, const UCell* end );
int      ur_ctxAddWordI( UBuffer*, UAtom atom );
UCell*   ur_ctxAddWord( UBuffer*, UAtom atom );
int      ur_ctxLookup( const UBuffer*, UAtom atom );
#define  ur_ctxCell(c,n)    ((c)->ptr.cell + n)

void     ur_arrInit( UBuffer*, int size, int count );
void     ur_arrReserve( UBuffer*, int count );
void     ur_arrExpand( UBuffer*, int index, int count );
void     ur_arrErase( UBuffer*, int start, int count );
void     ur_arrFree( UBuffer* );
void     ur_arrAppendInt32( UBuffer*, int32_t );

#ifdef __cplusplus
}
#endif


#define ur_ptr(T,buf)       ((T*) (buf)->ptr.v)
#define ur_avail(buf)       (buf)->ptr.i[-1]
#define ur_testAvail(buf)   (buf->ptr.v ? ur_avail(buf) : 0)

#define ur_arrExpand1(T,buf,elem) \
    ur_arrReserve(buf, buf->used + 1); \
    elem = ur_ptr(T,buf) + buf->used; \
    ++buf->used

#define ur_type(c)          (c)->id.type
#ifdef __BIG_ENDIAN__
#define ur_setId(c,t)       *((uint32_t*) (c)) = (t) << 24
#else
#define ur_setId(c,t)       *((uint32_t*) (c)) = t
#endif
#define ur_setFlags(c,m)    (c)->id.flags |= m
#define ur_clrFlags(c,m)    (c)->id.flags &= ~(m)
#define ur_flags(c,m)       ((c)->id.flags & m)
#define ur_is(c,t)          ((c)->id.type == (t))

#define ur_atom(c)          (c)->word.atom
#define ur_datatype(c)      (c)->datatype.n
#define ur_int(c)           (c)->number.i
#define ur_decimal(c)       (c)->number.d

#define ur_isWordType(t)    ((t) >= UT_WORD && (t) <= UT_OPTION)
#define ur_binding(c)       (c)->word.binding
#define ur_setBinding(c,bindType)   (c)->word.binding = bindType

#define ur_isBlockType(t)   ((t) >= UT_BLOCK && (t) <= UT_SETPATH)
#define ur_isStringType(t)  ((t) == UT_STRING || (t) == UT_FILE)

#define ur_isSeriesType(t)  ((t) >= UT_BINARY && (t) < UT_CONTEXT)
#define ur_isShared(n)      (n < 0)
#define ur_isSliced(c)      ((c)->series.end > -1)

#define ur_setWordUnbound(c,atm) \
    (c)->word.atom = atm; \
    (c)->word.ctx = UR_INVALID_BUF

//    (c)->word.index = -1

#define ur_setSeries(c,bn,sit) \
    (c)->series.buf = bn; \
    (c)->series.it = sit; \
    (c)->series.end = -1

#define ur_setSlice(c,bn,sit,send) \
    (c)->series.buf = bn; \
    (c)->series.it = sit; \
    (c)->series.end = send

#define ur_hold(n)          ur_holdBuffer(ut,n)
#define ur_release(h)       ur_releaseBuffer(ut,h)
#define ur_buffer(n)        (ut->dataStore.ptr.buf + (n))
#define ur_bufferE(n)       ur_bufferEnv(ut,n)
#define ur_bufferSer(c)     ur_bufferSeries(ut,c)
#define ur_bufferSerM(c)    ur_bufferSeriesM(ut,c)

#define ur_foreach(bi)      for(; bi.it != bi.end; ++bi.it)


#endif  /*EOF*/
