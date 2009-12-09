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


#define a2  (a1 + 1)
#define a3  (a1 + 2)


#ifdef DEBUG
CFUNC(cfunc_nop)
{
    (void) ut;
    (void) a1;
    // Must set result to something; it may be uninitialized.
    ur_setId(res, UT_UNSET);
    return UR_OK;
}
#endif


/*-cf-
    quit
    return: NA

    Exit interpreter.
*/
CFUNC(cfunc_quit)
{
    (void) a1;
    (void) res;
    return boron_throwWord( ut, UR_ATOM_QUIT );
}


/*-cf-
    halt
    return: NA

    Halt interpreter.
*/
CFUNC(cfunc_halt)
{
    (void) a1;
    (void) res;
    return boron_throwWord( ut, UR_ATOM_HALT );
}


/*-cf-
    return
        result
    return: NA

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

    Exit from loop, while, foreach, or forall.
*/
CFUNC(cfunc_break)
{
    *res = *a1;
    return boron_throwWord( ut, UR_ATOM_BREAK );
}


/*-cf-
    throw
        value
    return: NA
*/
CFUNC(cfunc_throw)
{
    (void) res;
    ur_blkPush( ur_errorBlock(ut), a1 );
    return UR_THROW;
}


/*-cf-
    catch
        body block!
    return: Result of block evaluation or thrown value.
*/
CFUNC(cfunc_catch)
{
    int ok;

    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "catch expected block!" );

    if( ! (ok = boron_doBlock( ut, a1, res )) )
    {
        UCell* cell = ur_blkPop( ur_errorBlock(ut) );
        assert(cell);
        *res = *cell;
        ok = UR_OK;
    }
    return ok;
}


/*-cf-
    try
        body block!
    return: Result of block evaluation or error.

    Do body and catch any errors.
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
            *a1 = *res;
            if( ! cfunc_load( ut, a1, res ) )
                return UR_THROW;
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
        words   word!/lit-word!/block!
        values
    return: unset!

    If words and values are both a block! then each word in words is set
    to the corresponding value in values.
*/
CFUNC(cfunc_set)
{
    UCell* cell;
    if( ur_isWordType( ur_type(a1) ) )
    {
        if( ! (cell = ur_wordCellM(ut, a1)) )
            return UR_THROW;
        *cell = *a2;
        ur_setId(res, UT_UNSET);
        return UR_OK;
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
        ur_setId(res, UT_UNSET);
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "set expected word!/block!" );
}


