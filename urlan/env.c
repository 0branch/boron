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

/** \defgroup urlan_core Core Programmer Interface
  \ingroup urlan
  @{
*/

/** \enum UrlanReturnCode
  These values can be returned by functions to indicate success or failure.
*/
/** \var UrlanReturnCode::UR_OK
  Returned to indicate successful evaluation/operation.
  This is guaranteed to always be one.
*/
/** \var UrlanReturnCode::UR_THROW
  Returned to indicate an evaluation exception occured.
  This is guaranteed to always be zero.
*/
/** \enum UrlanDataType
  Indentifers for the built-in datatypes.
  These values match the datatype name atom.
*/
/** \enum UrlanErrorType
  These descriptive codes are passed to ur_error().
*/
/** \enum UThreadMethod
  Operations on thread data.
*/
/** \def ur_type
  Return UrlanDataType of cell.
*/
/** \def ur_setId
  Set type and initialize the other 24 bits of UCellId to zero.
*/
/** \def ur_isShared
  True if buffer number refers to a buffer in the shared environment.
*/
/** \def ur_isSliced
  True if the end member of a series cell is set.
*/


#include "env.h"
#include "str.h"
#include "mem_util.h"


#define GC_HOLD_TEST  1

#define LOCK_GLOBAL     mutexLock( env->mutex );
#define UNLOCK_GLOBAL   mutexUnlock( env->mutex );

#define BUF_ERROR_BLK   0
#define BUF_THREAD_CTX  1


#include "datatypes.c"
#include "atoms.c"


static void _nopThreadFunc( UThread* ut, UThreadMethod op )
{
    (void) ut;
    (void) op;
}


static void _threadInitStore( UThread* ut )
{
    UIndex bufN[2];

    ur_arrInit( &ut->dataStore, sizeof(UBuffer), 64 );
    ur_arrInit( &ut->holds,     sizeof(UIndex),  16 );
    ur_binInit( &ut->gcBits, 64/8 );
    ut->freeBufCount = 0;
    ut->freeBufList = -1;
    ut->wordCell = 0;

    // Buffer index zero denotes an invalid buffer (UR_INVALID_BUF),
    // so remove it from general use.  This will be our stack of errors.

    ur_genBuffers( ut, 2, bufN );
    ur_blkInit( ur_buffer(bufN[0]), UT_BLOCK, 0 );  // BUF_ERROR_BLK
    ur_ctxInit( ur_buffer(bufN[1]), 0 );            // BUF_THREAD_CTX
    ur_hold( bufN[0] );
    ur_hold( bufN[1] );

    ut->env->threadFunc( ut, UR_THREAD_INIT );
}


static UThread* _threadMake( UEnv* env )
{
    UThread* ut = (UThread*) memAlloc( env->threadSize );
    if( ! ut )
        return 0;

    memSet( ut, 0, env->threadSize );   // Zeros user extensions as well.
    ut->env   = env;
    ut->types = env->types;

    _threadInitStore( ut );

    LOCK_GLOBAL

    if( env->threads )
    {
        // NOTE: env->threads (original created by ur_makeEnv) is not changed.
        UThread* link = env->threads;
        ut->nextThread = link->nextThread;
        link->nextThread = ut;
    }
    else
    {
        env->threads = ut->nextThread = ut;
    }

    UNLOCK_GLOBAL

    return ut;
}


static void _destroyDataStore( UEnv* env, UBuffer* store )
{
    UDatatype** dt = env->types;
    UBuffer* it  = store->ptr.buf;
    UBuffer* end = it + store->used;

    while( it != end )
    {
        if( it->type != UT_UNSET )
            dt[ it->type ]->destroy( it );
        ++it;
    }

    ur_arrFree( store );
}


/*
   Free memory used by UThread.

   This does NOT remove the thread from the environemnt thread list.
   ur_threadDestroy() must be used for that.
*/
static void _threadFree( UThread* ut )
{
    ut->env->threadFunc( ut, UR_THREAD_FREE );
    _destroyDataStore( ut->env, &ut->dataStore );
    ur_arrFree( &ut->holds );
    ur_binFree( &ut->gcBits );
    memFree( ut );
}


