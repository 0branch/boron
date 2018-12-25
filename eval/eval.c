/*
  Boron Evaluator
  Copyright 2016-2018 Karl Robillard
*/


#include <assert.h>
#include <string.h>
#include "boron.h"
#include "i_parse_blk.c"
#include "mem_util.h"

//#define REPORT_EVAL

#define UT(ptr)     ((UThread*) ptr)
#define PTR_I       (void*)(intptr_t)
#define cp_error    (void*)(intptr_t) ur_error

// FO_eval cell on stack.
#define evalAvail(c)    (c)->series.buf
#define evalPos(c)      ((const UCell**) c)[1]


#if UR_VERSION < 0x000400
// FIXME: ur_wordCell throws error, boron_wordValue does not. 
#define boron_wordValue     ur_wordCell
#define boron_setWordValue  ur_setWord

typedef struct
{
    UCell* it;
    UCell* end;
}
UBlockIt;
#endif


typedef struct
{
    const UCell* it;
    const UCell* end;
}
UBlockItC;


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

static const uint8_t _argRules[] =
{
    PB_SomeR, 3, PB_End,

    PB_Next, 6,
    PB_LitWord, LOCAL, PB_AnyT, UT_WORD, PB_ReportEnd, AR_LOCAL,

    PB_Next, 4,
    PB_Type, UT_WORD, PB_ReportEnd, AR_WORD,

    PB_Next, 4,
    PB_Type, UT_DATATYPE, PB_ReportEnd, AR_TYPE,

    PB_Next, 4,
    PB_Type, UT_LITWORD, PB_ReportEnd, AR_LITW,

    PB_Next, 6,
    PB_Type, UT_OPTION, PB_AnyT, UT_WORD, PB_ReportEnd, AR_OPT,

    PB_Next, 4,
    PB_Type, UT_INT, PB_ReportEnd, AR_VARIANT,

    PB_Type, UT_GETWORD, PB_ReportEnd, AR_EVAL
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


#define OPTION_FLAGS    id._pad0
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
    int optionCount;
    OptionEntry opt[ MAX_OPTIONS ];
}
ArgCompiler;


#if UR_VERSION < 0x000400
UBuffer* ur_generateBuf( UThread* ut, UIndex* index, int type )
{
    uint8_t t = type;
    return ur_generate( ut, 1, index, &t ); 
}


int boron_copyWordValue( UThread* ut, const UCell* wordC, UCell* res )
{
    const UCell* cell;
    if( ! (cell = ur_wordCell( ut, wordC )) )
		return UR_THROW;
    *res = *cell;
    return UR_OK;
}


UBuffer* ur_blockIt( const UThread* ut, const UCell* cell, UBlockIt* bi )
{
    const UBuffer* blk = ur_bufferSer(cell);
    UIndex pos  = cell->series.it;
    UIndex epos = cell->series.end;

    if( epos < 0 )
        epos = blk->used;
    if( epos < pos )
        epos = pos;

    bi->it  = blk->ptr.cell + pos;
    bi->end = blk->ptr.cell + epos;
    return (UBuffer*) blk;
}
#endif