/*-cf-
    get
        word    word!/context!
    return: Value of word or block of values in context.
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
    in
        context     context!
        word        word!
    return: Word bound to context or none!.
*/
CFUNC(cfunc_in)
{
    if( ur_is(a1, UT_CONTEXT) && ur_is(a2, UT_WORD) )
    {
        const UBuffer* ctx = ur_bufferSer( a1 );
        int wrdN = ur_ctxLookup( ctx, ur_atom(a2) );
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


/*-cf-
    bind
        words   word!/block!
        context word!/context!
    return: Bound words
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
                             ur_atomCStr(ut, ur_atom(ctxArg)) );
    }
    else if( ur_is(ctxArg, UT_CONTEXT) )
        ctxN = ctxArg->series.buf;
    else
        return errorType( "bind expected word!/context! for context" );

    if( ur_is(a1, UT_BLOCK) )
    {
        if( ! (blk = ur_bufferSerM(a1)) )
            return UR_THROW;

        ur_bind( ut, blk, ur_bufferSer(ctxArg),
                 ur_isShared(ctxN) ? UR_BIND_ENV : UR_BIND_THREAD );
        *res = *a1;
        return UR_OK;
    }
    else if( ur_is(a1, UT_WORD) )
    {
        UBindTarget bt;

        bt.ctx = ur_bufferSer(ctxArg);
        bt.ctxN = ctxN;
        bt.bindType = ur_isShared(ctxN) ? UR_BIND_ENV : UR_BIND_THREAD;

        *res = *a1;
        ur_bindCells( ut, res, res + 1, &bt );
        return UR_OK;
    }
    return errorType( "bind expected words argument of word!/block!" );
}


/*-cf-
    infuse
        block   block!
        context word!/context!
    return: Modified block.

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
                             ur_atomCStr(ut, ur_atom(ctxArg)) );
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


#define INT_MASK    ((1<<UT_CHAR) | (1<<UT_INT))
#define DEC_MASK    ((1<<UT_DECIMAL) | (1<<UT_TIME) | (1<<UT_DATE))
#define ur_isIntType(type)  ((1<<type) & INT_MASK)
#define ur_isDecType(type)  ((1<<type) & DEC_MASK)

#define MATH_FUNC(name,OP) \
 CFUNC(name) { \
    const UCell* ra = a2; \
    if( ur_isIntType(ur_type(a1)) ) { \
        if( ur_isIntType(ur_type(ra)) ) { \
            ur_setId(res, ur_type(a1)); \
            ur_int(res) = ur_int(a1) OP ur_int(ra); \
            return UR_OK; \
        } else if( ur_isDecType(ur_type(ra)) ) { \
            ur_setId(res, UT_DECIMAL); \
            ur_decimal(res) = ur_int(a1) OP ur_decimal(ra); \
            return UR_OK; \
        } \
    } \
    else if( ur_isDecType(ur_type(a1)) ) { \
        if( ur_isIntType(ur_type(ra)) ) { \
            ur_setId(res, UT_DECIMAL); \
            ur_decimal(res) = ur_decimal(a1) OP ur_int(ra); \
            return UR_OK; \
        } else if( ur_isDecType(ur_type(ra)) ) { \
            ur_setId(res, UT_DECIMAL); \
            ur_decimal(res) = ur_decimal(a1) OP ur_decimal(ra); \
            return UR_OK; \
        } \
    } \
    return ur_error( ut, UR_ERR_TYPE, "math operation expected numbers" ); \
}


/*-cf-
    add
        a   int!/decimal!
        b   int!/decimal!
    return: Sum of two numbers.
*/
/*-cf-
    sub
        a   int!/decimal!
        b   int!/decimal!
    return: Difference of two numbers.
*/
/*-cf-
    mul
        a   int!/decimal!
        b   int!/decimal!
    return: Product of two numbers.
*/
/*-cf-
    div
        a   int!/decimal!
        b   int!/decimal!
    return: Quotient of a divided by b.
*/
MATH_FUNC( cfunc_add, + )
MATH_FUNC( cfunc_sub, - )
MATH_FUNC( cfunc_mul, * )
MATH_FUNC( cfunc_div, / )


#define ANY3(c,t1,t2,t3)    ((1<<ur_type(c)) & ((1<<t1) | (1<<t2) | (1<<t3)))

#define BITWISE_FUNC(name,OP,msg) \
CFUNC(name) { \
    if( ANY3(a1, UT_LOGIC, UT_CHAR, UT_INT) && \
        ANY3(a2, UT_LOGIC, UT_CHAR, UT_INT) ) { \
        ur_setId(res, ur_type(a1)); \
        ur_int(res) = ur_int(a1) OP ur_int(a2); \
        return UR_OK; \
    } else \
        return ur_error(ut, UR_ERR_TYPE, "%s expected logic!/char!/int!", msg);\
}


/*-cf-
    and
        a   logic!/char!/int!
        b   logic!/char!/int!
    return: Bitwise AND.
*/
/*-cf-
    or
        a   logic!/char!/int!
        b   logic!/char!/int!
    return: Bitwise OR.
*/
/*-cf-
    xor
        a   logic!/char!/int!
        b   logic!/char!/int!
    return: Bitwise exclusive OR.
*/
BITWISE_FUNC( cfunc_and, &, "and" )
BITWISE_FUNC( cfunc_or,  |, "or")
BITWISE_FUNC( cfunc_xor, ^, "xor" )


/*-cf-
    minimum
        a
        b
    return: Lesser of two values.
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
*/
CFUNC(cfunc_maximum)
{
    *res = (ur_compare( ut, a1, a2 ) > 0) ? *a1 : *a2;
    return UR_OK;
}


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
            ur_bind( ut, bi.buf, ctx, UR_BIND_THREAD );
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
    does
        body  block!
    return: func!
    Create function which takes no arguments.
*/
CFUNC(cfunc_does)
{
    UCellFunc* cell = (UCellFunc*) res;
    (void) ut;

    ur_setId(cell, UT_FUNC);        // Sets argCount, localCount to 0.
    cell->argBufN   = UR_INVALID_BUF;
    cell->m.f.bodyN = a1->series.buf;
    cell->m.f.sigN  = UR_INVALID_BUF;
    return UR_OK;
}


/*-cf-
    func
        spec  block!
        body  block!
    return:  func!
    Create function.
*/
CFUNC(cfunc_func)
{
    UBuffer ctx;
    UBuffer optCtx;
    UBuffer* body;
    UCellFunc* fc = (UCellFunc*) res;

    if( ! ur_is(a1, UT_BLOCK) || ! ur_is(a2, UT_BLOCK) )
    {
        return ur_error( ut, UR_ERR_TYPE,
                         "func expected block! for spec & body" );
    }

    // Check that we can bind to body before we create temporary contexts.
    if( ! (body = ur_bufferSerM(a2)) )
        return UR_THROW;

    ur_ctxInit( &ctx, 0 );
    ur_ctxInit( &optCtx, 0 );

    ur_setId(fc, UT_FUNC);
    fc->argBufN   = UR_INVALID_BUF;
    fc->m.f.bodyN = a2->series.buf;
    fc->m.f.sigN  = a1->series.buf;

    // Assign after fc fully initialized to handle recycle.
    fc->argBufN = boron_makeArgProgram( ut, a1, &ctx, &optCtx, fc );

    {
    UBindTarget bt;

    bt.ctxN = fc->m.f.bodyN;
    body = ur_bufferSerM(a2);       // Re-aquire

    if( ctx.used )
    {
        bt.ctx      = &ctx;
        bt.bindType = UR_BIND_FUNC;
        ur_bindCells( ut, body->ptr.cell, body->ptr.cell + body->used, &bt );
    }

    if( optCtx.used )
    {
        bt.ctx      = &optCtx;
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
        exp
        body    block!
    return: Result of body if exp is true, or none if it is false.
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
        exp
        body    block!
    return: Result of body if exp is false, or none if it is true.

    This is the same as "if not exp body".
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
        exp
        body-t  block!
        body-f  block!
    return: result of body-t if exp is true, or body-f if it is false.
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
        exp     block!
        body    block!
    return: false

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
    loop
        range   int!/block!
        body    block!
    return: Result of body.

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
        options block!
        val
    return: Value after val or none! if val not found.
*/
CFUNC(cfunc_select)
{
    UBlockIter bi;
    const UCell* val = a2;

    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "select expected options block!" );

    ur_blkSlice( ut, &bi, a1 );
    ur_foreach( bi )
    {
        if( ur_equal( ut, val, bi.it ) )
        {
            if( ++bi.it == bi.end )
                break;
            *res = *bi.it;
            return UR_OK;
        }
    }

    ur_setId(res, UT_NONE);
    return UR_OK;
}


/*-cf-
    switch
        value
        options block!
    return: Result of selected switch case.

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
    first
        series
    return: First item in series or none!.
*/
CFUNC(cfunc_first)
{
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "first expected series" );
    SERIES_DT( type )->pick( ur_bufferSer(a1), a1->series.it, res );
    return UR_OK;
}


/*-cf-
    second
        series
    return: Second item in series or none!.
*/
CFUNC(cfunc_second)
{
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "second expected series" );
    SERIES_DT( type )->pick( ur_bufferSer(a1), a1->series.it + 1, res );
    return UR_OK;
}


/*-cf-
    third
        series
    return: Third item in series or none!.
*/
CFUNC(cfunc_third)
{
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "third expected series" );
    SERIES_DT( type )->pick( ur_bufferSer(a1), a1->series.it + 2, res );
    return UR_OK;
}


/*-cf-
    last
        series
    return: Last item in series or none! if empty.
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
    return: Incremented value

    Increments series or number bound to word.
*/
CFUNC(cfunc_2plus)
{
    UCell* cell;

    if( ! ur_is(a1, UT_WORD) )
        return ur_error( ut, UR_ERR_TYPE, "++ expected word!" );
    if( ! (cell = ur_wordCellM(ut, a1)) )
        return UR_THROW;

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

    *res = *cell;
    return UR_OK;
}


/*-cf-
    --
        'word word!
    return: Decremented value

    Decrements series or number bound to word.
*/
CFUNC(cfunc_2minus)
{
    UCell* cell;

    if( ! ur_is(a1, UT_WORD) )
        return ur_error( ut, UR_ERR_TYPE, "-- expected word!" );
    if( ! (cell = ur_wordCellM(ut, a1)) )
        return UR_THROW;

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

    *res = *cell;
    return UR_OK;
}


/*-cf-
    prev
        series
    return: Previous element of series or the head.
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


/*-cf-
    head
        series
    return: Start of series.
*/
CFUNC(cfunc_head)
{
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "head expected series" );
    *res = *a1;
    res->series.it = 0;
    return UR_OK;
}


