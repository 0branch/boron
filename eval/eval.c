/*
  Boron Evaluator
  Copyright 2016-2019 Karl Robillard
*/


#include <assert.h>
#include <string.h>
#include "boron.h"
#include "i_parse_blk.c"
#include "mem_util.h"

//#define REPORT_EVAL


/** \struct UCellCFuncEval
  Structure of CFUNC a1 argument when boron_defineCFunc() signature is ":eval".
*/
/** \var UCellCFuncEval::avail
  Number of valid cells following pos.
*/
/** \var UCellCFuncEval::pos
  Block program counter.  This must be updated before the CFUNC exits.
*/
/** \def boron_evalPos
  Macro to access :eval function program counter from inside CFUNC.
*/
/** \def boron_evalAvail
  Macro to get the number of cells available for evaluation following
  UCellCFuncEval::pos inside CFUNC.
*/


#define UT(ptr)     ((UThread*) ptr)
#define PTR_I       (void*)(intptr_t)
#define cp_error    (void*)(intptr_t) ur_error


enum FuncArgumentOpcodes
{
    // Instruction       Data       Description
    // -------------------------------------------------------------------
    FO_clearLocal,    // n          Set locals on stack to UT_NONE.
    FO_fetchArg,      //            Push _eval1 result onto stack.
    FO_litArg,        //            Push UCell at current pc onto stack.
    FO_checkType,     // t          Check last argument type.
    FO_checkTypeMask, // w u16 ..   Check last argument type mask.
    FO_optionRecord,  //            Push options record cell.
    FO_variant,       // n          Push variant int! onto stack.
    FO_eval,          //            Call cfunc which advances pc (block it).
    FO_end            //            End function program.
};

// 16-bit alignment pad.
#define CHECK_TYPE_PAD  0x10


enum ArgRuleId
{
    AR_WORD, AR_TYPE, AR_LITW, AR_OPT, AR_LOCAL, AR_VARIANT, AR_EVAL
};


#define LOCAL   1   // compileAtoms[1]  "|"
#define EXTERN  2   // compileAtoms[2]  "extern"
#define GHOST   3   // compileAtoms[3]  "ghost"

// _argRules offsets
#define LWORD       52
#define LOCAL_RULE  LWORD+8

static const uint8_t _argRules[] =
{
    PB_SomeR, 3, PB_End,

    PB_Next, 6,
    PB_LitWord, LOCAL, PB_AnyTs, LWORD, PB_ReportEnd, AR_LOCAL,

    PB_Next, 4,
    PB_Type, UT_WORD, PB_ReportEnd, AR_WORD,

    PB_Next, 4,
    PB_Type, UT_DATATYPE, PB_ReportEnd, AR_TYPE,

    PB_Next, 4,
    PB_Type, UT_LITWORD, PB_ReportEnd, AR_LITW,

    PB_Next, 6,
    PB_Type, UT_OPTION, PB_AnyTs, LWORD, PB_ReportEnd, AR_OPT,

    PB_Next, 4,
    PB_Type, UT_INT, PB_ReportEnd, AR_VARIANT,

    PB_Next, 4,
    PB_Type, UT_GETWORD, PB_ReportEnd, AR_EVAL,

    PB_Type, UT_STRING, PB_End,     // Ignore comment string!

    // LWORD word!/lit-word!/set-word! (set-word! only needed by funct)
    0x00,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,

    PB_ToLitWord, LOCAL, PB_End
};


typedef struct
{
    UIndex  atom;
    uint8_t optionIndex;
    uint8_t argCount;
    uint8_t programOffset;
    uint8_t _pad;
}
OptionEntry;


typedef struct
{
    uint16_t progOffset;
    uint16_t optionCount;
    OptionEntry opt[ 1 ];
}
ArgProgHeader;


#define OPTION_FLAGS    id.ext
#define MAX_OPTIONS 8

typedef struct
{
    UBlockParser bp;
    UBuffer sval;           // Context of stack value bindings.
    UBuffer localWords;     // Array of atoms.
    UBuffer externWords;    // Array of atoms.
    UBuffer* bin;
    UIndex stackMapN;
    int origUsed;
    int argEndPc;
    int funcArgCount;
    int optionCount;
    OptionEntry opt[ MAX_OPTIONS ];
}
ArgCompiler;


int boron_copyWordValue( UThread* ut, const UCell* wordC, UCell* res )
{
    const UCell* cell;
    if( ! (cell = ur_wordCell( ut, wordC )) )
		return UR_THROW;
    *res = *cell;
    return UR_OK;
}


static void _defineArg( UBuffer* ctx, int binding, UIndex stackMapN,
                        UIndex atom, UIndex index, int16_t optArgN )
{
    UCell* cell;
    ur_ctxAppendWord( ctx, atom );
    cell = ctx->ptr.cell + ctx->used - 1;
    ur_setId( cell, UT_WORD );
    ur_binding(cell) = binding;
    cell->word.ctx   = stackMapN;
    cell->word.atom  = atom;
    cell->word.index = index;
    cell->word.sel[0] = optArgN;
}


#define EMIT(op)    prog->ptr.b[ prog->used++ ] = op

