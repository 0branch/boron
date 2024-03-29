#ifndef URLAN_H
#define URLAN_H
/*
  Copyright 2009-2015,2019 Karl Robillard

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


#ifdef __sun__
#include <inttypes.h>
#ifdef _BIG_ENDIAN
#define __BIG_ENDIAN__
#endif
#else
#include <stdint.h>
#endif


#define UR_VERSION_STR  "2.0.8"
#define UR_VERSION      0x020008


enum UrlanDataType
{
    UT_UNSET,
    UT_DATATYPE,
    UT_NONE,
    UT_LOGIC,
    UT_CHAR,
    UT_INT,
    UT_DOUBLE,
    UT_BIGNUM,
    UT_TIME,
    UT_DATE,
    UT_COORD,
    UT_VEC3,
    UT_TIMECODE,
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
    UT_HASHMAP,
    UT_ERROR,

    UT_BI_COUNT,
    UT_MAX      = 64,
    UT_TYPEMASK = 99,
    UT_REFERENCE_BUF = UT_WORD
};


/* Cell flags */

#define UR_FLAG_INT_HEX         0x01
#define UR_FLAG_TIMECODE_DF     0x01
#define UR_FLAG_ERROR_SKIP_TRACE 0x01
#define UR_FLAG_PRINT_RECURSION 0x40
#define UR_FLAG_SOL             0x80


enum UrlanWordBindings
{
    UR_BIND_UNBOUND = 0, ///< ur_setId() zeros binding so this is default.
    UR_BIND_THREAD,      ///< Bound to buffer in thread dataStore.
    UR_BIND_ENV,         ///< Bound to buffer in shared env dataStore.
    UR_BIND_STACK,       ///< Bound to thread stack.
    UR_BIND_SELF,        ///< Evaluate to bound context rather than value.
    UR_BIND_SECURE,      ///< As UR_BIND_THREAD but unbind if not in context.
    UR_BIND_USER         ///< Start of user defined bindings.
};


enum UrlanBinaryEncoding
{
    UR_BENC_16,
    UR_BENC_2,
    UR_BENC_64,
    UR_BENC_COUNT
};


enum UrlanStringEncoding
{
    UR_ENC_LATIN1,
    UR_ENC_UTF8,
    UR_ENC_UCS2,
    UR_ENC_COUNT
};


typedef enum
{
    UR_THROW = 0,
    UR_OK    = 1
}
UStatus;


enum UrlanErrorType
{
    UR_ERR_TYPE,            /**< Invalid argument/parameter datatype. */ 
    UR_ERR_SCRIPT,          /**< General script evaluation error. */
    UR_ERR_SYNTAX,          /**< Syntax error. */
    UR_ERR_ACCESS,          /**< Problem accessing external resources. */
    UR_ERR_INTERNAL,        /**< Fatal internal problem. */
    UR_ERR_COUNT
};


enum UrlanVectorType
{
    UR_VEC_I16 = 66,        /* Currently the same as atoms. */
    UR_VEC_U16,
    UR_VEC_I32,
    UR_VEC_U32,
    UR_VEC_F32,
    UR_VEC_F64
};


#define UR_INVALID_BUF  0
#define UR_INVALID_HOLD -1
#define UR_INVALID_ATOM 0xffff


typedef int32_t     UIndex;
typedef uint16_t    UAtom;


typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint16_t ext;
}
UCellId;


typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint8_t  n;
    uint8_t  _pad0;
    uint32_t mask0;     /* LIMIT: Maximum of 96 datatypes. */
    uint32_t mask1;
    uint32_t mask2;
}
UCellDatatype;


typedef struct
{
    UCellId  id;
    int32_t  i32;
    union {
        int64_t  i64;
        double   d;
    } eb;
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
    float xyz[3];
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
        UBuffer*    buf;    //!< Array of buffers
        UCell*      cell;   //!< Array of cells
        char*       c;      //!< chars
        uint8_t*    b;      //!< bytes
        int16_t*    i16;    //!< int16_t
        uint16_t*   u16;    //!< uint16_t
        int32_t*    i;      //!< int32_t
        int32_t*    i32;    //!< int32_t
        uint32_t*   u32;    //!< uint32_t
        double*     d;      //!< doubles
        float*      f;      //!< floats
        void*       v;      //!< Other
    } ptr;
};


