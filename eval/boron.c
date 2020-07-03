/*
  Copyright 2009-2018 Karl Robillard

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


#include "boron.h"
#include "os.h"
#include "os_file.h"
#include "urlan_atoms.h"
#include "mem_util.h"
#include "str.h"
#include "boron_internal.h"
//#include "cpuCounter.h"
//#define CFUNC_SERIALIZED


/** \defgroup boron Boron Interpreter
  The Boron Interpreter.

  @{
*/

/**
\file boron.h
The Boron programmer interface.
*/


#define DT(dt)          (ut->types[ dt ])
#define SERIES_DT(dt)   ((const USeriesType*) (ut->types[ dt ]))

#define errorType(msg)      ur_error(ut, UR_ERR_TYPE, msg)
#define errorScript(msg)    ur_error(ut, UR_ERR_SCRIPT, msg)


enum BoronEvalCells
{
    BT_RESULT,
    BT_DSTACK,
    BT_FSTACK,
    BT_CELL_COUNT
};


extern UPortDevice port_file;
#ifdef CONFIG_SOCKET
extern UPortDevice port_socket;
#endif
#ifdef CONFIG_SSL
extern UPortDevice port_ssl;
#endif
#ifdef CONFIG_THREAD
extern UPortDevice port_thread;
#endif

#include "boron_types.c"


// Lookup stack position of arguments in function frames.
static UCell* _funcStackFrame( BoronThread* bt, UIndex funcN )
{
    const UIndex* fi;
    const UBuffer* fr = &bt->frames;
    if( fr->used )
    {
        fi = fr->ptr.i32 + fr->used;
        do
        {
            fi -= 2;
            if( fi[0] == funcN )
                return bt->thread.stack.ptr.cell + fi[1];
        }
        while( fi != fr->ptr.i32 );
    }
    return NULL;
}


static const UCell* boron_wordCell( UThread* ut, const UCell* wordC )
{
    UCell* a1;

    switch( ur_binding(wordC) )
    {
        case BOR_BIND_FUNC:
            if( (a1 = _funcStackFrame( BT, wordC->word.ctx )) )
                return a1 + wordC->word.index;
            break;

        case BOR_BIND_OPTION:
            if( (a1 = _funcStackFrame( BT, wordC->word.ctx )) )
            {
                UCell* res = &BT->optionCell;
                ur_setId(res, UT_LOGIC);
                if( CFUNC_OPTIONS & (1 << wordC->word.index) )
                    ur_logic(res) = 1;
                return res;
            }
            break;

        case BOR_BIND_OPTION_ARG:
            if( (a1 = _funcStackFrame( BT, wordC->word.ctx )) )
            {
                if( CFUNC_OPTIONS & (1 << wordC->word.index) )
                {
                    return CFUNC_OPT_ARG( (wordC->word.index + 1) ) +
                           wordC->word.sel[0];
                }
                else
                {
                    UCell* res = &BT->optionCell;
                    ur_setId(res, UT_NONE);
                    return res;
                }
            }
            break;

        default:
            assert( 0 && "unknown binding" );
            break;
    }
    return 0;
}


static UCell* boron_wordCellM( UThread* ut, const UCell* wordC )
{
    switch( ur_binding(wordC) )
    {
        case BOR_BIND_FUNC:
        {
            UCell* a1;
            if( (a1 = _funcStackFrame( BT, wordC->word.ctx )) )
                return a1 + wordC->word.index;
        }
            break;

        case BOR_BIND_OPTION:
        case BOR_BIND_OPTION_ARG:
            ur_error( ut, UR_ERR_SCRIPT, "cannot modify local option %s",
                      ur_atomCStr(ut, ur_atom(wordC)) );
            break;

        default:
            assert( 0 && "unknown binding" );
            break;
    }
    return 0;
}


static void boron_threadInit( UThread* ut )
{
    ut->wordCell  = boron_wordCell;
    ut->wordCellM = boron_wordCellM;
    ur_binInit( &BT->tbin, 0 );
    BT->requestAccess = NULL;

    ur_arrInit( &BT->frames, sizeof(UIndex), 0 );

    ur_arrReserve( &ut->stack, 512 );
    BT->stackLimit = ut->stack.ptr.cell + 512 - 8;
    boron_reset( ut );
}