static void _argRuleHandler( UBlockParser* par, int rule,
                             const UCell* it, const UCell* end )
{
    ArgCompiler* ap = (ArgCompiler*) par;
    UBuffer* prog = ap->bin;
    int op;

    // TODO: Ensure all words in spec. are unique.

    //printf( "KR   arg rule %d\n", rule );
    switch( rule )
    {
        case AR_WORD:
            if( it->word.atom < UT_BI_COUNT )
            {
                EMIT( FO_checkType );
                EMIT( it->word.atom );
                break;
            }
            op = FO_fetchArg;
            goto emit_arg;

        case AR_TYPE:
            if( ur_datatype(it) == UT_TYPEMASK )
            {
                UIndex wpos;
                int which = 0;
                int wbit;
                int64_t mask = (((int64_t) it->datatype.mask1) << 32) |
                               it->datatype.mask0;

                EMIT( FO_checkTypeMask );
                wpos = prog->used;
                EMIT( 0 );
                if( prog->used & 1 )
                {
                    which |= CHECK_TYPE_PAD;
                    EMIT( 0 );
                }

                for( wbit = 1; wbit <= 4; wbit <<= 1 )
                {
                    if( mask & 0xffff )
                    {
                        which |= wbit;
                        *((uint16_t*) (prog->ptr.b + prog->used)) = mask;
                        prog->used += 2;
                    }
                    mask >>= 16;
                }

                prog->ptr.b[ wpos ] = which;
            }
            else
            {
                EMIT( FO_checkType );
                EMIT( ur_datatype(it) );
            }
            break;

        case AR_LITW:
            op = FO_litArg;
emit_arg:
            EMIT( op );
            if( ! ap->optionCount )
                ++ap->funcArgCount;
            if( ap->stackMapN )
                _defineArg( &ap->sval, BOR_BIND_FUNC, ap->stackMapN,
                            it->word.atom, ap->sval.used, 0 );
            break;

        case AR_OPT:
            if( it->word.atom == par->atoms[ EXTERN ] )
            {
                for( ++it; it != end; ++it )
                    ur_arrAppendInt32( &ap->externWords, it->word.atom );
                break;
            }
            else if( it->word.atom == par->atoms[ GHOST ] )
            {
                par->rflag |= FUNC_FLAG_GHOST;
                break;
            }

            if( ap->optionCount < MAX_OPTIONS )
            {
                OptionEntry* ent;
                int n;
                int argCount = 0;

                if( ap->stackMapN )
                    _defineArg( &ap->sval, BOR_BIND_OPTION, ap->stackMapN,
                                it->word.atom, ap->optionCount, 0 );

                if( ! ap->optionCount )
                {
                    ap->argEndPc = prog->used;
                    EMIT( FO_end );     // Terminate args.
                    EMIT( FO_end );     // Reserve space for FO_clearLocal.
                }

                ent = &ap->opt[ ap->optionCount++ ];
                ent->atom = it->word.atom;
                ent->optionIndex = ap->optionCount;
                ent->programOffset = 0;
                ent->_pad = 0;
                ++it;
                n = end - it;
                if( n )
                {
                    ent->programOffset = prog->used - ap->origUsed;
                    for( ; it != end; ++it )
                    {
                        if( it->word.atom < UT_BI_COUNT )
                        {
                            EMIT( FO_checkType );
                            EMIT( it->word.atom );
                        }
                        else
                        {
                            EMIT(ur_is(it,UT_LITWORD) ? FO_litArg:FO_fetchArg);
                            if( ap->stackMapN )
                            {
                                _defineArg( &ap->sval, BOR_BIND_OPTION_ARG,
                                            ap->stackMapN, it->word.atom,
                                            ap->optionCount - 1, argCount );
                            }
                            ++argCount;
                        }
                        // TODO: Handle lit-word!
                    }
                    EMIT( FO_end );
                }
                ent->argCount = argCount;
            }
            break;

        case AR_LOCAL:
            for( ++it; it != end; ++it )
                ur_arrAppendInt32( &ap->localWords, it->word.atom );
            break;

        case AR_VARIANT:
            EMIT( FO_variant );
            EMIT( ur_int(it) );
            break;

        case AR_EVAL:
            EMIT( FO_eval );
            break;
    }
}


#ifdef AUTO_LOCALS
static void _appendSetWords( UThread* ut, UBuffer* buf, const UCell* blkC )
{
    UBlockIt bi;
    ur_blockIt( ut, &bi, blkC );
    ur_foreach( bi )
    {
        int type = ur_type(bi.it);
        if( type == UT_SETWORD )
            ur_arrAppendInt32( buf, bi.it->word.atom );
        else if( type == UT_BLOCK || type == UT_PAREN )
            _appendSetWords( ut, buf, bi.it );
    }
}
#endif


static void _zeroDuplicateU32( UBuffer* a )
{
    uint32_t* it  = a->ptr.u32;
    uint32_t* end = it + a->used;
    for( ; it != (end - 1); ++it )
    {
        if( find_uint32_t( it + 1, end, *it ) )
            *it = 0;
    }
}


static void _zeroDiffU32( UBuffer* a, const UBuffer* b )
{
    uint32_t* it  = a->ptr.u32;
    uint32_t* end = it + a->used;
    const uint32_t* bIt  = b->ptr.u32;
    const uint32_t* bEnd = bIt + b->used;
    for( ; it != end; ++it )
    {
        if( find_uint32_t( bIt, bEnd, *it ) )
            *it = 0;
    }
}


static void _removeValueU32( UBuffer* buf, uint32_t val )
{
    uint32_t* it  = buf->ptr.u32;
    uint32_t* end = it + buf->used;
    uint32_t* out;

    it = (uint32_t*) find_uint32_t( it, end, val );
    if( ! it )
        return;
    for( out = it++; it != end; ++it )
    {
        if( *it != 0 )
            *out++ = *it;
    }
    buf->used = out - buf->ptr.u32;
}


extern const UAtom* boron_compileAtoms( BoronThread* );

