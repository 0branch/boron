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
#include "bignum.h"
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


#ifndef OLD_EVAL
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
#endif


static const UCell* boron_wordCell( UThread* ut, const UCell* wordC )
{
#ifndef OLD_EVAL
	UCell* a1;
#endif
    switch( ur_binding(wordC) )
    {
#ifdef OLD_EVAL
        case BOR_BIND_FUNC:
        {
            LocalFrame* bottom = BT->bof;
            LocalFrame* top = BT->tof;
            while( top != bottom )
            {
                --top;
                if( top->funcBuf == wordC->word.ctx )
                    return top->args + wordC->word.index;
            }
        }
            ur_error( ut, UR_ERR_SCRIPT, "local word is out of scope" );
            break;

        case BOR_BIND_OPTION:
        {
            LocalFrame* bottom = BT->bof;
            LocalFrame* top = BT->tof;
            while( top != bottom )
            {
                --top;
                if( top->funcBuf == wordC->word.ctx )
                {
                    // Return option logic! value.
                    UCell* opt = top->args - 1;
                    ur_int(opt) = (OPT_BITS(opt) & (1 << wordC->word.index))
                                  ? 1 : 0;
                    return opt;
                }
            }
        }
            ur_error( ut, UR_ERR_SCRIPT, "local option is out of scope" );
            break;
#else
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
                else
                    ur_logic(res) = 0;
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
#endif

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
#ifdef OLD_EVAL
        case BOR_BIND_FUNC:
        {
            LocalFrame* bottom = BT->bof;
            LocalFrame* top = BT->tof;
            while( top != bottom )
            {
                --top;
                if( top->funcBuf == wordC->word.ctx )
                    return top->args + wordC->word.index;
            }
        }
            ur_error( ut, UR_ERR_SCRIPT, "local word is out of scope" );
            break;
#else
        case BOR_BIND_FUNC:
        {
            UCell* a1;
            if( (a1 = _funcStackFrame( BT, wordC->word.ctx )) )
                return a1 + wordC->word.index;
        }
            break;

        case BOR_BIND_OPTION_ARG:
#endif
        case BOR_BIND_OPTION:
            ur_error( ut, UR_ERR_SCRIPT, "cannot modify local option %s",
                      ur_atomCStr(ut, ur_atom(wordC)) );
            break;

        default:
            assert( 0 && "unknown binding" );
            break;
    }
    return 0;
}

//typedef const UCell* (*wc_func)( UThread*, const UCell* );


static void boron_threadInit( UThread* ut )
{
#ifdef OLD_EVAL
    UIndex bufN[3];
    UBuffer* buf;
    UCell* ed;
#endif


    ut->wordCell  = boron_wordCell;
    ut->wordCellM = boron_wordCellM;
    ur_binInit( &BT->tbin, 0 );
    BT->requestAccess = NULL;

#ifndef OLD_EVAL
    ur_arrInit( &BT->frames, sizeof(UIndex), 0 );

    ur_arrReserve( &ut->stack, 512 );
    boron_reset( ut );
#else
    ur_setId( &BT->fo, UT_LOGIC );

    // Create evalData block.  This never changes size so we can safely
    // keep a pointer to the cells.

    ur_genBuffers( ut, 3, bufN );
    BT->holdData = ur_hold(bufN[0]);    // Hold evalData forever.


    // evalData block
    buf = ur_buffer(bufN[0]);
    ur_blkInit( buf, UT_BLOCK, BT_CELL_COUNT );
    buf->used = BT_CELL_COUNT;
    BT->evalData = ed = buf->ptr.cell;


    // Default Result.
    ur_setId( ed + BT_RESULT, UT_UNSET );


    // Data Stack block
    BT->dstackN = bufN[1];
    buf = ur_buffer(bufN[1]);
    ur_blkInit( buf, UT_BLOCK, 256 );
    ++ed;
    ur_initSeries( ed, UT_BLOCK, bufN[1] );

    // tos is freely changed.  When a recycle occurs, the cfunc recycle
    // method will sync. the dstackN block used value.
    BT->tos = buf->ptr.cell;
    BT->eos = buf->ptr.cell + ur_avail(buf);


    // Frame Stack (array of pointers to cells on Data Stack).
    BT->fstackN = bufN[2];
    buf = ur_buffer(bufN[2]);
    ur_arrInit( buf, sizeof(LocalFrame), 256 / 2 );
    // Set type to something so ur_gcReport() doesn't report bufN[2] as unused.
    buf->type = UT_VECTOR;
    ++ed;
    ur_initSeries( ed, UT_BINARY, bufN[2] );    // UT_VECTOR

    // tof is freely changed.  When a recycle occurs, the cfunc recycle
    // method will sync. the fstackN block used value.
    BT->tof = BT->bof = ur_ptr(LocalFrame, buf);
    BT->eof = BT->tof + ur_avail(buf);
#endif
}