static void boron_threadMethod( UThread* ut, enum UThreadMethod op )
{
    switch( op )
    {
        case UR_THREAD_INIT:
            boron_threadInit( ut );
            break;

        case UR_THREAD_FREE:
            ur_arrFree( &BT->frames );
            ur_binFree( &BT->tbin );
            // Other data is in dataStore, so there is nothing more to free.
#ifdef CONFIG_ASSEMBLE
            if( BT->jit )
                jit_context_destroy( BT->jit );
#endif
            break;

        case UR_THREAD_FREEZE:
            boron_reset( ut );
            break;
    }
}


/**
  Reset thread after exception.
  Clears all stacks and exceptions.
*/
void boron_reset( UThread* ut )
{
    UCell* it = ut->stack.ptr.cell;
    ur_setId( it, UT_UNSET );           // Exception.
    ur_setId( it + 1, UT_UNSET );       // Value for named exception.
    ur_setId( it + 2, UT_UNSET );       // Initial result.
    ut->stack.used = 3;

    BT->frames.used = 0;
}


/**
  Make null terminated UTF-8 string in binary buffer.

  \param strC       Valid UT_STRING or UT_FILE cell.
  \param bin        Binary buffer to use.  If zero, then the temporary
                    thread binary will be used.

  \return Pointer to C string in bin.
*/
char* boron_cstr( UThread* ut, const UCell* strC, UBuffer* bin )
{
    return ur_cstr( strC, bin ? bin : &BT->tbin );
}


/**
  Make null terminated UTF-8 string in binary buffer.
  Any trailing slash or backslash is removed.

  \param strC       Valid UT_STRING or UT_FILE cell.
  \param bin        Binary buffer to use.  If zero, then the temporary
                    thread binary will be used.

  \return Pointer to C string in bin.
*/
char* boron_cpath( UThread* ut, const UCell* strC, UBuffer* bin )
{
    if( ! bin )
        bin = &BT->tbin;
    ur_cstr( strC, bin );
    if( bin->used )
    {
        int ch = bin->ptr.b[ bin->used - 1 ];
        if( ch == '/' || ch == '\\' )
            bin->ptr.c[ --bin->used ] = '\0';
    }
    return bin->ptr.c;
}


UBuffer* boron_tempBinary( const UThread* ut )
{
    return &BT->tbin;
}


#if 0
/*
  \param src        Valid UT_STRING cell.
  \param dstStr     Initialized string buffer.

  \return Pointer to null-terminated string in dstStr.
*/
char* boron_cstrSave( UThread* ut, const UCell* src, UBuffer* dstStr )
{
    const UBuffer* ss = ur_bufferSer(src);
    UIndex end = (src->series.end < 0) ? ss->used : src->series.end;
    if( end > src->series.it )
        ur_strAppend( dstStr, ss, src->series.it, end );
    ur_strTermNull( dstStr );
    return dstStr->ptr.c;
}
#endif


/**
  Throw named exception.

  \param atom       Exception name.
  \param stackPos   If not zero, set word binding to UR_BIND_STACK.

  \return UR_THROW
*/
UStatus boron_throwWord( UThread* ut, UAtom atom, UIndex stackPos )
{
    UCell* cell = ur_exception(ut);
    ur_setId( cell, UT_WORD );
    if( stackPos )
        ur_binding(cell) = UR_BIND_STACK;
    cell->word.ctx   = UR_INVALID_BUF;
    cell->word.atom  = atom;
    cell->word.index = stackPos;
    return UR_THROW;
}


/**
  Check if named exception was thrown.

  \param atom       Exception name.

  \return Non-zero if the thrown exception is the named word.
*/
int boron_catchWord( UThread* ut, UAtom atom )
{
    UCell* cell = ur_exception(ut);
    if( ur_is(cell, UT_WORD) && ur_atom(cell) == atom )
        return 1;
    return 0;
}


/*
  Call boron_doBlock() but throw away the result.   
*/
UStatus boron_doVoid( UThread* ut, const UCell* blkC )
{
    UStatus ok = boron_doBlock(ut, blkC, ur_push(ut, UT_UNSET));
    ur_pop(ut);
    return ok;
}


UIndex boron_seriesEnd( UThread* ut, const UCell* cell )
{
    const UBuffer* buf = ur_bufferSer( cell );
    if( cell->series.end > -1 && cell->series.end < buf->used )
        return cell->series.end;
    return buf->used;
}


#define a2  (a1 + 1)
#define a3  (a1 + 2)
#define ANY3(c,t1,t2,t3)    ((1<<ur_type(c)) & ((1<<t1) | (1<<t2) | (1<<t3)))