/*
  Compile function argument fetching program.

  \param specC      Cell of specification UT_BLOCK slice.
  \param prog       Program is appended to this UT_BINARY buffer.
  \param bodyN      Buffer index of code block or 0 for cfunc!.
  \param sigFlags   Contents set to non-zero if /ghost used in spec. block.

  The spec. block must only contain the following patterns:
      word!/lit-word!
      option! any word!/lit-word!
      '| any word!

  If bodyN is not zero, all argument and local words in the block will be
  bound to it.

  During evaluation the following cells can be placed on the UThread stack:

    [ opt-record ][ arguments ][ locals ][ opt args ][ optN args ]
                  ^
                  a1

  The stack map (or a1 CFUNC argument) points to the first argument cell.

  The option record indicates which options were evaluated and holds offsets to
  the optional agruments.  This is a single cell of type UT_UNSET.  Optional
  argument values are only present on the stack if the associated option was
  used and are ordered according to how the options occur in the invoking path!.
*/
void boron_compileArgProgram( BoronThread* bt, const UCell* specC,
                              UBuffer* prog, UIndex bodyN, int* sigFlags )
{
    UThread* ut = UT(bt);
    ArgCompiler ac;
    ArgProgHeader* head;
    const int headerSize = 4;
    int localCount = 0;         // Local values (no arguments).


    ac.origUsed = prog->used;
    ac.argEndPc = 0;
    ac.funcArgCount = 0;
    ac.optionCount = 0;
    ur_blockIt( ut, (UBlockIt*) &ac.bp.it, specC );

    ur_binReserve( prog, prog->used + 16 + headerSize );
    prog->used += headerSize;   // Reserve space for start of ArgProgHeader.

    //printf( "KR compile arg %ld\n", ac.bp.end - ac.bp.it );

    if( ac.bp.it != ac.bp.end )
    {
        ac.bp.ut = ut;
        ac.bp.atoms  = boron_compileAtoms(bt);
        ac.bp.rules  = _argRules;
        ac.bp.report = _argRuleHandler;
        ac.bp.rflag  = 0;
        ac.bin = prog;
        ac.stackMapN = bodyN;
        ur_ctxInit( &ac.sval, 0 );
        ur_arrInit( &ac.localWords,  sizeof(uint32_t), 0 );
        ur_arrInit( &ac.externWords, sizeof(uint32_t), 0 );

        {
        const UCell* start = ac.bp.it;
        const UCell* end   = ac.bp.end;
        const UCell* local;
        if( ur_parseBlockI( &ac.bp, _argRules + LOCAL_RULE, start ) )
            local = ac.bp.end = ac.bp.it;
        else
            local = NULL;
        ac.bp.it = start;
        ur_parseBlockI( &ac.bp, ac.bp.rules, start );
        if( local )
        {
            ac.bp.it  = local;
            ac.bp.end = end;
            ur_parseBlockI( &ac.bp, ac.bp.rules, local );
        }
        }

        if( bodyN )
        {
#ifdef AUTO_LOCALS
            UCell tmp;
            ur_initSeries( &tmp, UT_BLOCK, bodyN );
            _appendSetWords( ut, &ac.localWords, &tmp );
#endif
            if( ac.localWords.used > 1 )
                _zeroDuplicateU32( &ac.localWords );
            if( ac.externWords.used )
                _zeroDiffU32( &ac.localWords, &ac.externWords );
            _removeValueU32( &ac.localWords, 0 );

            if( ac.localWords.used )
            {
                const UIndex* ai = ac.localWords.ptr.i32;
                const UIndex* ae = ai + ac.localWords.used;
                int localIndex = ac.funcArgCount;

                while( ai != ae )
                {
                    //printf( "KR local %d %s\n", localIndex,
                    //        ur_atomCStr(ut, *ai) );
                    _defineArg( &ac.sval, BOR_BIND_FUNC, bodyN,
                                *ai++, localIndex++, 0 );
                }
                localCount = ac.localWords.used;
            }

            if( ac.sval.used )
            {
                const UBuffer* body = ur_bufferEnv(ut, bodyN);

                ur_ctxSort( &ac.sval );
                ur_bindCopy( ut, &ac.sval,
                             body->ptr.cell, body->ptr.cell + body->used );
            }
        }
        ur_ctxFree( &ac.sval );
        ur_arrFree( &ac.localWords );
        ur_arrFree( &ac.externWords );

        // Insert OptionEntry table & FO_optionRecord.
        if( ac.optionCount )
        {
            OptionEntry* ent;
            OptionEntry* ee;
            const int optRecSize = 1;
            int tsize = ac.optionCount * sizeof(OptionEntry) + optRecSize;
            int newUsed = prog->used + tsize;

            ur_binReserve( prog, newUsed + 3 );     // +3 for alignment pad.
            ent = (OptionEntry*) (prog->ptr.b + ac.origUsed + headerSize);
            ee  = ent + ac.optionCount;
            memmove( ((char*) ent) + tsize, ent,
                     prog->used - ac.origUsed - headerSize );
            memcpy( ent, ac.opt, tsize - optRecSize );
            prog->used = newUsed;

            for( ; ent != ee; ++ent )
            {
                if( ent->programOffset )
                    ent->programOffset += tsize;
            }

            *((uint8_t*) ent) = FO_optionRecord;

            if( localCount )
            {
                uint8_t* endArg = prog->ptr.b + ac.argEndPc + tsize;
                *endArg++ = FO_clearLocal;
                *endArg = localCount;
            }
        }

        *sigFlags = ac.bp.rflag;
    }
    else
    {
        *sigFlags = 0;
    }

    if( ! ac.optionCount )
    {
        if( localCount )
        {
            EMIT( FO_clearLocal );
            EMIT( localCount );
        }
        else
            EMIT( FO_end );
    }

    // Pad to 32-bit align OptionEntry table when programs are concatenated.
    while( prog->used & 3 )
        EMIT( FO_end );


    head = (ArgProgHeader*) (prog->ptr.b + ac.origUsed);
    head->progOffset  = headerSize + ac.optionCount * sizeof(OptionEntry);
    head->optionCount = ac.optionCount;
}