/* Buffer flags */
#define UR_STRING_ENC_UP    0x01


typedef struct UEnv         UEnv;
typedef struct UThread      UThread;
typedef struct UDatatype    UDatatype;


enum UThreadMethod
{
    UR_THREAD_INIT,
    UR_THREAD_FREE,
    UR_THREAD_FREEZE
};


struct UThread
{
    UBuffer     dataStore;
    UBuffer     stack;
    UBuffer     holds;
    UBuffer     gcBits;
    UCell       tmpWordCell;
    int32_t     freeBufCount;
    UIndex      freeBufList;
    UBuffer*    sharedStoreBuf;
    UEnv*       env;
    const UDatatype** types;
    const UCell* (*wordCell)( UThread*, const UCell* );
    UCell* (*wordCellM)( UThread*, const UCell* );
};

#define UR_MAIN_CONTEXT     1


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
    const UCell* it;
    const UCell* end;
}
UBlockIt;


typedef struct
{
    UCell* it;
    UCell* end;
}
UBlockItM;


typedef struct
{
    const UBuffer* ctx;
    UIndex ctxN;
    int bindType;
    UAtom self;
}
UBindTarget;


enum UrlanCompareTest
{
    UR_COMPARE_SAME,
    UR_COMPARE_EQUAL,
    UR_COMPARE_EQUAL_CASE,
    UR_COMPARE_ORDER,
    UR_COMPARE_ORDER_CASE,
};

enum UrlanOperator
{
    UR_OP_ADD,
    UR_OP_SUB,
    UR_OP_MUL,
    UR_OP_DIV,
    UR_OP_MOD,
    UR_OP_AND,
    UR_OP_OR,
    UR_OP_XOR
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
    int  (*convert)   ( UThread*, const UCell* from, UCell* res );
    void (*copy)      ( UThread*, const UCell* from, UCell* res );
    int  (*compare)   ( UThread*, const UCell* a, const UCell* b, int test );
    int  (*operate)   ( UThread*, const UCell*, const UCell*, UCell* res, int );
    const UCell*
         (*select)    ( UThread*, const UCell*, const UCell* sel, UCell* tmp );
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
    UR_FIND_LAST = 1,
    UR_FIND_CASE
};

typedef struct
{
    UDatatype dt;
    void (*pick)      ( const UBuffer* buf, UIndex n, UCell* res );
    void (*poke)      ( UBuffer* buf, UIndex n, const UCell* val );
    int  (*append)    ( UThread*, UBuffer* buf, const UCell* val );
    int  (*insert)    ( UThread*, UBuffer* buf, UIndex index,
                        const UCell* val, UIndex part );
    int  (*change)    ( UThread*, USeriesIterM* si, const UCell* val,
                        UIndex part );
    void (*remove)    ( UThread*, USeriesIterM* si, UIndex part );
    void (*reverse)   ( const USeriesIterM* si );
    int  (*find)      ( UThread*, const USeriesIter* si, const UCell* val,
                        int opt );
}
USeriesType;


typedef struct
{
    UAtom    atom;
    uint16_t index;     // LIMIT: 65535 words per context.
}
UAtomEntry;


typedef struct
{
    unsigned int atomLimit;         //!< Maximum number of atoms.
    unsigned int atomNamesSize;     //!< Fixed byte size of atom name buffer.
    unsigned int envSize;           //!< Byte size of environment structure.
    unsigned int threadSize;        //!< Byte size of thread structure.
    unsigned int dtCount;           //!< Number of entries in dtTable.
    const UDatatype** dtTable;      //!< Pointers to user defined datatypes.
    void (*threadMethod)(UThread*, enum UThreadMethod);
}
UEnvParameters;