//dev = ((UPortDevice**) ut->env->ports.ptr.v)[ buf->form ]; 


#ifdef CONFIG_CHECKSUM
#include "checksum.c"
#endif
#ifdef CONFIG_COMPRESS
#include "compress.c"
#endif

#include "construct.c"
#include "encode.c"
#include "sort.c"
#include "cfunc.c"
#include "format.c"
#include "eval.c"

#ifdef CONFIG_THREAD
#include "thread.c"
#endif

#ifdef CONFIG_ASSEMBLE
#include "asm.c"
#endif


/**
  Add C functions to context.

  C Function Rules:
  \li Arguments are in a held block; they are safe from garbage collection.
  \li Result is part of a held block; it is safe from garbage collection.
  \li Result will never be the same as any of the arguments.
  \li Must return UR_OK/UR_THROW.

  \param ctxN   Context to add UT_CFUNC values to.
  \param funcs  Table of BoronCFunc pointers.
  \param spec   Function specifications starting with name, one function
                per line.
  \param slen   Specification string length.
*/
UStatus boron_defineCFunc( UThread* ut, UIndex ctxN, const BoronCFunc* funcTable,
                           const char* spec, int slen )
{
    UBlockIt bi;
    UCell tmp;
    UIndex hold[2];
    UCellFunc* cell;
    UCell* term;
    const UBuffer* sblk;
    UBuffer* argProg;
    UIndex binN;
    int sigFlags;
    const UCell* start;
    const UCell* specCells;


#ifdef CFUNC_SERIALIZED
    if( ur_serializedHeader( (const uint8_t*) spec, slen ) )
    {
        const uint8_t* uspec = (const uint8_t*) spec;
        if( ! ur_unserialize( ut, uspec, uspec + slen, &tmp ) )
            return UR_THROW;
    }
    else
#endif
    {
        if( ! ur_tokenize( ut, spec, spec + slen, &tmp ) )
            return UR_THROW;
    }
    hold[0] = ur_hold( tmp.series.buf );

    argProg = ur_genBuffers( ut, 1, &binN );        // gc!
    ur_binInit( argProg, 0 );
    hold[1] = ur_hold( binN );

    // Append terminator cell for loop below.
    term = ur_blkAppendNew( ur_buffer(tmp.series.buf), UT_UNSET );
    ur_setFlags(term, UR_FLAG_SOL);

    sblk = ur_blockIt( ut, &bi, &tmp );
    specCells = sblk->ptr.cell;     // Save cell pointer in case of gc below.
    start = bi.it;
    ur_foreach( bi )
    {
        if( ur_flags(bi.it, UR_FLAG_SOL) )
        {
            if( ur_is(start, UT_WORD) )
            {
                cell = (UCellFunc*) ur_ctxAddWord( ur_buffer(ctxN),
                                                   ur_atom(start) );
                ur_setId(cell, UT_CFUNC);
                cell->argProgOffset = argProg->used;
                cell->argProgN = binN;
                cell->m.func  = *funcTable++;

                //printf( "KR cfunc %s\n", ur_atomCStr(ut, ur_atom(start)) );

                tmp.series.it  = (start - specCells) + 1;
                tmp.series.end =  bi.it - specCells;
                boron_compileArgProgram( BT, &tmp, argProg, 0, &sigFlags );
                if( sigFlags )
                    ur_setFlags((UCell*) cell, FUNC_FLAG_NOTRACE);
            }

            if( ur_is(bi.it, UT_UNSET) )
                break;
            start = bi.it;
        }
    }
    ur_release( hold[0] );
    ur_release( hold[1] );

    return UR_OK;
}


void boron_overrideCFunc( UThread* ut, const char* name, BoronCFunc func )
{
    UBuffer* ctx = ur_threadContext( ut );
    int nameLen = strLen(name);
    int n = ur_ctxLookup( ctx, ur_intern( ut, name, nameLen ) );
    if( n > -1 )
    {
        UCell* cell = ur_ctxCell( ctx, n );
        if( ur_is(cell, UT_CFUNC) )
        {
            ((UCellFunc*) cell)->m.func = func;

            // Special handling for 'read which is used internally by
            // 'load (and thus by 'do & boron_load()).
            if( nameLen == 4 &&
                name[0]=='r' && name[1]=='e' && name[2]=='a' && name[3]=='d' )
                BENV->funcRead = func;
        }
    }
}