#if 0
void boron_argProgramToStr( UThread* ut, const UBuffer* bin, UBuffer* str )
{
    const uint8_t* pc;
    int op;

    ur_strAppendChar( str, '[' );
    while( (op = *pc++) < FO_end )
    {
        switch( op )
        {
            case FO_clearLocal:
                ur_strAppendCStr( str, "| " );
                break;

            case FO_fetchArg:
                ur_strAppendCStr( str, "a " );
                break;

            case FO_litArg:
                ur_strAppendCStr( str, "'a " );
                break;

            case FO_checkType:
                op = *pc++;
                ur_strAppendCStr( str, ur_atomCStr(ut, op) );
                break;

            case FO_checkTypeMask:
            {
                UCell dt;
                int64_t mask = 0;
                int which = *pc++;
                if( which & CHECK_TYPE_PAD )
                    ++pc;
                if( which & 1 )
                {
                    mask |= *((uint16_t*) pc);
                    pc += 2;
                }
                if( which & 2 )
                {
                    mask |= ((int64_t) *((uint16_t*) pc)) << 16;
                    pc += 2;
                }
                if( which & 4 )
                {
                    mask |= ((int64_t) *((uint16_t*) pc)) << 32;
                    pc += 2;
                }
                ur_setId(&dt, UT_DATATYPE);
                ur_datatype(&dt) = UT_TYPEMASK;
                dt.datatype.mask0 = mask;
                dt.datatype.mask1 = mask >> 32;
                datatype_toString( ut, &dt, str, 0 );
            }
                break;

            case FO_optionRecord:
                break;

            case FO_variant:
                ur_strAppendInt( str, *pc++ );
                break;

            case FO_eval:
                break;
        }
    }
    ur_strAppendChar( str, ']' );
}
#endif


/**
  Throw a standardized error for an unexpected function argument.

  \param atom   Datatype name.
  \param argN   Zero-based argument number.

  \return UR_THROW
*/
UStatus boron_badArg( UThread* ut, UIndex atom, int argN )
{
    return ur_error( ut, UR_ERR_TYPE, "Unexpected %s for argument %d",
                     ur_atomCStr(ut, atom), argN + 1 );
}


#define CATCH_STACK_OVERFLOW    1

/*
  Fetch arguments and call cfunc!.
*/
static
const UCell* boron_callC( UThread* ut, const UCell* funC,
                          UBlockIt* options,
                          const UCell* it, const UCell* end, UCell* res )
{
    const ArgProgHeader* head;
    const uint8_t* pc;
    UCell* args;
    UCell* r2 = NULL;
    UIndex origStack = ut->stack.used;
    int op;

    args = ut->stack.ptr.cell + origStack;
    head = (const ArgProgHeader*) (ur_bufferSer(funC)->ptr.b +
                                   ((const UCellFunc*) funC)->argProgOffset);
    pc = ((const uint8_t*) head) + head->progOffset;

run_programC:
    while( (op = *pc++) < FO_end )
    {
        switch( op )
        {
            case FO_clearLocal:
                goto fetch_done;

            case FO_fetchArg:
                if( it == end )
                    goto func_short;
                r2 = ut->stack.ptr.cell + ut->stack.used;
#ifdef CATCH_STACK_OVERFLOW
                if( r2 > BT->stackLimit )
                    goto overflow;
#endif
                ++ut->stack.used;
                ur_setId(r2, UT_NONE);
                it = boron_eval1( ut, it, end, r2 );
                if( ! it )
                    goto cleanup;
                break;

            case FO_litArg:
                if( it == end )
                    goto func_short;
                r2 = ut->stack.ptr.cell + ut->stack.used;
                ++ut->stack.used;
                *r2 = *it++;
                break;

            case FO_checkType:
                op = *pc++;
                if( ur_type(r2) != op )
                {
bad_arg:
                    return PTR_I boron_badArg( ut, ur_type(r2), r2 - args );
                }
                break;

            case FO_checkTypeMask:
            {
                int64_t mask = 0;
                int which = *pc++;
                if( which & CHECK_TYPE_PAD )
                    ++pc;
                if( which & 1 )
                {
                    mask |= *((uint16_t*) pc);
                    pc += 2;
                }
                if( which & 2 )
                {
                    mask |= ((int64_t) *((uint16_t*) pc)) << 16;
                    pc += 2;
                }
                if( which & 4 )
                {
                    mask |= ((int64_t) *((uint16_t*) pc)) << 32;
                    pc += 2;
                }
                if( ! ((1LL << ur_type(r2)) & mask) )
                    goto bad_arg;
            }
                break;

            case FO_optionRecord:
                ur_setId(args, UT_UNSET);
                ++args;
                ++ut->stack.used;
                break;

            case FO_variant:
                r2 = ut->stack.ptr.cell + ut->stack.used;
                ++ut->stack.used;
                ur_setId(r2, UT_INT);
                ur_int(r2) = *pc++;
                break;

            case FO_eval:
                r2 = ut->stack.ptr.cell + ut->stack.used;
                ++ut->stack.used;
                ur_setId(r2, UT_UNSET);
                boron_evalAvail(r2) = end - it;
                boron_evalPos(r2)   = it;

                if( ((UCellFunc*) funC)->m.func( ut, args, res ) )
                    it = boron_evalPos(r2);
                else
                    it = NULL;

                ur_pop(ut);
                goto cleanup;
        }
    }

fetch_done:
    if( options )
    {
next_option:
        while( options->it != options->end )
        {
            const UCell* oc = options->it++;
            const OptionEntry* ent = head->opt;
            int i;
            for( i = 0; i < head->optionCount; ++i, ++ent )
            {
                if( ent->atom == oc->word.atom )
                {
                    args[-1].OPTION_FLAGS |= 1 << i;
                    if( ent->programOffset )
                    {
                        ((uint8_t*) args)[-1 - i] =
                            (ut->stack.ptr.cell + ut->stack.used) - args;
                        pc = ((const uint8_t*) head) + ent->programOffset;
                        goto run_programC;
                    }
                    goto next_option;
                }
            }
            return cp_error( ut, UR_ERR_SCRIPT, "Invalid option %s",
                             ur_atomCStr(ut, oc->word.atom) );
        }
    }

    if( ! ((UCellFunc*) funC)->m.func( ut, args, res ) )
    {
        if( ur_flags(funC, FUNC_FLAG_GHOST) )
        {
            r2 = ur_exception(ut);
            if( ur_is(r2, UT_ERROR) )
                ur_setFlags(r2, UR_FLAG_ERROR_SKIP_TRACE);
        }
        it = NULL;
    }

cleanup:
    ut->stack.used = origStack;
    return it;

func_short:
    return cp_error( ut, UR_ERR_SCRIPT, "End of block" );

#ifdef CATCH_STACK_OVERFLOW
overflow:
    return cp_error( ut, UR_ERR_SCRIPT, "Stack overflow" );
#endif
}