static void _defineArg( UBuffer* ctx, int binding, UIndex stackMapN,
                        UIndex atom, UIndex index )
{
    UCell* cell;
    ur_ctxAppendWord( ctx, atom );
    cell = ctx->ptr.cell + ctx->used - 1;
    ur_setId( cell, UT_WORD );
    ur_binding(cell) = binding;
    cell->word.ctx   = stackMapN;
    cell->word.atom  = atom;
    cell->word.index = index;
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
            if( ap->stackMapN )
                _defineArg( &ap->sval, BOR_BIND_FUNC, ap->stackMapN,
                            it->word.atom, ap->sval.used );
            break;

        case AR_OPT:
            if( it->word.atom == par->atoms[ EXTERN ] )
            {
                for( ++it; it != end; ++it )
                    ur_arrAppendInt32( &ap->externWords, it->word.atom );
                break;
            }

            if( ap->optionCount < MAX_OPTIONS )
            {
                OptionEntry* ent;
                union {
                    int16_t off[2];
                    UIndex  ind;
                } poff;
                int n;
                int argCount = 0;

                if( ap->stackMapN )
                    _defineArg( &ap->sval, BOR_BIND_OPTION, ap->stackMapN,
                                it->word.atom, ap->optionCount );

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
                            EMIT( FO_fetchArg );
                            if( ap->stackMapN )
                            {
                                poff.off[0] = -ap->optionCount;
                                poff.off[1] = argCount;
                                _defineArg( &ap->sval, BOR_BIND_OPTION_ARG,
                                            ap->stackMapN, it->word.atom,
                                            poff.ind );
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


static void _appendSetWords( UThread* ut, UBuffer* buf, const UCell* blkC )
{
    UBlockIt bi;
    ur_blockIt( ut, blkC, &bi );
    ur_foreach( bi )
    {
        int type = ur_type(bi.it);
        if( type == UT_SETWORD )
            ur_arrAppendInt32( buf, bi.it->word.atom );
        else if( type == UT_BLOCK || type == UT_PAREN )
            _appendSetWords( ut, buf, bi.it );
    }
}


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

  The spec. block must only contain the following patterns:
      word!
      lit-word!
      option! any word!
      '| any word!

  If bodyN is not zero, all argument and local words in the block will be
  bound to it.

  During evaluation the following cells can be placed on the UThread stack:

    [ opt-record ][ arguments ][ locals ][ opt args ][ optN args ]
                  ^
                  a1

  The stack map (or a1 CFUNC argument) points to the first argument cell.
  The option record indicates which options were evaluated and holds offsets to
  the optional agruments.
*/
void boron_compileArgProgram( BoronThread* bt, const UCell* specC,
                              UBuffer* prog, UIndex bodyN )
{
    ArgCompiler ac;
    ArgProgHeader* head;
    const int headerSize = 4;
    int localCount = 0;         // Local values (no arguments).


    ac.origUsed = prog->used;
    ac.argEndPc = 0;
    ac.optionCount = 0;
    ur_blockIt( UT(bt), specC, (UBlockIt*) &ac.bp.it );

    ur_binReserve( prog, prog->used + 16 + headerSize );
    prog->used += headerSize;   // Reserve space for start of ArgProgHeader.

    //printf( "KR compile arg %ld\n", ac.bp.end - ac.bp.it );

    if( ac.bp.it != ac.bp.end )
    {
        ac.bp.ut = UT(bt);
        ac.bp.atoms  = boron_compileAtoms(bt);
        ac.bp.rules  = _argRules;
        ac.bp.report = _argRuleHandler;
        ac.bp.rflag  = 0;
        ac.bin = prog;
        ac.stackMapN = bodyN;
        ur_ctxInit( &ac.sval, 0 );
        ur_arrInit( &ac.localWords,  sizeof(uint32_t), 0 );
        ur_arrInit( &ac.externWords, sizeof(uint32_t), 0 );

        ur_parseBlockI( &ac.bp, ac.bp.rules, ac.bp.it );

        if( bodyN )
        {
            UCell tmp;
            ur_initSeries( &tmp, UT_BLOCK, bodyN );
            _appendSetWords( UT(bt), &ac.localWords, &tmp );

            if( ac.localWords.used > 1 )
                _zeroDuplicateU32( &ac.localWords );
            if( ac.externWords.used )
                _zeroDiffU32( &ac.localWords, &ac.externWords );
            _removeValueU32( &ac.localWords, 0 );

            if( ac.localWords.used )
            {
                const UIndex* ai = ac.localWords.ptr.i32;
                const UIndex* ae = ai + ac.localWords.used;
                while( ai != ae )
                {
                    //printf( "KR local %s\n", ur_atomCStr(UT(bt), *ai) );
                    _defineArg( &ac.sval, BOR_BIND_FUNC, bodyN,
                                *ai++, ac.sval.used );
                }
                localCount = ac.localWords.used;
            }

            if( ac.sval.used )
            {
                const UBuffer* body = ur_bufferEnv(UT(bt), bodyN);

                ur_ctxSort( &ac.sval );
                ur_bindCopy( UT(bt), &ac.sval,
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
int boron_badArg( UThread* ut, UIndex atom, int argN )
{
    return ur_error( ut, UR_ERR_TYPE, "Unexpected %s for argument %d",
                     ur_atomCStr(ut, atom), argN + 1 );
}


/*
  Fetch arguments and call cfunc!.
*/
static
const UCell* boron_callC( UThread* ut, const UCell* funC,
                          UBlockItC* options,
                          const UCell* it, const UCell* end, UCell* res )
{
    const ArgProgHeader* head;
    const uint8_t* pc;
    UCell* args;
    UCell* r2;
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
                evalAvail(r2) = end - it;
                evalPos(r2)   = it;

                if( ((UCellFunc*) funC)->m.func( ut, args, res ) )
                    it = evalPos(r2);
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
        it = NULL;

cleanup:
    ut->stack.used = origStack;
    return it;

func_short:
    return cp_error( ut, UR_ERR_SCRIPT, "End of block" );
}


/*
  Fetch arguments and call func!.
*/
static
const UCell* boron_call( UThread* ut, const UCell* funC,
                         UBlockItC* options,
                         const UCell* it, const UCell* end, UCell* res )
{
    UBlockItC bi;
    const UCell* next;
    const ArgProgHeader* head;
    const uint8_t* pc;
    UCell* optRec = 0;
    UCell* r2;
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
    ur_blockIt( ut, funC, (UBlockIt*) &bi );
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
                    ur_traceError( ut, next, funC->series.buf, bi.it );
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
            cell = boron_wordValue( ut, it );
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
            UBlockItC sw;
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
                        if( ! boron_setWordValue( ut, sw.it, res ) )
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
            cell = boron_wordValue( ut, it );
            if( ! cell )
                goto unbound;
            *res = *cell;
            return ++it;

        case UT_PAREN:
            if( ! boron_doBlock( ut, it, res ) )
                return NULL;
            return ++it;

        case UT_PATH:
#if 1
        {
            UBlockItC path;
            const UCell* node;
            int headType;

            ur_blockIt( ut, it, (UBlockIt*) &path );
            headType = ur_type(path.it);
            assert( headType == UT_WORD || headType == UT_GETWORD );
            cell = boron_wordValue( ut, path.it );
            if( ! cell )
            {
                it = path.it;
                goto unbound;
            }
            ++it;

            while( ++path.it != path.end )
            {
                if( ur_type(path.it) == UT_GETWORD )
                {
                    node = boron_wordValue( ut, path.it );
                    if( ! node )
                        goto unbound;
                }
                else
                    node = path.it;
                
                /*
                UMethod method = ((UEnv*) ut->env)->method[ cell->type ];
                if( ! method( ut, UR_DO_PICK, cell, node ) )
                    return NULL;
                */
                switch( ur_type(cell) )
                {
                    case UT_COORD:
                        if( ur_is(node, UT_INT) )
                        {
                            coord_pick( cell, ur_int(node) - 1, res );
                            return it;
                        }
                        goto invalid_node;
                    case UT_VEC3:
                        if( ur_is(node, UT_INT) )
                        {
                            vec3_pick( cell, ur_int(node) - 1, res );
                            return it;
                        }
                        goto invalid_node;
                    case UT_VECTOR:
                        if( ur_is(node, UT_INT) )
                        {
                            const UBuffer* buf = ur_bufferSer(cell);
                            vector_pick( buf, cell->series.it+ur_int(node)-1,
                                         res );
                            return it;
                        }
                        goto invalid_node;
                    case UT_BLOCK:
                    case UT_PAREN:
                    case UT_PATH:
                        if( ur_is(node, UT_INT) )
                        {
                            int64_t i = ur_int(node);
                            if( i > 0 )
                            {
                                const UBuffer* blk = ur_bufferSer(cell);
                                if( --i < blk->used )
                                {
                                    cell = blk->ptr.cell + i;
                                    if( headType == UT_WORD )
                                    {
                                        if( ur_is(cell, UT_CFUNC) )
                                        {
                                            ++path.it;
                                            goto call_cfunc;
                                        }
                                        if( ur_is(cell, UT_FUNC) )
                                        {
                                            ++path.it;
                                            goto call_func;
                                        }
                                    }
                                    break;
                                }
                            }
                            ur_setId(res, UT_NONE);
                            return it;
                        }
                        else if( ur_is(node, UT_WORD) )
                        {
                            UBlockItC wi;
                            UAtom atom = ur_atom(node);
                            ur_blockIt( ut, cell, (UBlockIt*) &wi );
                            ur_foreach( wi )
                            {
                                if( ur_isWordType( ur_type(wi.it) ) &&
                                    (ur_atom(wi.it) == atom) )
                                {
                                    if( ++wi.it == wi.end )
                                        goto blk_none;
                                    cell = wi.it;
                                    if( headType == UT_WORD )
                                    {
                                        if( ur_is(cell, UT_CFUNC) )
                                        {
                                            ++path.it;
                                            goto call_cfunc;
                                        }
                                        if( ur_is(cell, UT_FUNC) )
                                        {
                                            ++path.it;
                                            goto call_func;
                                        }
                                    }
                                    break;
                                }
                            }
                            if( wi.it == wi.end )
                            {
blk_none:
                                ur_setId(res, UT_NONE);
                                return it;
                            }
                            break;
                        }
                        goto invalid_node;
                    case UT_CONTEXT:
                        if( ur_is(node, UT_WORD) )
                        {
                            const UBuffer* ctx = ur_bufferSer(cell);
                            int i = ur_ctxLookup( ctx, node->word.atom );
                            if( i >= 0 )
                            {
                                cell = ctx->ptr.cell + i;
                                if( headType == UT_WORD )
                                {
                                    if( ur_is(cell, UT_CFUNC) )
                                    {
                                        ++path.it;
                                        goto call_cfunc;
                                    }
                                    if( ur_is(cell, UT_FUNC) )
                                    {
                                        ++path.it;
                                        goto call_func;
                                    }
                                }
                                break;
                            }
                            if( node->word.atom == UR_ATOM_SELF )
                                break;  // cell already set to this context.
                        }
                        goto invalid_node;
                    case UT_CFUNC:
call_cfunc:
                        return boron_callC( ut, cell, &path, it, end, res );
                    case UT_FUNC:
call_func:
                        return boron_call( ut, cell, &path, it, end, res );
                    default:
                        goto invalid_node;
                }
            }
            *res = *cell;
            return it;
invalid_node:
            return cp_error( ut, UR_ERR_SCRIPT, "Invalid path %s node",
                             ur_atomCStr(ut, ur_type(node)) );
        }
#else
        // Cannot use ur_pathCell() as it does not give us the path needed by
        // boron_callC/boron_call.
        {
            int headType = ur_pathCell( ut, it++, res );
            if( headType == UT_WORD )
            {
                if( ur_is(res, UT_CFUNC) )
                {
                    UCell* fc = ur_pushCell(ut, res);
                    it = boron_callC( ut, fc, 0, it, end, res );
                    ur_pop(ut);
                }
                if( ur_is(res, UT_FUNC) )
                {
                    UCell* fc = ur_pushCell(ut, res);
                    it = boron_call( ut, fc, 0, it, end, res );
                    ur_pop(ut);
                }
            }
            else if( ! headType )
                return NULL; //goto traceError;
        }
            return it;
#endif

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
    return cp_error( ut, UR_ERR_SCRIPT, "Unbound word '%s",
                     ur_atomCStr(ut, it->word.atom) );
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

  \return res or NULL if an exception is thrown.
*/
UCell* boron_doBlock( UThread* ut, const UCell* blkC, UCell* res )
{
    UBlockItC bi;
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
    blk = ur_blockIt( ut, blkC, (UBlockIt*) &bi );
    psave = blk->storage;
    if( psave & UR_BUF_PROTECT )
        psave = 0;
    else
        blk->storage = psave | UR_BUF_PROTECT;
#else
    ur_blockIt( ut, blkC, (UBlockIt*) &bi );
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
            return NULL;
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
    return res;
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

    ur_generateBuf( ut, &ind, UT_BLOCK );   // gc!
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


#ifdef OLD_EVAL
/*
  NOTE: This is an eval-control function.

  -cf-
    do
        value
    return: Result of value.
    group: eval
*/
CFUNC(cfunc_do)
{
    if( ! boron_eval1( ut, a1, res ) )
        return UR_THROW;

    switch( ur_type(res) )
    {
        case UT_WORD:
        {
            const UCell* cell;
            if( ! (cell = boron_wordValue( ut, res )) )
                return UR_THROW; //goto traceError;
            if( ur_is(cell, UT_CFUNC) || ur_is(cell, UT_FUNC) )
                return boron_call( ut, (const UCellFunc*) cell, a1, res );
            *res = *cell;
        }
            break;

        case UT_LITWORD:
            res->id.type = UT_WORD;
            break;

        case UT_GETWORD:
        {
            const UCell* cell;
            if( ! (cell = boron_wordValue( ut, res )) )
                return UR_THROW; //goto traceError;
            *res = *cell;
        }
            break;

        case UT_STRING:
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, res );
            if( si.it == si.end )
            {
                ur_setId( res, UT_UNSET );
            }
            else
            {
                UCell tmp;
                UIndex hold;
                UIndex blkN;
                int ok;

                if( ur_strIsUcs2(si.buf) )
                    return ur_error( ut, UR_ERR_INTERNAL,
                                     "FIXME: Cannot do ucs2 string!" );

                blkN = ur_tokenize( ut, si.buf->ptr.c + si.it,
                                        si.buf->ptr.c + si.end, &tmp );
                if( ! blkN )
                    return UR_THROW;

                boron_bindDefault( ut, blkN );

                hold = ur_hold( blkN );
                ok = boron_doBlock( ut, &tmp, res );
                ur_release( hold );
                return ok;
            }
        }
            break;

        case UT_FILE:
        {
            int ok;
            UCell* tmp;
            if( ! (tmp = boron_stackPush(ut)) )     // Hold file arg.
                return UR_THROW;
            *tmp = *res;
            ok = cfunc_load( ut, tmp, res );
            boron_stackPop(ut);
            if( ! ok )
                return UR_THROW;
        }
            if( ! ur_is(res, UT_BLOCK) )
                return UR_OK;
            // Fall through to block...
#if __GNUC__ == 7
            __attribute__ ((fallthrough));
#endif

        case UT_BLOCK:
        case UT_PAREN:
            if( ur_isShared( res->series.buf ) )
            {
                return boron_doBlock( ut, res, res );
            }
            else
            {
                int ok;
                UIndex hold = ur_hold( res->series.buf );
                ok = boron_doBlock( ut, res, res );
                ur_release( hold );
                return ok;
            }

        case UT_PATH:
        {
            int ok;
            UCell* tmp;
            if( ! (tmp = boron_stackPush(ut)) )
                return UR_THROW;
            ok = ur_pathCell( ut, res, tmp );
            if( ok != UR_THROW )
            {
                if( (ur_is(tmp, UT_CFUNC) || ur_is(tmp, UT_FUNC)) &&
                    ok == UT_WORD )
                {
                    ok = boron_call( ut, (const UCellFunc*) tmp, a1, res );
                }
                else
                {
                    *res = *tmp;
                    ok = UR_OK;
                }
            }
            boron_stackPop(ut);
            return ok;
        }

        case UT_LITPATH:
            res->id.type = UT_PATH;
            break;

        case UT_FUNC:
        case UT_CFUNC:
        {
            int ok;
            UCell* tmp;
            if( (tmp = boron_stackPush(ut)) )   // Hold func cell.
            {
                *tmp = *res;
                ok = boron_call( ut, (UCellFunc*) tmp, a1, res );
                boron_stackPop(ut);
                return ok;
            }
        }
            return UR_THROW;

        default:
            // Result already set.
            break;
    }
    return UR_OK;
}
#endif


CFUNC_PUB( cf_load );

/*
  -cf-
    do
        value
    return: Result of value.
    group: eval
*/
CFUNC_PUB( cf_do )
{
    // NOTE: This is an FO_eval function.
    const UCell* it  = evalPos(a1);
    const UCell* end = it + evalAvail(a1);

    if( ! (it = boron_eval1( ut, it, end, res )) )
        return UR_THROW;

    switch( ur_type(res) )
    {
        case UT_WORD:
        {
            const UCell* cell;
            if( ! (cell = boron_wordValue( ut, res )) )
            {
                return ur_error( ut, UR_ERR_SCRIPT, "Unbound word '%s",
                                 ur_atomCStr(ut, res->word.atom) );
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
            if( ur_is(cell, UT_UNSET) )
                return ur_error( ut, UR_ERR_SCRIPT, "Unset word '%s",
                                 ur_atomCStr(ut, res->word.atom) );
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
                ok = boron_doBlock( ut, &tmp, res ) ? UR_OK : UR_THROW;
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
                return UR_OK;
            // Fall through to block...
            /* FALLTHRU */

        case UT_BLOCK:
        case UT_PAREN:
        {
            UCell* resOk;
            UIndex hold;
//#ifdef SHARED_STORE
            if( ur_isShared( res->series.buf ) )
                resOk = boron_doBlock( ut, res, res );
            else
//#endif
            {
                hold = ur_hold( res->series.buf );
                resOk = boron_doBlock( ut, res, res );
                ur_release( hold );
            }
            return resOk ? UR_OK : UR_THROW;
        }
#if 0
        case UT_PATH:
        {
            UBlockItC path;
            const UCell* cell;

            ur_blockIt( ut, it, (UBlockIt*) &path );
            cell = boron_wordValue( ut, path.it );
            if( ! cell )
            {
                return ur_error( ut, "Unbound word '%s",
                                 ur_atomCStr(ut, path.it->word.atom) );
            }
            ++it;
            printf( "KR %d\n", ur_type(cell) );
            if( cell->type == UT_CFUNC )
            {
                ++path.it;
                it = boron_callC( ut, cell, &path, it, end, res );
                goto end_func;
            }
            if( cell->type == UT_FUNC )
            {
                ++path.it;
                it = boron_call( ut, cell, &path, it, end, res );
                goto end_func;
            }
            return ur_error( ut, "do path! not implemented!" );
        }

        case UT_LITPATH:
            res->type = UT_PATH;
            break;
#endif
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
    evalPos(a1) = it;
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

    res = boron_doBlock( ut, &bcell, ut->stack.ptr.cell + ut->stack.used - 1 );

    ur_release(hold);
    return res;
}