#ifdef __cplusplus
extern "C" {
#endif

UEnvParameters* ur_envParam( UEnvParameters* par );
UThread* ur_makeEnv( const UEnvParameters* );
void     ur_freeEnv( UThread* );
void     ur_freezeEnv( UThread* );
UThread* ur_makeThread( const UThread* );
int      ur_destroyThread( UThread* );
int      ur_datatypeCount( UThread* );
UAtom    ur_intern( UThread*, const char* name, int len );
UAtom    ur_internAtom( UThread*, const char* it, const char* end );
UAtom*   ur_internAtoms( UThread*, const char* words, UAtom* atoms );
const char* ur_atomCStr( UThread*, UAtom atom );
void     ur_atomsSort( UAtomEntry* entries, int low, int high );
int      ur_atomsSearch( const UAtomEntry* entries, int count, UAtom atom );
UBuffer* ur_genBuffers( UThread*, int count, UIndex* index );
UBuffer* ur_generate( UThread*, int count, UIndex* index, const uint8_t* );
void     ur_destroyBuffer( UThread*, UBuffer* );
UIndex   ur_holdBuffer( UThread*, UIndex bufN );
void     ur_releaseBuffer( UThread*, UIndex hold );
void     ur_recycle( UThread* );
int      ur_markBuffer( UThread*, UIndex bufN );
UCell*   ur_push( UThread*, int type );
UCell*   ur_pushCell( UThread*, const UCell* );
UStatus  ur_error( UThread*, int errorType, const char* fmt, ... );
UBuffer* ur_threadContext( UThread* );
UBuffer* ur_envContext( UThread* );
void     ur_traceError( UThread*, const UCell* errC, UIndex blkN,
                        const UCell* pos );
void     ur_appendTrace( UThread*, UIndex blkN, UIndex it );
UIndex   ur_tokenize( UThread*, const char* it, const char* end, UCell* res );
UStatus  ur_tokenizeB( UThread*, UIndex blkN, int inputEncoding,
                       const uint8_t* start, const uint8_t* end );
UStatus  ur_serialize( UThread*, UIndex blkN, UCell* res );
UStatus  ur_unserialize( UThread*, const uint8_t* start, const uint8_t* end,
                         UCell* res );
void     ur_toStr( UThread*, const UCell* cell, UBuffer* str, int depth );
void     ur_toText( UThread*, const UCell* cell, UBuffer* str );
const UCell* ur_wordCell( UThread*, const UCell* cell );
UCell*   ur_wordCellM( UThread*, const UCell* cell );
UStatus  ur_setWord( UThread*, const UCell* wordCell, const UCell* src );
const UBuffer* ur_bufferEnv( UThread*, UIndex n );
const UBuffer* ur_bufferSeries( const UThread*, const UCell* cell );
UBuffer* ur_bufferSeriesM( UThread*, const UCell* cell );
void     ur_initSeries( UCell*, int type, UIndex buf );
void     ur_seriesSlice( const UThread*, USeriesIter* si, const UCell* cell );
UStatus  ur_seriesSliceM( UThread*, USeriesIterM* si, const UCell* cell );
const UBuffer* ur_blockIt( const UThread* ut, UBlockIt* bi,
                           const UCell* blkCell );
void     ur_bind( UThread*, UBuffer* blk, const UBuffer* ctx, int bindType );
void     ur_bindCells( UThread*, UCell* it, UCell* end, const UBindTarget* bt );
void     ur_bindCopy( UThread*, const UBuffer* ctx, UCell* it, UCell* end );
void     ur_unbindCells( UThread*, UCell* it, UCell* end, int deep );
void     ur_infuse( UThread*, UCell* it, UCell* end, const UBuffer* ctx );
int      ur_true( const UCell* cell );
int      ur_same( UThread*, const UCell* a, const UCell* b );
int      ur_equal( UThread*, const UCell* a, const UCell* b );
int      ur_equalCase( UThread*, const UCell* a, const UCell* b );
int      ur_compare( UThread*, const UCell* a, const UCell* b );
int      ur_compareCase( UThread*, const UCell* a, const UCell* b );

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
void     ur_binAppendArray( UBuffer*, const USeriesIter* si );
const char* ur_binAppendBase( UBuffer* buf, const char* it, const char* end,
                              enum UrlanBinaryEncoding enc );
void     ur_binFree( UBuffer* );
void     ur_binSlice( UThread*, UBinaryIter*, const UCell* cell );
UStatus  ur_binSliceM( UThread*, UBinaryIterM*, const UCell* cell );
void     ur_binToStr( UBuffer*, int encoding );

UBuffer* ur_makeBitsetCell( UThread*, int bitCount, UCell* res );

UIndex   ur_makeString( UThread*, int enc, int size );
UBuffer* ur_makeStringCell( UThread*, int enc, int size, UCell* cell );
UIndex   ur_makeStringLatin1( UThread*, const uint8_t* it, const uint8_t* end );
UIndex   ur_makeStringUtf8( UThread*, const uint8_t* it, const uint8_t* end );
void     ur_strInit( UBuffer*, int enc, int size );
void     ur_strInitUtf8( UBuffer*, const uint8_t* it, const uint8_t* end );
void     ur_strAppendCStr( UBuffer*, const char* );
void     ur_strAppendChar( UBuffer*, int );
void     ur_strAppendInt( UBuffer*, int32_t );
void     ur_strAppendInt64( UBuffer*, int64_t );
void     ur_strAppendHex( UBuffer*, uint32_t n, uint32_t hi );
void     ur_strAppendDouble( UBuffer*, double );
void     ur_strAppendFloat( UBuffer*, float );
void     ur_strAppendIndent( UBuffer*, int depth );
void     ur_strAppend( UBuffer*, const UBuffer* strB, UIndex itB, UIndex endB );
void     ur_strAppendBinary( UBuffer*, const uint8_t* it, const uint8_t* end,
                             enum UrlanBinaryEncoding enc );
void     ur_strTermNull( UBuffer* );
int      ur_strIsAscii( const UBuffer* );
void     ur_strFlatten( UBuffer* );
void     ur_strLowercase( UBuffer* str, UIndex start, UIndex send );
void     ur_strUppercase( UBuffer* str, UIndex start, UIndex send );
UIndex   ur_strFindChar( const UBuffer*, UIndex, UIndex, int ch, int opt );
UIndex   ur_strFindChars( const UBuffer*, UIndex start, UIndex end,
                          const uint8_t* charSet, int len );
UIndex   ur_strFindCharsRev( const UBuffer*, UIndex start, UIndex end,
                             const uint8_t* charSet, int len );
UIndex   ur_strFind( const USeriesIter*, const USeriesIter*, int matchCase );
UIndex   ur_strFindRev( const USeriesIter*, const USeriesIter*, int matchCase );
UIndex   ur_strMatch( const USeriesIter*, const USeriesIter*, int matchCase );
int      ur_strChar( const UBuffer*, UIndex pos );
char*    ur_cstring( const UBuffer*, UBuffer* bin, UIndex start, UIndex end );
#define  ur_strFree ur_arrFree
#define  ur_strIsUcs2(buf)  ((buf)->form == UR_ENC_UCS2)

UIndex   ur_makeBlock( UThread*, int size );
UBuffer* ur_makeBlockCell( UThread*, int type, int size, UCell* cell );
UIndex   ur_blkClone( UThread*, UIndex blkN );
void     ur_blkInit( UBuffer*, int type, int size );
UCell*   ur_blkAppendNew( UBuffer*, int type );
void     ur_blkAppendCells( UBuffer*, const UCell* cells, int count );
void     ur_blkInsert( UBuffer*, UIndex it, const UCell* cells, int count );
void     ur_blkPush( UBuffer*, const UCell* cell );
UCell*   ur_blkPop( UBuffer* );
void     ur_blkSlice( UThread*, UBlockIter*, const UCell* cell );
UStatus  ur_blkSliceM( UThread*, UBlockIterM*, const UCell* cell );
void     ur_blkCollectType( UThread*, const UCell* blkCell,
                            uint32_t typeMask, UBuffer* dest, int unique );
#define  ur_blkFree ur_arrFree

int      ur_pathResolve( UThread*, UBlockIt* pi, UCell* tmp, UCell** lastCell );
int      ur_pathCell( UThread*, const UCell* pc, UCell* res );
UStatus  ur_setPath( UThread*, const UCell* path, const UCell* src );

UIndex   ur_makeContext( UThread*, int size );
UBuffer* ur_makeContextCell( UThread*, int size, UCell* cell );
UBuffer* ur_ctxClone( UThread*, const UBuffer* src, UCell* cell );
UBuffer* ur_ctxMirror( UThread*, const UBuffer* src, UCell* cell );
void     ur_ctxInit( UBuffer*, int size );
void     ur_ctxReserve( UBuffer*, int size );
void     ur_ctxFree( UBuffer* );
UBuffer* ur_ctxSort( UBuffer* );
void     ur_ctxSetWords( UBuffer*, const UCell* it, const UCell* end );
int      ur_ctxAppendWord( UBuffer*, UAtom atom );
int      ur_ctxAddWordI( UBuffer*, UAtom atom );
UCell*   ur_ctxAddWord( UBuffer*, UAtom atom );
void     ur_ctxWordAtoms( const UBuffer*, UAtom* atoms );
int      ur_ctxLookup( const UBuffer*, UAtom atom );
const UBuffer* ur_sortedContext( UThread*, const UCell* );
#define  ur_ctxLookupNoSort ur_ctxLookup
#define  ur_ctxCell(c,n)    ((c)->ptr.cell + (n))

UIndex   ur_makeVector( UThread*, enum UrlanVectorType, int size );
UBuffer* ur_makeVectorCell( UThread*, enum UrlanVectorType, int size, UCell* );
void     ur_vecInit( UBuffer*, int type, int elemSize, int size );

void     ur_arrInit( UBuffer*, int size, int count );
void     ur_arrReserve( UBuffer*, int count );
void     ur_arrExpand( UBuffer*, int index, int count );
void     ur_arrErase( UBuffer*, int start, int count );
void     ur_arrFree( UBuffer* );
void     ur_arrAppendInt32( UBuffer*, int32_t );
void     ur_arrAppendFloat( UBuffer*, float );

#ifdef __cplusplus
}
#endif