#ifdef DEBUG
static void _validateEnv()
{
    UCell cell;
    int i;

    //printf( "KR sizeof(pthread_t) = %lu\n", sizeof(pthread_t) );

    assert( sizeof(UCell) == 16 );
    assert( &cell.word.ctx == &cell.series.buf );

    // Make sure endianess is correct.
    i = 1;
#ifdef __BIG_ENDIAN__
    assert(0 == *(char*)&i && "Undefine __BIG_ENDIAN__");
#else
    assert(1 == *(char*)&i && "Define __BIG_ENDIAN__");
#endif
}
#endif


static void _addDT( UEnv* env, int id, UDatatype* dt )
{
    static uint8_t reserved[] = ":type:XX";
    if( dt )
    {
        const char* end = dt->name;
        while( *end != '\0' )
            ++end;
        _internAtom( &env->atomTable, &env->atomNames,
                     (uint8_t*) dt->name, (uint8_t*)end );
    }
    else
    {
        reserved[ sizeof(reserved) - 3 ] = '0' + (id / 10);
        reserved[ sizeof(reserved) - 2 ] = '0' + (id % 10);
        _internAtom( &env->atomTable, &env->atomNames,
                     reserved, reserved + (sizeof(reserved) - 1) );
    }
    env->types[ id ] = dt;
}


extern UDatatype dt_coord;


/**
  Allocate UEnv and initial UThread.

  \param dt         Pointer to array of user defined datatypes.
                    Pass zero if dtCount is zero.
  \param dtCount    Number of datatypes in dt.
  \param thrSize    Byte size of each thread in the environment.
                    Pass zero if no extra memory is needed.
  \param thrMethod  Callback function to initialize and cleanup threads.
                    This may be zero.

  \return Pointer to initial thread.
*/
UThread* ur_makeEnv( UDatatype* dt, int dtCount,
                     unsigned int thrSize,
                     void (*thrMethod)(UThread*, UThreadMethod) )
{
    UEnv* env;
    UThread* ut;
    int i;

#ifdef DEBUG
    _validateEnv();
#endif

    env = memAlloc( sizeof(UEnv) );
    if( ! env )
        return 0;

    ur_arrInit( &env->dataStore, sizeof(UBuffer), 0 );

    ur_binInit( &env->atomNames, 8 * 2048 );
    ur_arrInit( &env->atomTable, sizeof(AtomRec), 2048 );
    _rebuildAtomHash( &env->atomTable );

    env->typeCount = UT_BI_COUNT + dtCount;

    env->threadSize = (thrSize > sizeof(UThread)) ? thrSize : sizeof(UThread);
    env->threadFunc = thrMethod ? thrMethod : _nopThreadFunc;

    env->threads = 0;

    if( mutexInitF( env->mutex ) )
    {
#ifdef _WIN32
        //GetLastError();
#else
        perror( "mutexInit" );
#endif
        memFree( env );
        return 0;
    }


    // Add datatypes first so UT_ defines match the index & atom.

#define addDT(id,ptr)   _addDT( env, id, ptr )

    addDT( UT_UNSET,    &dt_unset );
    addDT( UT_DATATYPE, &dt_datatype );
    addDT( UT_NONE,     &dt_none );
    addDT( UT_LOGIC,    &dt_logic );
    addDT( UT_CHAR,     &dt_char );
    addDT( UT_INT,      &dt_int );
    addDT( UT_DECIMAL,  &dt_decimal );
    addDT( UT_BIGNUM,   &dt_bignum );
    addDT( UT_TIME,     &dt_time );
    addDT( UT_DATE,     &dt_date );
    addDT( UT_COORD,    &dt_coord );
    addDT( UT_VEC3,     &dt_vec3 );

    addDT( UT_WORD,     &dt_word );
    addDT( UT_LITWORD,  &dt_litword );
    addDT( UT_SETWORD,  &dt_setword );
    addDT( UT_GETWORD,  &dt_getword );
    addDT( UT_OPTION,   &dt_option );

    addDT( UT_BINARY,   &dt_binary.dt );
    addDT( UT_BITSET,   &dt_bitset.dt );
    addDT( UT_STRING,   &dt_string.dt );
    addDT( UT_FILE,     &dt_file.dt );
    addDT( UT_VECTOR,   &dt_vector.dt );
    addDT( UT_BLOCK,    &dt_block.dt );
    addDT( UT_PAREN,    &dt_paren.dt );
    addDT( UT_PATH,     &dt_path.dt );
    addDT( UT_LITPATH,  &dt_litpath.dt );
    addDT( UT_SETPATH,  &dt_setpath.dt );

    addDT( UT_CONTEXT,  &dt_context );
    addDT( UT_ERROR,    &dt_error );

    i = UT_BI_COUNT;
    if( dtCount )
    {
        UDatatype* dtEnd = dt + dtCount;
        for( ; dt != dtEnd; ++dt, ++i )
            addDT( i, dt );
    }

    // Add atoms so the fixed atoms remain constant as the number of
    // datatypes changes.
    for( ; i < UT_MAX; ++i )
        addDT( i, 0 );


    ut = _threadMake( env );
    if( ! ut )
    {
        memFree( env );
        return 0;
    }


    // Intern commonly used atoms.
    {
    UAtom atoms[ 37 ];
    ur_internAtoms( ut,
                    "quit halt return break ghost words\n"
                    "latin1 utf8 ucs2\n"
                    "+ - / * = < > <= >=\n"
                    "x y z r g b a\n"
                    "| opt some any skip set copy to thru place\n"
                    "crc16 sha1",
                    atoms );
    assert( atoms[0] == UT_MAX );
    }

    //dumpAtoms( ut );

    return ut;
}


