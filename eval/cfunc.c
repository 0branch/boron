/*
  Copyright 2009-2013 Karl Robillard

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


/** \def CFUNC
  Macro to define C functions.

  If the function takes multiple arguments, just index off of \a a1.
  For instance, the third argument is (a1+2).
  
  \param ut     The ubiquitous thread pointer.
  \param a1     Pointer to argument cells.
  \param res    Pointer to result cell.
*/
/** \def CFUNC_OPTIONS
  Macro to get uint32_t option flags from inside a C function.
  Only use this if options were declared in the boron_addCFunc() sig.
*/


/*
  Undocumented debug function.
*/
CFUNC(cfunc_nop)
{
    (void) ut;
    (void) a1;
#if 1
    (void) res;
#else
    // Must set result to something; it may be uninitialized.
    ur_setId(res, UT_UNSET);
#endif
    return UR_OK;
}


/*-cf-
    quit
        /return     Returns value as exit status to operating system.
            value   Normally an int! between 0 and 255.
    return: NA
    group: control

    Exit interpreter.  The exit status will be 0 if the return value is not
    specified.
*/
CFUNC(cfunc_quit)
{
    if( CFUNC_OPTIONS & 1 )
    {
        *res = *a1;
    }
    else
    {
        ur_setId(res, UT_INT);
        ur_int(res) = 0;
    }
    return boron_throwWord( ut, UR_ATOM_QUIT );
}


/*-cf-
    halt
    return: NA
    group: control

    Halt interpreter.
*/
CFUNC(cfunc_halt)
{
    (void) a1;
    (void) res;
    return boron_throwWord( ut, UR_ATOM_HALT );
}


/*-cf-
    exit
    return: NA
    group: control

    Exit from function with result unset.
*/
CFUNC(cfunc_exit)
{
    (void) a1;
    ur_setId(res, UT_UNSET);
    return boron_throwWord( ut, UR_ATOM_RETURN );
}


/*-cf-
    return
        result
    return: NA
    group: control

    Exit from function with result.
*/
CFUNC(cfunc_return)
{
    *res = *a1;
    return boron_throwWord( ut, UR_ATOM_RETURN );
}


/*-cf-
    break
    return: NA
    group: control

    Exit from loop, while, foreach, or forall.
*/
CFUNC(cfunc_break)
{
    (void) a1;
    ur_setId(res, UT_UNSET);
    return boron_throwWord( ut, UR_ATOM_BREAK );
}


/*-cf-
    throw
        value
        /name       Give exception a name.
            word    word!
    return: NA
    group: control
    see: catch, try

    Stop evaluation of the current function and pass an exception up the
    call stack.
*/
CFUNC(cfunc_throw)
{
    UBuffer* blk = ur_errorBlock(ut);
    (void) res;

    ur_blkPush( blk, a1 );
    if( CFUNC_OPTIONS & 1 )
        ur_blkPush( blk, a2 );
    return UR_THROW;
}


/*-cf-
    catch
        body        block! Code to evaluate.
        /name       Only catch exceptions with a certain name.
            word    word!/block! Names to catch.
    return: Result of block evaluation or thrown value.
    group: control
    see: throw, try

    Do body and return any exception thrown.
*/
CFUNC(cfunc_catch)
{
    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "catch expected block!" );

    if( ! boron_doBlock( ut, a1, res ) )
    {
        UBuffer* blk = ur_errorBlock(ut);
        UCell* cell = blk->ptr.cell + (blk->used - 1);
        assert(blk->used);

        if( CFUNC_OPTIONS & 1 )
        {
            if( ! ur_is(cell, UT_WORD) )
                return UR_THROW;
            if( ur_is(a2, UT_WORD) )
            {
                if( ur_atom(a2) == ur_atom(cell) )
                {
caught_word:
                    --cell;
                    blk->used -= 2;
                    goto set_result;
                }
            }
            else if( ur_is(a2, UT_BLOCK) )
            {
                UBlockIter bi;
                ur_blkSlice( ut, &bi, a2 );
                ur_foreach( bi )
                {
                    if( ur_is(bi.it, UT_WORD) &&
                        ur_atom(bi.it) == ur_atom(cell) )
                        goto caught_word;
                }
            }
            return UR_THROW;
        }
        else
        {
            --blk->used;
        }

set_result:
        *res = *cell;
    }
    return UR_OK;
}


/*-cf-
    try
        body block! Code to evaluate.
    return: Result of block evaluation or error.
    group: control
    see: catch, throw

    Do body and catch any thrown error!.  Other thrown types are ignored
    and will be passed up the call chain.
*/
CFUNC(cfunc_try)
{
    int ok;

    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "try expected block!" );

    if( ! (ok = boron_doBlock( ut, a1, res )) )
    {
        UBuffer* blk = ur_errorBlock(ut);
        UCell* cell = blk->ptr.cell + (blk->used - 1);
        assert( blk->used > 0 );
        if( ur_is(cell, UT_ERROR) )
        {
            --blk->used;
            *res = *cell;
            ok = UR_OK;
        }
    }
    return ok;
}


/*-cf-
    recycle
    return: NA
    group: storage

    Run the garbage collector.
*/
CFUNC(cfunc_recycle)
{
    (void) a1;
    (void) res;
    ur_recycle( ut );
    return UR_OK;
}


static int boron_call( UThread*, const UCellFunc* fcell, UCell* blkC,
                       UCell* res );
static int cfunc_load( UThread*, UCell*, UCell* );

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
            if( ! (cell = ur_wordCell( ut, res )) )
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
            if( ! (cell = ur_wordCell( ut, res )) )
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