/*-cf-
    tail
        series
    return: End of series.
*/
CFUNC(cfunc_tail)
{
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "tail expected series" );
    *res = *a1;
    res->series.it = boron_seriesEnd(ut, res);
    return UR_OK;
}


/*-cf-
    pick
        series
        position    int!
    return: Value at position.
*/
CFUNC(cfunc_pick)
{
    UIndex n;
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "pick expected series" );

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
        return ur_error( ut, UR_ERR_TYPE, "pick expected logic!/int! position");

    SERIES_DT( type )->pick( ur_bufferSer(a1), a1->series.it + n, res );
    return UR_OK;
}


/*-cf-
    poke
        series
        position    int!
        value
    return: series.
*/
CFUNC(cfunc_poke)
{
    UBuffer* buf;
    UIndex n;
    int type = ur_type(a1);
    if( ! ur_isSeriesType( type ) )
        return ur_error( ut, UR_ERR_TYPE, "poke expected series" );

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
        return ur_error( ut, UR_ERR_TYPE, "poke expected logic!/int! position" );

    if( ! (buf = ur_bufferSerM(a1)) )
        return UR_THROW;

    SERIES_DT( type )->poke( buf, a1->series.it + n, a3 );
    *res = *a1;
    return UR_OK;
}


/*-cf-
    pop
        series
    return: Last item of series or none! if empty.

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
        series
        offset  logic!/int!
    return: Offset series.

    If offset is a logic! type then the series will move to the next element
    if its value is true.
*/
CFUNC(cfunc_skip)
{
    UIndex n;
    if( ! ur_isSeriesType( ur_type(a1) ) )
        return ur_error( ut, UR_ERR_TYPE, "skip expected series" );

    if( ur_is(a2, UT_INT) )
        n = ur_int(a2);
    else if( ur_is(a2, UT_LOGIC) )
        n = ur_int(a2) ? 1 : 0;
    else
        return ur_error( ut, UR_ERR_TYPE, "skip expected logic!/int! offset" );

    *res = *a1;
    if( n )
    {
        n += a1->series.it;
        if( n < 0 )
            n = 0;
        else
        {
            UIndex end = boron_seriesEnd( ut, a1 );
            if( n > end )
                n = end;
        }
        res->series.it = n;
    }
    return UR_OK;
}