/**
  Free environment and all threads.
  Caller must make sure no other threads are running.

  \param ut     UThread created with ur_makeEnv().  It is safe to pass zero.
*/
void ur_freeEnv( UThread* ut )
{
    UEnv* env;
    UThread* thr;
    UThread* next;


    if( ! ut || ! ut->env )
        return;
    env = ut->env;
    if( ut != env->threads )
        return;                     // Not the original thread.

    // Locking here doesn't really do anything.  Caller must make
    // sure no other threads are running.

    //LOCK_GLOBAL

    thr = env->threads;
    do
    {
        next = thr->nextThread;
        _threadFree( thr );
        thr = next;
    }
    while( thr != env->threads );

    //UNLOCK_GLOBAL
    mutexFree( env->mutex );


    ur_arrFree( &env->dataStore );

    ur_binFree( &env->atomNames );
    ur_arrFree( &env->atomTable );

    memFree( env );
}


/**
  Move the startup thread dataStore to the shared environment.
  This may only be called once after ur_makeEnv(), and must be done
  before a second thread is created.

  The thread method UR_THREAD_FREEZE is called before any adjustments
  are made.  After the store has been converted, the method UR_THREAD_INIT
  will be called on a fresh thread dataStore.

  \param ut     UThread created with ur_makeEnv().
*/
void ur_freezeEnv( UThread* ut )
{
    UEnv* env = ut->env;

    if( ! env || env->dataStore.used )
        return;

    env->threadFunc( ut, UR_THREAD_FREEZE );

    ur_recycle( ut );

    env->dataStore = ut->dataStore;
    ur_arrFree( &ut->holds );
    ur_binFree( &ut->gcBits );


    // Point all bindings & data store references to the shared environment.
    {
#define BLOCK_MASK \
    ((1 << UT_BLOCK) | (1 << UT_PAREN) | (1 << UT_CONTEXT) | \
     (1 << UT_PATH) | (1 << UT_LITPATH) | (1 << UT_SETPATH))

    UBuffer* it  = env->dataStore.ptr.buf;
    UBuffer* end = it + env->dataStore.used;
    UDatatype** dt = env->types;

    // TODO: Eliminate unused buffers.
    //while( end[-1].type == UT_UNSET )
    //    --end;

    while( it != end )
    {
        // TODO: Handle custom datatype buffers.
        if( BLOCK_MASK & (1 << it->type) )
        {
            UCell* ci   = it->ptr.cell;
            UCell* cend = ci + it->used;
            //printf( "KR freeze buf %ld\n", it - env->dataStore.ptr.buf );
            while( ci != cend )
            {
                if( ur_type(ci) >= UT_REFERENCE_BUF )
                    dt[ ur_type(ci) ]->toShared( ci );
                ++ci;
            }
        }
        ++it;
    }
    }


    // Make a fresh thread store.
    _threadInitStore( ut );
}