/*
  Fetch arguments and call func!.
*/
static
const UCell* boron_call( UThread* ut, const UCell* funC,
                         UBlockIt* options,
                         const UCell* it, const UCell* end, UCell* res )
{
    UBlockIt bi;
    const UCell* next;
    const ArgProgHeader* head;
    const uint8_t* pc;
    UCell* optRec = 0;
    UCell* r2 = NULL;
    UIndex origStack = ut->stack.used;
    UIndex argsPos = origStack;
    int needStackMap = 0;
    int op;


    if( funC->series.it == 0 )
        goto eval_body;

    {
    const UBuffer* blk = ur_bufferSer(funC);
    head = (const ArgProgHeader*) ur_bufferSer(blk->ptr.cell)->ptr.v;
    pc = ((const uint8_t*) head) + head->progOffset;
    }

run_program:
    while( (op = *pc++) < FO_end )
    {
        //printf( "KR call op %d\n", op );
        switch( op )
        {
            case FO_clearLocal:
                op = *pc++;
                {
                UCell* ls = ut->stack.ptr.cell + ut->stack.used;
                UCell* lend = ls + op;
                for( ; ls != lend; ++ls )
                    ur_setId(ls, UT_NONE);
                }
                ut->stack.used += op;
                needStackMap = 1;
                goto fetch_done;

            case FO_fetchArg:
                if( it == end )
                    goto func_short;
                r2 = ut->stack.ptr.cell + ut->stack.used;
#ifdef CATCH_STACK_OVERFLOW
                if( r2 > BT->stackLimit )
                    goto overflow;
#endif
                ++ut->stack.used;
                ur_setId(r2, UT_NONE);
                it = boron_eval1( ut, it, end, r2 );
                if( ! it )
                    goto cleanup;
                needStackMap = 1;
                break;

            case FO_litArg:
                if( it == end )
                    goto func_short;
                r2 = ut->stack.ptr.cell + ut->stack.used;
                ++ut->stack.used;
                *r2 = *it++;
                needStackMap = 1;
                break;

            case FO_checkType:
                op = *pc++;
                if( ur_type(r2) != op )
                {
bad_arg:
                    return PTR_I boron_badArg( ut, ur_type(r2),
                                        r2-(ut->stack.ptr.cell + origStack) );
                }
                break;

            case FO_checkTypeMask:
            {
                int64_t mask = 0;
                int which = *pc++;
                if( which & CHECK_TYPE_PAD )
                    ++pc;
                if( which & 1 )
                {
                    mask |= *((uint16_t*) pc);
                    pc += 2;
                }
                if( which & 2 )
                {
                    mask |= ((int64_t) *((uint16_t*) pc)) << 16;
                    pc += 2;
                }
                if( which & 4 )
                {
                    mask |= ((int64_t) *((uint16_t*) pc)) << 32;
                    pc += 2;
                }
                if( ! ((1LL << ur_type(r2)) & mask) )
                    goto bad_arg;
            }
                break;

            case FO_optionRecord:
                ++argsPos;
                optRec = ut->stack.ptr.cell + ut->stack.used;
                ++ut->stack.used;
                ur_setId(optRec, UT_UNSET);
                needStackMap = 1;
                break;

            case FO_variant:
                ++pc;
                break;
        }
    }

fetch_done:
    if( options )
    {
next_option:
        while( options->it != options->end )
        {
            const UCell* oc = options->it++;
            const OptionEntry* ent = head->opt;
            int i;
            for( i = 0; i < head->optionCount; ++i, ++ent )
            {
                if( ent->atom == oc->word.atom )
                {
                    optRec->OPTION_FLAGS |= 1 << i;
                    if( ent->programOffset )
                    {
                        ((uint8_t*) (optRec+1))[-1 - i] =
                            ut->stack.used - argsPos;
                        pc = ((const uint8_t*) head) + ent->programOffset;
                        goto run_program;
                    }
                    goto next_option;
                }
            }
            return cp_error( ut, UR_ERR_SCRIPT, "Invalid option %s",
                             ur_atomCStr(ut, oc->word.atom) );
        }
    }

    if( needStackMap )
    {
        UBuffer* smap = &BT->frames;
        UIndex* fi;
        UIndex newUsed = smap->used + 2;
        ur_arrReserve( smap, newUsed );
        fi = smap->ptr.i32 + smap->used;
        smap->used = newUsed;
        fi[0] = funC->series.buf;
        fi[1] = argsPos;
    }

eval_body:
    ur_blockIt( ut, &bi, funC );
    for( ; bi.it != bi.end; bi.it = next )
    {
#ifdef REPORT_EVAL
        fputs( "eval: ", stderr );
        ur_fwrite( ut, bi.it, UR_EMIT_MOLD, stderr );
        fputc( '\n', stderr );
#endif
        next = boron_eval1( ut, bi.it, bi.end, res );
        if( ! next )
        {
            if( ! boron_catchWord( ut, UR_ATOM_RETURN ) )
            {
                next = ur_exception(ut);
                if( ur_is(next, UT_ERROR) )
                {
                    if( ! ur_flags(funC, FUNC_FLAG_GHOST) )
                        ur_traceError( ut, next, funC->series.buf, bi.it );
                }
                it = NULL;
            }
            break;
        }
    }

    if( needStackMap )
    {
        BT->frames.used -= 2;
    }

cleanup:
    ut->stack.used = origStack;
    return it;

func_short:
    return cp_error( ut, UR_ERR_SCRIPT, "End of block" );

#ifdef CATCH_STACK_OVERFLOW
overflow:
    return cp_error( ut, UR_ERR_SCRIPT, "Stack overflow" );
#endif
}


