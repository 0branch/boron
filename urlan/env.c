/*
  Copyright 2009-2012 Karl Robillard

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
/** \var UrlanDataType::UT_TYPEMASK
  Used in UCellDatatype to declare a multi-type datatype!.
*/
/** \enum UrlanWordBindings
  Values for UCellWord::binding and ur_bind().
*/
/** \enum UrlanErrorType
  These descriptive codes are passed to ur_error().
*/
/** \enum UThreadMethod
  Operations on thread data for the UEnvParameters::threadMethod.
*/
/** \var UThreadMethod::UR_THREAD_INIT
  The new thread must be initialized.
*/
/** \var UThreadMethod::UR_THREAD_FREE
  The thread is being freed.
*/
/** \var UThreadMethod::UR_THREAD_FREEZE
  The thread dataStore is being moved to the shared environment.
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
/** \def ur_foreach
  Loop over all members of an iterator struct.
  These include UBlockIt, UBlockIter, USeriesIter.
*/


#include "env.h"
#include "str.h"
#include "mem_util.h"


//#define GC_HOLD_TEST  1

#define FREE_TERM       -1

#include "datatypes.c"
#include "atoms.c"


#define GEN_FREE    64
// No initial allocation when using GEN_FREE (would be redundant).
#ifdef GEN_FREE
#define INIT_BUF_COUNT  0
#else
#define INIT_BUF_COUNT  64
#endif


static void _nopThreadFunc( UThread* ut, enum UThreadMethod op )
{
    if( op == UR_THREAD_FREEZE )
        ut->stack.used = 0;
}


static void _threadInitStore( UThread* ut )
{
    static const uint8_t _initType[2] = {UT_BLOCK, UT_CONTEXT};
    UIndex bufN[2];

    ur_arrInit( &ut->dataStore, sizeof(UBuffer), INIT_BUF_COUNT );
    ut->holds.used = 0;
    ut->sharedStoreBuf = ut->env->sharedStore.ptr.buf;
    ut->freeBufCount = 0;
    ut->freeBufList = FREE_TERM;

    // Buffer index zero denotes an invalid buffer (UR_INVALID_BUF),
    // so remove it from general use.

    ur_generate( ut, 2, bufN, _initType );
    ur_hold( bufN[0] );     // UR_INVALID_BUF
    ur_hold( bufN[1] );     // UR_MAIN_CONTEXT
}


static UThread* _threadMake( UEnv* env )
{
    UThread* ut = (UThread*) memAlloc( env->threadSize );
    if( ! ut )
        return 0;

    memSet( ut, 0, env->threadSize );   // Zeros user extensions as well.
    ut->env   = env;
    ut->types = env->types;

    ur_arrInit( &ut->stack, sizeof(UCell), 0 );
    ur_arrInit( &ut->holds, sizeof(UIndex), 16 );
    ur_binInit( &ut->gcBits, INIT_BUF_COUNT / 8 );

    _threadInitStore( ut );
    env->threadFunc( ut, UR_THREAD_INIT );

    return ut;
}


/**
  Create a new thread.

  \param ut     Existing thread.

  \return Pointer to new thread in the same environment as ut, or
          NULL if memory could not be allocated.
*/
UThread* ur_makeThread( const UThread* ut )
{
    UEnv* env = ut->env;
    UThread* newt = _threadMake( env );
    if( newt )
    {
        LOCK_GLOBAL
        ++env->threadCount;
        UNLOCK_GLOBAL
    }
    return newt;
}