/*-cf-
    set
        words   word!/lit-word!/block!/path!
        values  Any value.
    return: unset!
    group: data
    see: get, in, value?

    Assign a value to one or more words.

        set 'a 22
        a
        == 22

    If words and values are both a block! then each word in words is set
    to the corresponding value in values.

        set [a b] [1 4.0]
        a
        == 1
        b
        == 4.0
*/
CFUNC(cfunc_set)
{
    UCell* cell;
    if( ur_isWordType( ur_type(a1) ) )
    {
        if( ! (cell = ur_wordCellM(ut, a1)) )
            return UR_THROW;
        *cell = *a2;
    }
    else if( ur_is(a1, UT_PATH) )
    {
        if( ! ur_setPath( ut, a1, a2 ) )
            return UR_THROW;
    }
    else if( ur_is(a1, UT_BLOCK) )
    {
        UBlockIterM bi;
        ur_blkSliceM( ut, &bi, a1 );
        if( ur_is(a2, UT_BLOCK) )
        {
            UBlockIter b2;
            ur_blkSlice( ut, &b2, a2 );
            ur_foreach( bi )
            {
                if( ur_isWordType( ur_type(bi.it) ) )
                {
                    if( ! (cell = ur_wordCellM(ut, bi.it)) )
                        return UR_THROW;
                    if( b2.it != b2.end )
                        *cell = *b2.it++;
                    else
                        ur_setId(cell, UT_NONE);
                }
            }
        }
        else
        {
            ur_foreach( bi )
            {
                if( ur_isWordType( ur_type(bi.it) ) )
                {
                    if( ! (cell = ur_wordCellM(ut, bi.it)) )
                        return UR_THROW;
                    *cell = *a2;
                }
            }
        }
    }
    else
    {
        return ur_error( ut, UR_ERR_TYPE, "set expected word!/block!/path!" );
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    get
        word    word!/context!
    return: Value of word or block of values in context.
    group: data
    see: in, set
*/
CFUNC(cfunc_get)
{
    if( ur_is(a1, UT_WORD) )
    {
        const UCell* cell;
        if( ! (cell = ur_wordCell( ut, a1 )) )
            return UR_THROW;
        *res = *cell;
        return UR_OK;
    }
    else if( ur_is(a1, UT_CONTEXT) )
    {
        UBuffer* blk = ur_makeBlockCell( ut, UT_BLOCK, 0, res );
        const UBuffer* ctx = ur_bufferSer( a1 );
        ur_blkAppendCells( blk, ctx->ptr.cell, ctx->used );
        return UR_OK;
    }
    return errorType( "get expected word!/context!" );
}


/*-cf-
    value?
        value   Any value.
    return: True unless value is an unset word.
    group: data
    see: get, set

    Determine if a word has already been set.

        value? 'blah
        == false
*/
CFUNC(cfunc_valueQ)
{
    int logic = 1;
    if( ur_isWordType( ur_type(a1) ) )
    {
        const UCell* cell;
        if( ! (cell = ur_wordCell( ut, a1 )) )
            return UR_THROW;
        if( ur_is(cell, UT_UNSET) )
           logic = 0;
    }
    ur_setId(res, UT_LOGIC);
    ur_int(res) = logic;
    return UR_OK;
}


/*-cf-
    in
        context     context!
        word        word!
    return: Word bound to context or none!.
    group: data
    see: set, value?
*/
CFUNC(cfunc_in)
{
    if( ur_is(a1, UT_CONTEXT) && ur_is(a2, UT_WORD) )
    {
        const UBuffer* ctx;
        int wrdN;

        if( ! (ctx = ur_sortedContext( ut, a1 )) )
            return UR_THROW;
        wrdN = ur_ctxLookup( ctx, ur_atom(a2) );
        if( wrdN < 0 )
        {
            ur_setId(res, UT_NONE);
        }
        else
        {
            int ctxN = a1->series.buf;
            *res = *a2;
            ur_setBinding( res,
                           ur_isShared(ctxN) ? UR_BIND_ENV : UR_BIND_THREAD );
            res->word.ctx   = ctxN;
            res->word.index = wrdN;
        }
        return UR_OK;
    }
    return errorType( "in expected context! and word!" );
}


#if 0
/*
    use
        context     context!
        block       block!
    return: Last evaluated value.
    group: eval

    Bind block to context and evaluate it.
*/
CFUNC(cfunc_use)
{
    UBuffer* blk;
    const UBuffer* ctx;
    const UCell* bc = a2;

    if( ur_is(a1, UT_CONTEXT) && ur_is(bc, UT_BLOCK) )
    {
        if( ! (blk = ur_bufferSerM(bc)) )
            return UR_THROW;
        if( ! (ctx = ur_sortedContext(ut, a1)) )
            return UR_THROW;
        ur_bind( ut, blk, ctx,
                 ur_isShared(a1->context.buf) ? UR_BIND_ENV : UR_BIND_THREAD );
        return boron_doBlock( ut, bc, res );
    }
    return errorType( "use expected context! and block!" );
}
#endif


extern void _contextWords( UThread* ut, const UBuffer* ctx, UIndex ctxN,
                           UCell* res );

/*-cf-
    words-of
        context     context!
    return: Block of words defined in context.
    group: data
    see: values-of
*/
/*-cf-
    values-of
        context     context!
    return: Block of values defined in context.
    group: data
    see: words-of
*/
CFUNC(cfunc_words_of)
{
    const UBuffer* ctx;

    if( ! ur_is(a1, UT_CONTEXT) )
        return errorType( "words-of expected context!" );

    if( ur_int(a2) )
    {
        const UCell* cell;
        UBuffer* blk;
        int used;

        ctx = ur_bufferSer(a1);
        // Save what we need from ctx before ur_makeBlockCell invalidates it.
        cell = ctx->ptr.cell;
        used = ctx->used;

        blk = ur_makeBlockCell( ut, UT_BLOCK, used, res );
        memCpy( blk->ptr.cell, cell, used * sizeof(UCell) );
        blk->used = used;
    }
    else
    {
        if( ! (ctx = ur_sortedContext( ut, a1 )) )
            return UR_THROW;
        _contextWords( ut, ctx, a1->context.buf, res );
    }
    return UR_OK;
}


/*-cf-
    binding?
        word    word!
    return: context!/datatype!
    group: data
    see: bind, unbind

    Get the context which a word is bound to.
*/
CFUNC(cfunc_bindingQ)
{
    if( ! ur_isWordType( ur_type(a1) ) )
        return errorType( "binding? expected word!" );
    switch( ur_binding(a1) )
    {
        case UR_BIND_THREAD:
        case UR_BIND_ENV:
            ur_setId( res, UT_CONTEXT );
            ur_setSeries( res, a1->word.ctx, 0 );
            break;

        case UR_BIND_FUNC:
            ur_setId( res, UT_WORD );
            ur_setWordUnbound( res, UT_FUNC );
            break;

        case UR_BIND_OPTION:
            ur_setId( res, UT_WORD );
            ur_setWordUnbound( res, UT_OPTION );
            break;

        default:
            ur_setId( res, UT_NONE );
            break;
    }
    return UR_OK;
}


#define BIND_ERR_MSG    "%s expected words argument of word!/block!"

/*-cf-
    bind
        words   word!/block!
        context word!/context!
    return: Bound words
    group: data
    see: binding?, unbind
*/
CFUNC(cfunc_bind)
{
    UIndex ctxN;
    UBuffer* blk;
    UCell* ctxArg = a2;

    if( ur_is(ctxArg, UT_WORD) )
    {
        ctxN = ctxArg->word.ctx;
        if( ctxN == UR_INVALID_BUF )
            return ur_error( ut, UR_ERR_SCRIPT, "bind word '%s is unbound",
                             ur_wordCStr( ctxArg ) );
    }
    else if( ur_is(ctxArg, UT_CONTEXT) )
        ctxN = ctxArg->series.buf;
    else
        return errorType( "bind expected word!/context! for context" );

    if( ur_is(a1, UT_BLOCK) )
    {
        const UBuffer* ctx;

        if( ! (blk = ur_bufferSerM(a1)) )
            return UR_THROW;
        if( ! (ctx = ur_sortedContext(ut, ctxArg)) )
            return UR_THROW;
        ur_bind( ut, blk, ctx,
                 ur_isShared(ctxN) ? UR_BIND_ENV : UR_BIND_THREAD );
        *res = *a1;
        return UR_OK;
    }
    else if( ur_is(a1, UT_WORD) )
    {
        UBindTarget bt;

        if( ! (bt.ctx = ur_sortedContext(ut, ctxArg)) )
            return UR_THROW;
        bt.ctxN = ctxN;
        bt.bindType = ur_isShared(ctxN) ? UR_BIND_ENV : UR_BIND_THREAD;
        bt.self = UR_INVALID_ATOM;

        *res = *a1;
        ur_bindCells( ut, res, res + 1, &bt );
        return UR_OK;
    }
    return ur_error(ut, UR_ERR_TYPE, BIND_ERR_MSG, "bind" );
}


/*-cf-
    unbind
        words   word!/block!
        /deep   If words is a block, unbind all sub-blocks.
    return: Unbound words
    group: data
    see: bind, binding?
*/
CFUNC(cfunc_unbind)
{
#define OPT_UNBIND_DEEP   0x01
    *res = *a1;
    if( ur_is(a1, UT_BLOCK) )
    {
        UBlockIterM bi;
        if( ! ur_blkSliceM( ut, &bi, a1 ) )
            return UR_THROW;
        ur_unbindCells( ut, bi.it, bi.end, CFUNC_OPTIONS & OPT_UNBIND_DEEP );
        return UR_OK;
    }
    else if( ur_is(a1, UT_WORD) )
    {
        ur_unbindCells( ut, res, res + 1, 0 );
        return UR_OK;
    }
    return ur_error(ut, UR_ERR_TYPE, BIND_ERR_MSG, "unbind" );
}


/*-cf-
    infuse
        block   block!
        context word!/context!
    return: Modified block.
    group: data
    see: bind

    Replace words with their value in context.
*/
CFUNC(cfunc_infuse)
{
    UIndex ctxN;
    UBlockIterM bi;
    UCell* ctxArg = a2;

    if( ur_is(ctxArg, UT_WORD) )
    {
        ctxN = ctxArg->word.ctx;
        if( ctxN == UR_INVALID_BUF )
            return ur_error( ut, UR_ERR_SCRIPT, "infuse word '%s is unbound",
                             ur_wordCStr( ctxArg ) );
    }
    else if( ur_is(ctxArg, UT_CONTEXT) )
        ctxN = ctxArg->series.buf;
    else
        return errorType( "infuse expected word!/context! for context" );

    if( ! ur_is(a1, UT_BLOCK) )
        return errorType( "infuse expected block!" );
    if( ! ur_blkSliceM( ut, &bi, a1 ) )
        return UR_THROW;

    ur_infuse( ut, bi.it, bi.end, ur_bufferSer(ctxArg) );
    *res = *a1;
    return UR_OK;
}


#define OPER_FUNC(name,OP) \
CFUNC(name) { \
    int t = ur_type(a1); \
    if( t < ur_type(a2) ) \
        t = ur_type(a2); \
    return ut->types[ t ]->operate( ut, a1, a2, res, OP ); \
}


/*-cf-
    add
        a   int!/decimal!/vec3!
        b   int!/decimal!/vec3!/block!
    return: Sum of two numbers.
    group: math

    The second argument may be a block:
        add 0 [4 1 3]
        == 8
*/
/*-cf-
    sub
        a   int!/decimal!/vec3!
        b   int!/decimal!/vec3!/block!
    return: Difference of two numbers.
    group: math
*/
/*-cf-
    mul
        a   int!/decimal!/vec3!
        b   int!/decimal!/vec3!/block!
    return: Product of two numbers.
    group: math
*/
/*-cf-
    div
        a   int!/decimal!/vec3!
        b   int!/decimal!/vec3!
    return: Quotient of a divided by b.
    group: math
    see: mod
*/
/*-cf-
    mod
        a   int!/decimal!/coord!
        b   int!/decimal!/coord!
    return: Remainder of a divided by b.
    group: math
    see: div
*/
OPER_FUNC( cfunc_add, UR_OP_ADD )
OPER_FUNC( cfunc_sub, UR_OP_SUB )
OPER_FUNC( cfunc_mul, UR_OP_MUL )
OPER_FUNC( cfunc_div, UR_OP_DIV )
OPER_FUNC( cfunc_mod, UR_OP_MOD )


/*-cf-
    and
        a   logic!/char!/int!
        b   logic!/char!/int!/block!
    return: Bitwise AND.
    group: math
    see: or, xor
*/
/*-cf-
    or
        a   logic!/char!/int!
        b   logic!/char!/int!/block!
    return: Bitwise OR.
    group: math
    see: and, xor
*/
/*-cf-
    xor
        a   logic!/char!/int!
        b   logic!/char!/int!/block!
    return: Bitwise exclusive OR.
    group: math
    see: and, or
*/
OPER_FUNC( cfunc_and, UR_OP_AND )
OPER_FUNC( cfunc_or,  UR_OP_OR )
OPER_FUNC( cfunc_xor, UR_OP_XOR )


/*-cf-
    minimum
        a
        b
    return: Lesser of two values.
    group: math
    see: maximum
*/
CFUNC(cfunc_minimum)
{
    *res = (ur_compare( ut, a1, a2 ) < 0) ? *a1 : *a2;
    return UR_OK;
}


/*-cf-
    maximum
        a
        b
    return: Greater of two values.
    group: math
    see: minimum
*/
CFUNC(cfunc_maximum)
{
    *res = (ur_compare( ut, a1, a2 ) > 0) ? *a1 : *a2;
    return UR_OK;
}


/*-cf-
    abs
        n   int!/decimal!/time!/bignum!
    return: Absolute value of n.
    group: math
*/
CFUNC(cfunc_abs)
{
    int type = ur_type(a1);
    switch( type )
    {
        case UT_INT:
            ur_setId(res, type);
            ur_int(res) = abs( ur_int(a1) );
            break;

        case UT_DECIMAL:
        case UT_TIME:
            ur_setId(res, type);
            ur_decimal(res) = fabs( ur_decimal(a1) );
            break;

        case UT_BIGNUM:
            *res = *a1;
            bignum_abs( res );
            break;

        default:
            return ur_error( ut, UR_ERR_TYPE, "abs expected int!/decimal!" );
    }
    return UR_OK;
}


static int _mathFunc( UThread* ut, const UCell* a1, UCell* res,
                      double (*func)(double) )
{
    double n;
    if( ur_is(a1, UT_DECIMAL) )
        n = ur_decimal(a1);
    else if( ur_is(a1, UT_INT) )
        n = (double) ur_int(a1);
    else
        return errorType( "math function expected int!/decimal!" );
    ur_setId(res, UT_DECIMAL);
    ur_decimal(res) = func( n );
    return UR_OK;
}


/*-cf-
    sqrt
        n   int!/decimal!
    return: Square root of number.
    group: math
*/
/*-cf-
    cos
        n   int!/decimal!
    return: Cosine of number.
    group: math
*/
/*-cf-
    sin
        n   int!/decimal!
    return: Sine of number.
    group: math
*/
/*-cf-
    atan
        n   int!/decimal!
    return: Arc tangent of number.
    group: math
*/
CFUNC(cfunc_sqrt) { return _mathFunc( ut, a1, res, sqrt ); }
CFUNC(cfunc_cos)  { return _mathFunc( ut, a1, res, cos ); }
CFUNC(cfunc_sin)  { return _mathFunc( ut, a1, res, sin ); }
CFUNC(cfunc_atan) { return _mathFunc( ut, a1, res, atan ); }


extern int context_make( UThread* ut, const UCell* from, UCell* res );
extern UDatatype dt_context;

static int context_make_override( UThread* ut, const UCell* from, UCell* res )
{
    if( ! context_make( ut, from, res ) )
        return UR_THROW;
    if( ur_is(from, UT_BLOCK) )
        return boron_doVoid( ut, from );
    return UR_OK;
}


/*-cf-
    make
        prototype   datatype!/context!
        attributes
    return: New value.
    group: data
    see: construct, to-_type_

    Create a new value.
*/
CFUNC(cfunc_make)
{
    if( ur_is(a1, UT_DATATYPE) )
    {
        int t = ur_datatype(a1);
        if( t < UT_MAX )
            return DT( t )->make( ut, a2, res );
    }
    else if( ur_is(a1, UT_CONTEXT) )
    {
        UBlockIterM bi;
        UBuffer* ctx = ur_ctxClone( ut, ur_bufferSer(a1), res );
        if( ur_is(a2, UT_BLOCK) )
        {
            if( ! ur_blkSliceM( ut, &bi, a2 ) )
                return UR_THROW;
            ur_ctxSetWords( ctx, bi.it, bi.end );
            ur_bind( ut, bi.buf, ur_ctxSort(ctx), UR_BIND_SELF );
            return boron_doVoid( ut, a2 );
        }
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "make requires a context! or single datatype!" );
}


/*-cf-
    copy
        value
        /deep   If value is a block, copy all sub-blocks.
    return: New value.
    group: data
*/
CFUNC(cfunc_copy)
{
#define OPT_COPY_DEEP   0x01
    int type = ur_type(a1);

    if( ur_isBlockType( type ) && (CFUNC_OPTIONS & OPT_COPY_DEEP) )
    {
        *res = *a1;
        res->series.buf = ur_blkClone( ut, a1->series.buf );
        return UR_OK;
    }
    DT( type )->copy( ut, a1, res );
    return UR_OK;
}


/*-cf-
    reserve
        series
        size    Number of elements to reserve.
    return: Series.
    group: data, storage
    see: free

    Expand the capacity of series buffer.  This cannot be used to make the
    buffer smaller.
*/
CFUNC(cfunc_reserve)
{
    UBuffer* buf;
    int size;
    int type = ur_type(a1);

    if( ! ur_isSeriesType( type ) )
        return errorType( "reserve expected series" );
    if( ! ur_is(a2, UT_INT) )
        return errorType( "reserve expected int! size" );
    if( ! (buf = ur_bufferSerM(a1)) )
        return UR_THROW;

    size = a1->series.it + ur_int(a2);
#if 1
    switch( type )
    {
        case UT_BINARY:
        case UT_BITSET:
            ur_binReserve( buf, size );
            break;

        case UT_BLOCK:
        case UT_PAREN:
        case UT_VECTOR:
        case UT_STRING:
        case UT_FILE:
            ur_arrReserve( buf, size );
            break;
    }
#else
    DT( type )->reserve( ut, a1, size );
#endif
    *res = *a1;
    return UR_OK;
}


/*-cf-
    does
        body  block!
    return: func!
    group: eval
    see: func

    Create a function which takes no arguments.
*/
CFUNC(cfunc_does)
{
    UCellFunc* cell = (UCellFunc*) res;
    UIndex bodyN = ur_blkClone( ut, a1->series.buf );   // gc!

    ur_setId(cell, UT_FUNC);        // Sets argCount, localCount to 0.
    cell->argBufN   = UR_INVALID_BUF;
    cell->m.f.bodyN = bodyN;
    cell->m.f.sigN  = UR_INVALID_BUF;
    return UR_OK;
}


/*-cf-
    func
        spec  block!
        body  block!
    return:  func!
    group: eval
    see: does

    Create function.
*/
CFUNC(cfunc_func)
{
    UBuffer ctx;
    UBuffer optCtx;
    UBuffer* body;
    UIndex bodyN;
    UCellFunc* fc = (UCellFunc*) res;

    if( ! ur_is(a1, UT_BLOCK) || ! ur_is(a2, UT_BLOCK) )
    {
        return ur_error( ut, UR_ERR_TYPE,
                         "func expected block! for spec & body" );
    }

    ur_ctxInit( &ctx, 0 );
    ur_ctxInit( &optCtx, 0 );

    bodyN = ur_blkClone( ut, a2->series.buf );      // gc!

    ur_setId(fc, UT_FUNC);
    fc->argBufN   = UR_INVALID_BUF;
    fc->m.f.bodyN = bodyN;
    fc->m.f.sigN  = a1->series.buf;

    // Assign after fc fully initialized to handle recycle.
    // Body block is held since fc is the result.
    fc->argBufN = boron_makeArgProgram( ut, a1, &ctx, &optCtx, fc );    // gc!

    {
    UBindTarget bt;

    bt.ctxN = bodyN;
    bt.self = UR_INVALID_ATOM;
    body = ur_buffer( bt.ctxN );

    if( ctx.used )
    {
        bt.ctx      = ur_ctxSort(&ctx);
        bt.bindType = UR_BIND_FUNC;
        ur_bindCells( ut, body->ptr.cell, body->ptr.cell + body->used, &bt );
    }

    if( optCtx.used )
    {
        bt.ctx      = ur_ctxSort(&optCtx);
        bt.bindType = UR_BIND_OPTION;
        ur_bindCells( ut, body->ptr.cell, body->ptr.cell + body->used, &bt );
    }
    }

    ur_ctxFree( &optCtx );
    ur_ctxFree( &ctx );

    return UR_OK;
}


/*-cf-
    not
        value
    return: Inverse logic! of value.
    group: data
    see: complement, negate
*/
CFUNC(cfunc_not)
{
    (void) ut;
    ur_setId(res, UT_LOGIC);
    ur_int(res) = ur_isTrue(a1) ? 0 : 1;
    return UR_OK;
}


/*-cf-
    if
        test    Test condition.
        body    block!  Code to evaluate when true.
    return: Result of body if test is true, or none! when it is false.
    group: control
    see: either, ifn, while

    Conditionally evaluate code.
*/
CFUNC(cfunc_if)
{
    if( ur_isTrue(a1) )
    {
        UCell* bc = a2;
        if( ur_is(bc, UT_BLOCK) )
            return boron_doBlock( ut, bc, res );
        *res = *bc;
        return UR_OK;
    }
    ur_setId(res, UT_NONE);
    return UR_OK;
}


/*-cf-
    ifn
        test    Test condition.
        body    block!  Code to evaluate when false.
    return: Result of body if test is false, or none! when it is true.
    group: control
    see: either, if

    This is shorthand for "if not test body".
*/
CFUNC(cfunc_ifn)
{
    if( ! ur_isTrue(a1) )
    {
        UCell* bc = a2;
        if( ur_is(bc, UT_BLOCK) )
            return boron_doBlock( ut, bc, res );
        *res = *bc;
        return UR_OK;
    }
    ur_setId(res, UT_NONE);
    return UR_OK;
}


/*-cf-
    either
        test    Test condition.
        body-t  block!  Code to evaluate when true.
        body-f  block!  Code to evaluate when false.
    return: result of body-t if exp is true, or body-f if it is false.
    group: control
    see: if, ifn
*/
CFUNC(cfunc_either)
{
    UCell* bc = ur_isTrue(a1) ? a2 : a3;
    if( ur_is(bc, UT_BLOCK) )
        return boron_doBlock( ut, bc, res );
    return ur_error( ut, UR_ERR_TYPE, "either expected block! body" );
}


/*-cf-
    while
        exp     block! Test condition.
        body    block! Code to evaluate.
    return: false
    group: control
    see: forever, if, loop

    Repeat body as long as exp is true.
*/
CFUNC(cfunc_while)
{
    UCell* body = a2;

    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "while expected block! expression" );
    if( ! ur_is(body, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "while expected block! body" );

    while( 1 )
    {
        if( ! boron_doBlock( ut, a1, res ) )
            return UR_THROW;
        if( ! ur_isTrue(res) )
            break;
        if( ! boron_doBlock( ut, body, res ) )
        {
            if( _catchThrownWord( ut, UR_ATOM_BREAK ) )
                break;
            return UR_THROW;
        }
    }
    return UR_OK;
}


/*-cf-
    forever
        body    block!  Code to evaluate.
    return: Result of body.
    group: control
    see: loop, while

    Repeat body until break or exception thrown.
*/
CFUNC(cfunc_forever)
{
    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "forever expected block! body" );
    while( 1 )
    {
        if( ! boron_doBlock( ut, a1, res ) )
        {
            if( _catchThrownWord( ut, UR_ATOM_BREAK ) )
                break;
            return UR_THROW;
        }
    }
    return UR_OK;
}


/*-cf-
    loop
        range   int!/block!
        body    block!
    return: Result of body.
    group: control
    see: forever, while

    Use 'break to terminate loop.
*/
CFUNC(cfunc_loop)
{
    UCell* body = a2;

    if( ! ur_is(body, UT_BLOCK) )
        return errorType( "loop expected block! body" );

    if( ur_is(a1, UT_INT) )
    {
        int n;
        for( n = ur_int(a1); n > 0; --n )
        {
            if( ! boron_doBlock( ut, body, res ) )
            {
                if( _catchThrownWord( ut, UR_ATOM_BREAK ) )
                    break;
                return UR_THROW;
            }
        }
        return UR_OK;
    }
    else if( ur_is(a1, UT_BLOCK) )
    {
        const UCell* cword = 0;
        int32_t n[3];   // First, last, & increment.

        n[0] = 1;
        n[1] = 0;
        n[2] = 1;

        {
        UBlockIter bi;
        int state = 0;

        ur_blkSlice( ut, &bi, a1 );
        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_WORD ) )
            {
                cword = bi.it;
            }
            else if( ur_is(bi.it, UT_INT ) )
            {
                if( state < 3 )
                    n[ state++ ] = ur_int(bi.it);
            }
            else
                errorType( "loop range values must be word!/int!" );
        }
        if( state == 1 )
        {
            n[1] = n[0];
            n[0] = 1;
        }
        }

        for( ; n[0] <= n[1]; n[0] += n[2] )
        {
            if( cword )
            {
                UCell* counter = ur_wordCellM( ut, cword );
                if( ! counter )
                    return UR_THROW;
                ur_setId(counter, UT_INT);
                ur_int(counter) = n[0];
            }
            if( ! boron_doBlock( ut, body, res ) )
            {
                if( _catchThrownWord( ut, UR_ATOM_BREAK ) )
                    break;
                return UR_THROW;
            }
        }
        return UR_OK;
    }
    return errorType( "loop expected int!/block! range" );
}