extern void vector_pick( const UBuffer* buf, UIndex n, UCell* res );

/**
  Evaluate one value.

  The caller must ensure the cells of the block remain fixed during the
  call.

  \param it     Cell position to evaluate.
                The caller must ensure that this is less than end.
  \param end    End of block.
  \param res    Result cell.  This must be in held block.

  \return Next cell to evaluate or NULL if an error was thrown.
*/
const UCell* boron_eval1( UThread* ut, const UCell* it, const UCell* end,
                          UCell* res )
{
    const UCell* cell;

    switch( ur_type(it) )
    {
        case UT_WORD:
            cell = ur_wordCell( ut, it );
            if( ! cell )
                goto unbound;
            ++it;
            if( ur_is(cell, UT_CFUNC) )
                return boron_callC( ut, cell, 0, it, end, res );
            if( ur_is(cell, UT_FUNC) )
                return boron_call( ut, cell, 0, it, end, res );
            if( ur_is(cell, UT_UNSET) )
                return cp_error( ut, UR_ERR_SCRIPT, "Unset word '%s",
                                 ur_atomCStr(ut, it[-1].word.atom) );
            *res = *cell;
            return it;

        case UT_LITWORD:
            *res = *it++;
            ur_type(res) = UT_WORD;
            return it;

        case UT_SETWORD:
        case UT_SETPATH:
        {
            UBlockIt sw;
            sw.it  = it;
            sw.end = it + 1;
            while( sw.end != end &&
                   (ur_is(sw.end, UT_SETWORD) || ur_is(sw.end, UT_SETPATH)) )
                ++sw.end;
            if( sw.end == end )
                return cp_error( ut, UR_ERR_SCRIPT, "End of block" );
            if( (it = boron_eval1( ut, sw.end, end, res )) )
            {
                ur_foreach( sw )
                {
                    if( ur_is(sw.it, UT_SETWORD) )
                    {
                        if( ! ur_setWord( ut, sw.it, res ) )
                            return NULL;
                    }
                    else
                    {
                        if( ! ur_setPath( ut, sw.it, res ) )
                            return NULL;
                    }
                }
            }
        }
            return it;

        case UT_GETWORD:
            cell = ur_wordCell( ut, it );
            if( ! cell )
                goto unbound;
            *res = *cell;
            return ++it;

        case UT_PAREN:
            if( ! boron_doBlock( ut, it, res ) )
                return NULL;
            return ++it;

        case UT_PATH:
        {
            UBlockIt path;
            int headType;

            ur_blockIt( ut, &path, it++ );
            headType = ur_pathValue( ut, &path, res );
            if( headType == UT_WORD )
            {
                if( ur_is(res, UT_CFUNC) )
                {
                    UCell* fc = ur_pushCell(ut, res);
                    it = boron_callC( ut, fc, &path, it, end, res );
                    ur_pop(ut);
                }
                else if( ur_is(res, UT_FUNC) )
                {
                    UCell* fc = ur_pushCell(ut, res);
                    it = boron_call( ut, fc, &path, it, end, res );
                    ur_pop(ut);
                }
            }
            else if( ! headType )
                return NULL; //goto traceError;
        }
            return it;

        case UT_LITPATH:
            *res = *it++;
            ur_type(res) = UT_PATH;
            return it;

        case UT_CFUNC:
            return boron_callC( ut, it, 0, it+1, end, res );

        case UT_FUNC:
            return boron_call( ut, it, 0, it+1, end, res );
    }

    *res = *it;
    return ++it;

unbound:
    return UR_THROW;
    //return cp_error( ut, UR_ERR_SCRIPT, "Unbound word '%s",
    //                 ur_atomCStr(ut, it->word.atom) );
}