// OS_WORD should be the same as the uname operating system name.
#if defined(__ANDROID__)
#define OS_WORD "Android"       // Breaks uname convention.
#elif defined(__APPLE__)
#define OS_WORD "Darwin"
#elif defined(__FreeBSD__)
#define OS_WORD "FreeBSD"
#elif defined(__linux)
#define OS_WORD "Linux"
#elif defined(__sun__)
#define OS_WORD "SunOS"
#elif defined(_WIN32)
#define OS_WORD "Windows"
#else
#define OS_WORD "unknown"
#endif

#if defined(__arm__)
#define ARCH_WORD "arm"
#elif defined(__x86_64__) || defined(_M_X64)
#define ARCH_WORD "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define ARCH_WORD "x86"
#elif defined(__PPC__)
#define ARCH_WORD "ppc"
#elif defined(__sparc)
#define ARCH_WORD "sparc"
#else
#define ARCH_WORD "unknown"
#endif

#ifdef __BIG_ENDIAN__
#define ARCH_BIG  "true"
#else
#define ARCH_BIG  "false"
#endif

#include "boot.c"
static const char setupScript[] =
    "do bind ["
    "  os: '" OS_WORD " arch: '" ARCH_WORD " big-endian: " ARCH_BIG
    "] environs\n";


/*-hf- eq?
        a
        b
    return: logic!
    group: math, data
    Same as equal?
*/
/*-hf- tail?
        series
    return: logic!
    group: series
    see: empty?, head?
    Same as empty?
*/
/*-hf- close
        port
    return: unset!
    group: storage, io
    see: free, open
    Same as free.
*/
/*-hf- context
        spec block!
    return: New context!
    group: data
*/
/*-hf- charset
        spec string!
    return: New bitset!
    group: data
*/
/*-hf- error 
        message string!
    return: Throws error.
    group: control
*/
/*-hf- join
        a
        b
    return: New series.
    group: series
    Concatenate a and b.
*/
/*-hf- rejoin
        block
    return: New series.
    group: series
    Reduce block and concatenate results.
*/
/*-hf- replace
        series
        pat     Pattern to look for.
        rep     Replacement value.
        /all    Replace all occurances of the pattern, not just the first.
    return: Modified series at original position.
    group: series
*/
/*-hf- split-path
        path file!/string!
    return: Block with path and filename.
    group: data
    If no directory separator is found, then path (the first block item)
    will be none!.
*/
/*-hf- term-dir
        dir file!/string!
    return: Modified dir
    group: data
    Ensure that the directory has a trailing slash.
*/


/*
  Adds the following three words for each type: type type? to-type.
  The type atoms (type!) must already be interned.
*/
static void _addDatatypeWords( UThread* ut, int typeCount )
{
    UBuffer* ctx;
    BoronCFunc* fbuf;
    BoronCFunc* fi;
    char* cbuf;
    char* cp;
    char sig[8];
    const char* typeName;
    int i;


    // Datatype words.
    ctx = ur_threadContext( ut );
    for( i = 0; i < typeCount; ++i )
        ur_makeDatatype( ur_ctxAddWord(ctx, i), i );


    // Add datatype query & convert functions.
    fbuf = fi = (BoronCFunc*) malloc(typeCount * (sizeof(BoronCFunc)*2+16+19));
    cbuf = cp = (char*) (fbuf + (typeCount * 2));
    strcpy( sig, " a  0\n" );
    for( i = 0; i < typeCount; ++i )
    {
        typeName = ur_atomCStr( ut, i );
        if( *typeName == ':' )
            continue;           // Skip unassigned types (see _addDT).

        // Set variant number in signature.
        if( i > 9 )
            sig[3] = '0' + (i / 10);
        sig[4] = '0' + (i % 10);

        cp = str_copy( cp, typeName );
        cp[-1] = '?';           // Change ! to ?.
        cp = str_copy( cp, sig );

        cp = str_copy( cp, "to-" );
        cp = str_copy( cp, typeName );
        cp = str_copy( cp - 1, sig );

        *fi++ = cfunc_datatypeQ;
        *fi++ = cfunc_to_type;
    }
    *cp = '\0';
    //printf( "KR type functs (%ld chars)\n%s\n", cp - cbuf, cbuf );
    boron_defineCFunc( ut, UR_MAIN_CONTEXT, fbuf, cbuf, cp - cbuf );
    free( fbuf );
}