/*-cf-
    select
        series
        match
        /last   Search from end of series.
        /case   Case of characters in strings must match
    return: Value after match or none! if match not found.
    group: data
*/
CFUNC(cfunc_select)
{
#define OPT_SELECT_LAST 0x01
#define OPT_SELECT_CASE 0x02
    USeriesIter si;
    const USeriesType* dt;
    int type = ur_type(a1);
    int n = 0;

    if( ! ur_isSeriesType( type ) )
        return errorType( "select expected series" );

    if( CFUNC_OPTIONS & OPT_SELECT_LAST )
        n |= UR_FIND_LAST;
    if( CFUNC_OPTIONS & OPT_SELECT_CASE )
        n |= UR_FIND_CASE;

    ur_seriesSlice( ut, &si, a1 );
    dt = SERIES_DT( type );
    n = dt->find( ut, &si, a2, n );
    ++n;
    if( n > 0 && n < si.end )
        dt->pick( si.buf, n, res );
    else
        ur_setId(res, UT_NONE);
    return UR_OK;
}


/*-cf-
    switch
        value
        options block!
    return: Result of selected switch case.
    group: control

    If the size of the options block is odd, then the last value will be
    the default result.
*/
CFUNC(cfunc_switch)
{
    UBlockIter bi;
    const UCell* found = 0;

    if( ! ur_is(a2, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "switch expected block! body" );

    ur_blkSlice( ut, &bi, a2 );
    if( (bi.end - bi.it) & 1 )
        found = --bi.end;       // Default option

    for( ; bi.it != bi.end; bi.it += 2 )
    {
        if( ur_equal( ut, a1, bi.it ) )
        {
            found = bi.it + 1;
            break;
        }
    }

    if( found )
    {
        if( ur_is(found, UT_BLOCK) || ur_is(found, UT_PAREN) )
            return boron_doBlock( ut, found, res );

        *res = *found;
        return UR_OK;
    }

    ur_setId(res, UT_NONE);
    return UR_OK;
}


/*-cf-
    case
        options block!
    return: Result of value following the first true case.
    group: control
*/
CFUNC(cfunc_case)
{
    UCell bc2;
    const UCell* found;

    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "case expected block! body" );

    ur_setId( &bc2, UT_BLOCK );
    bc2.series.buf = a1->series.buf;
    bc2.series.it  = 0;
    bc2.series.end = ur_bufferSer( a1 )->used;

    while( bc2.series.it < bc2.series.end )
    {
        if( ! boron_eval1( ut, &bc2, res ) )
            return UR_THROW;
        if( ur_isTrue( res ) )
        {
            if( bc2.series.it >= bc2.series.end )
                break;
            found = ur_bufferSer( a1 )->ptr.cell + bc2.series.it;
            if( ur_is(found, UT_BLOCK) || ur_is(found, UT_PAREN) )
                return boron_doBlock( ut, found, res );

            *res = *found;
            return UR_OK;
        }
        else
        {
            ++bc2.series.it;
        }
    }

    ur_setId(res, UT_NONE);
    return UR_OK;
}


extern void coord_pick( const UCell* cell, int index, UCell* res );
extern void vec3_pick ( const UCell* cell, int index, UCell* res );
#define ORD_ERR_MSG "%s expected series/coord!/vec3!"


/*-cf-
    first
        series  series/coord!/vec3!
    return: First item in series or none!.
    group: series
    see: last, second, third
*/
CFUNC(cfunc_first)
{
    int type = ur_type(a1);
    if( ur_isSeriesType( type ) )
        SERIES_DT( type )->pick( ur_bufferSer(a1), a1->series.it, res );
    else if( type == UT_COORD )
        coord_pick( a1, 0, res );
    else if( type == UT_VEC3 )
        vec3_pick( a1, 0, res );
    else
        return ur_error( ut, UR_ERR_TYPE, ORD_ERR_MSG, "first" );
    return UR_OK;
}


/*-cf-
    second
        series  series/coord!/vec3!
    return: Second item in series or none!.
    group: series
    see: first, third
*/
CFUNC(cfunc_second)
{
    int type = ur_type(a1);
    if( ur_isSeriesType( type ) )
        SERIES_DT( type )->pick( ur_bufferSer(a1), a1->series.it + 1, res );
    else if( type == UT_COORD )
        coord_pick( a1, 1, res );
    else if( type == UT_VEC3 )
        vec3_pick( a1, 1, res );
    else
        return ur_error( ut, UR_ERR_TYPE, ORD_ERR_MSG, "second" );
    return UR_OK;
}


/*-cf-
    third
        series  series/coord!/vec3!
    return: Third item in series or none!.
    group: series
    see: first, second
*/
CFUNC(cfunc_third)
{
    int type = ur_type(a1);
    if( ur_isSeriesType( type ) )
        SERIES_DT( type )->pick( ur_bufferSer(a1), a1->series.it + 2, res );
    else if( type == UT_COORD )
        coord_pick( a1, 2, res );
    else if( type == UT_VEC3 )
        vec3_pick( a1, 2, res );
    else
        return ur_error( ut, UR_ERR_TYPE, ORD_ERR_MSG, "third" );
    return UR_OK;
}


/*-cf-
    last
        series
    return: Last item in series or none! if empty.
    group: series
    see: first
*/
CFUNC(cfunc_last)
{
    USeriesIter si;
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) )
        return errorType( "last expected series" );
    ur_seriesSlice( ut, &si, a1 );
    if( si.it == si.end )
        ur_setId(res, UT_NONE);
    else
        SERIES_DT( type )->pick( ur_bufferSer(a1), si.end - 1, res );
    return UR_OK;
}


/*-cf-
    ++
        'word word!
    return: Value before increment.
    group: series

    Increments series or number bound to word.
*/
CFUNC(cfunc_2plus)
{
    UCell* cell;

    if( ! ur_is(a1, UT_WORD) )
        return ur_error( ut, UR_ERR_TYPE, "++ expected word!" );

    if( ! (cell = ur_wordCellM(ut, a1)) )
        return UR_THROW;
    *res = *cell;

    if( ur_isSeriesType( ur_type(cell) ) )
    {
        if( cell->series.it < boron_seriesEnd(ut, cell) )
            ++cell->series.it;
    }
    else if( ur_is(cell, UT_INT) )
        ur_int(cell) += 1;
    else if( ur_is(cell, UT_DECIMAL) )
        ur_decimal(cell) += 1.0;
    else
        return ur_error( ut, UR_ERR_TYPE, "++ only works on series or number" );

    return UR_OK;
}


/*-cf-
    --
        'word word!
    return: Value before decrement.
    group: series

    Decrements series or number bound to word.
*/
CFUNC(cfunc_2minus)
{
    UCell* cell;

    if( ! ur_is(a1, UT_WORD) )
        return ur_error( ut, UR_ERR_TYPE, "-- expected word!" );

    if( ! (cell = ur_wordCellM(ut, a1)) )
        return UR_THROW;
    *res = *cell;

    if( ur_isSeriesType( ur_type(cell) ) )
    {
        if( cell->series.it > 0 )
            --cell->series.it;
    }
    else if( ur_is(cell, UT_INT) )
        ur_int(cell) -= 1;
    else if( ur_is(cell, UT_DECIMAL) )
        ur_decimal(cell) -= 1.0;
    else
        return ur_error( ut, UR_ERR_TYPE, "-- only works on series or number" );

    return UR_OK;
}


#if 0
/*- cf -
    rot
        value   int!/coord!/vec3! or series
        step    int!
    return: Rotated value.
    group: series

    Rotate components of value.
    If step is positive then components are moved up and those at the end
    are moved to the start.
    If step is negative then components are moved dowan and those at the start
    are moved to the end.
    For integers this does a bitwise shift (positive step is left).
*/
/*
    rot cycle shift
*/
CFUNC(cfunc_rot)
{
    static const uint8_t _coordRotOff[] = { 0,0,2,5,9,14,20 };
    static const uint8_t _coordRot[] = {
        0,1,
        0,1,2,
        0,1,2,3,
        0,1,2,3,4,
        0,1,2,3,4,5,0,1,2,3,4,5
    };
    int rot;
    int type = ur_type(a1);

    if( ! ur_is(a2, UT_INT) )
        return ur_error( ut, UR_ERR_TYPE, "rot expected int! step" );
    rot = ur_int(a2);

    if( ur_isSeriesType( type ) )
    {
        UIndex size;

        *res = *a1;
        size = boron_seriesEnd(ut, res);
        if( rot > 0 )
            res->series.it = (res->series.it + rot) % size;
        else
            res->series.it = (res->series.it + rot) % size;
    }
    else if( type == UT_INT )
    {
        //ur_setId(res, UT_INT);
        res->id = a1->id;   // Preserve hex flag.
        if( rot >= 0 )
            ur_int(res) = ur_int(a1) << rot;
        else
            ur_int(res) = ur_int(a1) >> -rot;
    }
    else if( type == UT_COORD )
    {
        int len = a1->coord.len;
        const uint8_t* index;
        int i;

        rot %= len;
        if( rot < 0 )
            rot += len;
        index = _coordRot + _coordRotOff[len] - rot;

        ur_setId(res, UT_COORD);
        res->coord.len = len;
        for( i = 0; i < len; ++i )
            res->coord.n[i] = a1->coord.n[ *index++ ];
    }
    else if( type == UT_VEC3 )
    {
        int i;
        const uint8_t* index;

        rot %= 3;
        if( rot < 0 )
            rot += 3;
        index = _coordRot + 5 - rot;

        ur_setId(res, UT_VEC3);
        for( i = 0; i < 3; ++i )
            res->vec3.xyz[i] = a1->vec3.xyz[ *index++ ];
    }
    else
        return ur_error( ut, UR_ERR_TYPE,
                         "rotd expected int!/coord!/vec3! or series" );

    return UR_OK;
}
#endif


/*-cf-
    prev
        series
    return: Previous element of series or the head.
    group: series
    see: head, next, skip
*/
CFUNC(cfunc_prev)
{
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "prev expected series" );
    *res = *a1;
    if( res->series.it > 0 )
        --res->series.it;
    return UR_OK;
}


/*-cf-
    next
        series
    return: Next element of series or the tail.
    group: series
    see: prev, tail, skip
*/
CFUNC(cfunc_next)
{
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "next expected series" );
    *res = *a1;
    if( res->series.it < boron_seriesEnd(ut, res) )
        ++res->series.it;
    return UR_OK;
}


static int positionPort( UThread* ut, const UCell* portC, int where )
{
    UCell tmp;
    PORT_SITE(dev, pbuf, portC);
    if( ! dev )
        return errorScript( "port is closed" );
    ur_setId( &tmp, UT_INT );
    ur_int( &tmp ) = 0;
    return dev->seek( ut, pbuf, &tmp, where );
}


/*-cf-
    head
        series  Series or port!
    return: Start of series.
    group: series
    see: head?, skip, tail

    For seekable ports, head re-positions it to the start.
*/
CFUNC(cfunc_head)
{
    int type = ur_type(a1);
    *res = *a1;
    if( ur_isSeriesType( type ) )
    {
        res->series.it = 0;
        return UR_OK;
    }
    else if( type == UT_PORT )
    {
        return positionPort( ut, a1, UR_PORT_HEAD );
    }
    return errorType( "head expected series or port!" );
}


/*-cf-
    tail
        series  Series or port!
    return: End of series.
    group: series
    see: head, skip, tail?

    For seekable ports, tail re-positions it to the end.
*/
CFUNC(cfunc_tail)
{
    int type = ur_type(a1);
    *res = *a1;
    if( ur_isSeriesType( type ) )
    {
        res->series.it = boron_seriesEnd(ut, res);
        return UR_OK;
    }
    else if( type == UT_PORT )
    {
        return positionPort( ut, a1, UR_PORT_TAIL );
    }
    return errorType( "tail expected series or port!" );
}


/*-cf-
    pick
        series      Series or coord!/vec3!
        position    int!/logic!
    return: Value at position or none! if position is out of range.
    group: series
    see: index?, poke

    Note that series use one-based indexing.

    If position is a logic! value, then true will return the first series
    value, and false the second.
*/
CFUNC(cfunc_pick)
{
    UIndex n;
    int type;

    if( ur_is(a2, UT_INT) )
    {
        n = ur_int(a2);
        if( n > 0 )
            --n;
        else if( ! n )
        {
            ur_setId(res, UT_NONE);
            return UR_OK;
        }
    }
    else if( ur_is(a2, UT_LOGIC) )
        n = ur_int(a2) ? 0 : 1;
    else
        return errorType( "pick expected logic!/int! position" );

    type = ur_type(a1);
    if( ur_isSeriesType( type ) )
        SERIES_DT( type )->pick( ur_bufferSer(a1), a1->series.it + n, res );
    else if( type == UT_VEC3 )
        vec3_pick( a1, n, res );
    else if( type == UT_COORD )
        coord_pick( a1, n, res );
    else
        return errorType( "pick expected series/coord!/vec3!" );
    return UR_OK;
}


extern int coord_poke( UThread*, UCell* cell, int index, const UCell* src );
extern int vec3_poke ( UThread*, UCell* cell, int index, const UCell* src );

/*-cf-
    poke
        series      series/coord!/vec3!
        position    int!/logic!
        value
    return: series.
    group: series
    see: index?, pick

    Note that series use one-based indexing.

    If position is a logic! value, then true will set the first series
    value, and false the second.
*/
CFUNC(cfunc_poke)
{
    UBuffer* buf;
    UIndex n;
    int type;

    if( ur_is(a2, UT_INT) )
    {
        n = ur_int(a2);
        if( n > 0 )
            --n;
        else if( ! n )
            return errorScript( "poke position out of range" );
    }
    else if( ur_is(a2, UT_LOGIC) )
        n = ur_int(a2) ? 0 : 1;
    else
        return errorType( "poke expected logic!/int! position" );

    *res = *a1;
    type = ur_type(a1);
    if( ur_isSeriesType( type ) )
    {
        if( ! (buf = ur_bufferSerM(a1)) )
            return UR_THROW;
        SERIES_DT( type )->poke( buf, a1->series.it + n, a3 );
        return UR_OK;
    }
    else if( type == UT_VEC3 )
        return vec3_poke( ut, res, n, a3 );
    else if( type == UT_COORD )
        return coord_poke( ut, res, n, a3 );
    return errorType( "poke expected series/coord!/vec3!" );
}


/*-cf-
    pop
        series
    return: Last item of series or none! if empty.
    group: series

    Removes last item from series and returns it.
*/
CFUNC(cfunc_pop)
{
    USeriesIterM si;
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) )
        return errorType( "pop expected series" );
    if( ! ur_seriesSliceM( ut, &si, a1 ) )
        return UR_THROW;
    if( si.it == si.end )
    {
        ur_setId(res, UT_NONE);
    }
    else
    {
        const USeriesType* dt = SERIES_DT( type );
        si.it = si.end - 1;
        dt->pick( si.buf, si.it, res );
        dt->remove( ut, &si, 0 );
    }
    return UR_OK;
}