#define ur_ptr(T,buf)       ((T*) (buf)->ptr.v)
#define ur_avail(buf)       (buf)->ptr.i32[-1]
#define ur_testAvail(buf)   (buf->ptr.v ? ur_avail(buf) : 0)

#define ur_arrExpand1(T,buf,elem) \
    ur_arrReserve(buf, buf->used + 1); \
    elem = ur_ptr(T,buf) + buf->used; \
    ++buf->used

#define ur_type(c)          (c)->id.type
#ifdef __BIG_ENDIAN__
#define ur_setId(c,t)       *((uint32_t*) (c)) = (t) << 24
#define ur_initCoord(c,n)   *((uint32_t*) (c)) = UT_COORD << 24 | (n)
#else
#define ur_setId(c,t)       *((uint32_t*) (c)) = t
#define ur_initCoord(c,n)   *((uint32_t*) (c)) = UT_COORD | (n) << 16
#endif
#define ur_setFlags(c,m)    (c)->id.flags |= m
#define ur_clrFlags(c,m)    (c)->id.flags &= ~(m)
#define ur_flags(c,m)       ((c)->id.flags & m)
#define ur_is(c,t)          ((c)->id.type == (t))

#define ur_atom(c)          (c)->word.atom
#define ur_datatype(c)      (c)->datatype.n
#define ur_logic(c)         (c)->id.ext
#define ur_char(c)          (c)->number.eb.i64
#define ur_int(c)           (c)->number.eb.i64
#define ur_double(c)        (c)->number.eb.d

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

#define ur_stackTop(ut)     (ut->stack.ptr.cell + ut->stack.used - 1)
#define ur_exception(ut)    ut->stack.ptr.cell
#define ur_pop(ut)          --(ut)->stack.used
#define ur_hold(n)          ur_holdBuffer(ut,n)
#define ur_release(h)       ur_releaseBuffer(ut,h)
#define ur_buffer(n)        (ut->dataStore.ptr.buf + (n))
#define ur_bufferE(n)       ur_bufferEnv(ut,n)
#define ur_bufferSer(c)     ur_bufferSeries(ut,c)
#define ur_bufferSerM(c)    ur_bufferSeriesM(ut,c)

#define ur_foreach(bi)      for(; bi.it != bi.end; ++bi.it)

#define ur_wordCStr(c)      ur_atomCStr(ut, ur_atom(c))

#define ur_cstr(strC,bin) \
    ur_cstring(ur_bufferSer(strC), bin, strC->series.it, strC->series.end)


#endif  /*EOF*/