/**
  Get number of datatypes installed in the environment.

  \return Number of datatypes.
*/
int ur_datatypeCount( UThread* ut )
{
    return ut->env->typeCount;
}


/**
  Add a single atom to the shared environment.

  \param it   Start of word.
  \param end  End of word.

  \return Atom of word.
*/
UAtom ur_internAtom( UThread* ut, const char* it, const char* end )
{
    UAtom atom;
    UEnv* env = ut->env;

    LOCK_GLOBAL
    atom = _internAtom( &env->atomTable, &env->atomNames,
                        (uint8_t*) it, (uint8_t*) end );
    UNLOCK_GLOBAL

    return atom;
}


/**
  Add atoms to the shared environment.

  \param words  C string containing words separated by whitespace.  
  \param atoms  Return area for atoms of the words.

  \return Pointer marking the end of the entries set in atoms.
*/
UAtom* ur_internAtoms( UThread* ut, const char* words, UAtom* atoms )
{
    UEnv* env = ut->env;
    UBuffer* table = &env->atomTable;
    UBuffer* names = &env->atomNames;
    const char* cp = words;
    const char* end;


    LOCK_GLOBAL

    while( *cp )
    {
        cp = str_skipWhite( cp );
        end = str_toWhite( cp );
        *atoms++ = _internAtom( table, names, (uint8_t*) cp, (uint8_t*) end );
        cp = end;
    }

    UNLOCK_GLOBAL

    return atoms;
}


/**
  Generate new buffers in dataStore.
  This may trigger the garbage collector.

  The new buffers are completely unintialized, so the caller must make them
  valid before the next garbage recycle.

  \param count  Number of buffers to obtain.
  \param index  Return array of buffer ids.  This must large enough to
                to hold count indices.

  \return The buffer indicies are stored in index.
*/
void ur_genBuffers( UThread* ut, int count, UIndex* index )
{
    UBuffer* next;
    UBuffer* store = &ut->dataStore;
    int i;

    if( ut->freeBufCount < count )
    {
        ur_recycle( ut );
        if( ut->freeBufCount < count )
        {
            ur_arrReserve( store, store->used + count );
            for( i = 0; i < count; ++i )
                index[i] = store->used + i;
            store->used += count;
            return;
        }
    }
#ifdef GC_HOLD_TEST
    else
        ur_recycle( ut );
#endif

    assert( ut->freeBufList > -1 );

    for( i = 0; i < count; ++i )
    {
        next = store->ptr.buf + ut->freeBufList;

        assert( next->type == UT_UNSET );
        assert( next->ptr.v == 0 );

        index[i] = ut->freeBufList;
        ut->freeBufList = next->used;
    }
    ut->freeBufCount -= count;
}


/**
  Destroy buffer in dataStore.

  The buffer must be valid.  This function must not be called with a
  buffer which has not been generated with ur_genBuffers(), has not been
  initialized, or has already been destroyed.

  \param buf    Pointer to valid buffer.
*/
void ur_destroyBuffer( UThread* ut, UBuffer* buf )
{
#if 0
    printf( "ur_destroyBuffer %ld %s\n",
            buf - ut->dataStore.ptr.buf, ut->types[buf->type]->name );
#endif
    ut->types[ buf->type ]->destroy( buf );

    // Flag as unused.
    buf->type = UT_UNSET;

    // Link to free list.
    buf->used = ut->freeBufList;
    ut->freeBufList = buf - ut->dataStore.ptr.buf;

    ++ut->freeBufCount;
}


/**
  \def ur_hold
  Convenience macro for ur_holdBuffer().
*/

/**
  Keeps buffer in thread dataStore from being garbage collected by ur_recycle().

  \return Hold id to be used with ur_releaseBuffer().
*/
UIndex ur_holdBuffer( UThread* ut, UIndex bufN )
{
    UBuffer* buf = &ut->holds;
    UIndex n = buf->used;

    //ur_arrAppendInt32( buf, bufN );
    ur_arrReserve( buf, n + 1 );
    buf->ptr.i[ n ] = bufN;
    ++buf->used;

    return n;
}


