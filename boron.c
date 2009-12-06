/*
  Copyright 2009 Karl Robillard

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
#include "urlan_atoms.h"
#include "mem_util.h"
#include "str.h"
#include "quickSortIndex.h"


/** \defgroup boron Boron Interpreter
  The Boron Interpreter.

  @{
*/

/**
\file boron.h
The Boron programmer interface.
*/


int boron_doBlock( UThread* ut, const UCell* ec, UCell* res );
int boron_eval1( UThread* ut, UCell* blkC, UCell* res );

#define MAX_OPT     8       // LIMIT: 8 options per func/cfunc.
#define OPT_BITS(c) (c)->series.end

#define DT(dt)          (ut->types[ dt ])
#define SERIES_DT(dt)   ((const USeriesType*) (ut->types[ dt ]))

#define errorType(msg)      ur_error(ut, UR_ERR_TYPE, msg)
#define errorScript(msg)    ur_error(ut, UR_ERR_SCRIPT, msg)


typedef struct
{
    UCell* args;
    UIndex funcBuf;
}
LocalFrame;


typedef struct BoronThread
{
    UThread ut;
    UCell*  evalData;
    UCell*  tos;
    UCell*  eos;
    LocalFrame* bof;
    LocalFrame* tof;
    LocalFrame* eof;
    UIndex  holdData;
    UIndex  dstackN;
    UIndex  fstackN;
    UIndex  tempN;
    uint32_t funcOptions;
}
BoronThread;

#define BT      ((BoronThread*) ut)
#define RESULT  (BT->evalData + BT_RESULT)

enum BoronEvalCells
{
    BT_RESULT,
    BT_DSTACK,
    BT_FSTACK,
    BT_TEMP,
    BT_CELL_COUNT
};


#include "boron_types.c"


static const UCell* boron_wordCell( UThread* ut, const UCell* cell )
{
    switch( ur_binding(cell) )
    {
        case UR_BIND_FUNC:
        {
            LocalFrame* bottom = BT->bof;
            LocalFrame* top = BT->tof;
            while( top != bottom )
            {
                --top;
                if( top->funcBuf == cell->word.ctx )
                    return top->args + cell->word.index;
            }
        }
            ur_error( ut, UR_ERR_SCRIPT, "local word is out of scope" );
            break;

        case UR_BIND_OPTION:
        {
            LocalFrame* bottom = BT->bof;
            LocalFrame* top = BT->tof;
            while( top != bottom )
            {
                --top;
                if( top->funcBuf == cell->word.ctx )
                {
                    // Return option logic! value.
                    UCell* opt = top->args - 1;
                    ur_int(opt) = (OPT_BITS(opt) && (1 << cell->word.index))
                                  ? 1 : 0;
                    return opt;
                }
            }
        }
            ur_error( ut, UR_ERR_SCRIPT, "local option is out of scope" );
            break;

        default:
            assert( 0 && "unknown binding" );
            break;
    }
    return 0;
}