static void boron_threadMethod( UThread* ut, enum UThreadMethod op )
{
    switch( op )
    {
        case UR_THREAD_INIT:
            boron_threadInit( ut );
            break;

        case UR_THREAD_FREE:
#ifndef OLD_EVAL
            ur_arrFree( &BT->frames );
#endif
            ur_binFree( &BT->tbin );
            // Other data is in dataStore, so there is nothing more to free.
#ifdef CONFIG_ASSEMBLE
            if( BT->jit )
                jit_context_destroy( BT->jit );
#endif
            break;

        case UR_THREAD_FREEZE:
#ifdef OLD_EVAL
            ur_buffer(BT->dstackN)->used = 0;
            ur_buffer(BT->fstackN)->used = 0;
            ur_release( BT->holdData );
            BT->dstackN = 0;    // Disables cfunc_recycle2.
#endif
            break;
    }
}


/**
  Reset thread after exception.
  Clears all stacks and exceptions.
*/
void boron_reset( UThread* ut )
{
#ifdef OLD_EVAL
    UBuffer* buf;

    // Clear data stack.
    buf = ur_buffer( BT->dstackN );
    BT->tos = buf->ptr.cell;

    // Clear frame stack.
    buf = ur_buffer( BT->fstackN );
    BT->tof = ur_ptr(LocalFrame, buf);

    // Clear exceptions.
    buf = ur_errorBlock(ut);
    buf->used = 0;
#else
    UCell* it = ut->stack.ptr.cell;
    ur_setId( it, UT_UNSET );           // Exception.
    ur_setId( it + 1, UT_UNSET );       // Value for named exception.
    ur_setId( it + 2, UT_UNSET );       // Initial result.
    ut->stack.used = 3;

    BT->frames.used = 0;
#endif
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
int boron_throwWord( UThread* ut, UAtom atom, UIndex stackPos )
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


#ifdef OLD_EVAL
static int _catchThrownWord( UThread* ut, UAtom atom )
{
    UBuffer* blk = ur_errorBlock(ut);
    UCell* cell = blk->ptr.cell + (blk->used - 1);
    if( ur_is(cell, UT_WORD) && (ur_atom(cell) == atom) )
    {
        --blk->used;
        return 1;
    }
    return 0;
}
#else
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
#endif


#if 0
static void reportStack( UThread* ut )
{
    printf( "KR stack used: %ld\n",
            BT->tos - ur_buffer(BT->dstackN)->ptr.cell );
}
#endif

#ifdef OLD_EVAL
UCell* boron_stackPush( UThread* ut )
{
    if( BT->tos == BT->eos )
    {
        ur_error( ut, UR_ERR_INTERNAL, "data stack overflow" );
        return 0;
    }
    return BT->tos++;
}

UCell* boron_stackPushN( UThread* ut, int n )
{
    UCell* top = BT->tos;
    if( n > (BT->eos - BT->tos) )
    {
        ur_error( ut, UR_ERR_INTERNAL, "data stack overflow" );
        return 0;
    }
    BT->tos += n;
    return top;
}

#define boron_stackPop(ut)      --((BoronThread*) ut)->tos
#define boron_stackPopN(ut,N)   ((BoronThread*) ut)->tos -= N


int boron_framePush( UThread* ut, UCell* args, UIndex funcBuf )
{
    LocalFrame* frame = BT->tof;
    if( frame == BT->eof )
        return ur_error( ut, UR_ERR_INTERNAL, "frame stack overflow" );
    frame->args = args;
    frame->funcBuf = funcBuf;
    ++BT->tof;
    return UR_OK;
}

#define boron_framePop(ut)      --BT->tof
#endif


/*
  Call boron_doBlock() but throw away the result.   
*/
int boron_doVoid( UThread* ut, const UCell* blkC )
{
    int ok;
    UCell* tmp;
#ifdef OLD_EVAL
    if( (tmp = boron_stackPush(ut)) )   // Hold result.
    {
        ok = boron_doBlock( ut, blkC, tmp ) ? UR_OK : UR_THROW;
        boron_stackPop(ut);
        return ok;
    }
    return UR_THROW;
#else
    tmp = ur_push(ut,UT_UNSET);     // Hold result.
    ok = boron_doBlock( ut, blkC, tmp ) ? UR_OK : UR_THROW;
    ur_pop(ut);
    return ok;
#endif
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
#include "eval.c"

#ifdef CONFIG_THREAD
#include "thread.c"
#endif

#ifdef CONFIG_ASSEMBLE
#include "asm.c"
#endif


/**
  Add C function to the thread context.

  \deprecated   boron_defineCFunc() should be used instead of this function.

  C Function Rules:
  \li Arguments are in a held block; they are safe from garbage collection.
  \li Result is part of a held block; it is safe from garbage collection.
  \li Result will never be the same as any of the arguments.

  \param func   Function.  This must return UR_OK/UR_THROW.
  \param sig    ASCII string describing the calling signature.
*/
void boron_addCFunc( UThread* ut, BoronCFunc func, const char* sig )
{
    boron_defineCFunc( ut, UR_MAIN_CONTEXT, &func, sig, strlen(sig) );
}


/**
  Add C functions to context.

  \param ctxN   Context to add UT_CFUNC values to.
  \param funcs  Table of BoronCFunc pointers.
  \param spec   Function specifications starting with name, one function
                per line.
  \param slen   Specification string length.
*/
int boron_defineCFunc( UThread* ut, UIndex ctxN, const BoronCFunc* funcTable,
                       const char* spec, int slen )
{
    UBlockIter bi;
    UCell tmp;
    UIndex hold[2];
    UCellFunc* cell;
    UCell* term;
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

    argProg = ur_genBuffers( ut, 1, &binN );
    ur_binInit( argProg, 0 );

    hold[0] = ur_hold( tmp.series.buf );
    hold[1] = ur_hold( binN );

    // Append terminator cell for loop below.
    term = ur_blkAppendNew( ur_buffer(tmp.series.buf), UT_UNSET );
    ur_setFlags(term, UR_FLAG_SOL);

    ur_blkSlice( ut, &bi, &tmp );
    specCells = bi.buf->ptr.cell;   // bi.buf can change during gc below.
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
                    ur_setFlags((UCell*) cell, FUNC_FLAG_GHOST);
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
    int n = ur_ctxLookup( ctx, ur_intern( ut, name, strLen(name) ) );
    if( n > -1 )
    {
        UCell* cell = ur_ctxCell( ctx, n );
        if( ur_is(cell, UT_CFUNC) )
            ((UCellFunc*) cell)->m.func = func;
    }
}


// OS_WORD should be the same as the uname operating system name.
#if defined(__APPLE__)
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

static const char setupScript[] =
    "environs: make context! [\n"
    "  version: 0,3,3\n"
    "  os: '" OS_WORD " arch: '" ARCH_WORD " big-endian: " ARCH_BIG
    "]\n"
    "q: :quit  yes: true  no: false\n"
    "eq?: :equal?\n"
    "tail?: :empty?\n"
    "close: :free\n"
    "context: func [b block!] [make context! b]\n"
    "charset: func [s char!/string!] [make bitset! s]\n"
    "error: func [s string! /ghost] [throw make error! s]\n"
    "join: func [a b] [\n"
    "  a: either series? a [copy a][to-text a]\n"
    "  append a reduce b\n"
    "]\n"
    "rejoin: func [b block!] [\n"
    "  if empty? b: reduce b [return b]\n"
    "  append either series? first b\n"
    "    [copy first b]\n"
    "    [to-text first b]\n"
    "  next b\n"
    "]\n"
    "replace: func [series pat rep /all | f size] [\n"
    "  size: either series? pat [size? pat][1]\n"
    "  either all [\n"
    "    f: series\n"
    "    while [f: find f pat] [f: change/part f rep size]\n"
    "  ][\n"
    "    if f: find series pat [change/part f rep size]\n"
    "  ]\n"
    "  series\n"
    "]\n"
    "split-path: func [path | end] [\n"
    "  either end: find/last path '/'\n"
    "    [++ end reduce [slice path end  end]]\n"
    "    [reduce [none path]]\n"
    "]\n"
    "term-dir: func [path] [terminate/dir path '/']\n"
    "funct: func [spec body] [\n"
    "  ifn find spec '| [append spec '|]\n"
    "  func collect/unique/into set-word! body spec body\n"
    "]\n"
    "\n";

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
  This calls boron_makeEnvP() internally.

  \param dtTable    Array of pointers to user defined datatypes.
                    Pass zero if dtCount is zero.
  \param dtCount    Number of datatypes in dtTable.
*/
UThread* boron_makeEnv( const UDatatype** dtTable, unsigned int dtCount )
{
    UEnvParameters par;

    boron_envParam( &par );

    if( dtCount > 0 )
    {
        par.dtCount = dtCount;
        par.dtTable = dtTable;
    }

    return boron_makeEnvP( &par );
}


/**
  Make Boron environment and initial thread.

  \param par        Environment parameters.
*/
UThread* boron_makeEnvP( UEnvParameters* par )
{
    UAtom atoms[ 13 ];
    UThread* ut;
    unsigned int dtCount;

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

    ut = ur_makeEnvP( par );
    }
    if( ! ut )
        return 0;

    COUNTER( timeB );

    // Need to override some Urlan methods.
    dt_context.make = context_make_override;


    ur_internAtoms( ut, "none true false file udp tcp thread"
        " func | extern ghost"
#ifdef CONFIG_SSL
        " udps tcps"
#endif
        , atoms );

    // Set compileAtoms for boron_compileArgProgram.
    memcpy( BENV->compileAtoms, atoms + 7, 4 * sizeof(UAtom) );

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
    ur_int(cell) = 1;

    cell = ur_ctxAddWord( ctx, atoms[2] );
    ur_setId(cell, UT_LOGIC);
    ur_int(cell) = 0;
    }

    _addDatatypeWords( ut, UT_BI_COUNT + dtCount );
    ur_ctxSort( ur_threadContext( ut ) );


    // Add C functions.
    COUNTER( timeC );
    if( ! boron_defineCFunc( ut, UR_MAIN_CONTEXT, _cfuncs,
                             _cfuncSpecs, sizeof(_cfuncSpecs)-1 ) )
        goto fail;


    COUNTER( timeD );
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


#ifdef OLD_EVAL
/*
  Evaluate arguments and invoke function.
  blkC->series.it is advanced.

  \param fcell  Function cell.  This cell must be in a held block.
  \param blkC   Block cell where series.it < series.end.
                The series.buf must be held.
  \param res    Result.  This cell must be in a held block.

  \return UR_OK/UR_THROW.
*/
static int boron_call( UThread* ut, const UCellFunc* fcell, UCell* blkC,
                       UCell* res )
{
    if( fcell->argBufN )
    {
        UCellFunc fcopy;
        UCell* args;
        int ok;
        int nc = 0;

        // Copy fcell since it can become invalid in boron_eval1 (it may
        // be in a context which grows).  Only needed if FO_fetchArg used.
        fcopy = *fcell;

        // Run function argument program.
        {
            UCell* it;
            UCell* end;
            const uint8_t* progStart = ur_bufferE( fcopy.argBufN )->ptr.b;
            const uint8_t* pc = progStart;
            while( (ok = *pc++) < FO_end ) 
            {
                switch( ok )
                {
                    case FO_clearLocal:
                    case FO_clearLocalOpt:
                        nc = *pc++;

                        if( ! (args = boron_stackPushN( ut, nc )) )
                        {
                            BT->fo.optionMask = 0;
                            goto traceError;
                        }
                        end = args + nc;
                        if( ok == FO_clearLocalOpt )
                        {
                            *args++ = *((UCell*) &BT->fo);
                            BT->fo.optionMask = 0;
                        }

                        for( it = args; it != end; ++it )
                            ur_setId(it, UT_NONE);
                        it = args;
                        break;

                    case FO_fetchArg:
                        if( blkC->series.it >= blkC->series.end ) 
                            goto func_short;
                        if( ! (ok = boron_eval1( ut, blkC, it++ )) )
                            goto cleanup;
                        break;

                    case FO_litArg:
                        if( blkC->series.it >= blkC->series.end ) 
                            goto func_short;
                        {
                        const UBuffer* blk = ur_bufferSer(blkC);
                        *it++ = blk->ptr.cell[ blkC->series.it++ ];
                        }
                        break;

                    case FO_variant:
                        ur_setId(it, UT_INT);
                        ur_int(it) = *pc++;
                        ++it;
                        break;

                    case FO_checkArg:
                        if( ur_type(it - 1) != *pc++ )
                        {
bad_arg:
                            ur_error( ut, UR_ERR_TYPE,
                                      "function argument %d is invalid",
                                      it - args );
                            goto cleanup_trace;
                        }
                        break;

                    case FO_checkArgMask:
                    {
                        const uint32_t* mask = (const uint32_t*) pc;
                        int tm = ur_type(it - 1);
                        if( tm > 31 )
                        {
                            ++mask;
                            tm -= 32;
                        }
                        if( ! (*mask & (1 << tm)) )
                            goto bad_arg;
                    }
                        pc += sizeof(uint32_t) * 2;
                        break;

                    case FO_option:
                    {
                        UCellFuncOpt* fopt = (UCellFuncOpt*) (args - 1);
                        if( fopt->jumpIt < fopt->jumpEnd )
                        {
                            pc = progStart + fopt->optionJump[ fopt->jumpIt++ ];
                        }
                        else
                        {
                            // TODO: Should continue with program
                            //       (there may be a variant, etc).
                            goto prog_done;
                        }
                    }
                        break;

                    case FO_setArgPos:
                        it = args + *pc++;
                        break;

                    case FO_nop:
                        break;

                    case FO_nop2:
                        ++pc;
                        break;
                }
            }
        }

prog_done:

        assert( nc );

        if( fcopy.id.type == UT_CFUNC )
        {
            ok = fcopy.m.func( ut, args, res );
            if( ! ok && ! (fcopy.id.flags & FUNC_FLAG_GHOST) )
                goto cleanup_trace;
        }
        else
        {
            UCell tmp;
            ur_initSeries(&tmp, UT_BLOCK, fcopy.m.f.bodyN);

            if( (ok = boron_framePush( ut, args, fcopy.m.f.bodyN )) )
            {
                ok = boron_doBlock( ut, &tmp, res ) ? UR_OK : UR_THROW;
                boron_framePop( ut );
                if( ! ok && ! (fcopy.id.flags & FUNC_FLAG_GHOST) )
                {
                    if( _catchThrownWord( ut, UR_ATOM_RETURN ) )
                        ok = UR_OK;
                    else
                        goto cleanup_trace;
                }
            }
        }

cleanup:

        boron_stackPopN( ut, nc );
        return ok;

func_short:

        ur_error( ut, UR_ERR_SCRIPT, "Unexpected end of block" );

cleanup_trace:

        boron_stackPopN( ut, nc );
        goto traceError;
    }
    else
    {
        if( fcell->id.type == UT_CFUNC )
        {
            // Pass blkC so 'eval-control' cfuncs can do custom evaluation.
            if( ! fcell->m.func( ut, blkC, res ) )
                goto traceError;
        }
        else
        {
            UCell tmp;
            ur_initSeries(&tmp, UT_BLOCK, fcell->m.f.bodyN);

            if( ! boron_doBlock( ut, &tmp, res ) )
            {
                if( ! _catchThrownWord( ut, UR_ATOM_RETURN ) )
                    goto traceError;
            }
        }
        return UR_OK;
    }

traceError:

    // NOTE: This slows down throw of non-error values.
    ur_appendTrace( ut, blkC->series.buf, blkC->series.it-1 );
    return UR_THROW;
}


/**
  Evaluate one value in block.

  blkC->series.it is advanced.

  \param blkC   Block cell where series.it < series.end.
                The series.buf must be held.
  \param res    Result.  This cell must be in a held block.

  \return UR_OK/UR_THROW.
*/
int boron_eval1( UThread* ut, UCell* blkC, UCell* res )
{
    const UCell* cell;

    cell = ur_bufferSer(blkC)->ptr.cell + blkC->series.it;

    switch( ur_type(cell) )
    {
        case UT_WORD:
            if( ! (cell = ur_wordCell( ut, cell )) )
                goto traceError;
            if( ur_is(cell, UT_CFUNC) || ur_is(cell, UT_FUNC) )
                goto call_func;
#ifdef CONFIG_ASSEMBLE
            if( ur_is(cell, UT_AFUNC) )
            {
                ++blkC->series.it;
                return _asmCall( ut, (UCellFunc*) cell, blkC, res );
            }
#endif
            if( ur_is(cell, UT_UNSET) )
            {
                ur_error( ut, UR_ERR_SCRIPT, "unset word '%s",
                          ur_wordCStr( ur_bufferSer(blkC)->ptr.cell +
                                       blkC->series.it ) );
                goto traceError;
            }
            goto set_res;

        case UT_LITWORD:
            *res = *cell;
            res->id.type = UT_WORD;
            ++blkC->series.it;
            break;

        case UT_SETWORD:
        case UT_SETPATH:
        {
            const UCell* scell = cell;
            UIndex sit = blkC->series.it;
            do
            {
                ++cell;
                ++sit;
                if( sit == blkC->series.end )
                    goto end_of_block;
            }
            while( ur_is(cell, UT_SETWORD) || ur_is(cell, UT_SETPATH) );

            blkC->series.it = sit;
            if( ! boron_eval1( ut, blkC, res ) )
                return UR_THROW;

            for( ; scell != cell; ++scell )
            {
                if( ur_is(scell, UT_SETWORD) )
                {
                    if( ! ur_setWord( ut, scell, res ) )
                    {
                        --blkC->series.it;
                        goto traceError;
                    }
                }
                else
                {
                    if( ! ur_setPath( ut, scell, res ) )
                    {
                        --blkC->series.it;
                        goto traceError;
                    }
                }
            }
        }
            break;

        case UT_GETWORD:
            if( ! (cell = (UCell*) ur_wordCell( ut, cell )) )
                goto traceError;
            goto set_res;

        case UT_PAREN:
            if( ! boron_doBlock( ut, cell, res ) )
                return UR_THROW;
            ++blkC->series.it;
            break;

        case UT_PATH:
        {
            int headType;

            BT->fo.jumpEnd = 0;
            headType = ur_pathCell( ut, cell, res );
            if( ! headType )
                goto traceError;
            if( (ur_is(res, UT_CFUNC) || ur_is(res, UT_FUNC)) &&
                headType == UT_WORD )
            {
                cell = res;
                goto call_func_option;
            }
        }
            ++blkC->series.it;
            break;

        case UT_LITPATH:
            *res = *cell;
            res->id.type = UT_PATH;
            ++blkC->series.it;
            break;

        case UT_FUNC:
        case UT_CFUNC:
            goto call_func;

        default:
set_res:
            *res = *cell;
            ++blkC->series.it;
            break;
    }
    return UR_OK;

call_func:

    BT->fo.jumpEnd = 0;

call_func_option:

    ++blkC->series.it;
    if( boron_call( ut, (UCellFunc*) cell, blkC, res ) )
        return UR_OK;
    return UR_THROW;

end_of_block:

    ur_error( ut, UR_ERR_SCRIPT, "Unexpected end of block" );

traceError:

    // NOTE: This slows down throw of non-error values.
    ur_appendTrace( ut, blkC->series.buf, blkC->series.it );
    return UR_THROW;
}
#endif


/**
  Load block! from file and give it default bindings.

  \param file     UTF-8 filename of Boron script or serialized data.
  \param res      The result cell for the new block value.

  \return UR_OK/UR_THROW.
*/
int boron_load( UThread* ut, const char* file, UCell* res )
{
    UBuffer* str;
    UCell* arg;
    UIndex bufN;
    int ok;

    str = ur_genBuffers( ut, 1, &bufN );        // gc!
    ur_strInit( str, UR_ENC_UTF8, 0 );
    ur_strAppendCStr( str, file );

#ifdef OLD_EVAL
    if( ! (arg = boron_stackPush(ut)) )
        return UR_THROW;
    ur_initSeries( arg, UT_STRING, bufN );
    ok = cfunc_load( ut, arg, res );
    boron_stackPop(ut);
#else
    arg = ur_push( ut, UT_STRING );
    ur_setSeries( arg, bufN, 0 );
    ok = cfunc_load( ut, arg, res );
    ur_pop(ut);
#endif

    return ok;
}


#ifdef OLD_EVAL
/**
  Evaluate block and get result.

  blkC and res may point to the same cell.

  \param blkC   Block to do.  This buffer must be held.
  \param res    Result. This cell must be in a held block.

  \return UR_OK/UR_THROW.
*/
int boron_doBlock( UThread* ut, const UCell* blkC, UCell* res )
{
    UCell bc2;

    //ur_blkSlice( ut, &bi, blkC );
    {
        const UBuffer* buf = ur_bufferSer(blkC);
        if( ! buf->ptr.b )
        {
            bc2.series.it = bc2.series.end = 0;
        }
        else
        {
            bc2 = *blkC;
            if( (bc2.series.end < 0) || (bc2.series.end > buf->used) )
                bc2.series.end = buf->used;
        }
    }

    ur_setId( res, UT_UNSET );

    while( bc2.series.it < bc2.series.end )
    {
        if( ! boron_eval1( ut, &bc2, res ) )
            return UR_THROW;
    }
    //reportStack( ut );

    return UR_OK;
}


/**
  Evaluate block and get result.

  \param blkN   Index of block to do.  This buffer must be held.
  \param res    Result. This cell must be in a held block.

  \return UR_OK/UR_THROW.
*/
int boron_doBlockN( UThread* ut, UIndex blkN, UCell* res )
{
    UCell bc2;

    ur_setId( res, UT_UNSET );

    ur_setId( &bc2, UT_BLOCK );
    bc2.series.buf = blkN;
    bc2.series.it  = 0;
    bc2.series.end = ur_bufferE( blkN )->used;

    while( bc2.series.it < bc2.series.end )
    {
        if( ! boron_eval1( ut, &bc2, res ) )
            return UR_THROW;
    }
    return UR_OK;
}


/**
  Evaluate C string.

  \param cmd  String to evaluate.
  \param len  Length of cmd string.  May be -1 if cmd is null terminated.

  \return UR_OK/UR_THROW.
*/
int boron_doCStr( UThread* ut, const char* cmd, int len )
{
    const char* end;
    UIndex blkN;

    if( ! cmd )
        return UR_OK;

    if( len < 0 )
    {
        end = cmd;
        while( *end )
            ++end;
    }
    else
    {
        end = cmd + len;
    }

    if( end != cmd )
    {
        UCell tmp;
        UIndex hold;
        int ok;

        blkN = ur_tokenize( ut, cmd, end, &tmp );
        if( blkN )
        {
            boron_bindDefault( ut, blkN );

            hold = ur_hold( blkN );
            ok = boron_doBlock( ut, &tmp, RESULT );
            ur_release( hold );
            return ok;
        }
        return UR_THROW;
    }
    return UR_OK;
}


/**
  Get result of last boron_doCStr() call.
*/
UCell* boron_result( UThread* ut )
{
    return RESULT;
}
#endif


/**
  Get most recent exception.

  \return Pointer to top cell on ur_errorBlock(), or zero if there are no
          exceptions.
*/
#ifdef OLD_EVAL
UCell* boron_exception( UThread* ut )
{
    UBuffer* blk = ur_errorBlock(ut);
    if( blk->used )
        return blk->ptr.cell + (blk->used - 1);
    return 0;
}
#endif


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
int boron_requestAccess( UThread* ut, const char* msg, ... )
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