/*-cf-
    skip
        series  Series or port!
        offset  logic!/int!
        /wrap   Cycle around to other end when new position is out of range.
    return: Offset series.
    group: series

    If offset is a logic! type then the series will move to the next element
    if its value is true.
*/
CFUNC(cfunc_skip)
{
#define OPT_SKIP_WRAP   0x01
    if( ur_isSeriesType( ur_type(a1) ) )
    {
        UIndex n;
        UIndex end;

        if( ur_is(a2, UT_INT) )
            n = ur_int(a2);
        else if( ur_is(a2, UT_LOGIC) )
            n = ur_int(a2) ? 1 : 0;
        else
            return errorType( "skip expected logic!/int! offset" );

        *res = *a1;
        if( n )
        {
            n += a1->series.it;
            if( n < 0 )
            {
                if( (CFUNC_OPTIONS & OPT_SKIP_WRAP) &&
                    (end = boron_seriesEnd( ut, a1 )) )
                {
                    do
                        n += end;
                    while( n < 0 );
                }
                else
                    n = 0;
            }
            else
            {
                end = boron_seriesEnd( ut, a1 );
                if( (CFUNC_OPTIONS & OPT_SKIP_WRAP) && end )
                {
                    while( n >= end )
                        n -= end;
                }
                else if( n > end )
                    n = end;
            }
            res->series.it = n;
        }
        return UR_OK;
    }
    else if( ur_is(a1, UT_PORT) )
    {
        PORT_SITE(dev, pbuf, a1);
        if( ! dev )
            return errorScript( "port is closed" );
        *res = *a1;
        return dev->seek( ut, pbuf, a2, UR_PORT_SKIP );
    }
    return errorType( "skip expected series/port!" );
}


/*-cf-
    append
        series      Series or context!
        value       Data to append.
        /block      If series and value are blocks, push value as a single item.
        /repeat     Repeat append.
            count   int!
    return: Modified series or bound word!.
    group: series
    see: remove, terminate

    Add data to end of series.

    Examples:
        append "apple" 's'
        == "apples"

        append/repeat #{0000} 0xf6 4
        == #{0000F6F6F6F6}

        dat: [a b]
        append dat [1 2]
        == [a b 1 2]
        append/block dat [3 4]
        == [a b 1 2 [3 4]]
*/
CFUNC(cfunc_append)
{
#define OPT_APPEND_BLOCK    0x01
#define OPT_APPEND_REPEAT   0x02
    UBuffer* buf;
    const USeriesType* dt;
    uint32_t opt;
    int type = ur_type(a1);
    int count;

    if( ur_isSeriesType( type ) )
    {
        if( ! (buf = ur_bufferSerM(a1)) )
            return UR_THROW;

        dt = SERIES_DT( type );
        if( (opt = CFUNC_OPTIONS) )
        {
            count = (opt & OPT_APPEND_REPEAT) ? ur_int(a3) : 1;
            if( (opt & OPT_APPEND_BLOCK) && (type == UT_BLOCK) )
            {
                while( --count >= 0 )
                    ur_blkPush( buf, a2 );
            }
            else
            {
                while( --count >= 0 )
                {
                    if( ! dt->append( ut, buf, a2 ) )
                        return UR_THROW;
                }
            }
        }
        else
        {
            if( ! dt->append( ut, buf, a2 ) )
                return UR_THROW;
        }

        *res = *a1;
        return UR_OK;
    }
    else if( type == UT_CONTEXT )
    {
        if( ! (buf = ur_bufferSerM(a1)) )
            return UR_THROW;

        type = ur_type(a2);
        if( ur_isWordType( type ) )
        {
            UIndex n = a1->series.buf;
            ur_setId(res, UT_WORD);
            ur_setBinding( res, ur_isShared(n) ? UR_BIND_ENV : UR_BIND_THREAD );
            res->word.ctx   = n;
            res->word.index = ur_ctxAddWordI( buf, ur_atom(a2) );
            return UR_OK;
        }
#if 0
        else if( type == UT_BLOCK )
        {
            UBlockIter bi;
            ur_blkSlice( ut, &bi, a2 );
            ur_ctxSetWords( buf, bi.it, bi.end );
            *res = *a2;
            return UR_OK;
        }
#endif
        return errorType( "append context! expected word!" );
    }
    return errorType( "append expected series or context!" );
}


/*-cf-
    insert
        series
        value
        /block      Insert block value as a single item.
        /part       Insert only a limited number of elements from value.
            limit
        /repeat     Repeat insertion.
            count   int!
    return: Modified series.
    group: series
*/
CFUNC(cfunc_insert)
{
#define OPT_INSERT_BLOCK    0x01
#define OPT_INSERT_PART     0x02
#define OPT_INSERT_REPEAT   0x04
    UBuffer* buf;
    const USeriesType* dt;
    uint32_t opt;
    UIndex part = INT32_MAX;
    int type = ur_type(a1);
    int count = 1;

    if( ! ur_isSeriesType( type ) )
        return errorType( "insert expected series" );

    if( ! (buf = ur_bufferSerM(a1)) )
        return UR_THROW;

    if( (opt = CFUNC_OPTIONS) )
    {
        if( opt & OPT_INSERT_REPEAT )
           count = ur_int(a1 + 3);

        if( (opt & OPT_INSERT_BLOCK) && (type == UT_BLOCK) )
        {
            while( --count >= 0 )
                ur_blkInsert( buf, a1->series.it, a2, 1 );
            goto done;
        }
        else if( opt & OPT_INSERT_PART )
        {
            UCell* parg = a3;
            if( ur_is(parg, UT_INT) )
                part = ur_int(parg);
            else if( ur_isSeriesType( ur_type(parg) ) )
                part = parg->series.it - a1->series.it;
            else
                return ur_error( ut, UR_ERR_TYPE,
                                 "insert /part expected series or int!" );
            if( part < 1 )
                goto done;
        }
    }

    dt = SERIES_DT( type );
    while( --count >= 0 )
    {
        if( ! dt->insert( ut, buf, a1->series.it, a2, part ) )
            return UR_THROW;
    }
done:
    *res = *a1;
    return UR_OK;
}


/*-cf-
    change
        series
        replacement
        /slice          Remove slice and insert replacement.
        /part           Remove to limit and insert replacement.
            limit       Series or int!
    return: Series at end of change.
    group: series
*/
CFUNC(cfunc_change)
{
#define OPT_CHANGE_SLICE    0x01
#define OPT_CHANGE_PART     0x02
    USeriesIterM si;
    UIndex part = 0;
    int type = ur_type(a1);
    uint32_t opt = CFUNC_OPTIONS;

    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "change expected series" );
    if( ! ur_seriesSliceM( ut, &si, a1 ) )
        return UR_THROW;

    if( (opt & OPT_CHANGE_SLICE) /*&& ur_isSliced(a1)*/ )
    {
        part = si.end - si.it;
    }
    else if( opt & OPT_CHANGE_PART )
    {
        UCell* parg = a3;
        if( ur_is(parg, UT_INT) )
            part = ur_int(parg);
        else if( ur_isSeriesType( ur_type(parg) ) )
            part = parg->series.it - si.it;
        else
            return ur_error( ut, UR_ERR_TYPE,
                             "change /part expected series or int!" );
    }

    if( ! SERIES_DT( type )->change( ut, &si, a2, part ) )
        return UR_THROW;

    *res = *a1;
    res->series.it = si.it;
    return UR_OK;
}


/*-cf-
    remove
        series      series or none!
        /slice      Remove to end of slice.
        /part       Remove more than one element.
            number  int!
    return: series or none!
    group: series
    see: append, clear, remove-each

    Remove element at series position.
*/
CFUNC(cfunc_remove)
{
#define OPT_REMOVE_SLICE    0x01
#define OPT_REMOVE_PART     0x02
    USeriesIterM si;
    uint32_t opt = CFUNC_OPTIONS;
    int part = 0;
    int type = ur_type(a1);

    if( ! ur_isSeriesType( type ) )
    {
        if( ur_is(a1, UT_NONE) )
        {
            ur_setId(res, UT_NONE);
            return UR_OK;
        }
        return ur_error( ut, UR_ERR_TYPE, "remove expected series or none!" );
    }
    if( ! ur_seriesSliceM( ut, &si, a1 ) )
        return UR_THROW;

    if( opt & OPT_REMOVE_PART )
    {
        if( ! ur_is(a2, UT_INT) )
            return ur_error( ut, UR_ERR_TYPE, "remove expected int! part" );
        part = ur_int(a2);
    }
    else if( opt & OPT_REMOVE_SLICE )
    {
        part = si.end - si.it;
    }

    SERIES_DT( type )->remove( ut, &si, part );
    *res = *a1;
    return UR_OK;
}


/*-cf-
    reverse
        series
        /part   Limit change to part of series.
            number  int!
    return: series
    group: series
    see: swap

    Reverse the order of elements in a series.
*/
CFUNC(cfunc_reverse)
{
#define OPT_REVERSE_PART     0x01
    USeriesIterM si;
    int part;
    int type = ur_type(a1);

    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "reverse expected series" );
    if( ! ur_seriesSliceM( ut, &si, a1 ) )
        return UR_THROW;

    if( CFUNC_OPTIONS & OPT_REVERSE_PART )
    {
        if( ! ur_is(a2, UT_INT) )
            return ur_error( ut, UR_ERR_TYPE, "reverse expected int! part" );
        part = ur_int(a2);
        if( part < 1 )
            goto done;
        if( part < (si.end - si.it) )
            si.end = si.it + part;
    }

    SERIES_DT( type )->reverse( &si );
done:
    *res = *a1;
    return UR_OK;
}


/*-cf-
    find
        series
        value       Element or pattern to search for.
        /last       Search from end of series.
        /case       Case of characters in strings must match.
        /part       Restrict search to part of series.
            limit   series/int!
    return: Position of value in series or none!.
    group: series
*/
CFUNC(cfunc_find)
{
#define OPT_FIND_LAST   UR_FIND_LAST
#define OPT_FIND_CASE   UR_FIND_CASE
#define OPT_FIND_PART   0x04
    USeriesIter si;
    UIndex i;
    uint32_t opt = CFUNC_OPTIONS;
    int type = ur_type(a1);

    if( ! ur_isSeriesType( type ) )
        return errorType( "find expected series" );
    ur_seriesSlice( ut, &si, a1 );

    if( opt & OPT_FIND_PART )
    {
        UIndex part;
        UCell* parg = a3;

        if( ur_is(parg, UT_INT) )
            part = ur_int(parg);
        else if( ur_isSeriesType( ur_type(parg) ) )
            part = parg->series.it - si.it;
        else
            return errorType( "find /part expected series or int!" );

        if( part < 1 )
            goto set_none;
        part += si.it;
        if( part < si.end )
            si.end = part;
    }

    i = SERIES_DT( type )->find( ut, &si, a2, opt );
    if( i < 0 )
    {
set_none:
        ur_setId(res, UT_NONE);
    }
    else
    {
        *res = *a1;
        res->series.it = i;
    }
    return UR_OK;
}


/*-cf-
    clear
        series  series or none!
    return: Empty series or none!.
    group: series
    see: remove

    Erase to end of series.
*/
CFUNC(cfunc_clear)
{
    UBuffer* buf;

    if( ! ur_isSeriesType( ur_type(a1) ) )
    {
        if( ur_is(a1, UT_NONE) )
        {
            ur_setId(res, UT_NONE);
            return UR_OK;
        }
        return ur_error( ut, UR_ERR_TYPE, "clear expected series or none!" );
    }
    if( ! (buf = ur_bufferSerM(a1)) )
        return UR_THROW;

    // Limit erase to slice end?
    if( a1->series.it < buf->used )
        buf->used = a1->series.it;

    *res = *a1;
    return UR_OK;
}


extern void coord_slice( const UCell* cell, int index, int count, UCell* res );

/*-cf-
    slice
        start   Series or coord!
        limit   Series or none!/int!/coord!
    return: Start with adjusted end.
    group: series
    see: skip

    Slice gives a series an end position.

    A positive integer limit value sets the length of the slice.
    If limit is negative, then that number of elements (negated) are removed
    from the end.

        slice "There and back" 5
        == "There"
        slice "There and back" -5
        == "There and"

    A coord! limit value will modify both the start and the end.  The start
    will be adjusted by the first coord! number (like skip). The second
    coord! number will set the length or remove from the end (if negative).

        slice "There and back" 6,3
        == "and"
        slice "There and back" 6,-3
        == "and b"

    If limit is from the the same series as start, then the end is simply
    set to the limit start position.

        f: %my_song.mp3
        slice f find f '.'
        == %my_song

    Passing a none! limit removes any slice end and returns an un-sliced series.
*/
CFUNC(cfunc_slice)
{
    const UBuffer* buf;
    const UCell* limit = a2;
    int end;

    if( ! ur_isSeriesType( ur_type(a1) ) )
    {
        if( ur_is(a1, UT_COORD) )
        {
            if( ur_is(limit, UT_INT) )
            {
                end = ur_int(limit);
                if( end < 0 )
                    end += a1->coord.len;
                coord_slice( a1, 0, end, res );
            }
            else if( ur_is(limit, UT_COORD) )
                coord_slice( a1, limit->coord.n[0], limit->coord.n[1], res );
            else if( ur_is(limit, UT_NONE) )
                *res = *a1;
            else
                goto bad_limit;
            return UR_OK;
        }
        return ur_error( ut, UR_ERR_TYPE, "slice expected series or coord!" );
    }

    buf = ur_bufferSer(a1);
    *res = *a1;

    if( ur_is(limit, UT_NONE) )
    {
        res->series.it  = 0;
        res->series.end = -1;
    }
    else if( ur_is(limit, UT_INT) )
    {
        end = ur_int(limit);
set_end:
        if( end < 0 )
        {
            res->series.end = (res->series.end < 0) ? buf->used + end
                                                    : res->series.end + end;
            if( res->series.end < res->series.it )
                res->series.end = res->series.it;
        }
        else
        {
            res->series.end = res->series.it + end;
            if( res->series.end >= buf->used )
                res->series.end = -1;
        }
    }
    else if( ur_is(limit, UT_COORD) )
    {
        res->series.it += limit->coord.n[0];
        if( res->series.it < 0 )
            res->series.it = 0;
        end = limit->coord.n[1];
        goto set_end;
    }
    else if( ur_type(a1) == ur_type(a2) )
    {
        if( limit->series.it < res->series.it )
            res->series.end = res->series.it;
        else
            res->series.end = limit->series.it;
    }
    else
    {
bad_limit:
        return ur_error( ut, UR_ERR_TYPE,
                         "slice expected none!/int!/coord! limit" );
    }
    return UR_OK;
}


/*-cf-
    empty?
        value       series or none!
    return: logic!
    group: series

    Return true if the size of a series is zero, its position is out of range,
    or the value is none!.
*/
CFUNC(cfunc_emptyQ)
{
    USeriesIter si;

    if( ! ur_isSeriesType( ur_type(a1) ) )
    {
        if( ur_is(a1, UT_NONE) )
        {
            si.it = 1;
            goto set_logic;
        }
        return ur_error( ut, UR_ERR_TYPE, "empty? expected series or none!" );
    }

    ur_seriesSlice( ut, &si, a1 );
    si.it = (si.it == si.end) ? 1 : 0;

set_logic:
    ur_setId( res, UT_LOGIC );
    ur_int(res) = si.it;
    return UR_OK;
}