static void _destroyDataStore( UEnv* env, UBuffer* store )
{
    const UDatatype** dt = env->types;
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
*/
static void _threadFree( UThread* ut )
{
    ut->env->threadFunc( ut, UR_THREAD_FREE );
    _destroyDataStore( ut->env, &ut->dataStore );
    ur_arrFree( &ut->stack );
    ur_arrFree( &ut->holds );
    ur_binFree( &ut->gcBits );
    memFree( ut );
}


/**
  Free memory used by UThread.

  To destroy a thread created with ur_makeEnv(), use ur_freeEnv().

  \param ut     Thread created with ur_makeThread()

  \return Non-zero if thread is not NULL and not an initial thread.
*/
int ur_destroyThread( UThread* ut )
{
    UEnv* env = ut->env;

    if( ut && (ut != env->initialThread) )
    {
        assert( env->threadCount );
        LOCK_GLOBAL
        --env->threadCount;
        UNLOCK_GLOBAL

        _threadFree( ut );
        return 1;
    }
    return 0;
}


#ifdef DEBUG
static void _validateEnv()
{
    UCell cell;
    int i;

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


static void _addDT( UEnv* env, int id, const UDatatype* dt )
{
    static uint8_t reserved[] = ":type:XX";
    if( dt )
    {
        const char* end = dt->name;
        while( *end != '\0' )
            ++end;
        _internAtom( 0, &env->atomTable, &env->atomNames,
                     (uint8_t*) dt->name, (uint8_t*)end );
    }
    else
    {
        reserved[ sizeof(reserved) - 3 ] = '0' + (id / 10);
        reserved[ sizeof(reserved) - 2 ] = '0' + (id % 10);
        _internAtom( 0, &env->atomTable, &env->atomNames,
                     reserved, reserved + (sizeof(reserved) - 1) );
    }
    env->types[ id ] = dt;
}


extern UDatatype dt_coord;
extern USeriesType dt_vector;
#if CONFIG_HASHMAP
extern UDatatype dt_hashmap;
#endif
#if CONFIG_TIMECODE
extern UDatatype dt_timecode;
#endif


/**
  Initialize UEnvParameters structure to default values.
*/
UEnvParameters* ur_envParam( UEnvParameters* par )
{
#ifdef CONFIG_ATOM_LIMIT
    par->atomLimit     = CONFIG_ATOM_LIMIT;
#else
    par->atomLimit     = 2048;
#endif

#ifdef CONFIG_ATOM_NAMES
    par->atomNamesSize = CONFIG_ATOM_NAMES;
#else
    par->atomNamesSize = 2048 * 12;
#endif

    par->envSize       = sizeof(UEnv);
    par->threadSize    = sizeof(UThread);
    par->dtCount       = 0;
    par->dtTable       = 0;
    par->threadMethod  = _nopThreadFunc;

    return par;
}


/**
  \param atomLimit  Maximum number of atoms.
                    Memory usage is ((12 + 12) * atomLimit) bytes.
  \param dtTable    Array of pointers to user defined datatypes.
                    Pass zero if dtCount is zero.
  \param dtCount    Number of datatypes in dtTable.
  \param thrSize    Byte size of each thread in the environment.
                    Pass zero if no extra memory is needed.
  \param thrMethod  Callback function to initialize and cleanup threads.
                    This may be zero.
*/

/**
  Allocate UEnv and initial UThread.

  \param par    Environment parameters.

  \return Pointer to initial thread.
*/
UThread* ur_makeEnv( const UEnvParameters* par )
{
    UEnv* env;
    UThread* ut;
    int i;

#if 0
    printf( "KR sizeof(pthread_t) = %lu\n", sizeof(pthread_t) );
    printf( "KR sizeof(UEnv)      = %lu\n", sizeof(UEnv) );
    printf( "KR sizeof(UThread)   = %lu\n", sizeof(UThread) );
    printf( "KR sizeof(UBuffer)   = %lu\n", sizeof(UBuffer) );
#endif

#ifdef DEBUG
    _validateEnv();
#endif

    env = memAlloc( par->envSize );
    if( ! env )
        return 0;

    ur_arrInit( &env->sharedStore, sizeof(UBuffer), 0 );

    ur_binInit( &env->atomNames, par->atomNamesSize );
    ur_arrInit( &env->atomTable, sizeof(AtomRec), par->atomLimit );
    _rebuildAtomHash( &env->atomTable );

    env->typeCount = UT_BI_COUNT + par->dtCount;

    env->threadSize = par->threadSize;
    env->threadFunc = par->threadMethod;

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
    addDT( UT_DOUBLE,   &dt_double );
    addDT( UT_BIGNUM,   0 );
    addDT( UT_TIME,     &dt_time );
    addDT( UT_DATE,     &dt_date );
    addDT( UT_COORD,    &dt_coord );
    addDT( UT_VEC3,     &dt_vec3 );
#if CONFIG_TIMECODE
    addDT( UT_TIMECODE, &dt_timecode );
#else
    addDT( UT_TIMECODE, 0 );
#endif

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
#if CONFIG_HASHMAP
    addDT( UT_HASHMAP,  &dt_hashmap );
#else
    addDT( UT_HASHMAP,  0 );
#endif
    addDT( UT_ERROR,    &dt_error );

    i = UT_BI_COUNT;
    if( par->dtCount )
    {
        const UDatatype** tt = par->dtTable;
        const UDatatype** dtEnd = tt + par->dtCount;
        for( ; tt != dtEnd; ++tt, ++i )
            addDT( i, *tt );
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
    env->threadCount = 0;       // Does not include initial thread.
    env->initialThread = ut;


    // Intern commonly used atoms.
    {
    UAtom atoms[ 59 ];
    ur_internAtoms( ut,
                    "i8 u8 i16 u16 i32 u32 f32 f64 i64 u64\n"
                    "none true false on off yes no\n"
                    "quit halt return break continue extern local self\n"
                    "latin1 utf8 ucs2 url\n"
                    "+ - / * = < > <= >=\n"
                    "x y z r g b a\n"
                    "| opt some any skip set copy to thru into place bits\n"
                    "big-endian little-endian",
                    atoms );
    assert( atoms[0] == UT_MAX );
    }

    //dumpAtoms( ut );

    return ut;
}


/**
  Free environment and the initial thread.

  The caller must make sure no other threads created with ur_makeThread()
  are running.

  \param ut     UThread created with ur_makeEnv().  It is safe to pass zero.
*/
void ur_freeEnv( UThread* ut )
{
    UEnv* env;

    if( ! ut )
        return;
    env = ut->env;
    if( ut != env->initialThread )
        return;                     // Not the original thread.

    _threadFree( ut );

#ifdef DEBUG
    if( env->threadCount )
        fprintf( stderr, "ur_freeEnv: %d threads have not been freed\n",
                 env->threadCount );
#endif

    _destroyDataStore( env, &env->sharedStore );

    mutexFree( env->mutex );
    ur_binFree( &env->atomNames );
    ur_arrFree( &env->atomTable );

    memFree( env );
}


/**
  Move the startup thread dataStore to the shared environment.
  This may only be called once after ur_makeEnv(), and must be done
  before a second thread is created.

  The thread method UR_THREAD_FREEZE is called before any adjustments
  are made.  After the store has been converted, all buffer holds are removed
  and a fresh thread dataStore is created.

  \param ut     UThread created with ur_makeEnv().
*/
void ur_freezeEnv( UThread* ut )
{
    UEnv* env = ut->env;

    if( ! env || env->sharedStore.used )
        return;

    env->threadFunc( ut, UR_THREAD_FREEZE );

    // NOTE: The FREEZE method may change ut->stack so we don't touch it here.

    ur_recycle( ut );

    env->sharedStore = ut->dataStore;


    // Point all bindings & data store references to the shared environment.
    {
#define BLOCK_MASK \
    ((1 << UT_BLOCK) | (1 << UT_PAREN) | (1 << UT_CONTEXT) | \
     (1 << UT_PATH) | (1 << UT_LITPATH) | (1 << UT_SETPATH))

    UBuffer* it  = env->sharedStore.ptr.buf;
    UBuffer* end = it + env->sharedStore.used;
    const UDatatype** dt = env->types;

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
            //printf( "KR freeze buf %ld\n", it - env->sharedStore.ptr.buf );
            while( ci != cend )
            {
                if( ur_type(ci) >= UT_REFERENCE_BUF )
                    dt[ ur_type(ci) ]->toShared( ci );
                ++ci;
            }

            if( it->type == UT_CONTEXT )
                ur_ctxSort( it );
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

  \param name   Pointer to atom name.
  \param len    Length of name.

  \return Atom of word or UR_INVALID_ATOM if an error was generated.

  \sa ur_internAtom, ur_internAtoms
*/
UAtom ur_intern( UThread* ut, const char* name, int len )
{
    return ur_internAtom( ut, name, name + len );
}


/**
  Add a single atom to the shared environment.

  \param it   Start of word.
  \param end  End of word.

  \return Atom of word or UR_INVALID_ATOM if an error was generated.

  \sa ur_intern, ur_internAtoms
*/
UAtom ur_internAtom( UThread* ut, const char* it, const char* end )
{
    UAtom atom;
    UEnv* env = ut->env;

    LOCK_GLOBAL
    atom = _internAtom( ut, &env->atomTable, &env->atomNames,
                        (uint8_t*) it, (uint8_t*) end );
    UNLOCK_GLOBAL

    return atom;
}


/*
  This is an internal function used to optimize tokenize.
*/
UAtom ur_internAtomUnlocked( UThread* ut, const char* it, const char* end )
{
    UEnv* env = ut->env;
    return _internAtom( ut, &env->atomTable, &env->atomNames,
                        (uint8_t*) it, (uint8_t*) end );
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
        if( ! *cp )
            break;
        end = str_toWhite( cp );
        *atoms++ = _internAtom( 0, table, names, (uint8_t*)cp, (uint8_t*)end );
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

  \return Pointer to first buffer.  The buffer indicies are stored in index.
*/
UBuffer* ur_genBuffers( UThread* ut, int count, UIndex* index )
{
    UBuffer* next;
    UBuffer* store = &ut->dataStore;
    int i;

    if( ut->freeBufCount < count )
    {
#ifdef GEN_FREE
        int newCount = count + GEN_FREE;
#else
#define newCount    count
#endif
        ur_recycle( ut );
        if( ut->freeBufCount < newCount )
        {
            int id;
            int end;

            ur_arrReserve( store, store->used + newCount );
            id = store->used;
            end = id + count;
            while( id < end )
                *index++ = id++;
#ifdef GEN_FREE
            next = store->ptr.buf + id;
            ut->freeBufCount += GEN_FREE;
            end += GEN_FREE;
            while( id < end )
            {
                next->type  = UT_UNSET;
                next->used  = ut->freeBufList;
                next->ptr.v = 0;
                ++next;
                ut->freeBufList = id++;
            }
#endif
            store->used += newCount;
            return store->ptr.buf + index[ -count ];
        }
    }
#ifdef GC_HOLD_TEST
    else
    {
        ur_recycle( ut );
        {
        UBuffer tmp;
        ur_arrInit( &tmp, sizeof(UBuffer), store->used );
        memcpy( tmp.ptr.buf, store->ptr.buf, sizeof(UBuffer) * store->used );
        tmp.used = store->used;
        ur_arrFree( store );
        *store = tmp;
        }
    }
#endif

    assert( ut->freeBufList > FREE_TERM );

    for( i = 0; i < count; ++i )
    {
        next = store->ptr.buf + ut->freeBufList;

        assert( next->type == UT_UNSET );
        assert( next->ptr.v == 0 );

        index[i] = ut->freeBufList;
        ut->freeBufList = next->used;
    }
    ut->freeBufCount -= count;
    return store->ptr.buf + index[0];
}


/**
  Generate and initialize buffers of given types.

  Strings will have an UR_ENC_LATIN1 form.
  Vectors will have an UR_VEC_I32 form.

  \param count  Number of buffers to obtain.
  \param index  Return array of buffer ids.  This must large enough to
                to hold count indices.
  \param types  Array of UrlanDataType.

  \return Pointer to first buffer.  The buffer indicies are stored in index.
*/
UBuffer* ur_generate( UThread* ut, int count, UIndex* index,
                      const uint8_t* types )
{
    UBuffer* first = ur_genBuffers( ut, count, index );
    UBuffer* dsBuffers = ut->dataStore.ptr.buf;
    UBuffer* buf;
    UIndex* end = index + count;
    UIndex n;

    while( index != end )
    {
        n = *index++;
        buf = dsBuffers + n;

        // The following code is a semi-generic version of ur_binInit,
        // ur_strInit, ur_vecInit, ur_blkInit, & ur_ctxInit when size is 0.

        *((uint64_t*) buf) = 0;
        buf->type = *types++;
        buf->ptr.v = NULL;

        switch( buf->type )
        {
            case UT_STRING:
            case UT_FILE:
                buf->elemSize = 1;
                //buf->form = UR_ENC_LATIN1;
                break;
            case UT_VECTOR:
                buf->elemSize = 4;
                buf->form = UR_VEC_I32;
                break;
            case UT_BLOCK:
            case UT_PAREN:
            case UT_PATH:
            case UT_LITPATH:
            case UT_SETPATH:
                buf->elemSize = sizeof(UCell);
                break;
        }
    }
    return first;
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
  \def ur_pop
  Decrement stack.used.  Must not be called if stack.used is zero.
*/

/**
  Set id of cell on top of stack and increment stack.used.

  \return Pointer to initilized cell on stack.
*/
UCell* ur_push( UThread* ut, int type )
{
    UCell* cell = ut->stack.ptr.cell + ut->stack.used;
    ++ut->stack.used;
    ur_setId(cell, type);
    return cell;
}


/**
  Copy cell to top of stack and increment stack.used.

  \return Pointer to copy on stack.
*/
UCell* ur_pushCell( UThread* ut, const UCell* src )
{
    UCell* cell = ut->stack.ptr.cell + ut->stack.used;
    ++ut->stack.used;
    *cell = *src;
    return cell;
}


/**
  Get thread global context.

  \return Pointer to thread global context.
*/
UBuffer* ur_threadContext( UThread* ut )
{
    return ur_buffer( UR_MAIN_CONTEXT );
}


/**
  Get shared global context.

  \return Pointer to context, or zero if ur_freezeEnv() has not been called.
*/
UBuffer* ur_envContext( UThread* ut )
{
    UEnv* env = ut->env;
    if( env->sharedStore.used )
        return env->sharedStore.ptr.buf + UR_MAIN_CONTEXT;
    return 0;
}


/**
  Create error! exception.

  The errorType is only descriptive, it has no real function.

  \param errorType  UrlanErrorType (UR_ERR_TYPE, UR_ERR_SCRIPT, etc.).
  \param fmt        Error message with printf() style formatting.

  \return UR_THROW
*/
UStatus ur_error( UThread* ut, int errorType, const char* fmt, ... )
{
#define MAX_ERR_LEN 256
    static const uint8_t _types[2] = {UT_STRING, UT_BLOCK};
    UIndex bufN[2];
    UBuffer* str;
    UCell* cell;
    va_list arg;

    str = ur_generate( ut, 2, bufN, _types );
    ur_arrReserve( str, MAX_ERR_LEN );

    va_start( arg, fmt );
    vaStrPrint( str->ptr.c, MAX_ERR_LEN, fmt, arg );
    va_end( arg );
    str->used = strLen(str->ptr.c);


    cell = ur_exception(ut);
    ur_setId(cell, UT_ERROR);
    cell->error.exType = errorType;
    cell->error.messageStr = bufN[0];
    cell->error.traceBlk   = bufN[1];

    return UR_THROW;
}


void ur_traceError( UThread* ut, const UCell* errC, UIndex blkN,
                    const UCell* pos )
{
    const UBuffer* blk;
    UCell* cell;
    if( ur_flags(errC, UR_FLAG_ERROR_SKIP_TRACE) )
    {
        ur_clrFlags((UCell*) errC, UR_FLAG_ERROR_SKIP_TRACE);
    }
    else if( ! ur_isShared(errC->error.traceBlk) )
    {
        blk = ur_bufferEnv(ut, blkN);
        cell = ur_blkAppendNew(ur_buffer(errC->error.traceBlk), UT_BLOCK);
        ur_setSeries( cell, blkN, pos - blk->ptr.cell );
    }
}


/**
  Append block position to the current exception error! trace.

  \param blkN   Block id.
  \param it     Position in block blkN.
*/
void ur_appendTrace( UThread* ut, UIndex blkN, UIndex it )
{
    UCell* cell;
    const UCell* errC = ur_exception(ut);
    if( ur_is(errC, UT_ERROR) && ! ur_isShared(errC->error.traceBlk) )
    {
        cell = ur_blkAppendNew(ur_buffer(errC->error.traceBlk), UT_BLOCK);
        ur_setSeries(cell, blkN, it);
    }
}


/**
  Check if a value is "true".  Note that a word bound to 'none is true.

  \return Non-zero if cell is not none! or a false logic! value.
*/
int ur_true( const UCell* cell )
{
    int t = ur_type(cell);
    if( t == UT_LOGIC )
        return ur_logic(cell);
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
  Case-sensitive equality test.
  \return Non-zero if values are equivalent.
*/
int ur_equalCase( UThread* ut, const UCell* a, const UCell* b )
{
    int t = ur_type(a);
    if( t < ur_type(b) )
        t = ur_type(b);
    return ut->types[ t ]->compare( ut, a, b, UR_COMPARE_EQUAL_CASE );
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
  Case-sensitive ordering comparison.
  \return 1, 0, or -1, if cell a is greater than, equal to, or less than
          cell b.
*/
int ur_compareCase( UThread* ut, const UCell* a, const UCell* b )
{
    int t = ur_type(a);
    if( t < ur_type(b) )
        t = ur_type(b);
    return ut->types[ t ]->compare( ut, a, b, UR_COMPARE_ORDER_CASE );
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

  \return  Pointer to value cell.  If the word does not have a valid binding
           then an error is generated and zero is returned.
*/
const UCell* ur_wordCell( UThread* ut, const UCell* cell )
{
    switch( ur_binding(cell) )
    {
        case UR_BIND_UNBOUND:
            ur_error( ut, UR_ERR_SCRIPT, "unbound word '%s",
                      ur_wordCStr( cell ) );
            return 0;

        case UR_BIND_THREAD:
            return (ut->dataStore.ptr.buf + cell->word.ctx)->ptr.cell +
                   cell->word.index;

        case UR_BIND_ENV:
            return (ut->sharedStoreBuf - cell->word.ctx)->ptr.cell +
                   cell->word.index;

        case UR_BIND_STACK:
            return ut->stack.ptr.cell + cell->word.index;

        case UR_BIND_SELF:
        {
            UCell* self = &ut->tmpWordCell;
            ur_setId( self, UT_CONTEXT );
            ur_setSeries( self, cell->word.ctx, 0 );
            return self;
        }
    }
#if 1
    return ut->wordCell( ut, cell );
#else
    ur_error( ut, UR_ERR_SCRIPT, "word '%s has invalid binding",
              ur_wordCStr( cell ) );
    return 0;
#endif
}


/**
  Get modifiable word value.

  \param cell   Pointer to word cell.

  \return  Pointer to value cell.  If the word does not have a valid binding
           then an error is generated and zero is returned.
*/
UCell* ur_wordCellM( UThread* ut, const UCell* cell )
{
    switch( ur_binding(cell) )
    {
        case UR_BIND_UNBOUND:
            ur_error( ut, UR_ERR_SCRIPT, "unbound word '%s",
                      ur_wordCStr( cell ) );
            return 0;

        case UR_BIND_THREAD:
            return (ut->dataStore.ptr.buf + cell->word.ctx)->ptr.cell +
                   cell->word.index;

        case UR_BIND_ENV:
            ur_error( ut, UR_ERR_SCRIPT, "word '%s is in shared storage",
                      ur_wordCStr( cell ) );
            return 0;

        case UR_BIND_STACK:
            return ut->stack.ptr.cell + cell->word.index;

        case UR_BIND_SELF:
            ur_error( ut, UR_ERR_SCRIPT, "word '%s has self binding",
                      ur_wordCStr( cell ) );
            return 0;
    }
#if 1
    return ut->wordCellM( ut, cell );
#else
    ur_error( ut, UR_ERR_SCRIPT, "word '%s has invalid binding",
              ur_wordCStr( cell ) );
    return 0;
#endif
}


/**
  Set word.  This copies src into the cell which the word is bound to.

  If the word is unbound or bound to the shared environment then an error
  is generated and UR_THROW is returned.

  \param word   Word to set.
  \param src    Source value to copy.

  \return UR_OK/UR_THROW
*/
UStatus ur_setWord( UThread* ut, const UCell* word, const UCell* src )
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

#define BUF_ENV(N) \
    (ur_isShared(N) ? ut->sharedStoreBuf-N : ut->dataStore.ptr.buf+N)

/**
  Get buffer from either the thread dataStore or environment sharedStore.
  The macro ur_bufferE() should normally be used to call this function.

  \param n  Buffer identifier.

  \return Pointer to buffer n.
*/
const UBuffer* ur_bufferEnv( UThread* ut, UIndex n )
{
    return BUF_ENV(n);
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
const UBuffer* ur_bufferSeries( const UThread* ut, const UCell* cell )
{
    UIndex n = cell->series.buf;
    return BUF_ENV(n);
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
                  ur_atomCStr( ut, ut->sharedStoreBuf[-n].type ) );
        return 0;
    }
    return ut->dataStore.ptr.buf + n;
}


void ur_initSeries( UCell* cell, int type, UIndex buf )
{
    ur_setId(cell, type);
    ur_setSeries(cell, buf, 0);
}


/** \struct USeriesIter
  Iterator for const series of any type.
  ur_foreach() can be used to loop over the cells.

  \var USeriesIter::buf
  Buffer pointer.

  \var USeriesIter::it
  Start position.

  \var USeriesIter::end
  End position.
*/

/**
  Set USeriesIter to series slice.
  
  \param si    Iterator struct to fill.
  \param cell  Pointer to a valid series cell.
*/
void ur_seriesSlice( const UThread* ut, USeriesIter* si, const UCell* cell )
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
UStatus ur_seriesSliceM( UThread* ut, USeriesIterM* si, const UCell* cell )
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


/** \struct UBlockIt
  Iterator for const UCell array.
  ur_foreach() can be used to loop over the cells.

  \var UBlockIt::it
    Start position.

  \var UBlockIt::end
    End position.
*/

/**
  Set UBlockIt to the start and end of a block slice.

  \param bi         Iterator struct to fill.
  \param blkCell    Pointer to a valid block cell.

  \return Pointer to buffer referenced by blkCell.
*/
const UBuffer* ur_blockIt( const UThread* ut, UBlockIt* bi,
                           const UCell* blkCell )
{
    const UBuffer* blk;
    UIndex n, end;

    n = blkCell->series.buf;
    blk = BUF_ENV(n);
    end = blk->used;

    n = blkCell->series.end;
    if( n > -1 && n < end )
        end = n;

    n = blkCell->series.it;
    if( n > end )
        n = end;

    bi->it  = blk->ptr.cell + n;
    bi->end = blk->ptr.cell + end;
    return blk;
}


#ifdef DEBUG
void dumpBuf( UThread* ut, UIndex bufN )
{
    const UBuffer* buf = BUF_ENV(bufN);
    if( ur_isSeriesType(buf->type) || (buf->type == UT_CONTEXT) )
    {
        UBuffer str;
        UCell cell;

        ur_initSeries( &cell, buf->type, bufN );

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


void dumpStore( UThread* ut )
{
    const UBuffer* store = &ut->dataStore;
    const UBuffer* buf  = store->ptr.buf;
    const UBuffer* bend = buf + store->used;
    const UCell* it;
    const UCell* end;
    int type;

    for( ; buf != bend; ++buf )
    {
        switch( buf->type )
        {
            case UT_BLOCK:
            case UT_PAREN:
            case UT_CONTEXT:
                dprint( "%ld %s [", buf - store->ptr.buf,
                        ur_atomCStr(ut, buf->type)  );
                it  = buf->ptr.cell;
                end = it + buf->used;
                for( ; it != end; ++it )
                {
                    type = ur_type(it);
                    if( ur_isSeriesType(type) || (type == UT_CONTEXT) )
                        dprint( "%d ", it->series.buf );
                    else
                        dprint( ". " );
                }
                dprint( "]\n" );
                break;
        }
    }
}
#endif


/** @} */


//EOF