#ifdef CONFIG_RANDOM
extern CFUNC_PUB( cfunc_random );
#endif
#ifdef CONFIG_SOCKET
extern CFUNC_PUB( cfunc_set_addr );
extern CFUNC_PUB( cfunc_hostname );
#endif
#ifdef CONFIG_EXECUTE
extern CFUNC_PUB( cfunc_execute );
extern CFUNC_PUB( cfunc_with_flock );
#endif
extern CFUNC_PUB( cfunc_sleep );
extern CFUNC_PUB( cfunc_wait );

#ifdef CFUNC_SERIALIZED
#include "cfunc_table.c"
#else
#define DEF_CF(f,spec)  f,
static BoronCFunc _cfuncs[] =
{
#include "cfunc_table.c"
};

#undef DEF_CF
#define DEF_CF(f,spec)  spec
static const char _cfuncSpecs[] =
{
#include "cfunc_table.c"
};
#endif


/**
  Initialize UEnvParameters structure to default Boron values.
*/
UEnvParameters* boron_envParam( UEnvParameters* par )
{
    ur_envParam( par );
    par->envSize      = sizeof(BoronEnv);
    par->threadSize   = sizeof(BoronThread);
    par->threadMethod = boron_threadMethod;
    return par;
}


/**
  Make Boron environment and initial thread.

  \param par    Environment parameters initialized with boron_envParam().
*/
UThread* boron_makeEnv( UEnvParameters* par )
{
    UAtom atoms[ 14 ];
    UThread* ut;
    UCell* res;
    unsigned int dtCount;
    UStatus ok;

//#define TIME_MAKEENV
#ifdef TIME_MAKEENV
    uint64_t timeA, timeB, timeC, timeD, timeE;
#define COUNTER(t)  t = cpuCounter()
    COUNTER( timeA );
#else
#define COUNTER(t)
#endif

    {
    const UDatatype* table[ UT_MAX - UT_BI_COUNT ];
    const UDatatype** tt;
    unsigned int i;

    for( i = 0; i < (sizeof(boron_types) / sizeof(UDatatype)); ++i )
        table[i] = boron_types + i;
    dtCount = i + par->dtCount;

    tt = par->dtTable;
    for( ; i < dtCount; ++i )
        table[i] = *tt++;
    par->dtTable = table;
    par->dtCount = dtCount;

    ut = ur_makeEnv( par );
    }
    if( ! ut )
        return 0;

    COUNTER( timeB );

    // Need to override some Urlan methods.
    dt_context.make = context_make_override;

    BENV->funcRead = cfunc_read;

    ur_internAtoms( ut, "none true false file udp tcp thread"
        " func | local extern no-trace"
#ifdef CONFIG_SSL
        " udps tcps"
#endif
        , atoms );

    // Set compileAtoms for boron_compileArgProgram.
    memcpy( BENV->compileAtoms, atoms + 7, 5 * sizeof(UAtom) );

    // Register ports.
    ur_ctxInit( &BENV->ports, 4 );
    boron_addPortDevice( ut, &port_file,   atoms[3] );
#ifdef CONFIG_SOCKET
    boron_addPortDevice( ut, &port_socket, atoms[4] );
    boron_addPortDevice( ut, &port_socket, atoms[5] );
#endif
#ifdef CONFIG_THREAD
    boron_addPortDevice( ut, &port_thread, atoms[6] );

    // thread_queue() stores buffers in cells.
    assert( sizeof(UBuffer) <= sizeof(UCell) );
#endif
#ifdef CONFIG_SSL
    boron_addPortDevice( ut, &port_ssl,    atoms[10] );
    boron_addPortDevice( ut, &port_ssl,    atoms[11] );
#endif


    // Add some useful words.
    {
    UBuffer* ctx;
    UCell* cell;

    ctx = ur_threadContext( ut );
    ur_ctxReserve( ctx, 512 );

    cell = ur_ctxAddWord( ctx, atoms[0] );
    ur_setId(cell, UT_NONE);

    cell = ur_ctxAddWord( ctx, atoms[1] );
    ur_setId(cell, UT_LOGIC);
    ur_logic(cell) = 1;

    cell = ur_ctxAddWord( ctx, atoms[2] );
    ur_setId(cell, UT_LOGIC);
    //ur_logic(cell) = 0;
    }

    _addDatatypeWords( ut, UT_BI_COUNT + dtCount );
    ur_ctxSort( ur_threadContext( ut ) );


    // Add C functions.
    COUNTER( timeC );
    if( ! boron_defineCFunc( ut, UR_MAIN_CONTEXT, _cfuncs,
                             _cfuncSpecs, sizeof(_cfuncSpecs)-1 ) )
        goto fail;


    COUNTER( timeD );
#if 1
    res = ut->stack.ptr.cell + 2;
    ur_unserialize( ut, boot_data, boot_data + boot_len, res );
    if( ! ur_is(res, UT_BLOCK) )
        goto fail;
    boron_bindDefault( ut, res->series.buf );
    ok = boron_doBlock( ut, res, ur_push(ut, UT_UNSET) );
    ur_pop(ut);
    if( ! ok )
        goto fail;
#else
    if( ! boron_evalUtf8( ut, boot_data, sizeof(boot_len)-1 ) )
        goto fail;
#endif
    if( ! boron_evalUtf8( ut, setupScript, sizeof(setupScript)-1 ) )
    {
fail:
        ur_freeEnv( ut );
        ut = 0;
    }

#ifdef TIME_MAKEENV
    COUNTER( timeE );
    printf( "timeA: %ld\n", timeB - timeA );
    printf( "timeB: %ld\n", timeC - timeB );
    printf( "timeC: %ld\n", timeD - timeC );
    printf( "timeD: %ld\n", timeE - timeD );
#endif

    return ut;
}