static UCell* boron_wordCellM( UThread* ut, const UCell* cell )
{
    switch( ur_binding(cell) )
    {
        case UR_BIND_FUNC:
        {
            LocalFrame* bottom = BT->bof;
            LocalFrame* top = BT->tof;
            while( top != bottom )
            {
                --top;
                if( top->funcBuf == cell->word.ctx )
                    return top->args + cell->word.index;
            }
        }
            ur_error( ut, UR_ERR_SCRIPT, "local word is out of scope" );
            break;

        case UR_BIND_OPTION:
            ur_error( ut, UR_ERR_SCRIPT, "cannot modify local option" );
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
    UIndex bufN[4];
    UBuffer* buf;
    UCell* ed;


    ut->wordCell  = boron_wordCell;
    ut->wordCellM = boron_wordCellM;

    // Create evalData block.  This never changes size so we can safely
    // keep a pointer to the cells.

    ur_genBuffers( ut, 4, bufN );
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
    ur_setId( ed, UT_BLOCK );
    ur_setSeries( ed, bufN[1], 0 );

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
    ur_setId( ed, UT_BINARY );  // UT_VECTOR
    ur_setSeries( ed, bufN[2], 0 );

    // tof is freely changed.  When a recycle occurs, the cfunc recycle
    // method will sync. the fstackN block used value.
    BT->tof = BT->bof = ur_ptr(LocalFrame, buf);
    BT->eof = BT->tof + ur_avail(buf);


    // Temporary binary
    BT->tempN = bufN[3];
    ur_binInit( ur_buffer(bufN[3]), 0 );
    ++ed;
    ur_setId( ed, UT_BINARY );
    ur_setSeries( ed, bufN[3], 0 );


    // Install recycle method here or else it would get called by
    // ur_genBuffers above.
    boron_types[1].recycle = cfunc_recycle2;
}


static void boron_threadMethod( UThread* ut, UThreadMethod op )
{
    switch( op )
    {
        case UR_THREAD_INIT:
            boron_threadInit( ut );
            break;

        case UR_THREAD_FREE:
            // All data is stored in dataStore, so there is nothing to free.
            break;

        case UR_THREAD_FREEZE:
            boron_types[1].recycle = unset_recycle;
            ur_buffer(BT->dstackN)->used = 0;
            ur_buffer(BT->fstackN)->used = 0;
            ur_release( BT->holdData );
            break;
    }
}


/**
  Reset thread after exception.
  Clears all stacks and exceptions.
*/
void boron_reset( UThread* ut )
{
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
}


extern int copyLatin1ToUtf8( uint8_t* dest, const uint8_t* src, int len );
extern int copyUcs2ToUtf8( uint8_t* dest, const uint16_t* src, int len );

/**
  Make null terminated UTF-8 string in binary buffer.

  \param strC       Valid UT_STRING or UT_FILE cell.
  \param bin        Binary buffer to use.  If zero, then the temporary
                    thread binary will be used.

  \return Pointer to C string in bin.
*/
const char* boron_cstr( UThread* ut, const UCell* strC, UBuffer* bin )
{
    const UBuffer* str = ur_bufferSer(strC);
    int len = str->used;

    if( ! bin )
        bin = ur_buffer( BT->tempN );

    if( (strC->series.end > -1) && (strC->series.end < len) )
        len = strC->series.end;
    len -= strC->series.it;
    if( len > 0 )
    {
        ur_binReserve( bin, (len * 2) + 1 );

        switch( str->form )
        {
            case UR_ENC_LATIN1:
                // TODO: Prevent overflow of dest.
                len = copyLatin1ToUtf8( bin->ptr.b,
                                        str->ptr.b + strC->series.it, len );
                break;

            case UR_ENC_UTF8:
                memCpy( bin->ptr.b, str->ptr.b + strC->series.it, len ); 
                break;

            case UR_ENC_UCS2:
                // TODO: Prevent overflow of dest.
                len = copyUcs2ToUtf8( bin->ptr.b,
                                      str->ptr.u16 + strC->series.it, len );
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


/**
  Make null terminated UTF-8 string in binary buffer.
  Any trailing slash or backslash is removed.

  \param strC       Valid UT_STRING or UT_FILE cell.
  \param bin        Binary buffer to use.  If zero, then the temporary
                    thread binary will be used.

  \return Pointer to C string in bin.
*/
const char* boron_cpath( UThread* ut, const UCell* strC, UBuffer* bin )
{
    if( ! bin )
        bin = ur_buffer( BT->tempN );
    boron_cstr( ut, strC, bin );
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
  Append word cell to ur_errorBlock().
  \return UR_THROW
*/
int boron_throwWord( UThread* ut, UAtom atom )
{
    UBuffer* blk = ur_errorBlock(ut);
    UCell* cell = ur_blkAppendNew( blk, UT_WORD );
    ur_setWordUnbound( cell, atom );
    return UR_THROW;
}


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


#if 0
static void reportStack( UThread* ut )
{
    printf( "KR stack used: %ld\n",
            BT->tos - ur_buffer(BT->dstackN)->ptr.cell );
}
#endif

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


/*
  Call boron_doBlock() but throw away the result.   
*/
int boron_doVoid( UThread* ut, const UCell* blkC )
{
    int ok;
    UCell* tmp;
    if( (tmp = boron_stackPush(ut)) )   // Hold result.
    {
        ok = boron_doBlock( ut, blkC, tmp );
        boron_stackPop(ut);
        return ok;
    }
    return UR_THROW;
}


UIndex boron_seriesEnd( UThread* ut, const UCell* cell )
{
    const UBuffer* buf = ur_bufferSer( cell );
    if( cell->series.end > -1 && cell->series.end < buf->used )
        return cell->series.end;
    return buf->used;
}


#ifdef CONFIG_CHECKSUM
#include "boron_checksum.c"
#endif
#ifdef CONFIG_COMPRESS
#include "boron_compress.c"
#endif
#ifdef CONFIG_RANDOM
#include "boron_random.c"
#endif

#include "boron_cfunc.c"


/**
  Add C function to the thread context.

  C Function Rules:
  \li Arguments are in a held block; they are safe from garbage collection.
  \li Result is part of a held block; it is safe from garbage collection.
  \li Result will never be the same as any of the arguments.

  \param func   Function.  This must return UR_OK/UR_THROW.
  \param sig    ASCII string describing the calling signature.
*/
void boron_addCFunc( UThread* ut, int (*func)(UThread*,UCell*,UCell*),
                     const char* sig )
{
    UCellFunc* cell;
    const char* cp;
    const char* end;
    UAtom atom;


    cp  = str_skipWhite( sig );
    end = str_toWhite( cp );
    if( cp == end )
        return;
    atom = ur_internAtom( ut, cp, end );

    cell = (UCellFunc*) ur_ctxAddWord( ur_threadContext(ut), atom );

    ur_setId(cell, UT_CFUNC);
    cell->argBufN = UR_INVALID_BUF;
    cell->m.func  = func;

    cp = str_skipWhite( end );
    if( cp != end )
    {
        UCell tmp;
        UIndex hold;
        if( ur_tokenize( ut, cp, cp + strLen(cp), &tmp ) )
        {
            hold = ur_hold( tmp.series.buf );
            cell->argBufN = boron_makeArgProgram( ut, &tmp, 0, 0, cell );
            ur_release( hold );
        }
    }
}


#if defined(__APPLE__)
#define OS_WORD "Darwin"
#elif defined(__FreeBSD__)
#define OS_WORD "FreeBSD"
#elif defined(__linux)
#define OS_WORD "Linux"
#elif defined(__sun__)
#define OS_WORD "Solaris"
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

static const char setupScript[] =
    "environs: make context! [\n"
    "  version: 0,1,0 os: '" OS_WORD " arch: '" ARCH_WORD "]\n"
    "q: :quit  yes: true  no: false\n"
    "eq?: :equal?\n"
    "tail?: :empty?\n"
    "context: func [b block!] [make context! b]\n"
    "charset: func [s string!] [make bitset! s]\n"
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
    "    while [f: find series pat] [change/part f rep size]\n"
    "  ][\n"
    "    if f: find series pat [change/part f rep size]\n"
    "  ]\n"
    "  series\n"
    "]\n"
    "split-path: func [path | end] [\n"
    "  either end: find/last path '/'\n"
    "    [reduce [slice path ++ end  end]]\n"
    "    [reduce [none path]]\n"
    "]\n"
    "term-dir: func [path] [terminate/dir path '/']\n"
    "\n";

/*-hf- eq?
        a
        b
    return: logic!
    Same as equal?
*/
/*-hf- tail?
        series
    return: logic!
    Same as empty?
*/
/*-hf- context
        spec block!
    return: New context!
*/
/*-hf- charset
        spec string!
    return: New bitset!
*/
/*-hf- error 
        message string!
    return: Throws error.
*/
/*-hf- join
        a
        b
    return: New series.
    Concatenate a and b.
*/
/*-hf- rejoin
        block
    return: New series.
    Reduce block and concatenate results.
*/
/*-hf- replace
        series
        pat     Pattern to look for.
        rep     Replacement value.
        /all    Replace all occurances of the pattern, not just the first.
    return: Modified series.
*/
/*-hf- split-path
        path file!/string!
    return: Block with path and filename.
    If no directory separator is found, then path (the first block item)
    will be none!.
*/
/*-hf- term-dir
        dir file!/string!
    return: Modified dir
    Ensure that the directory has a trailing slash.
*/


/**
  Make Boron environment and initial thread.
*/
UThread* boron_makeEnv()
{
    UThread* ut = ur_makeEnv( boron_types,
                              sizeof(boron_types) / sizeof(UDatatype),
                              sizeof(BoronThread), boron_threadMethod );
    if( ! ut )
        return 0;


    // Need to override some Urlan methods.
    dt_context.make = context_make_override;


    // Add some useful words.
    {
    UAtom atoms[ 3 ];
    char name[32];
    UBuffer* ctx;
    UCell* cell;
    int i;
    int typeCount;

    ur_internAtoms( ut, "none true false", atoms );
    ctx = ur_threadContext( ut );

    cell = ur_ctxAddWord( ctx, atoms[0] );
    ur_setId(cell, UT_NONE);

    cell = ur_ctxAddWord( ctx, atoms[1] );
    ur_setId(cell, UT_LOGIC);
    ur_int(cell) = 1;

    cell = ur_ctxAddWord( ctx, atoms[2] );
    ur_setId(cell, UT_LOGIC);
    ur_int(cell) = 0;

    // Datatype words.
    typeCount = UT_BI_COUNT + (sizeof(boron_types) / sizeof(UDatatype));
    for( i = 0; i < typeCount; ++i )
        ur_makeDatatype( ur_ctxAddWord(ctx, i), i );

    // Datatype query cfuncs.
    for( i = 0; i < typeCount; ++i )
    {
        char* cp = str_copy( name, ur_atomCStr( ut, i ) );
        cp[-1] = '?';
        *cp++ = ' ';
        *cp++ = 'v';
        *cp++ = ' ';
        if( i > 9 )
            *cp++ = '0' + (i / 10);
        *cp++ = '0' + (i % 10);
        *cp   = '\0';
        boron_addCFunc( ut, cfunc_datatypeQ, name );
    }
    }


    // Add C functions.

#define addCFunc(func,spec)    boron_addCFunc(ut, func, spec)

#ifdef DEBUG
    addCFunc( cfunc_nop,     "nop" );
#endif
    addCFunc( cfunc_quit,    "quit" );
    addCFunc( cfunc_halt,    "halt" );
    addCFunc( cfunc_return,  "return val" );
    addCFunc( cfunc_break,   "break" );
    addCFunc( cfunc_throw,   "throw val /ghost" );
    addCFunc( cfunc_catch,   "catch val" );
    addCFunc( cfunc_try,     "try val" );
    addCFunc( cfunc_recycle, "recycle" );
    addCFunc( cfunc_do,      "do" );            // val (eval-control)
    addCFunc( cfunc_set,     "set w val" );
    addCFunc( cfunc_get,     "get w" );
    addCFunc( cfunc_in,      "in c w" );
    addCFunc( cfunc_bind,    "bind b w" );
    addCFunc( cfunc_infuse,  "infuse b w" );
    addCFunc( cfunc_add,     "add a b" );
    addCFunc( cfunc_sub,     "sub a b" );
    addCFunc( cfunc_mul,     "mul a b" );
    addCFunc( cfunc_div,     "div a b" );
    addCFunc( cfunc_and,     "and a b" );
    addCFunc( cfunc_or,      "or a b" );
    addCFunc( cfunc_xor,     "xor a b" );
    addCFunc( cfunc_minimum, "minimum a b" );
    addCFunc( cfunc_maximum, "maximum a b" );
    addCFunc( cfunc_make,    "make type spec" );
    addCFunc( cfunc_copy,    "copy val /deep" );
    addCFunc( cfunc_does,    "does body" );
    addCFunc( cfunc_func,    "func spec body" );
    addCFunc( cfunc_probe,   "probe val" );
    addCFunc( cfunc_prin,    "prin val" );
    addCFunc( cfunc_print,   "print val" );
    addCFunc( cfunc_to_string, "to-string val" );
    addCFunc( cfunc_to_text, "to-text val" );
    addCFunc( cfunc_all,     "all val" );
    addCFunc( cfunc_any,     "any val" );
    addCFunc( cfunc_reduce,  "reduce val" );
    addCFunc( cfunc_not,     "not val" );
    addCFunc( cfunc_if,      "if exp body /ghost" );
    addCFunc( cfunc_ifn,     "ifn exp body /ghost" );
    addCFunc( cfunc_either,  "either exp a b /ghost" );
    addCFunc( cfunc_while,   "while exp body /ghost" );
    addCFunc( cfunc_loop,    "loop n body /ghost" );
    addCFunc( cfunc_select,  "select data val" );
    addCFunc( cfunc_switch,  "switch val body" );
    addCFunc( cfunc_first,   "first ser" );
    addCFunc( cfunc_second,  "second ser" );
    addCFunc( cfunc_last,    "last ser" );
    addCFunc( cfunc_2plus,   "++ 'w" );
    addCFunc( cfunc_2minus,  "-- 'w" );
    addCFunc( cfunc_prev,    "prev ser" );
    addCFunc( cfunc_next,    "next ser" );
    addCFunc( cfunc_head,    "head ser" );
    addCFunc( cfunc_tail,    "tail ser" );
    addCFunc( cfunc_pick,    "pick ser n" );
    addCFunc( cfunc_poke,    "poke ser n val" );
    addCFunc( cfunc_skip,    "skip ser n" );
    addCFunc( cfunc_append,  "append ser val /block" );
    addCFunc( cfunc_change,  "change ser val /slice /part limit" );
    addCFunc( cfunc_remove,  "remove ser /part n" );
    addCFunc( cfunc_find,    "find ser val /last /case /part n" );
    addCFunc( cfunc_clear,   "clear ser" );
    addCFunc( cfunc_slice,   "slice ser n" );
    addCFunc( cfunc_emptyQ,  "empty? ser" );
    addCFunc( cfunc_headQ,   "head? ser" );
    addCFunc( cfunc_sizeQ,   "size? ser" );
    addCFunc( cfunc_indexQ,     "index? ser" );
    addCFunc( cfunc_seriesQ,    "series? ser" );
    addCFunc( cfunc_any_blockQ, "any-block? val" );
    addCFunc( cfunc_any_wordQ,  "any-word? val" );
    addCFunc( cfunc_complement, "complement val" );
    addCFunc( cfunc_intersect,  "intersect a b" );
    addCFunc( cfunc_difference, "difference a b" );
    addCFunc( cfunc_sort,       "sort ser" );
    addCFunc( cfunc_foreach,    "foreach 'w s body /ghost" );
    addCFunc( cfunc_forall,     "forall 'w body /ghost" );
    addCFunc( cfunc_map,        "map 'w ser body /ghost" );
    addCFunc( cfunc_existsQ,    "exists? file" );
    addCFunc( cfunc_dirQ,       "dir? file" );
    addCFunc( cfunc_make_dir,   "make-dir path" );
    addCFunc( cfunc_getenv,     "getenv val" );
    addCFunc( cfunc_read,       "read from /text /into b" );
    addCFunc( cfunc_write,      "write to data /append" );
    addCFunc( cfunc_delete,     "delete file" );
    addCFunc( cfunc_rename,     "rename a b" );
    addCFunc( cfunc_load,       "load from" );
    addCFunc( cfunc_parse,      "parse input rules" );
    addCFunc( cfunc_sameQ,      "same? a b" );
    addCFunc( cfunc_equalQ,     "equal? a b" );
    addCFunc( cfunc_gtQ,        "gt? a b" );
    addCFunc( cfunc_ltQ,        "lt? a b" );
    addCFunc( cfunc_typeQ,      "type? a" );
    addCFunc( cfunc_encodingQ,  "encoding? a" );
    addCFunc( cfunc_lowercase,  "lowercase s" );
    addCFunc( cfunc_uppercase,  "uppercase s" );
    addCFunc( cfunc_trim,       "trim s /indent /lines" );
    addCFunc( cfunc_terminate,  "terminate ser val /dir" );
    addCFunc( cfunc_to_hex,     "to-hex n" );
    addCFunc( cfunc_to_dec,     "to-dec n" );
    addCFunc( cfunc_now,        "now /date" );
#ifdef CONFIG_CHECKSUM
    addCFunc( cfunc_checksum,   "checksum val /sha1 /crc16 /crc32" );
#endif
#ifdef CONFIG_COMPRESS
    addCFunc( cfunc_compress,   "compress s" );
    addCFunc( cfunc_decompress, "decompress b" );
#endif
#ifdef CONFIG_RANDOM
    addCFunc( cfunc_random,     "random a /seed" );
#endif


    if( ! boron_doCStr( ut, setupScript, sizeof(setupScript)-1 ) )
    {
        ur_freeEnv( ut );
        ut = 0;
    }

    return ut;
}


/**
  Destroy Boron environment.

  \param ut     Initial thread created by boron_makeEnv().
*/
void boron_freeEnv( UThread* ut )
{
    ur_freeEnv( ut );
}


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
        UCell* args;
        int ok;
        int nc = 0;

        // Run function argument program.
        {
            UCell* it;
            UCell* end;
            const UBuffer* abuf = ur_bufferE( fcell->argBufN );
            const uint8_t* pc = abuf->ptr.b;
            while( (ok = *pc++) < FO_end ) 
            {
                switch( ok )
                {
                    case FO_clearLocal:
                    case FO_clearLocalOpt:
                        nc = *pc++;

                        if( ! (args = boron_stackPushN( ut, nc )) )
                            goto traceError;
                        end = args + nc;
                        if( ok == FO_clearLocalOpt )
                        {
                            ur_setId(args, UT_LOGIC);
                            OPT_BITS(args) = BT->funcOptions;
                            BT->funcOptions = 0;
                            ++args;
                        }

                        for( it = args; it != end; ++it )
                            ur_setId(it, UT_NONE);
                        it = args;
                        break;

                    case FO_fetchArg:
                        if( blkC->series.it >= blkC->series.end ) 
                            goto func_short;
                        if( ! (ok = boron_eval1( ut, blkC, it++ )) )
                            goto cleanup_trace;
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
                        if( OPT_BITS(args - 1) & (1 << *pc) )
                            pc += 2;
                        else
                            pc += pc[1];
                        break;

                    case FO_nop:
                        break;

                    case FO_nop2:
                        ++pc;
                        break;
                }
            }
        }
        assert( nc );

        if( fcell->id.type == UT_CFUNC )
        {
            ok = fcell->m.func( ut, args, res );
            if( ! ok && ! (fcell->id.flags & FUNC_FLAG_GHOST) )
                goto cleanup_trace;
        }
        else
        {
            UCell tmp;
            ur_setId(&tmp, UT_BLOCK);
            ur_setSeries(&tmp, fcell->m.f.bodyN, 0);

            if( (ok = boron_framePush( ut, args, fcell->m.f.bodyN )) )
            {
                ok = boron_doBlock( ut, &tmp, res );
                boron_framePop( ut );
                if( ! ok && ! (fcell->id.flags & FUNC_FLAG_GHOST) )
                {
                    if( _catchThrownWord( ut, UR_ATOM_RETURN ) )
                        ok = UR_OK;
                    else
                        goto cleanup_trace;
                }
            }
        }

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
            ur_setId(&tmp, UT_BLOCK);
            ur_setSeries(&tmp, fcell->m.f.bodyN, 0);

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


/*
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
            if( ur_is(cell, UT_UNSET) )
            {
                ur_error( ut, UR_ERR_SCRIPT, "unset word '%s",
                          ur_atomCStr( ut, ur_atom(
                             ur_bufferSer(blkC)->ptr.cell + blkC->series.it) ) );
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
                goto traceError;

            for( ; scell != cell; ++scell )
            {
                if( ur_is(scell, UT_SETWORD) )
                {
                    if( ! ur_setWord( ut, scell, res ) )
                        goto traceError;
                }
                else
                {
                    if( ! ur_setPath( ut, scell, res ) )
                        goto traceError;
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
                goto traceError;
            ++blkC->series.it;
            break;

        case UT_PATH:
            if( ! ur_pathCell( ut, cell, res ) )
                goto traceError;
            if( ur_is(res, UT_CFUNC) || ur_is(res, UT_FUNC) )
            {
                cell = res;
                goto call_func;
            }
            ++blkC->series.it;
            break;

        case UT_LITPATH:
            *res = *cell;
            res->id.type = UT_PATH;
            ++blkC->series.it;
            break;

        case UT_CFUNC:          // Required for cfunc_verifyFuncArgs.
            goto call_func;

        default:
set_res:
            *res = *cell;
            ++blkC->series.it;
            break;
    }
    return UR_OK;

call_func:

    ++blkC->series.it;
    if( boron_call( ut, (UCellFunc*) cell, blkC, res ) )
        return UR_OK;
    // NOTE: This slows down throw of non-error values.
    //ur_appendTrace( ut, blkC->series.buf, --blkC->series.it );
    return UR_THROW;

end_of_block:

    ur_error( ut, UR_ERR_SCRIPT, "Unexpected end of block" );

traceError:

    // NOTE: This slows down throw of non-error values.
    //ur_appendTrace( ut, blkC->series.buf, blkC->series.it );
    return UR_THROW;
}


static void boron_bindDefaultB( UThread* ut, UIndex blkN )
{
    UBlockIterM bi;
    int type;
    int wrdN;
    UBuffer* threadCtx = ur_threadContext(ut);
    UBuffer* envCtx = ur_envContext(ut);

    bi.buf = ur_buffer( blkN );
    bi.it  = bi.buf->ptr.cell;
    bi.end = bi.it + bi.buf->used;

    ur_foreach( bi )
    {
        type = ur_type(bi.it);
        if( type == UT_SETWORD )
        {
            wrdN = ur_ctxAddWordI( threadCtx, ur_atom(bi.it) );
            if( envCtx )
            {
                // Lift default value of word from environment.
                int ewN = ur_ctxLookup( envCtx, ur_atom(bi.it) );
                if( ewN > -1 )
                    *ur_ctxCell(threadCtx, wrdN) = *ur_ctxCell(envCtx, ewN);
            }
            goto assign;
        }
        else if( ur_isWordType(type) )
        {
            wrdN = ur_ctxLookup( threadCtx, ur_atom(bi.it) );
            if( wrdN > -1 )
                goto assign;

            if( envCtx )
            {
                wrdN = ur_ctxLookup( envCtx, ur_atom(bi.it) );
                if( wrdN > -1 )
                {
                    // TODO: Have ur_freezeEnv() remove unset words.
                    if( ! ur_is( ur_ctxCell(envCtx, wrdN), UT_UNSET ) )
                    {
                        ur_setBinding( bi.it, UR_BIND_ENV );
                        bi.it->word.ctx = -1; //-BUF_THREAD_CTX;
                        bi.it->word.index = wrdN;
                        continue;
                    }
                }
            }

            wrdN = ur_ctxAddWordI( threadCtx, ur_atom(bi.it) );
assign:
            ur_setBinding( bi.it, UR_BIND_THREAD );
            bi.it->word.ctx = 1; //BUF_THREAD_CTX;
            bi.it->word.index = wrdN;
        }
        else if( ur_isBlockType(type) )
        {
            if( ! ur_isShared( bi.it->series.buf ) )
                boron_bindDefaultB( ut, bi.it->series.buf );
        }
        /*
        else if( type >= UT_BI_COUNT )
        {
            ut->types[ type ]->bind( ut, it, bt );
        }
        */
    }
}


/**
  Bind block in thread dataStore to default contexts.
*/
void boron_bindDefault( UThread* ut, UIndex blkN )
{
    UBlockIter bi;

    bi.buf = ur_buffer(blkN);
    bi.it  = bi.buf->ptr.cell;
    bi.end = bi.it + bi.buf->used;

    ur_ctxSetWords( ur_threadContext(ut), bi.it, bi.end );

    boron_bindDefaultB( ut, blkN );
}


/*
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
        {
            // NOTE: This slows down throw of non-error values.
            //ur_appendTrace( ut, bc2.series.buf, bc2.series.it );
            return UR_THROW;
        }
    }
    //reportStack( ut );

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


/**
  Get most recent exception.

  \return Pointer to top cell on ur_errorBlock(), or zero if there are no
          exceptions.
*/
UCell* boron_exception( UThread* ut )
{
    UBuffer* blk = ur_errorBlock(ut);
    if( blk->used )
        return blk->ptr.cell + (blk->used - 1);
    return 0;
}


/*
UBuffer* boron_tempBinary( UThread* ut )
{
    return ur_buffer( BT->tempN );
}
*/


/** @} */

//EOF