/*-cf-
    head?
        series
    return: logic!  True if position is at the start.
    group: series
    see: head, tail?

    Test if the series position is at the start.
*/
CFUNC(cfunc_headQ)
{
    if( ! ur_isSeriesType( ur_type(a1) ) )
        return ur_error( ut, UR_ERR_TYPE, "head? expected series" );

    ur_setId( res, UT_LOGIC );
    ur_int(res) = (a1->series.it < 1) ? 1 : 0;
    return UR_OK;
}


/*-cf-
    size?
        series
    return: int!
    group: series
    see: index?

    Length of series from current position to end.
*/
CFUNC(cfunc_sizeQ)
{
    int len;
    if( ur_isSeriesType( ur_type(a1) ) )
    {
        USeriesIter si;
        ur_seriesSlice( ut, &si, a1 );
        len = si.end - si.it;
    }
    else if( ur_is(a1, UT_COORD) )
        len = a1->coord.len;
    else
        return ur_error( ut, UR_ERR_TYPE, "size? expected series or coord!" );

    ur_setId( res, UT_INT );
    ur_int(res) = len;
    return UR_OK;
}


/*-cf-
    index?
        series  Series or word.
    return: int!
    group: series
    see: size?

    Current position of series.
*/
CFUNC(cfunc_indexQ)
{
    int type = ur_type(a1);

    if( ur_isSeriesType( type ) )
        ur_int(res) = a1->series.it + 1;
    else if( ur_isWordType( type ) )
        ur_int(res) = ur_atom(a1);
    //else if( type == UT_PORT )
    else
        return errorType( "index? expected series or word" );

    ur_setId( res, UT_INT );
    return UR_OK;
}


/*-cf-
    series?
        value
    return: True if value is a series type.
    group: data, series
*/
CFUNC(cfunc_seriesQ)
{
    (void) ut;
    ur_setId( res, UT_LOGIC );
    ur_int(res) = ur_isSeriesType( ur_type(a1) ) ? 1 : 0;
    return UR_OK;
}


/*-cf-
    any-block?
        value
    return: True if value is a block type.
    group: data

    Test if value is one of: block!/paren!/path!/lit-path!/set-path!
*/
CFUNC(cfunc_any_blockQ)
{
    (void) ut;
    ur_setId( res, UT_LOGIC );
    ur_int(res) = ur_isBlockType( ur_type(a1) ) ? 1 : 0;
    return UR_OK;
}


/*-cf-
    any-word?
        value
    return: True if value is a word type.
    group: data

    Test if value is one of: word!/lit-word!/set-word!/get-word!/option!
*/
CFUNC(cfunc_any_wordQ)
{
    (void) ut;
    ur_setId( res, UT_LOGIC );
    ur_int(res) = ur_isWordType( ur_type(a1) ) ? 1 : 0;
    return UR_OK;
}


/*-cf-
    complement
        value   logic!/char!/int!/binary!/bitset!
    return: Complemented value.
    group: data
    see: negate, not
*/
CFUNC(cfunc_complement)
{
    *res = *a1;
    switch( ur_type(a1) )
    {
        case UT_LOGIC:
            ur_int(res) ^= 1;
            break;

        case UT_CHAR:
        case UT_INT:
            ur_int(res) = ~ur_int(a1);
            break;

        case UT_BINARY:
        case UT_BITSET:
        {
            UBinaryIterM bi;
            if( ! ur_binSliceM( ut, &bi, res ) )
                return UR_THROW;
            ur_foreach( bi )
            {
                *bi.it = ~(*bi.it);
            }
        }
            break;

        default:
            return ur_error( ut, UR_ERR_TYPE,
                    "complement expected logic!/char!/int!/binary!/bitset!" );
    }
    return UR_OK;
}


/*-cf-
    negate
        value   int!/decimal!/time!/bignum!/coord!/vec3!/bitset!
    return: Negated value.
    group: data
    see: complement, not
*/
CFUNC( cfunc_negate )
{
    int type = ur_type(a1);
    switch( type )
    {
        case UT_INT:
            ur_setId(res, type);
            ur_int(res) = -ur_int(a1);
            break;

        case UT_DECIMAL:
        case UT_TIME:
            ur_setId(res, type);
            ur_decimal(res) = -ur_decimal(a1);
            break;

        case UT_BIGNUM:
            // Must setId before bignum call (clears first Limb).
            ur_setId(res, type);
            bignum_negate( a1, res );
            break;

        case UT_COORD:
        {
            int16_t* it  = a1->coord.n;
            int16_t* end = it + a1->coord.len;
            int16_t* dst = res->coord.n;
            while( it != end )
            {
                *dst++ = -*it;
                ++it;
            }
            ur_setId(res, type);
            res->coord.len = a1->coord.len;
        }
            break;

        case UT_VEC3:
            ur_setId(res, type);
            res->vec3.xyz[0] = -a1->vec3.xyz[0];
            res->vec3.xyz[1] = -a1->vec3.xyz[1];
            res->vec3.xyz[2] = -a1->vec3.xyz[2];
            break;

        case UT_BITSET:
            return cfunc_complement( ut, a1, res );

        default:
            return ur_error( ut, UR_ERR_TYPE,
                             "negate expected number or bitset!" );
    }
    return UR_OK;
}


enum SetOperation
{
    SET_OP_INTERSECT,
    SET_OP_DIFF,
    SET_OP_UNION
};


static int set_relation( UThread* ut, const UCell* a1, UCell* res,
                         enum SetOperation op )
{
    USeriesIter si;
    const USeriesType* dt;
    const UCell* argB = a2;
    int type = ur_type(a1);

    if( type != ur_type(argB) )
        return ur_error( ut, UR_ERR_TYPE,
                 "intersect/difference expected series of the same type" );

    dt = SERIES_DT( type );

    if( ur_isBlockType(type) )
    {
        UBlockIter bi;
        UBuffer* blk = ur_makeBlockCell( ut, type, 0, res );

        ur_blkSlice( ut, &bi, a1 );

        switch( op )
        {
            case SET_OP_INTERSECT:
            {
                USeriesIter ri;

                ur_seriesSlice( ut, &si, argB );

                ri.buf = blk;
                ri.it = ri.end = 0;

                ur_foreach( bi )
                {
                    if( (dt->find( ut, &si, bi.it, 0 ) > -1) &&
                        (dt->find( ut, &ri, bi.it, 0 ) == -1) )
                    {
                        ur_blkPush( blk, bi.it );
                        ++ri.end;
                    }
                }
            }
            break;

            case SET_OP_DIFF:
                ur_seriesSlice( ut, &si, argB );

                ur_foreach( bi )
                {
                    if( dt->find( ut, &si, bi.it, 0 ) < 0 )
                        ur_blkPush( blk, bi.it );
                }
                break;

            case SET_OP_UNION:
            {
                si.buf = blk;
                si.it = si.end = 0;
union_loop:
                ur_foreach( bi )
                {
                    if( dt->find( ut, &si, bi.it, 0 ) < 0 )
                    {
                        ur_blkPush( blk, bi.it );
                        ++si.end;
                    }
                }

                if( type )
                {
                    type = 0;
                    ur_blkSlice( ut, &bi, argB );
                    goto union_loop;
                }
            }
                break;
        }
    }
    else
    {
        return ur_error( ut, UR_ERR_INTERNAL,
                         "FIXME: set_relation only supports block!" );
    }

    return UR_OK;
}


/*-cf-
    intersect
        setA    series
        setB    series
    return: New series that contains only the elements common to both sets.
    group: series
    see: difference, union
*/
CFUNC(cfunc_intersect)
{
    return set_relation( ut, a1, res, SET_OP_INTERSECT );
}


/*-cf-
    difference
        setA    series
        setB    series
    return: New series that contains the elements of setA which are not in setB.
    group: series
    see: intersect, union

    This function generates the set-theoretic difference, not the symmetric
    difference (the elements unique to both sets).
*/
CFUNC(cfunc_difference)
{
    return set_relation( ut, a1, res, SET_OP_DIFF );
}


/*-cf-
    union
        setA    series
        setB    series
    return: New series that contains the distinct elements of both sets.
    group: series
    see: difference, intersect
*/
CFUNC(cfunc_union)
{
    return set_relation( ut, a1, res, SET_OP_UNION );
}


static inline UIndex _sliceEnd( const UBuffer* buf, const UCell* cell )
{
    if( cell->series.end > -1 && cell->series.end < buf->used )
        return cell->series.end;
    return buf->used;
}


/*-cf-
    foreach
        'words  word!/block!  Value of element(s).
        series
        body    block!  Code to evaluate for each element.
    return: Result of body.
    group: control
    see: forall

    Iterate over each element of a series.
*/
/*-cf-
    remove-each
        'words  word!/block!  Value of element(s).
        series
        body    block!  Code to evaluate for each element.
    return: Result of body.
    group: control
    see: remove

    Remove elements when result of body is true.

    Example:
        items: [1 5 2 3]
        remove-each i items [gt? i 2]
        == true
        probe items
        == [1 2]
*/
CFUNC(cfunc_foreach)
{
    const USeriesType* dt;
    UCell* sarg = a2;
    UCell* body = a3;
    UCell* words;
    UCell* cell;
    USeriesIter si;
    UBlockIterM wi;
    int remove = ur_int(a1 + 3);


    // TODO: Handle custom series type.
    if( ! ur_isSeriesType( ur_type(a2) ) )
        return errorType( "foreach expected series" );
    if( ! ur_is(body, UT_BLOCK) )
        return errorType( "foreach expected block! body" );

    if( ur_is(a1, UT_WORD) )
    {
        words  = a1;
        wi.end = a1 + 1;
        goto loop;
    }
    else if( ur_is(a1, UT_BLOCK) )
    {
        if( ! ur_blkSliceM( ut, &wi, a1 ) ) // FIXME: Prevents shared foreach.
            return UR_THROW;
        words = wi.it;

        {
        UBuffer* localCtx;
        int wordsShared = 0;

        ur_foreach( wi )
        {
            if( ! ur_is(wi.it, UT_WORD) )
                return errorType( "foreach has non-words in word block" );
            if( ur_isShared(wi.it->word.ctx) )
                wordsShared = 1;
        }
        if( wordsShared )
        {
            if( ur_isShared(body->series.buf) )
                return errorScript( "foreach cannot bind shared body" );

            localCtx = ur_buffer(ur_makeContext( ut, wi.end - words ));
            wi.it = words;
            ur_foreach( wi )
                ur_ctxAddWordI( localCtx, ur_atom(wi.it) );
            ur_bind( ut, wi.buf, ur_ctxSort(localCtx), UR_BIND_THREAD );
            ur_bind( ut, ur_buffer(body->series.buf), localCtx, UR_BIND_THREAD );
        }
        }

        // Now that all validation is done the loop can finally begin.
        goto loop;
    }
    return errorType( "foreach expected word!/block! for words" );

loop:

    dt = SERIES_DT( ur_type(sarg) );
    if( remove )
    {
        remove = wi.end - words;
        if( ! ur_seriesSliceM( ut, (USeriesIterM*) &si, sarg ) )
            return UR_THROW;
    }
    else
    {
        ur_seriesSlice( ut, &si, sarg );
    }
    while( si.it < si.end )
    {
        wi.it = words;
        ur_foreach( wi )
        {
            if( ! (cell = ur_wordCellM(ut, wi.it)) )
                return UR_THROW;
            dt->pick( si.buf, si.it++, cell ); 
        }
        if( ! boron_doBlock( ut, body, res ) )
        {
            if( _catchThrownWord( ut, UR_ATOM_BREAK ) )
                break;
            return UR_THROW;
        }
        // Re-aquire buf & end.
        si.buf = ur_bufferSer( sarg );
        si.end = _sliceEnd( si.buf, sarg );

        if( remove && ur_isTrue(res) )
        {
            si.it  -= remove;
            si.end -= remove;
            dt->remove( ut, (USeriesIterM*) &si, remove );
        }
    }
    return UR_OK;
}


/*-cf-
    forall
        'ref    word!   Reference to series.
        body    block!  Code to evaluate for each element.
    return: Result of body.
    group: control
    see: foreach

    Iterate over each element of a series, changing the reference position. 

    Example:
        a: [1 2 3]
        forall a [probe a]

        [1 2 3]
        [2 3]
        [3]
*/
CFUNC(cfunc_forall)
{
    UCell* sarg;
    UCell* body = a2;
    USeriesIter si;
    int type;

    if( ! ur_is(body, UT_BLOCK) )
        return errorType( "forall expected block! body" );

    if( ur_is(a1, UT_WORD) )
    {
        if( ! (sarg = ur_wordCellM(ut, a1)) )
            return UR_THROW;
        type = ur_type(sarg);
        if( ! ur_isSeriesType( type ) )
            goto err;

        ur_seriesSlice( ut, &si, sarg );
        while( si.it < si.end )
        {
            if( ! boron_doBlock( ut, body, res ) )
            {
                if( _catchThrownWord( ut, UR_ATOM_BREAK ) )
                    break;
                return UR_THROW;
            }
            if( ! (sarg = ur_wordCellM(ut, a1)) )
                return UR_THROW;
            if( ur_type(sarg) != type )     // Checks if word cell changed.
                break;
            ++sarg->series.it;
            ur_seriesSlice( ut, &si, sarg );
        }
        return UR_OK;
    }
err:
    return errorType( "forall expected series word!" );
}


/*-cf-
    map
        'word   word!
        series
        body    block!
    return: Modified series
    group: series

    Replace each element of series with result of body.
    Use 'break in body to terminate mapping.
*/
CFUNC(cfunc_map)
{
    const USeriesType* dt;
    UCell* sarg = a2;
    UCell* body = a3;
    UCell* cell;
    USeriesIter si;


    if( ! ur_is(a1, UT_WORD) )
        return errorType( "map expected word!" );
    if( ! ur_isSeriesType( ur_type(sarg) ) )
        return errorType( "map expected series" );
    if( ur_isShared( sarg->series.buf ) )
        return errorType( "map cannot modify shared series" );
    if( ! ur_is(body, UT_BLOCK) )
        return errorType( "map expected block! body" );

    ur_seriesSlice( ut, &si, sarg );
    dt = SERIES_DT( ur_type(sarg) );

    ur_foreach( si )
    {
        if( ! (cell = ur_wordCellM(ut, a1)) )
            return UR_THROW;
        dt->pick( ur_bufferSer(sarg), si.it, cell ); 
        if( ! boron_doBlock( ut, body, res ) )
        {
            if( _catchThrownWord( ut, UR_ATOM_BREAK ) )
                break;
            return UR_THROW;
        }
        dt->poke( ur_bufferSerM(sarg), si.it, res ); 
    }
    *res = *sarg;
    return UR_OK;
}


/*-cf-
    all
        tests block!    Expressions to test.
    return: logic!
    group: control
    see: any

    Return true only when all expressions are true.
*/
CFUNC(cfunc_all)
{
    USeriesIter si;

    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "all expected block!" );

    ur_seriesSlice( ut, &si, a1 );
    if( si.it != si.end )
    {
        UCell ec2 = *a1;
        ec2.series.end = si.end;
        while( ec2.series.it < ec2.series.end )
        {
            if( ! boron_eval1( ut, &ec2, res ) )
                return UR_THROW;
            if( ! ur_isTrue( res ) )
                return UR_OK;
        }
    }

    ur_setId(res, UT_LOGIC);
    ur_int(res) = 1;
    return UR_OK;
}