/**
  Destroy Boron environment.

  \param ut     Initial thread created by boron_makeEnvP().
*/
void boron_freeEnv( UThread* ut )
{
    if( ut )
    {
        ur_ctxFree( &BENV->ports );
        ur_freeEnv( ut );
    }
}


const UAtom* boron_compileAtoms( BoronThread* bt )
{
    return ((BoronEnv*) bt->thread.env)->compileAtoms;
}


/**
  Load block! from file and give it default bindings.

  \param file     UTF-8 filename of Boron script or serialized data.
  \param res      The result cell for the new block value.

  \return UR_OK/UR_THROW.
*/
UStatus boron_load( UThread* ut, const char* file, UCell* res )
{
    UBuffer* str;
    UCell* arg;
    UIndex bufN;
    UStatus ok;

    str = ur_genBuffers( ut, 1, &bufN );        // gc!
    ur_strInit( str, UR_ENC_UTF8, 0 );
    ur_strAppendCStr( str, file );

    arg = ur_push( ut, UT_STRING );
    ur_setSeries( arg, bufN, 0 );
    ok = cfunc_load( ut, arg, res );
    ur_pop(ut);

    return ok;
}


/** \enum UserAccess
  These are the values returned by the boron_requestAccess() callback.
*/
/** \var UserAccess::UR_ACCESS_DENY
   Forbid access to the resource.
*/
/** \var UserAccess::UR_ACCESS_ALLOW
   Grant access to the resource.
*/
/** \var UserAccess::UR_ACCESS_ALWAYS
   Grant access to the resource and allow all future requests.
*/


/**
  Set the callback function that will request security access from the user.

  If this function is not set then boron_requestAccess() will always return
  UR_OK.

  \param func   Callback function that must return a UserAccess value.

  \sa boron_requestAccess(), UserAccess
*/
void boron_setAccessFunc( UThread* ut, int (*func)( UThread*, const char* ) )
{
    BT->requestAccess = func;
}


/**
  Request user permission to access a resource.

  \param  msg   Printf style format string.

  \return UR_OK/UR_THROW.

  \sa boron_setAccessFunc()
*/
UStatus boron_requestAccess( UThread* ut, const char* msg, ... )
{
    if( BT->requestAccess )
    {
        const int bufSize = 256;
        va_list arg;
        UBuffer bin;
        int access;

        ur_binInit( &bin, bufSize );

        va_start( arg, msg );
        vsnprintf( bin.ptr.c, bufSize, msg, arg );
        va_end( arg );

        bin.ptr.c[ bufSize - 1 ] = '\0';
        access = BT->requestAccess( ut, bin.ptr.c );

        ur_binFree( &bin );

        switch( access )
        {
            case UR_ACCESS_ALLOW:
                return UR_OK;

            case UR_ACCESS_ALWAYS:
                BT->requestAccess = 0;
                return UR_OK;
        }
        return ur_error( ut, UR_ERR_ACCESS, "User denied access" );
    }
    return UR_OK;
}


/** @} */

//EOF