/**
  \def ur_release
  Convenience macro for ur_releaseBuffer().
*/

/**
  Enables garbage collection of dataStore buffer which was held by
  ur_holdBuffer().
*/
void ur_releaseBuffer( UThread* ut, UIndex hold )
{
    UBuffer* buf = &ut->holds;
    UIndex* it = buf->ptr.i + hold;

    assert( hold > -1 && hold < buf->used );

    *it = UR_INVALID_HOLD;
    if( hold == (buf->used - 1) )
    {
        UIndex* stop = buf->ptr.i - 1;
        do
            --it;
        while( (it != stop) && (*it == UR_INVALID_HOLD) );
        buf->used = it - buf->ptr.i + 1;
    }
}


/**
  Get thread error block.

  \return Pointer to block of errors.
*/
UBuffer* ur_errorBlock( UThread* ut )
{
    return ur_buffer( BUF_ERROR_BLK );
}


/**
  Get thread global context.

  \return Pointer to thread global context.
*/
UBuffer* ur_threadContext( UThread* ut )
{
    return ur_buffer( BUF_THREAD_CTX );
}


/**
  Get shared global context.

  \return Pointer to context, or zero if ur_freezeEnv() has not been called.
*/
UBuffer* ur_envContext( UThread* ut )
{
    UEnv* env = ut->env;
    if( env->dataStore.used )
        return env->dataStore.ptr.buf + BUF_THREAD_CTX;
    return 0;
}


/**
  Append error! to the error block.

  The errorType is only descriptive, it has no real function.

  \param errorType  UrlanErrorType (UR_ERR_DATATYPE, UR_ERR_SCRIPT, etc.).
  \param fmt        Error message with printf() style formatting.

  \return UR_THROW
*/
int ur_error( UThread* ut, int errorType, const char* fmt, ... )
{
#define MAX_ERR_LEN 256
    UIndex bufN[2];
    UBuffer* str;
    UCell* cell;
    va_list arg;

    (void) errorType;

    ur_genBuffers( ut, 2, bufN );

    str = ur_buffer( bufN[0] );
    ur_strInit( str, UR_ENC_LATIN1, MAX_ERR_LEN );

    ur_blkInit( ur_buffer( bufN[1] ), UT_BLOCK, 0 );

    va_start( arg, fmt );
    vaStrPrint( str->ptr.c, MAX_ERR_LEN, fmt, arg );
    va_end( arg );
    str->used = strLen(str->ptr.c);


    cell = ur_blkAppendNew( ur_errorBlock(ut), UT_ERROR );
    cell->error.exType = errorType;
    cell->error.messageStr = bufN[0];
    cell->error.traceBlk   = bufN[1];

    return UR_THROW;
}


/**
  Append block position to the UCellError::traceBlk of the error on top of
  the error stack.

  \param blkN   Block id.
  \param it     Position in block blkN.
*/
void ur_appendTrace( UThread* ut, UIndex blkN, UIndex it )
{
    UCell* cell;
    UBuffer* buf = ur_buffer( BUF_ERROR_BLK );
    if( buf->used )
    {
        cell = buf->ptr.cell + (buf->used - 1);
        if( ur_is(cell, UT_ERROR) )
        {
            if( cell->error.traceBlk == UR_INVALID_BUF )
                cell->error.traceBlk = ur_makeBlock( ut, 8 );

            buf = ur_buffer( cell->error.traceBlk );
            cell = ur_blkAppendNew( buf, UT_BLOCK );
            ur_setSeries(cell, blkN, it );
        }
    }
}


/**
  \return Non-zero if cell is not none! or false.
*/
int ur_isTrue( const UCell* cell )
{
    int t = ur_type(cell);
    //return ! ((t == UT_NONE) ||
    //          ((t == UT_LOGIC) && (ur_int(cell) == 0)));
    if( t == UT_LOGIC )
        return ur_int(cell);
    return t != UT_NONE;
}