/*-cf-
    any
        tests block!    Expressions to test.
    return: Result of first true test or false.
    group: control
    see: all

    Return true if any expressions are true.
*/
CFUNC(cfunc_any)
{
    USeriesIter si;

    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "any expected block!" );

    ur_seriesSlice( ut, &si, a1 );
    if( si.it != si.end )
    {
        UCell ec2 = *a1;
        ec2.series.end = si.end;
        while( ec2.series.it < ec2.series.end )
        {
            if( ! boron_eval1( ut, &ec2, res ) )
                return UR_THROW;
            if( ur_isTrue( res ) )
                return UR_OK;
        }
    }

    ur_setId(res, UT_LOGIC);
    ur_int(res) = 0;
    return UR_OK;
}


/*-cf-
    reduce
        value
    return: Reduced value.
    group: data

    If value is a block then a new block is created with values set to the
    evaluated results of the original.
*/
CFUNC(cfunc_reduce)
{
    if( ur_is(a1, UT_BLOCK) )
    {
        UBlockIter bi;
        int ok = UR_OK;

        ur_blkSlice( ut, &bi, a1 );
        if( bi.it == bi.end )
        {
            ur_makeBlockCell( ut, UT_BLOCK, 0, res );
        }
        else
        {
            UCell ec2;
            UCell* bres;
            UIndex n;

            ec2 = *a1;
            ec2.series.end = bi.end - bi.buf->ptr.cell;

            bres = ur_makeBlockCell(ut, UT_BLOCK, bi.buf->used, res)->ptr.cell;
            n = res->series.buf;

            while( ec2.series.it < ec2.series.end )
            {
                ++ur_buffer(n)->used;
                ur_setId( bres, UT_UNSET );
                if( ! (ok = boron_eval1( ut, &ec2, bres++ )) )
                    break;
            }
        }
        return ok;
    }
    else
    {
        *res = *a1;
        return UR_OK;
    }
}


/*-cf-
    mold
        value
        /contents   Omit the outer braces from block and context values.
    return: string!
    group: data
    see: to-text

    Convert value to its string form with datatype syntax features.
*/
CFUNC(cfunc_mold)
{
#define OPT_MOLD_CONTENTS   0x01
    UBuffer* buf;
    int enc, len;

    if( ur_isStringType( ur_type(a1) ) )
    {
        const UBuffer* str = ur_bufferSer(a1);
        enc = str->form;
        len = str->used + 2;
    }
    else
    {
        enc = UR_ENC_LATIN1;
        len = 0;
    }

    buf = ur_makeStringCell( ut, enc, len, res ),
    buf->flags |= UR_STRING_ENC_UP;
    ur_toStr( ut, a1, buf, (CFUNC_OPTIONS & OPT_MOLD_CONTENTS) ? -1 : 0 );
    buf->flags &= ~UR_STRING_ENC_UP;
    return UR_OK;
}


#ifdef __ANDROID__
#undef stdout
#define stdout  stderr
#endif

/*-cf-
    probe
        value
    return: value
    group: io
    see: mold, print

    Print value with its datatype syntax features.
*/
CFUNC(cfunc_probe)
{
    UBuffer str;

    ur_strInit( &str, UR_ENC_UTF8, 0 );
    ur_toStr( ut, a1, &str, 0 );
    ur_strTermNull( &str );
    fputs( str.ptr.c, stdout );
    putc( '\n', stdout );
    ur_strFree( &str );

    *res = *a1;
    return UR_OK;
}


/*-cf-
    prin
        value
    return: unset!
    group: io
    see: print

    Print reduced value without a trailing linefeed.
*/
CFUNC(cfunc_prin)
{
    if( cfunc_reduce( ut, a1, res ) )
    {
        UBuffer str;

        ur_strInit( &str, UR_ENC_UTF8, 0 );
        ur_toText( ut, res, &str );
        ur_strTermNull( &str );
        fputs( str.ptr.c, stdout );
        ur_strFree( &str );

        ur_setId( res, UT_UNSET );
        return UR_OK;
    }
    return UR_THROW;
}


/*-cf-
    print
        value
    return: unset!
    group: io
    see: prin, probe

    Print reduced value and a trailing linefeed.
*/
CFUNC(cfunc_print)
{
    if( cfunc_prin( ut, a1, res ) )
    {
        putc( '\n', stdout );
        return UR_OK;
    }
    return UR_THROW;
}


/*-cf-
    to-text
        value
    return: string!
    group: data
    see: mold

    Convert value to text without datatype syntax features.

    This example compares to-text with mold:
        values: ["str" 'c' [a b]]
        to-text values
        == "str c a b"
        mold values
        == {["str" 'c' [a b]]}
*/
CFUNC(cfunc_to_text)
{
    int enc = UR_ENC_LATIN1;
    int len = 0;

    if( ur_isStringType( ur_type(a1) ) )
    {
        const UBuffer* str = ur_bufferSer(a1);
        enc = str->form;
        len = str->used;
    }
    ur_toText( ut, a1, ur_makeStringCell( ut, enc, len, res ) );
    return UR_OK;
}


/*-cf-
    exists?
        path    file!/string!
    return: True if file or directory exists.
    group: os
    see: dir?, info?

    Test if path exists in the filesystem.
*/
/*-cf-
    dir?
        path    file!/string!
    return: logic! or none! if path does not exist.
    group: os
    see: exists?, info?

    Test if path is a directory.
*/
/*-cf-
    info?
        path    file!/string!
    return: block! of information or none! if path does not exist.
    group: os
    see: exists?, dir?

    Get information about a file.  The values returned are the file type,
    byte size, modification date, path, and permissions.

    Example:
        info? %Makefile
        == [file 9065 2013-10-13T17:15:51-07:00 %Makefile 7,5,5,0]
*/
CFUNC(cfunc_infoQ)
{
    static const char*   _infoName[3] = { "exists?", "dir?", "info?" };
    static const uint8_t _infoMask[3] =
    {
        FI_Type, FI_Type, FI_Size | FI_Time | FI_Type
    };
    static const char* _infoType[5] =
    {
        "file", "link", "dir", "socket", "other"
    };
    OSFileInfo info;
    int ok;
    int func = ur_int(a2);


    assert( func >= 0 && func <= 2 );
    if( ! ur_isStringType( ur_type(a1) ) )
    {
        return ur_error( ut, UR_ERR_TYPE, "%s expected file!/string!",
                         _infoName[ func ] );
    }
    ok = ur_fileInfo( boron_cpath(ut, a1, 0), &info, _infoMask[ func ] );
    switch( func )
    {
        case 0:
set_logic:
            ur_setId(res, UT_LOGIC);
            ur_int(res) = ok ? 1 : 0;
            return UR_OK;

        case 1:
            if( ok )
            {
                ok = (info.type == FI_Dir);
                goto set_logic;
            }
            break;

        case 2:
            if( ok )
            {
                const char* tn = _infoType[ info.type ];
                UCell* cell;
                UBuffer* blk = ur_makeBlockCell( ut, UT_BLOCK, 5, res );
                blk->used = 5;
                cell = blk->ptr.cell;

                ur_setId(cell, UT_WORD);
                ur_setWordUnbound( cell,
                                   ur_internAtom( ut, tn, tn + strLen(tn) ) );
                ++cell;

                if( info.size > INT32_MAX )
                {
                    ur_setId(cell, UT_BIGNUM);
                    bignum_setl( cell, info.size );
                }
                else
                {
                    ur_setId(cell, UT_INT);
                    ur_int(cell) = (int) info.size;
                }
                ++cell;

                ur_setId(cell, UT_DATE);
                ur_decimal(cell) = info.modified;
                ++cell;

                *cell++ = *a1;

                ur_setId(cell, UT_COORD);
                cell->coord.len = 4;
                memCpy( cell->coord.n, info.perm, sizeof(int16_t) * 4 );
                return UR_OK;
            }
            break;
    }
    ur_setId(res, UT_NONE);
    return UR_OK;
}


extern int ur_makeDir( UThread* ut, const char* path );

static int _makeDirParents( UThread* ut, char* path, char* end )
{
#define MAX_PATH_PARTS  16
    OSFileInfo info;
    uint16_t index[ MAX_PATH_PARTS ];
    char* it = path + 1;
    int parts = 0;
    int i = 0;

    while( it != end )
    {
        if( (*it == '/' || *it == '\\') && (it[-1] != ':') )
        {
            *it = '\0';
            if( parts >= MAX_PATH_PARTS )
                break;
            index[ parts++ ] = it - path;
        }
        ++it;
    }

    while( (i < parts) && ur_fileInfo( path, &info, FI_Type ) )
        path[ index[ i++ ] ] = '/';

    while( i < parts )
    {
        if( ! ur_makeDir( ut, path ) )
            return UR_THROW;
        path[ index[ i++ ] ] = '/';
    }
    return UR_OK;
}


/*-cf-
    make-dir
        dir file!/string!
        /all    Make any missing parent directories.
    return: unset!
    group: os
*/
CFUNC(cfunc_make_dir)
{
#define OPT_MAKE_DIR_ALL    0x01
    if( ur_isStringType( ur_type(a1) ) )
    {
        UBuffer* bin = ur_buffer( BT->tempN );
        char* path = boron_cpath( ut, a1, bin );

        if( ! boron_requestAccess( ut, "Make directory \"%s\"", path ) )
            return UR_THROW;

        if( CFUNC_OPTIONS & OPT_MAKE_DIR_ALL )
        {
            if( ! _makeDirParents( ut, path, path + bin->used ) )
                return UR_THROW;
        }
        if( ! ur_makeDir( ut, path ) )
            return UR_THROW;
        ur_setId(res, UT_UNSET);
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "make-dir expected file!/string!" );
}


#include <errno.h>

/*-cf-
    change-dir
        dir     string!/file!
    return: unset!
    group: os

    Set current working directory.
*/
CFUNC(cfunc_change_dir)
{
#ifdef _WIN32
#define chdir   _chdir
#endif
    if( ur_isStringType( ur_type(a1) ) )
    {
        if( chdir( boron_cstr(ut, a1, 0) ) == 0 )
        {
            ur_setId(res, UT_UNSET);
            return UR_OK;
        }
        return ur_error( ut, UR_ERR_ACCESS, strerror(errno) );
    }
    return ur_error( ut, UR_ERR_TYPE, "change-dir expected file!/string!" );
}