#ifdef REPORT_EVAL
extern void ur_dblk( UThread* ut, UIndex n );
#ifdef SHARED_STORE
#define ITOB(n)     ((n) >> 1)
#else
#define ITOB(n)     n
#endif
#endif

/**
  Evaluate block and get result.

  The caller must ensure the cells of the block remain fixed during the
  call.

  blkC and res may point to the same cell, but only if the block is held
  somewhere else.

  \param blkC   Block to evaluate.  This buffer must be held.
  \param res    Result. This cell must be in a held block.

  \return UR_OK or UR_THROW if an exception is thrown.
*/
UStatus boron_doBlock( UThread* ut, const UCell* blkC, UCell* res )
{
    UBlockIt bi;
    UIndex blkN = blkC->series.buf;
    const UCell* next;
//#define DO_PROTECT
#ifdef DO_PROTECT
    UBuffer* blk;
    int psave;
#define ur_tbuf(ut,N)   (((UBuffer**) ut)[0] + (N >> 1))
#endif


#ifdef REPORT_EVAL
    printf( "do:%c%d ", ur_isShared(blkN) ? '^' : ' ', ITOB(blkN) );
    ur_dblk( ut, blkN );
#endif

#ifdef DO_PROTECT
    // Prevent block modification.
    blk = ur_blockIt( ut, &bi, blkC );
    psave = blk->storage;
    if( psave & UR_BUF_PROTECT )
        psave = 0;
    else
        blk->storage = psave | UR_BUF_PROTECT;
#else
    ur_blockIt( ut, &bi, blkC );
#endif

    // NOTE: blkC must not be used after this point.

    for( ; bi.it != bi.end; bi.it = next )
    {
        next = boron_eval1( ut, bi.it, bi.end, res );
        if( ! next )
        {
            next = ur_exception(ut);
            if( ur_is(next, UT_ERROR) )
                ur_traceError( ut, next, blkN, bi.it );
#ifdef DO_PROTECT
            res = NULL;
            break;
#else
            return UR_THROW;
#endif
        }
    }

#ifdef DO_PROTECT
    // Re-enable block modification.
    if( psave )
    {
        blk = ur_tbuf(ut, blkC->buf);
        blk->storage &= ~UR_BUF_PROTECT;
    }
#endif
    return UR_OK;
}


/**
  Evaluate each value of a block and place the results into a new block.

  The caller must ensure the cells of the block remain fixed during the
  call.

  blkC and res may point to the same cell, but only if the block is held
  somewhere else.

  \param blkC   Block to reduce.  This buffer must be held.
  \param res    Result. This cell must be in a held block.

  \return res or NULL if an exception is thrown.
*/
UCell* boron_reduceBlock( UThread* ut, const UCell* blkC, UCell* res )
{
    UBlockIter bi;
    UIndex ind;
    uint8_t t = UT_BLOCK;

    ur_generate( ut, 1, &ind, &t );         // gc!
    ur_initSeries(res, UT_BLOCK, ind);

    ur_blkSlice( ut, &bi, blkC );
    while( bi.it != bi.end )
    {
        bi.it = boron_eval1( ut, bi.it, bi.end,
                             ur_blkAppendNew(ur_buffer(ind), UT_UNSET) );
        if( ! bi.it )
            return NULL;
    }
    return res;
}