/**
  \return Non-zero if values are the same.
*/
int ur_same( UThread* ut, const UCell* a, const UCell* b )
{
    int t = ur_type(a);
    if( t == ur_type(b) )
        return ut->types[ t ]->compare( ut, a, b, UR_COMPARE_SAME );
    return 0;
}


/**
  \return Non-zero if values are equivalent.
*/
int ur_equal( UThread* ut, const UCell* a, const UCell* b )
{
    int t = ur_type(a);
    if( t < ur_type(b) )
        t = ur_type(b);
    return ut->types[ t ]->compare( ut, a, b, UR_COMPARE_EQUAL );
}


/**
  \return 1, 0, or -1, if cell a is greater than, equal to, or less than
          cell b.
*/
int ur_compare( UThread* ut, const UCell* a, const UCell* b )
{
    int t = ur_type(a);
    if( t < ur_type(b) )
        t = ur_type(b);
    return ut->types[ t ]->compare( ut, a, b, UR_COMPARE_ORDER );
}


/**
  Append data representation of cell to a string.
 
  \param str    String to add to.
*/
void ur_toStr( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    ut->types[ ur_type(cell) ]->toString( ut, cell, str, depth );
}


/**
  Append textual representation of cell to a string.

  \param str    String to add to.
*/
void ur_toText( UThread* ut, const UCell* cell, UBuffer* str )
{
    ut->types[ ur_type(cell) ]->toText( ut, cell, str, 0 );
}


/**
  Get word value for read-only operations.

  \param cell   Pointer to word cell.

  \return  Pointer to value cell.  If the word does not reference a valid
           cell then an error is generated and zero is returned.
*/
const UCell* ur_wordCell( UThread* ut, const UCell* cell )
{
    switch( ur_binding(cell) )
    {
        case UR_BIND_UNBOUND:
            ur_error( ut, UR_ERR_SCRIPT, "unbound word '%s",
                      ur_atomCStr( ut, ur_atom(cell) ) );
            return 0;

        case UR_BIND_THREAD:
            return (ut->dataStore.ptr.buf + cell->word.ctx)->ptr.cell +
                   cell->word.index;

        case UR_BIND_ENV:
            return (ut->env->dataStore.ptr.buf - cell->word.ctx)->ptr.cell +
                   cell->word.index;

        default:
            return ut->wordCell( ut, cell );
    }

    ur_error( ut, UR_ERR_SCRIPT, "word '%s has invalid binding",
              ur_atomCStr( ut, ur_atom(cell) ) );
    return 0;
}


/**
  Get modifiable word value.

  \param cell   Pointer to word cell.

  \return  Pointer to value cell.  If the word does not reference a valid
           cell then an error is generated and zero is returned.
*/
UCell* ur_wordCellM( UThread* ut, const UCell* cell )
{
    switch( ur_binding(cell) )
    {
        case UR_BIND_UNBOUND:
            ur_error( ut, UR_ERR_SCRIPT, "unbound word '%s",
                      ur_atomCStr( ut, ur_atom(cell) ) );
            return 0;

        case UR_BIND_THREAD:
            return (ut->dataStore.ptr.buf + cell->word.ctx)->ptr.cell +
                   cell->word.index;

        case UR_BIND_ENV:
            ur_error( ut, UR_ERR_SCRIPT, "word '%s is in shared storage",
                      ur_atomCStr( ut, ur_atom(cell) ) );
            return 0;

        default:
            return ut->wordCellM( ut, cell );
    }

    ur_error( ut, UR_ERR_SCRIPT, "word '%s has invalid binding",
              ur_atomCStr( ut, ur_atom(cell) ) );
    return 0;
}


/**
  Set word.  This copies src into the cell which the word is bound to.

  If the word is unbound or bound to the shared environment then an error
  is generated and UR_THROW is returned.

  \param word   Word to set.
  \param src    Source value to copy.

  \return UR_OK/UR_THROW
*/
int ur_setWord( UThread* ut, const UCell* word, const UCell* src )
{
    UCell* dest = ur_wordCellM( ut, word );
    if( dest )
    {
        *dest = *src;
        return UR_OK;
    }
    return UR_THROW;
}