/*-cf-
    current-dir
    return: File! of current working directory.
    group: os
*/
CFUNC(cfunc_current_dir)
{
#ifdef _WIN32
#define getcwd  _getcwd
#define DIR_SLASH   '\\'
#else
#define DIR_SLASH   '/'
#endif
    UBuffer* str;
    (void) a1;

    str = ur_makeStringCell( ut, UR_ENC_LATIN1, 512, res );
    ur_type(res) = UT_FILE;
    if( getcwd( str->ptr.c, 512 ) )
    {
        str->used = strLen( str->ptr.c );
        if( str->ptr.c[ str->used - 1 ] != DIR_SLASH )
        {
            str->ptr.c[ str->used ] = DIR_SLASH;
            ++str->used;
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_ACCESS, strerror(errno) );
}


/*-cf-
    getenv
        val string!
    return: string! or none!
    group: os

    Get operating system environment variable.

    Example:
        getenv "PATH"
        == {/usr/local/bin:/usr/bin:/bin:/usr/games:/home/karl/bin}
*/
CFUNC(cfunc_getenv)
{
    if( ur_is(a1, UT_STRING) )
    {
        const char* cp = getenv( boron_cstr(ut, a1, 0) );
        if( cp )
        {
            UBuffer* str;
            int len = strLen(cp);

            str = ur_makeStringCell( ut, UR_ENC_LATIN1, len, res );
            memCpy( str->ptr.c, cp, len );
            str->used = len;
        }
        else
            ur_setId(res, UT_NONE);
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "getenv expected string!" );
}


/*-cf-
    open
        device  int!/string!/file!/block!
        /read   Read-only mode.
        /write  Write-only mode.
        /new    Create empty file.
        /nowait Non-blocking reads.
    return: port!
    group: io
    see: close

    Create port!.
*/
CFUNC(cfunc_open)
{
    if( ur_is(a1, UT_FILE) )
        return port_file.open( ut, &port_file, a1, CFUNC_OPTIONS, res );
    return port_makeOpt( ut, a1, CFUNC_OPTIONS, res );
}


#if 0
/*
    close
        port    port!
    return: unset!

    Destroy port!.
*/
CFUNC(cfunc_close)
{
    UBuffer* buf;
    if( ! ur_is(a1, UT_PORT) )
        return ur_error( ut, UR_ERR_TYPE, "close expected port!" );
    if( ! (buf = ur_bufferSerM(a1)) )
        return UR_THROW;
    DT( UT_PORT )->destroy( buf );
    ur_setId(res, UT_UNSET);
    return UR_OK;
}
#endif


#define OPT_READ_TEXT   0x01
#define OPT_READ_INTO   0x02
#define OPT_READ_APPEND 0x04
#define OPT_READ_PART   0x08

/*
  \param len   Default length.

  Return read length of /part or the greater of default size and the
  /into buffer available memory.  A /part less than zero returns zero.
  If an error is thrown -1 is returned.
*/
static int _readBuffer( UThread* ut, uint32_t opt, const UCell* a1,
                        UCell* res, int len )
{
    int n;

    if( opt & OPT_READ_PART )
    {
        n = ur_int(a1 + 3);
        len = (n > 1) ? n : 0;
    }

    if( opt & (OPT_READ_INTO | OPT_READ_APPEND) )
    {
        UBuffer* buf;
        const UCell* ic = a1 + 1;
        int type;
        int rlen;

        if( opt & OPT_READ_APPEND )
            ++ic;
        type = ur_type(ic);

        if( type == UT_BINARY || type == UT_STRING )
        {
            buf = ur_bufferSerM(ic);
            if( ! buf )
                return -1;
            if( opt & OPT_READ_INTO )
                buf->used = 0;
            rlen = len + buf->used;

            n = ur_testAvail( buf );
            if( n < rlen )
            {
                if( type == UT_STRING )
                {
                    if( ur_strIsUcs2( buf ) )
                    {
                        errorType( "cannot read /into UCS2 string!" );
                        return -1;
                    }
                    ur_arrReserve( buf, rlen );
                }
                else
                {
                    ur_binReserve( buf, rlen );
                }
                //printf( "KR resb len:%d used:%d %d->%d\n",
                //        len, buf->used, n, ur_avail( buf ) );
                n = ur_avail( buf );
            }

            if( ! (opt & OPT_READ_PART) )
            {
                len = n;
                if( opt & OPT_READ_APPEND )
                    len -= buf->used;
            }

            *res = *ic;
        }
        else
        {
            errorType( "read /into expected binary!/string! buffer" );
            return -1;
        }
    }
    else if( opt & OPT_READ_TEXT )
    {
        ur_makeStringCell( ut, UR_ENC_UTF8, len, res );
    }
    else
    {
        ur_makeBinaryCell( ut, len, res );
    }
    return len;
}


extern int ur_readDir( UThread*, const char* filename, UCell* res );

/*-cf-
    read
        source      file!/string!/port!
        /text       Read as text rather than binary.
        /into       Put data into existing buffer.
            buffer  binary!/string!
        /append     Append data to existing buffer.
            abuf    binary!/string!
        /part       Read a specific number of bytes.
            size    int!
    return: binary!/string!/block!/none!
    group: io
    see: load, write

    Read binary! or UTF-8 string!.  None is returned when nothing is read
    (e.g. if the end of a file is reached or a TCP socket is disconnected).

    When source is a file name the entire file will be read into memory
    unless /part is used.

    If the /text option is used or the /into buffer is a string! then the
    file is read as UTF-8 data and carriage returns are filtered on Windows.

    If source is a directory name then a block containing file names is
    returned.
*/
CFUNC(cfunc_read)
{
    const char* filename;
    OSFileInfo info;
    uint32_t opt = CFUNC_OPTIONS;
    int len;


    if( ur_is(a1, UT_PORT) )
    {
        PORT_SITE(dev, pbuf, a1);
        if( ! dev )
            return errorScript( "cannot read from closed port" );

        len = dev->defaultReadLen;
        if( len > 0 )
        {
            len = _readBuffer( ut, opt, a1, res, len ); // gc!
            if( len <= 0 )
                return (len < 0) ? UR_THROW : UR_OK;
            pbuf = ur_buffer( a1->port.buf );   // Re-aquire
        }

        return dev->read( ut, pbuf, res, len );
    }

    if( ! ur_isStringType( ur_type(a1) ) )
        return errorType( "read expected file!/string!/port! source" );

    filename = boron_cpath( ut, a1, 0 );

    if( ! ur_fileInfo( filename, &info, FI_Size | FI_Type ) )
        return ur_error( ut, UR_ERR_ACCESS,
                         "could not access file %s", filename );

    if( info.type == FI_Dir )
        return ur_readDir( ut, filename, res );

    len = _readBuffer( ut, opt, a1, res, (int) info.size ); // gc!
    if( len > 0 )
    {
        UBuffer* dest;
        const char* mode;
        FILE* fp;
        size_t n;

#ifdef _WIN32
        if( ur_is(res, UT_STRING) || (opt & OPT_READ_TEXT) )
            mode = "r";     // Read as text to filter carriage ret.
        else
#endif
            mode = "rb";

        fp = fopen( filename, mode );
        if( ! fp )
        {
            return ur_error( ut, UR_ERR_ACCESS,
                             "could not open file %s", filename );
        }
        dest = ur_buffer( res->series.buf );
        n = fread( dest->ptr.b + dest->used, 1, len, fp );
        if( n > 0 )
        {
            dest->used += n;
            if( dest->type == UT_STRING )
                ur_strFlatten( dest );
        }
        else if( ferror( fp ) )
        {
            fclose( fp );
            return ur_error( ut, UR_ERR_ACCESS, "fread error %s", filename );
        }
        else
        {
            ur_setId(res, UT_NONE);
        }
        fclose( fp );
    }
    else if( len < 0 )
        return UR_THROW;
    else
        ur_setId(res, UT_NONE);
    return UR_OK;
}


/*-cf-
    write
        dest    file!/string!/port!
        data    binary!/string!/context!
        /append
        /text   Emit new lines with carriage returns on Windows.
    return: unset!
    group: io
    see: read, save
*/
CFUNC(cfunc_write)
{
#define OPT_WRITE_APPEND    0x01
#define OPT_WRITE_TEXT      0x02
    const UCell* data = a2;

    if( ur_is(a1, UT_PORT) )
    {
        PORT_SITE(dev, pbuf, a1);
        if( ! dev )
            return errorScript( "cannot write to closed port" );
        return dev->write( ut, pbuf, data );
    }

    if( ! ur_isStringType( ur_type(a1) ) )
        return errorType( "write expected file!/string!/port! dest" );

    if( ur_is(data, UT_CONTEXT) )
    {
        UBuffer* str = ur_makeStringCell( ut, UR_ENC_UTF8, 0, res );
        ut->types[ UT_CONTEXT ]->toText( ut, data, str, 0 );
        data = res;
    }

    if( ur_is(data, UT_BINARY) || ur_is(data, UT_STRING) )
    {
        FILE* fp;
        const char* filename;
        const char* mode;
        USeriesIter si;
        UIndex size;
        size_t n;

        filename = boron_cstr( ut, a1, 0 );

        if( ! boron_requestAccess( ut, "Write file \"%s\"", filename ) )
            return UR_THROW;

        ur_seriesSlice( ut, &si, data );
        size = si.end - si.it;

        if( ur_is(data, UT_STRING) )
        {
            if( ur_strIsUcs2(si.buf) ||
                ((si.buf->form == UR_ENC_LATIN1) && ! ur_strIsAscii(si.buf)) )
            {
                // Convert to UTF-8.

                UIndex nn = ur_makeString( ut, UR_ENC_UTF8, 0 );
                // si.buf is invalid after make.
                si.buf = ur_buffer(nn);
                ur_strAppend( (UBuffer*) si.buf, ur_bufferSer(data),
                              si.it, si.end );
                si.it = 0;
                size = si.buf->used;
            }
        }

        n = CFUNC_OPTIONS;
        {
        int append = n & OPT_WRITE_APPEND;
        if( n & OPT_WRITE_TEXT )
            mode = append ? "a" : "w";
        else
            mode = append ? "ab" : "wb";
        }

        fp = fopen( filename, mode );
        if( ! fp )
        {
            return ur_error( ut, UR_ERR_ACCESS,
                             "could not open %s", filename );
        }

        n = fwrite( si.buf->ptr.b + si.it, 1, size, fp );
        fclose( fp );

        ur_setId(res, UT_UNSET);
        return UR_OK;
    }
    else
        return errorType( "write expected binary!/string!/context! data" );
}


/*-cf-
    delete
        file    file!/string!
    return: unset! or error thrown.
    group: os
*/
CFUNC(cfunc_delete)
{
    if( ur_isStringType( ur_type(a1) ) )
    {
        const char* fn;

        fn = boron_cstr(ut, a1, 0);
        if( ! boron_requestAccess( ut, "Delete file \"%s\"", fn ) )
            return UR_THROW;

        if( remove( fn ) != 0 )
            return ur_error( ut, UR_ERR_ACCESS, strerror(errno) );
        ur_setId(res, UT_UNSET);
        return UR_OK;
    }
    return errorType( "delete expected file!/string!" );
}


/*-cf-
    rename
        file        file!/string!
        new-name    file!/string!
    return: unset! or error thrown.
    group: os
*/
CFUNC(cfunc_rename)
{
    if( ur_isStringType( ur_type(a1) ) &&
        ur_isStringType( ur_type(a2) ) )
    {
        const char* cp1;
        const char* cp2;
        UBuffer* tmp;
        int ok;

        tmp = ur_buffer( ur_makeBinary( ut, 0 ) );

        cp1 = boron_cstr(ut, a1, 0);
        cp2 = boron_cstr(ut, a2, tmp);

        if( ! boron_requestAccess( ut, "Rename file \"%s\"", cp1 ) )
            return UR_THROW;

#ifdef _WIN32
        // We want Unix rename overwrite behavior.
        {
        OSFileInfo info;
        ok = ur_fileInfo( cp2, &info, FI_Type );
        if( ok && (info.type == FI_File) )
        {
            if( remove( cp2 ) == -1 )
                return ur_error( ut, UR_ERR_ACCESS, strerror(errno) );
        }
        }
#endif
        ok = rename( cp1, cp2 );
        if( ok != 0 )
            return ur_error( ut, UR_ERR_ACCESS, strerror(errno) );
        ur_setId(res, UT_UNSET);
        return UR_OK;
    }
    return errorType( "rename expected file!/string!" );
}


/*-cf-
    serialize
        data    block!
    return: binary!
    group: data
    see: unserialize

    Pack data into binary image for transport.
    Series positions, slices, and non-global word bindings are retained.
*/
CFUNC( cfunc_serialize )
{
    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "serialize expected block!" );
    return ur_serialize( ut, a1->series.buf, res );
}


/*-cf-
    unserialize
        data    binary!
    return: Re-materialized block!.
    group: data
    see: serialize
*/
CFUNC( cfunc_unserialize )
{
    UBinaryIter bi;
    if( ! ur_is(a1, UT_BINARY) )
        return ur_error( ut, UR_ERR_TYPE, "unserialize expected binary!" );
    ur_binSlice( ut, &bi, a1 );
    return ur_unserialize( ut, bi.it, bi.end, res );
}


extern int ur_serializedHeader( const uint8_t* data, int len );

/*-cf-
    load
        file    file!/string!/binary!
    return: block! or none! if file is empty.
    group: io
    see: read, save

    Load file or serialized data with default bindings.
*/
CFUNC(cfunc_load)
{
    if( ur_is(a1, UT_BINARY) )
    {
        if( cfunc_unserialize( ut, a1, res ) )
        {
bind_sb:
            boron_bindDefault( ut, res->series.buf );
            return UR_OK;
        }
    }
    else
    {
        UCell args[3];

        ur_setId(args, UT_LOGIC);
        OPT_BITS(args) = 0;             // Clear options.

        args[1] = *a1;

        ur_setId(args + 2, UT_NONE);

        if( cfunc_read( ut, args + 1, res ) )
        {
            const uint8_t* cp;
            UBuffer* bin;
            UIndex hold;
            UIndex blkN;
#if CONFIG_COMPRESS == 2
check_str:
#endif
            bin = ur_buffer( res->series.buf );
            if( ! bin->used )
            {
                ur_setId(res, UT_NONE);
                return UR_OK;
            }

            // Skip any Unix shell interpreter line.
            cp = bin->ptr.b;
            if( cp[0] == '#' && cp[1] == '!' ) 
            {
                cp = find_uint8_t( cp, cp + bin->used, '\n' );
                if( ! cp )
                    cp = bin->ptr.b;
            }
            else if( ur_serializedHeader( cp, bin->used ) )
            {
                if( ! ur_unserialize( ut, cp, cp + bin->used, res ) )
                    return UR_THROW;
                goto bind_sb;
            }
#if CONFIG_COMPRESS == 2
            else if( bin->used > (12 + 8) )
            {
                const uint8_t* pat = (const uint8_t*) "BZh";
                cp = find_pattern_uint8_t( cp, cp + 12, pat, pat + 3 );
                if( cp && (cp[3] >= '1') && (cp[3] <= '9') )
                {
                    *args = *res;
                    hold = ur_hold( res->series.buf );
                    blkN = cfunc_decompress( ut, args, res );
                    ur_release( hold );
                    if( ! blkN )
                        return UR_THROW;
                    goto check_str;
                }
                else
                    cp = bin->ptr.b;
            }
#endif

            hold = ur_hold( res->series.buf );
            blkN = ur_tokenize( ut, (char*) cp, bin->ptr.c + bin->used, res );
            ur_release( hold );

            if( blkN )
            {
                boron_bindDefault( ut, blkN );
                return UR_OK;
            }
        }
    }
    return UR_THROW;
}


/*-cf-
    save
        dest    file!/string!/port!
        data
    return: unset!
    group: io
    see: load, write

    Convert data to string! then write it.  Use 'load to restore the data.
*/
CFUNC(cfunc_save)
{
    UCell args[3];
    UBuffer* str;

    ur_setId(args, UT_LOGIC);
    OPT_BITS(args) = 0;             // Clear options.

    args[1] = *a1;

    str = ur_makeStringCell( ut, UR_ENC_UTF8, 0, res );
    ur_toStr( ut, a2, str, -1 );
    args[2] = *res;

    return cfunc_write( ut, args + 1, res );
}


extern int ur_parseBinary( UThread* ut, UBuffer*, UIndex start, UIndex end,
                           UIndex* parsePos, const UBuffer* ruleBlk,
                           int (*eval)( UThread*, const UCell* ) );

extern int ur_parseBlock( UThread* ut, UBuffer*, UIndex start, UIndex end,
                          UIndex* parsePos, const UBuffer* ruleBlk,
                          int (*eval)( UThread*, const UCell* ) );

extern int ur_parseString( UThread* ut, UBuffer*, UIndex start, UIndex end,
                           UIndex* parsePos, const UBuffer* ruleBlk,
                           int (*eval)( UThread*, const UCell* ), int );

/*-cf-
    parse
        input   string!/block!
        rules   block!
        /case   Character case must match when comparing strings.
    return:  True if end of input reached.
    group: data
*/
CFUNC(cfunc_parse)
{
#define OPT_PARSE_CASE  0x01
    if( ANY3(a1, UT_BINARY, UT_STRING, UT_BLOCK) &&
        ur_is(a2, UT_BLOCK) )
    {
        USeriesIterM si;
        const UBuffer* rules;
        UIndex pos;
        int ok = 0;

        if( ! ur_seriesSliceM( ut, &si, a1 ) )
            return UR_THROW;

        if( ! (rules = ur_bufferSer(a2)) )
            return UR_THROW;

        switch( ur_type(a1) )
        {
            case UT_BINARY:
                ok = ur_parseBinary( ut, si.buf, si.it, si.end, &pos, rules,
                                     boron_doVoid );
                break;
            case UT_STRING:
                ok = ur_parseString( ut, si.buf, si.it, si.end, &pos, rules,
                                     boron_doVoid,
                                     CFUNC_OPTIONS & OPT_PARSE_CASE );
                break;
            case UT_BLOCK:
                ok = ur_parseBlock( ut, si.buf, si.it, si.end, &pos, rules,
                                    boron_doVoid );
                break;
        }
        if( ! ok )
            return UR_THROW;

        if( ur_isSliced(a1) )
        {
            // Result for a slice may be wrong if the series size gets changed.
            pos = (pos == si.end);
        }
        else
        {
            // Re-aquire.
            if( ! (si.buf = ur_bufferSerM(a1)) )
                return UR_THROW;
            // Pos can be greater than used if input was erased.
            pos = (pos >= si.buf->used);
        }

        ur_setId(res, UT_LOGIC);
        ur_int(res) = pos ? 1 : 0;
        return UR_OK;
    }

    return ur_error( ut, UR_ERR_TYPE,
                     "parse expected input block!/string! and rule block!" );
}


/*-cf-
    same?
        a
        b
    return: True if two values are identical.
    group: data
    see: equal?
*/
CFUNC(cfunc_sameQ)
{
    ur_setId(res, UT_LOGIC);
    ur_int(res) = ur_same( ut, a1, a2 ) ? 1 : 0;
    return UR_OK;
}


/*-cf-
    equal?
        a
        b
    return: True if two values are equivalent.
    group: data, math
    see: eq?, ne?, lt?, gt?, same?
*/
CFUNC(cfunc_equalQ)
{
    ur_setId(res, UT_LOGIC);
    ur_int(res) = ur_equal( ut, a1, a2 ) ? 1 : 0;
    return UR_OK;
}


/*-cf-
    ne?
        a
        b
    return: True if two values are not equivalent.
    group: data, math
    see: eq?, gt?, lt?
*/
CFUNC(cfunc_neQ)
{
    ur_setId(res, UT_LOGIC);
    ur_int(res) = ur_equal( ut, a1, a2 ) ? 0 : 1;
    return UR_OK;
}


/*-cf-
    gt?
        a
        b
    return: True if first value is greater than the second.
    group: math
    see: eq?, lt?, ne?
*/
CFUNC(cfunc_gtQ)
{
    ur_setId(res, UT_LOGIC);
    ur_int(res) = (ur_compare( ut, a1, a2 ) > 0) ? 1 : 0;
    return UR_OK;
}


/*-cf-
    lt?
        a
        b
    return: True if first value is less than the second.
    group: math
    see: eq?, gt?, ne?
*/
CFUNC(cfunc_ltQ)
{
    ur_setId(res, UT_LOGIC);
    ur_int(res) = (ur_compare( ut, a1, a2 ) < 0) ? 1 : 0;
    return UR_OK;
}