/*-cf-
    append
        series  Series or context!
        value
        /block  If series and value are blocks, push value as a single item.
    return: Modified series or bound word!.
*/
CFUNC(cfunc_append)
{
#define OPT_APPEND_BLOCK    0x01
    UBuffer* buf;
    int type = ur_type(a1);

    if( ur_isSeriesType( type ) )
    {
        if( ! (buf = ur_bufferSerM(a1)) )
            return UR_THROW;

        if( (type == UT_BLOCK) && (CFUNC_OPTIONS & OPT_APPEND_BLOCK) )
            ur_blkPush( buf, a2 );
        else if( ! SERIES_DT( type )->append( ut, buf, a2 ) )
            return UR_THROW;
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
    change
        series
        replacement
        /slice          Remove slice and insert replacement.
        /part limit     Remove to limit and insert replacement.
    return: Series at end of change.
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
        /part
            number  int!
    return: series or none!

    Remove element at series position.
*/
CFUNC(cfunc_remove)
{
#define OPT_REMOVE_PART 0x01
    USeriesIterM si;
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

    if( CFUNC_OPTIONS & OPT_REMOVE_PART )
    {
        if( ! ur_is(a2, UT_INT) )
            return ur_error( ut, UR_ERR_TYPE, "remove expected int! part" );
        part = ur_int(a2);
    }

    SERIES_DT( type )->remove( ut, &si, part );
    *res = *a1;
    return UR_OK;
}


/*-cf-
    find
        series
        value
        /last
        /case   Case of characters in strings must match
        /part
            limit   series/int!
    return: Position of value in series or none!.
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


/*-cf-
    slice
        series  series
        limit   none!/int!/coord!
    return: Adjusted slice.
*/
CFUNC(cfunc_slice)
{
    const UBuffer* buf;
    const UCell* limit = a2;
    int end;

    if( ! ur_isSeriesType( ur_type(a1) ) )
        return ur_error( ut, UR_ERR_TYPE, "slice expected series" );

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
        return ur_error( ut, UR_ERR_TYPE,
                         "slice expected none!/int!/coord! limit" );
    }
    return UR_OK;
}


/*-cf-
    empty?
        series
    return: logic!
*/
CFUNC(cfunc_emptyQ)
{
    USeriesIter si;

    if( ! ur_isSeriesType( ur_type(a1) ) )
        return ur_error( ut, UR_ERR_TYPE, "empty? expected series" );

    ur_seriesSlice( ut, &si, a1 );
    ur_setId( res, UT_LOGIC );
    ur_int(res) = (si.it == si.end) ? 1 : 0;
    return UR_OK;
}


/*-cf-
    head?
        series
    return: logic!
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
*/
CFUNC(cfunc_indexQ)
{
    int type = ur_type(a1);

    if( ur_isSeriesType( type ) )
        ur_int(res) = a1->series.it + 1;
    else if( ur_isWordType( type ) )
        ur_int(res) = ur_atom(a1);
    else
        return errorType( "index? expected series or word" );

    ur_setId( res, UT_INT );
    return UR_OK;
}


/*-cf-
    series?
        value
    return: True if value is a series type.
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
    return: True if value is a block type.
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


static int set_relation( UThread* ut, const UCell* a1, UCell* res,
                         int intersect )
{
    USeriesIter si;
    const USeriesType* dt;
    const UCell* argB = a2;
    int type = ur_type(a1);

    if( type != ur_type(argB) )
        return ur_error( ut, UR_ERR_TYPE,
                 "intersect/difference expected series of the same type" );
    ur_seriesSlice( ut, &si, argB );

    dt = SERIES_DT( type );

    if( ur_isBlockType(type) )
    {
        UBlockIter bi;
        UBuffer* blk = ur_makeBlockCell( ut, type, 0, res );
        ur_blkSlice( ut, &bi, a1 );
        if( intersect )
        {
            USeriesIter ri;

            ri.buf = ur_buffer(res->series.buf);
            ri.it = ri.end = 0;

            ur_foreach( bi )
            {
                if( (dt->find( ut, &si, bi.it, 0 ) > -1) &&
                    (dt->find( ut, &ri, bi.it, 0 ) == -1) )
                {
                    ur_blkPush( blk, bi.it );
                    ri.end = blk->used;
                }
            }
        }
        else
        {
            ur_foreach( bi )
            {
                if( dt->find( ut, &si, bi.it, 0 ) < 0 )
                    ur_blkPush( blk, bi.it );
            }
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
    return: New value that contains only the elements common to both sets.
*/
CFUNC(cfunc_intersect)
{
    return set_relation( ut, a1, res, 1 );
}


/*-cf-
    difference
        setA    series
        setB    series
    return: New value that contains only the elements unique to each set.
*/
CFUNC(cfunc_difference)
{
    return set_relation( ut, a1, res, 0 );
}


/*-cf-
    sort
        set     series
    return: New series with sorted elements.
*/
CFUNC(cfunc_sort)
{
    int type = ur_type(a1);

    if( ur_isBlockType(type) )
    {
        QuickSortIndex qs;
        UBlockIter bi;
        UBuffer* blk;
        const UCell* cells;
        uint32_t* ip;
        int len;

        ur_blkSlice( ut, &bi, a1 );
        len = bi.end - bi.it;

        // Make invalidates bi.buf.
        blk = ur_makeBlockCell( ut, type, len, res );

        qs.index    = ((uint32_t*) (blk->ptr.cell + len)) - len;
        qs.user     = (void*) ut;
        qs.data     = (uint8_t*) bi.it;
        qs.elemSize = sizeof(UCell);
        qs.compare  = (QuickSortFunc) ur_compare;

        quickSortIndex( &qs, 0, len, 1 );

        cells = bi.it;
        ip = qs.index;
        ur_foreach( bi )
        {
            ur_blkPush( blk, cells + *ip );
            ++ip;
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_INTERNAL, "FIXME: sort only supports block!" );
}


/*-cf-
    foreach
        'words   word!/block!
        series
        body    block!
    return: Result of body.
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


    // TODO: Handle custom series type.
    if( ! ur_isSeriesType( ur_type(a2) ) )
        return errorType( "foreach expected series" );
    if( ! ur_is(body, UT_BLOCK) )
        return errorType( "foreach expected block! body" );

    ur_seriesSlice( ut, &si, sarg );
    dt = SERIES_DT( ur_type(sarg) );

    if( ur_is(a1, UT_WORD) )
    {
        while( si.it < si.end )
        {
            if( ! (cell = ur_wordCellM(ut, a1)) )
                return UR_THROW;
            dt->pick( ur_bufferSer(sarg), si.it++, cell ); 
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
            ur_bind( ut, wi.buf, localCtx, UR_BIND_THREAD );
            ur_bind( ut, ur_buffer(body->series.buf), localCtx, UR_BIND_THREAD );
        }
        }

        // Now that all validation is done the loop can finally begin.

        while( si.it < si.end )
        {
            si.buf = ur_bufferSer(sarg);    // Re-aquire
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
        }
        return UR_OK;
    }
    return errorType( "foreach expected word!/block! for words" );
}


/*-cf-
    forall
        'word   word!
        body    block!
    return: Result of body.
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
        tests block!
    return: logic!
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

    ur_setId(res, UT_LOGIC );
    ur_int(res) = 1;
    return UR_OK;
}


/*-cf-
    any
        tests block!
    return: Result of first true test or false.
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

    ur_setId(res, UT_LOGIC );
    ur_int(res) = 0;
    return UR_OK;
}


/*-cf-
    reduce
        value
    return: Reduced value.

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
                if( ! (ok = boron_eval1( ut, &ec2, bres++ )) )
                    break;
                ++ur_buffer(n)->used;
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
    probe
        value
    return: unset!

    Print value in its serialized form.
*/
CFUNC(cfunc_probe)
{
    UBuffer str;

    ur_strInit( &str, UR_ENC_UTF8, 0);
    ur_toStr( ut, a1, &str, 0 );
    ur_strTermNull( &str );
    fputs( str.ptr.c, stdout );
    putc( '\n', stdout );
    ur_strFree( &str );

    ur_setId( res, UT_UNSET );
    return UR_OK;
}


/*-cf-
    prin
        value
    return: unset!

    Print reduced value without a trailing linefeed.
*/
CFUNC(cfunc_prin)
{
    if( cfunc_reduce( ut, a1, res ) )
    {
        UBuffer str;

        ur_strInit( &str, UR_ENC_UTF8, 0);
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
    to-string
        val
    return: string!
*/
CFUNC(cfunc_to_string)
{
    int enc = UR_ENC_LATIN1;

    if( ur_is(a1, UT_STRING) )
    {
        const UBuffer* str = ur_bufferSer(a1);
        enc = str->form;
    }
    ut->types[ ur_type(a1) ]->toString( ut, a1,
                                        ur_makeStringCell(ut, enc, 0, res), 0 );

    return UR_OK;
}


/*-cf-
    to-text
        val
    return: string!
*/
CFUNC(cfunc_to_text)
{
    int enc = UR_ENC_LATIN1;

    if( ur_is(a1, UT_STRING) )
    {
        const UBuffer* str = ur_bufferSer(a1);
        enc = str->form;
    }
    ur_toText( ut, a1, ur_makeStringCell( ut, enc, 0, res ) );

    return UR_OK;
}


/*-cf-
    exists?
        file file!/string!
    return: True if file or directory exists.
*/
CFUNC(cfunc_existsQ)
{
    if( ur_isStringType( ur_type(a1) ) )
    {
        OSFileInfo info;
        int ok = ur_fileInfo( boron_cpath(ut, a1, 0), &info, FI_Type );

        ur_setId(res, UT_LOGIC);
        ur_int(res) = ok ? 1 : 0;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "exists? expected file!/string!" );
}


/*-cf-
    dir?
        path    file!/string!
    return: logic! or none! if path does not exist.

    Test if path is a directory.
*/
CFUNC(cfunc_dirQ)
{
    if( ur_isStringType( ur_type(a1) ) )
    {
        OSFileInfo info;
        if( ur_fileInfo( boron_cpath(ut, a1, 0), &info, FI_Type ) )
        {
            ur_setId(res, UT_LOGIC);
            ur_int(res) = (info.type == FI_Dir);
        }
        else
        {
            ur_setId(res, UT_NONE);
        }
        return UR_OK;
    }
    return errorType( "make-dir expected file!/string!" );
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
*/
CFUNC(cfunc_make_dir)
{
#define OPT_MAKE_DIR_ALL    0x01
    if( ur_isStringType( ur_type(a1) ) )
    {
        UBuffer* bin = ur_buffer( BT->tempN );
        char* path = boron_cpath( ut, a1, bin );
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


/*-cf-
    getenv
        val string!
    return: string! or none!

    Get operating system environment variable.
::
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


extern int ur_readDir( UThread*, const char* filename, UCell* res );

/*-cf-
    read
        file    file!/string!
        /text
        /into
            buffer  binary!/string!
    return: binary!/string!/block!

    Read binary! or UTF-8 string!.

    If /text or /into string! options are used then the file is read as
    UTF-8 data and carriage returns are filtered on Windows.

    If file is a directory, then a block containing filenames is returned.
*/
CFUNC(cfunc_read)
{
#define OPT_READ_TEXT   0x01
#define OPT_READ_INTO   0x02
    UBuffer* dest;
    const char* filename;
    const char* mode;
    OSFileInfo info;
    uint32_t opt = CFUNC_OPTIONS;

    if( ! ur_isStringType( ur_type(a1) ) )
        return errorType( "read expected file!/string!" );
    filename = boron_cpath( ut, a1, 0 );

    if( ! ur_fileInfo( filename, &info, FI_Size | FI_Type ) )
        return ur_error( ut, UR_ERR_ACCESS,
                         "could not access file %s", filename );

    if( info.type == FI_Dir )
        return ur_readDir( ut, filename, res );

    if( opt & OPT_READ_INTO )
    {
        const UCell* bufArg = a2;
        if( (ur_is(bufArg, UT_BINARY) || ur_is(bufArg, UT_STRING)) )
        {
            dest = ur_bufferSerM(bufArg);
            if( ! dest )
                return UR_THROW;

            if( dest->type == UT_STRING )
                ur_arrReserve( dest, (int) info.size );
            else
                ur_binReserve( dest, (int) info.size );

            if( ur_is(bufArg, UT_STRING) || (opt & OPT_READ_TEXT) )
                mode = "r";     // Read as text to filter carriage ret.
            else
                mode = "rb";
            *res = *bufArg;
        }
        else
        {
            return errorType( "read expected binary!/string! buffer" );
        }
    }
    else if( opt & OPT_READ_TEXT )
    {
        mode = "r";
        dest = ur_makeStringCell( ut, UR_ENC_UTF8, (int) info.size, res );
    }
    else
    {
        mode = "rb";
        dest = ur_makeBinaryCell( ut, (int) info.size, res );
    }

    if( info.size > 0 )
    {
        FILE* fp;
        size_t n;

        fp = fopen( filename, mode );
        if( ! fp )
        {
            return ur_error( ut, UR_ERR_ACCESS,
                             "could not open file %s", filename );
        }
        n = fread( dest->ptr.b, 1, (size_t) info.size, fp );
        if( n > 0 )
        {
            dest->used = n;
            //if( dest->type == UT_STRING )
            //    ur_strFlatten( dest );
        }
        fclose( fp );
    }
    return UR_OK;
}


/*-cf-
    write
        file    file!/string!
        data    binary!/string!/context!
        /append
    return: unset!
*/
CFUNC(cfunc_write)
{
#define OPT_WRITE_APPEND    0x01
    const UCell* data = a2;

    if( ! ur_isStringType( ur_type(a1) ) )
        return errorType( "write expected file!/string! filename" );

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
        USeriesIter si;
        UIndex size;
        size_t n;
        int append = CFUNC_OPTIONS & OPT_WRITE_APPEND;

        filename = boron_cstr( ut, a1, 0 );

        ur_seriesSlice( ut, &si, data );
        size = si.end - si.it;

        if( ur_is(data, UT_STRING) )
        {
            if( ur_strIsUcs2(si.buf) )
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
            // Write as text to add carriage ret.
            fp = fopen( filename, append ? "a" : "w" );
        }
        else
        {
            fp = fopen( filename, append ? "ab" : "wb" );
        }

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


#include <errno.h>

/*-cf-
    delete
        file    file!/string!
    return: unset! or error thrown.
*/
CFUNC(cfunc_delete)
{
    if( ur_isStringType( ur_type(a1) ) )
    {
        // if( ur_userAllows( ut, "Delete file \"%s\"", cp ) )
        int ok = remove( boron_cstr(ut, a1, 0) );
        if( ok != 0 )
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
*/
CFUNC(cfunc_rename)
{
    if( ur_isStringType( ur_type(a1) ) &&
        ur_isStringType( ur_type(a2) ) )
    {
        // if( ur_userAllows( ut, "Rename file \"%s\"", cp ) )
        const char* cp1;
        const char* cp2;
        UBuffer* tmp;
        int ok;

        tmp = ur_buffer( ur_makeBinary( ut, 0 ) );

        cp1 = boron_cstr(ut, a1, 0);
        cp2 = boron_cstr(ut, a2, tmp);

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
    load
        file    file!/string!
    return: block! or none! if file is empty.
*/
CFUNC(cfunc_load)
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

check_str:

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
#ifdef CONFIG_COMPRESS
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
    return UR_THROW;
}


extern int ur_parseBlock( UThread* ut, UBuffer*, UIndex start, UIndex end,
                          UIndex* parsePos, const UBuffer* ruleBlk,
                          int (*eval)( UThread*, const UCell* ) );

extern int ur_parseString( UThread* ut, UBuffer*, UIndex start, UIndex end,
                           UIndex* parsePos, const UBuffer* ruleBlk,
                           int (*eval)( UThread*, const UCell* ) );

/*-cf-
    parse
        input  string!/block!
        rules  block!  
    return:  True if end of input reached.
*/
CFUNC(cfunc_parse)
{
    if( (ur_is(a1, UT_BLOCK) || ur_is(a1, UT_STRING)) &&
        ur_is(a2, UT_BLOCK) )
    {
        USeriesIterM si;
        const UBuffer* rules;
        UIndex pos;
        int ok;

        if( ! ur_seriesSliceM( ut, &si, a1 ) )
            return UR_THROW;

        if( ! (rules = ur_bufferSer(a2)) )
            return UR_THROW;

        if( ur_is(a1, UT_STRING) )
            ok = ur_parseString( ut, si.buf, si.it, si.end, &pos, rules,
                                 boron_doVoid );
        else
            ok = ur_parseBlock( ut, si.buf, si.it, si.end, &pos, rules,
                                boron_doVoid );
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
            pos = (pos == si.buf->used);
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
*/
CFUNC(cfunc_equalQ)
{
    ur_setId(res, UT_LOGIC);
    ur_int(res) = ur_equal( ut, a1, a2 ) ? 1 : 0;
    return UR_OK;
}


/*-cf-
    gt?
        a
        b
    return: True if first value is greater than the second.
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
*/
CFUNC(cfunc_ltQ)
{
    ur_setId(res, UT_LOGIC);
    ur_int(res) = (ur_compare( ut, a1, a2 ) < 0) ? 1 : 0;
    return UR_OK;
}


/*-cf-
    type?
        value
    return: Datatype of value.
*/
CFUNC(cfunc_typeQ)
{
    (void) ut;
    ur_makeDatatype( res, ur_type(a1) );
    return UR_OK;
}


/*-cf-
    encoding? val
    return: Encoding type word! or none! if val is not a string.
*/
CFUNC(cfunc_encodingQ)
{
    static UAtom encAtoms[4] = {
        UR_ATOM_LATIN1, UR_ATOM_UTF8, UR_ATOM_UCS2, UT_UNSET
    };
    if( ur_isStringType( ur_type(a1) ) )
    {
        const UBuffer* buf = ur_bufferSer(a1);
        ur_setId(res, UT_WORD);
        ur_setWordUnbound(res, encAtoms[buf->form & 3] );
    }
    else
        ur_setId(res, UT_NONE);
    return UR_OK;
}


/*-cf-
    lowercase
        value   char!/string!/file!
    return: Value converted to lowercase.
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
        series  binary!/string!
        value
        /dir    Check if end is '/' or '\'.
    return: Modified series.

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
    now
        /date   Return date! rather than time!
    return: time! or date!
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


/*-cf-
    datatype?
        value
    return: True if value is a datatype!.

    Each datatype has its own test function which is named the same as the
    type but ending with '?' rather than a '!'.
    For example, to test if a value is a string! use "string? val".
*/
CFUNC(cfunc_datatypeQ)
{
    (void) ut;
    // Type variation is in a2.
    ur_int(res) = (ur_type(a1) == ur_int(a2)) ? 1 : 0;
    ur_setId(res, UT_LOGIC);
    return UR_OK;
}


//EOF