/*
  -cf-
    do
        value
    return: Result of value.
    group: eval
*/
CFUNC_PUB( cfunc_do )
{
    // NOTE: This is an FO_eval function.
    const UCell* it  = boron_evalPos(a1);
    const UCell* end = it + boron_evalAvail(a1);

    if( ! (it = boron_eval1( ut, it, end, res )) )
        return UR_THROW;
    boron_evalPos(a1) = it;

    switch( ur_type(res) )
    {
        case UT_WORD:
        {
            const UCell* cell;
            if( ! (cell = ur_wordCell( ut, res )) )
            {
                return UR_THROW;
                //return ur_error( ut, UR_ERR_SCRIPT, "Unbound word '%s",
                //                 ur_atomCStr(ut, res->word.atom) );
                //goto traceError;
            }
            if( ur_is(cell, UT_CFUNC) )
            {
                it = boron_callC( ut, cell, 0, it, end, res );
                goto end_func;
            }
            if( ur_is(cell, UT_FUNC) )
            {
                it = boron_call( ut, cell, 0, it, end, res );
                goto end_func;
            }
            *res = *cell;
        }
            break;

        case UT_LITWORD:
            res->id.type = UT_WORD;
            break;

        case UT_GETWORD:
            return boron_copyWordValue( ut, res, res );
            //if( == UR_THROW ) goto traceError;

        case UT_STRING:
        {
            UCell tmp;
            USeriesIter si;
            UIndex hold;
            UIndex blkN;
            int ok;

            ur_seriesSlice( ut, &si, res );
            if( si.it == si.end )
            {
                ur_setId( res, UT_UNSET );
            }
            else
            {
                if( ur_strIsUcs2(si.buf) )
                    return ur_error( ut, UR_ERR_INTERNAL,
                                     "FIXME: Cannot do ucs2 string!" );

                blkN = ur_tokenize( ut, si.buf->ptr.c + si.it,
                                        si.buf->ptr.c + si.end, &tmp ); // gc!
                if( ! blkN )
                    return UR_THROW;

                hold = ur_hold(blkN);
                boron_bindDefault( ut, blkN );
                ur_initSeries( &tmp, UT_BLOCK, blkN );
                ok = boron_doBlock( ut, &tmp, res );
                ur_release(hold);
                return ok;
            }
        }
            break;

        case UT_FILE:
        {
            int ok = cfunc_load( ut, ur_pushCell(ut, res), res );
            ur_pop(ut);
            if( ! ok )
                return UR_THROW;
        }
            if( res->id.type != UT_BLOCK )
                break;
            // Fall through to block...
            /* FALLTHRU */

        case UT_BLOCK:
        case UT_PAREN:
        {
            int ok;
            UIndex hold;
//#ifdef SHARED_STORE
            if( ur_isShared( res->series.buf ) )
                ok = boron_doBlock( ut, res, res );
            else
//#endif
            {
                hold = ur_hold( res->series.buf );
                ok = boron_doBlock( ut, res, res );
                ur_release( hold );
            }
            return ok;
        }
        case UT_PATH:
        {
            UBlockIt path;
            UCell* tmp;
            int ok;

            ur_blockIt( ut, &path, res );

            tmp = ur_push(ut, UT_UNSET);
            ok = ur_pathValue( ut, &path, tmp );
            if( ok != UR_THROW )
            {
                if( ok == UT_WORD )
                {
                    if( ur_is(tmp, UT_CFUNC) )
                    {
                        it = boron_callC( ut, tmp, &path, it, end, res );
                        ur_pop(ut);
                        goto end_func;
                    }
                    else if( ur_is(tmp, UT_FUNC) )
                    {
                        it = boron_call( ut, tmp, &path, it, end, res );
                        ur_pop(ut);
                        goto end_func;
                    }
                }
                *res = *tmp;
                ok = UR_OK;
            }
            ur_pop(ut);
            return ok;
        }

        case UT_LITPATH:
            ur_type(res) = UT_PATH;
            break;

        case UT_CFUNC:
        case UT_FUNC:
        {
            const UCell* fcell = ur_pushCell( ut, res ); // Hold func cell.
            if( ur_is(res, UT_CFUNC) )
                it = boron_callC( ut, fcell, 0, it, end, res );
            else
                it = boron_call( ut, fcell, 0, it, end, res );
            ur_pop(ut);
        }
            goto end_func;

        default:
            break;      // Result already set.
    }
    return UR_OK;

end_func:
    if( ! it )
        return UR_THROW;
    boron_evalPos(a1) = it;
    return UR_OK;
}


static void _bindDefaultB( UThread* ut, UIndex blkN )
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
        if( ur_isWordType(type) )
        {
            if( threadCtx->used )
            {
                wrdN = ur_ctxLookup( threadCtx, ur_atom(bi.it) );
                if( wrdN > -1 )
                    goto assign;
            }

            if( type == UT_SETWORD )
            {
                wrdN = ur_ctxAppendWord( threadCtx, ur_atom(bi.it) );
                if( envCtx )
                {
                    // Lift default value of word from environment.
                    int ewN = ur_ctxLookup( envCtx, ur_atom(bi.it) );
                    if( ewN > -1 )
                        *ur_ctxCell(threadCtx, wrdN) = *ur_ctxCell(envCtx, ewN);
                }
            }
            else
            {
                if( envCtx )
                {
                    wrdN = ur_ctxLookup( envCtx, ur_atom(bi.it) );
                    if( wrdN > -1 )
                    {
                        // TODO: Have ur_freezeEnv() remove unset words.
                        if( ! ur_is( ur_ctxCell(envCtx, wrdN), UT_UNSET ) )
                        {
                            ur_setBinding( bi.it, UR_BIND_ENV );
                            bi.it->word.ctx = -UR_MAIN_CONTEXT;
                            bi.it->word.index = wrdN;
                            continue;
                        }
                    }
                }
                wrdN = ur_ctxAppendWord( threadCtx, ur_atom(bi.it) );
            }
assign:
            ur_setBinding( bi.it, UR_BIND_THREAD );
            bi.it->word.ctx = UR_MAIN_CONTEXT;
            bi.it->word.index = wrdN;
        }
        else if( ur_isBlockType(type) )
        {
            if( ! ur_isShared( bi.it->series.buf ) )
                _bindDefaultB( ut, bi.it->series.buf );
        }
        /*
        else if( type >= UT_BI_COUNT )
        {
            ut->types[ type ]->bind( ut, it, bt );
        }
        */
    }
}


extern UBuffer* ur_ctxSortU( UBuffer*, int unsorted );

/**
  Bind block in thread dataStore to default contexts.
*/
void boron_bindDefault( UThread* ut, UIndex blkN )
{
    ur_ctxSortU( ur_threadContext( ut ), 16 );
    _bindDefaultB( ut, blkN );
}


/**
  Run script and put result in the last stack cell.

  To preserve the last value the caller must push a new value on top
  (e.g. boron_stackPush(ut, UT_UNSET)).

  If an exception occurs, the thrown value is at the bottom of the stack
  (use the ur_exception macro).

  \return Result on stack or NULL if an exception is thrown.
*/
UCell* boron_evalUtf8( UThread* ut, const char* script, int len )
{
    UCell* res;
    UCell bcell;
    UIndex bufN;    // Code block.
    UIndex hold;

    if( len < 0 )
        len = strlen(script);
    bufN = ur_tokenize( ut, script, script + len, &bcell ); // gc!
    hold = ur_hold(bufN);

    boron_bindDefault( ut, bufN );

    res = ut->stack.ptr.cell + ut->stack.used - 1;
    if( ! boron_doBlock( ut, &bcell, res ) )
        res = NULL;

    ur_release(hold);
    return res;
}