/*-cf-
    zero?
        value   int!/char!/decimal!
    return: logic!
    group: math

    Return true if value is the number zero.
*/
CFUNC(cfunc_zeroQ)
{
    int logic;
    (void) ut;

    if( ur_is(a1, UT_INT) || ur_is(a1, UT_CHAR) )
        logic = ur_int(a1) ? 0 : 1; 
    else if( ur_is(a1, UT_DECIMAL) )
        logic = ur_decimal(a1) ? 0 : 1; 
    else
        logic = 0;

    ur_setId(res, UT_LOGIC);
    ur_int(res) = logic;
    return UR_OK;
}


/*-cf-
    type?
        value
    return: Datatype of value.
    group: data
    see: to-_type_

    Determine type of value.
*/
CFUNC(cfunc_typeQ)
{
    (void) ut;
    ur_makeDatatype( res, ur_type(a1) );
    return UR_OK;
}


/*-cf-
    swap
        data    binary!
        /group  Specify number of elements to reverse
            size    int!
    return: Modified data.
    group: series
    see: reverse

    Swap adjacent elements of a series.
*/
CFUNC(cfunc_swap)
{
#define OPT_SWAP_GROUP  0x01
//#define OPT_SWAP_ORDER  0x02    /order block   block!
    if( ur_is(a1, UT_BINARY) )
    {
        USeriesIterM si;
        if( ! ur_seriesSliceM( ut, &si, a1 ) )
            return UR_THROW;

        if( CFUNC_OPTIONS & OPT_SWAP_GROUP )
        {
            uint8_t* bp;
            int group = ur_int(a2);
            if( group < 2 || group > (si.end - si.it) )
                return ur_error( ut, UR_ERR_SCRIPT,
                                 "swap group size (%d) is invalid", group );
            bp = si.buf->ptr.b + si.it;
            si.it += group;
            for( ; si.it <= si.end; si.it += group, bp += group )
                reverse_uint8_t( bp, bp + group );
        }
        else
        {
            uint8_t* bp;
            uint8_t* bend;
            int tmp;
            if( (si.end - si.it) & 1 )
                --si.end;
            bp = si.buf->ptr.b + si.it;
            bend = si.buf->ptr.b + si.end;
            for( ; bp != bend; bp += 2 )
            {
                tmp = bp[0];
                bp[0] = bp[1];
                bp[1] = tmp;
            }
        }

        *res = *a1;
        return UR_OK;
    }
    return errorType( "swap expected binary!" );
}


/*-cf-
    lowercase
        value   char!/string!/file!
    return: Value converted to lowercase.
    group: series
    see: uppercase
*/
CFUNC(cfunc_lowercase)
{
    if( ur_isStringType( ur_type(a1) ) )
    {
        USeriesIterM si;
        if( ! ur_seriesSliceM( ut, &si, a1 ) )
            return UR_THROW;
        *res = *a1;
        ur_strLowercase( si.buf, si.it, si.end );
        return UR_OK;
    }
    else if( ur_is(a1, UT_CHAR) )
    {
        ur_setId(res, UT_CHAR);
        ur_int(res) = ur_charLowercase( ur_int(a1) );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "lowercase expected char!/string!/file!" );
}


/*-cf-
    uppercase
        value   char!/string!/file!
    return: Value converted to uppercase.
    group: series
    see: lowercase
*/
CFUNC(cfunc_uppercase)
{
    if( ur_isStringType( ur_type(a1) ) )
    {
        USeriesIterM si;
        if( ! ur_seriesSliceM( ut, &si, a1 ) )
            return UR_THROW;
        *res = *a1;
        ur_strUppercase( si.buf, si.it, si.end );
        return UR_OK;
    }
    else if( ur_is(a1, UT_CHAR) )
    {
        ur_setId(res, UT_CHAR);
        ur_int(res) = ur_charUppercase( ur_int(a1) );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "uppercase expected char!/string!/file!" );
}


#define TRIM_FUNC_HEAD      trim_head_u16
#define TRIM_FUNC_TAIL      trim_tail_u16
#define TRIM_FUNC_LINES     trim_lines_u16
#define TRIM_FUNC_INDENT    trim_indent_u16
#define TRIM_T              uint16_t
#include "trim_string.c"

#define TRIM_FUNC_HEAD      trim_head_char
#define TRIM_FUNC_TAIL      trim_tail_char
#define TRIM_FUNC_LINES     trim_lines_char
#define TRIM_FUNC_INDENT    trim_indent_char
#define TRIM_T              char
#include "trim_string.c"


/*-cf-
    trim
        string  string!
        /indent   Remove same amount of whitespace from start of all lines.
        /lines    Remove all newlines and extra whitespace.
    return: Modified string.
    group: series
*/
CFUNC(cfunc_trim)
{
#define OPT_TRIM_INDENT 0x01
#define OPT_TRIM_LINES  0x02
    uint32_t opt = CFUNC_OPTIONS;

    if( ur_is(a1, UT_STRING) )
    {
        USeriesIterM si;
        UIndex origEnd;

        if( ! ur_seriesSliceM( ut, &si, a1 ) )
            return UR_THROW;

        origEnd = si.end;
        *res = *a1;

        //dprint( "KR trim %d\n", opt );
        if( opt & OPT_TRIM_INDENT )
        {
            if( ur_strIsUcs2(si.buf) )
            {
                uint16_t* ss = si.buf->ptr.u16;
                si.end -= trim_indent_u16( ss + si.it, ss + si.end );
            }
            else
            {
                char* ss = si.buf->ptr.c;
                si.end -= trim_indent_char( ss + si.it, ss + si.end );
            }
        }
        else if( opt & OPT_TRIM_LINES )
        {
            if( ur_strIsUcs2(si.buf) )
            {
                uint16_t* ss = si.buf->ptr.u16;
                si.it  += trim_head_u16 ( ss + si.it, ss + si.end );
                si.end -= trim_tail_u16 ( ss + si.it, ss + si.end );
                si.end -= trim_lines_u16( ss + si.it, ss + si.end );
            }
            else
            {
                char* ss = si.buf->ptr.c;
                si.it  += trim_head_char ( ss + si.it, ss + si.end );
                si.end -= trim_tail_char ( ss + si.it, ss + si.end );
                si.end -= trim_lines_char( ss + si.it, ss + si.end );
            }
        }
        else
        {
            if( ur_strIsUcs2(si.buf) )
            {
                uint16_t* ss = si.buf->ptr.u16;
                si.it  += trim_head_u16( ss + si.it, ss + si.end );
                si.end -= trim_tail_u16( ss + si.it, ss + si.end );
            }
            else
            {
                char* ss = si.buf->ptr.c;
                si.it  += trim_head_char( ss + si.it, ss + si.end );
                si.end -= trim_tail_char( ss + si.it, ss + si.end );
            }
        }

        res->series.it = si.it;
        if( si.end != origEnd )
        {
            if( ur_isSliced(res) )
            {
                // TODO: Erase from end.
                res->series.end = si.end;
            }
            else
                si.buf->used = si.end;
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "trim expected string!" );
}


/*-cf-
    terminate
        series  Series to append to.
        value   Value to append.
        /dir    Check if end is '/' or '\'.
    return: Modified series.
    group: series
    see: append

    Append value to series only if it does not already end with it.
*/
CFUNC(cfunc_terminate)
{
#define OPT_TERMINATE_DIR   0x01
    USeriesIterM si;
    const UCell* val = a2;
    int type = ur_type(a1);

    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "terminate expected series" );

    ur_seriesSliceM( ut, &si, a1 );
    if( si.it != si.end )
    {
        SERIES_DT( type )->pick( si.buf, si.end - 1, res );
        if( ur_equal( ut, val, res ) )
            goto done;
        if( CFUNC_OPTIONS & OPT_TERMINATE_DIR )
        {
            if( ur_is(res, UT_CHAR) &&
                (ur_int(res) == '/' || ur_int(res) == '\\') )
                goto done;
        }
    }
    if( ! SERIES_DT( type )->append( ut, si.buf, val ) )
        return UR_THROW;
done:
    *res = *a1;
    return UR_OK;
}


/*-cf-
    to-hex
        number  char!/int!/bignum!
    return: Number shown as hexidecimal.
    group: data
*/
CFUNC(cfunc_to_hex)
{
    if( ur_is(a1, UT_INT) || ur_is(a1, UT_BIGNUM) )
    {
        ur_setFlags(a1, UR_FLAG_INT_HEX);
        *res = *a1;
        return UR_OK;
    }
    else if( ur_is(a1, UT_CHAR) )
    {
        *res = *a1;
        ur_type(res) = UT_INT;
        ur_setFlags(res, UR_FLAG_INT_HEX);
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "to-hex expected char!/int!/bignum!" );
}


/*-cf-
    to-dec
        number  int!/bignum!
    return: Number shown as decimal.
    group: data
*/
CFUNC(cfunc_to_dec)
{
    if( ur_is(a1, UT_INT) || ur_is(a1, UT_BIGNUM) )
    {
        ur_clrFlags(a1, UR_FLAG_INT_HEX);
        *res = *a1;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "to-dec expected int!/bignum!" );
}


/*-cf-
    mark-sol
        value
        /block  Mark block rather than value at block position.
        /clear  Clear start of line flag.
    return: Value with flag set.
    group: data

    Flag value so that it is printed at the start of a line.
*/
CFUNC(cfunc_mark_sol)
{
#define OPT_MARK_SOL_BLOCK   0x01
#define OPT_MARK_SOL_CLEAR   0x02
    uint32_t opt = CFUNC_OPTIONS;
    int type = ur_type(a1);

    *res = *a1;

    if( ur_isBlockType( type ) && ! (opt & OPT_MARK_SOL_BLOCK) )
    {
        UBlockIterM bi;
        if( ! ur_blkSliceM( ut, &bi, a1 ) )
            return UR_THROW;
        if( bi.it == bi.end )
            return UR_OK;
        res = bi.it;
    }
    if( opt & OPT_MARK_SOL_CLEAR )
        ur_clrFlags(res, UR_FLAG_SOL);
    else
        ur_setFlags(res, UR_FLAG_SOL);
    return UR_OK;
}


extern double ur_now();

/*-cf-
    now
        /date   Return date! rather than time!
    return: time! or date!
    group: io
*/
CFUNC(cfunc_now)
{
#define OPT_NOW_DATE    0x01
    uint32_t opt = CFUNC_OPTIONS;
    (void) ut;

    if( opt & OPT_NOW_DATE )
        ur_setId(res, UT_DATE);
    else
        ur_setId(res, UT_TIME);
    ur_decimal(res) = ur_now();
    return UR_OK;
}


void ur_setCellU64( UCell* res, uint64_t n )
{
    if( n > 0x7fffffff )
    {
        ur_setId(res, UT_BIGNUM);
        bignum_setl(res, n);
    }
    else
    {
        ur_setId(res, UT_INT);
        ur_int(res) = (int32_t) n;
    }
}


#include "cpuCounter.h"

/*-cf-
    cpu-cycles
        loop    int!  Number of times to evaluate block.
        block   block!
    return: int!/bignum!
    group: io

    Get the number of CPU cycles used to evaluate a block.
*/
CFUNC(cfunc_cpu_cycles)
{
#ifdef HAVE_CPU_COUNTER
    uint64_t cycles = 0;
    uint64_t low = ~0L;
    int loop = ur_int(a1);

    // Signature verifies args.

    if( loop < 0 )
        loop = 1;
    while( loop-- )
    {
        cycles = cpuCounter();
        if( ! boron_doBlock( ut, a2, res ) )
            return UR_THROW;
        cycles = cpuCounter() - cycles;
        if( cycles < low )
            low = cycles;
    }

    ur_setCellU64( res, cycles );
    return UR_OK;
#else
    (void) a1;
    (void) res;
    return ur_error( ut, UR_ERR_INTERNAL,
                     "FIXME: cpu-cycles is not implemented on this system" );
#endif
}


/*-cf-
    free
        resource    series/port!
    return: unset!
    group: storage
    see: close, reserve

    Clear series and free its memory buffer or close port.
*/
CFUNC_PUB(cfunc_free)
{
    UBuffer* buf;
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) && (type != UT_PORT) )
        return errorType( "free expected series/port!" );
    if( ! (buf = ur_bufferSerM(a1)) )
        return UR_THROW;
    DT( type )->destroy( buf );
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


#ifdef CONFIG_CHECKSUM
extern uint32_t ur_hash( const uint8_t* str, const uint8_t* end );

/*-cf-
    hash
        string      word!/string!
    return: int!
    group: data

    Compute hash value from string (treated as lowercase).
*/
CFUNC(cfunc_hash)
{
    uint32_t hash = ur_type(a1);

    if( ur_isStringType(hash) )
    {
        USeriesIter si;
        ur_seriesSlice( ut, &si, a1 );
        if( ur_strIsUcs2( si.buf ) )
            return errorType( "FIXME: hash does not handle UCS2 strings" );
        hash = ur_hash( si.buf->ptr.b + si.it, si.buf->ptr.b + si.end );
    }
    else if( ur_isWordType(hash) )
    {
        const uint8_t* str = (const uint8_t*) ur_wordCStr(a1);
        hash = ur_hash( str, str + strLen((const char*) str) );
    }
    else
    {
        return errorType( "hash expected word!/string!" );
    }

    ur_setId(res, UT_INT);
    ur_setFlags(res, UR_FLAG_INT_HEX);
    ur_int(res) = hash;
    return UR_OK;
}
#endif


/*-cf-
    _datatype?_
        value
    return: True if value is a specific datatype.
    group: data
    see: to-_type_

    Each datatype has its own test function which is named the same as the
    type but ending with '?' rather than a '!'.

    This example shows testing for string! and int! types:
        string? 20
        == false
        int? 20
        == true
*/
CFUNC(cfunc_datatypeQ)
{
    (void) ut;
    // Type variation is in a2.
    ur_int(res) = (ur_type(a1) == ur_int(a2)) ? 1 : 0;
    ur_setId(res, UT_LOGIC);
    return UR_OK;
}


/*-cf-
    to-_type_
        value   Any value.
    return: New datatype!.
    group: data
    see: make, type?, _datatype?_

    Convert a value to another datatype.

    Each datatype has its own convert function which is named the same as the
    type but starting with "to-".
    For example, to convert a value to a string! use:
        to-string 20
        == "20"
*/
CFUNC(cfunc_to_type)
{
    // Type variation is in a2.
    return DT( ur_int(a2) )->convert( ut, a1, res );
}


/*-cf-
    collect
        types       datatype!
        source      block!/paren!
        /unique     Only add equal values once.
        /into       Add values to dest rather than a new block.
            dest    block!
    return: New block containing matching values.
    group: data

    Get all values of a certain type from source block.
*/
CFUNC(cfunc_collect)
{
#define OPT_COLLECT_UNIQUE  0x01
#define OPT_COLLECT_INTO    0x02
    UBuffer* dest;
    uint32_t opt = CFUNC_OPTIONS;

    if( opt & OPT_COLLECT_INTO )
    {
        if( ! (dest = ur_bufferSerM(a3)) )
            return UR_THROW;
        *res = *a3;
    }
    else
    {
        ur_makeBlockCell( ut, UT_BLOCK, 0, res );
        dest = ur_buffer(res->series.buf);
    }
    ur_blkCollectType( ut, a2, a1->datatype.mask0, dest,
                       opt & OPT_COLLECT_UNIQUE );
    return UR_OK;
}


//EOF