/**
  \def ur_buffer
  Macro to get buffer known to be in thread dataStore.
*/

/**
  \def ur_bufferE
  Convenience macro for ur_bufferEnv().
*/

/**
  Get buffer from either the thread or shared environment dataStore.
  The macro ur_bufferE() should normally be used to call this function.

  \param n  Buffer identifier.

  \return Pointer to buffer n.
*/
const UBuffer* ur_bufferEnv( UThread* ut, UIndex n )
{
    if( ur_isShared(n) )
        return ut->env->dataStore.ptr.buf - n; 
    return ut->dataStore.ptr.buf + n;
}


/**
  \def ur_bufferSer
  Convenience macro for ur_bufferSeries().
*/

/**
  \def ur_bufferSerM
  Convenience macro for ur_bufferSeriesM().
*/

/**
  Get series buffer.
  The macro ur_bufferSer() should normally be used to call this function.

  \param cell   Pointer to valid series or bound word cell.

  \return Pointer to buffer referenced by cell->series.buf.
*/
const UBuffer* ur_bufferSeries( UThread* ut, const UCell* cell )
{
    UIndex n = cell->series.buf;
    if( ur_isShared(n) )
        return ut->env->dataStore.ptr.buf - n; 
    return ut->dataStore.ptr.buf + n;
}


/**
  Get modifiable series buffer.
  The macro ur_bufferSerM() should normally be used to call this function.

  \param cell   Pointer to valid series or bound word cell.

  \return Pointer to buffer referenced by cell->series.buf.  If the buffer
          is in shared storage then an error is generated and zero is returned.
*/
UBuffer* ur_bufferSeriesM( UThread* ut, const UCell* cell )
{
    UIndex n = cell->series.buf;
    if( ur_isShared(n) )
    {
        ur_error( ut, UR_ERR_SCRIPT, "Cannot modify %s in shared storage",
                  ur_atomCStr( ut, ut->env->dataStore.ptr.buf[-n].type ) );
        return 0;
    }
    return ut->dataStore.ptr.buf + n;
}


/**
  Set USeriesIter to series slice.
  
  \param si    Iterator struct to fill.
  \param cell  Pointer to a valid series cell.
*/
void ur_seriesSlice( UThread* ut, USeriesIter* si, const UCell* cell )
{
    const UBuffer* buf = si->buf = ur_bufferSer( cell );
    si->it  = (cell->series.it < buf->used) ? cell->series.it : buf->used;
    si->end = (cell->series.end < 0) ? buf->used : cell->series.end;
    if( si->end < si->it )
        si->end = si->it;
}


/**
  Set USeriesIterM to modifiable series slice.
  
  \param si    Iterator struct to fill.
  \param cell  Pointer to a valid series cell.

  \return UR_OK/UR_THROW
*/
int ur_seriesSliceM( UThread* ut, USeriesIterM* si, const UCell* cell )
{
    UBuffer* buf = si->buf = ur_bufferSerM( cell );
    if( ! buf )
        return UR_THROW;
    si->it  = (cell->series.it < buf->used) ? cell->series.it : buf->used;
    si->end = (cell->series.end < 0) ? buf->used : cell->series.end;
    if( si->end < si->it )
        si->end = si->it;
    return UR_OK;
}


#ifdef DEBUG
void dumpBuf( UThread* ut, UIndex bufN )
{
    UBuffer* buf = ur_isShared(bufN) ? (ut->env->dataStore.ptr.buf - bufN)
                                     : ur_buffer(bufN);
    if( ur_isSeriesType(buf->type) || (buf->type == UT_CONTEXT) )
    {
        UBuffer str;
        UCell cell;

        ur_setId( &cell, buf->type );
        ur_setSeries( &cell, bufN, 0 );

        ur_strInit( &str, UR_ENC_UTF8, 512 );
        ut->types[ buf->type ]->toString( ut, &cell, &str, 0 );
        ur_strTermNull( &str );
        dprint( "%s\n", str.ptr.c );
        ur_strFree( &str );
    }
    else
    {
        dprint( "UBuffer type %d  used %d\n", buf->type, buf->used );
    }
}
#endif


/** @} */


//EOF
