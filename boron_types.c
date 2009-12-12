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


#include "unset.h"


//----------------------------------------------------------------------------
// UT_FUNC


/*
  Function Argument UBuffer members:
    type        UT_FUNC
    elemSize    Number of options
    form        Offset from ptr.b to option table
    flags       Unused
    used        Unused
    ptr.b       Byte-code followed by atom/index for each option
*/


typedef struct
{
    UCellId  id;
    UIndex   argBufN;
    union {
        int (*func)( UThread*, UCell*, UCell* );
        struct
        {
            UIndex bodyN;
            UIndex sigN;
        } f;
    } m;
}
UCellFunc;

#define FUNC_FLAG_GHOST     1
#define FCELL  ((UCellFunc*) cell)


enum FuncArgumentOpcodes
{
    // Instruction       Data    Description
    // -------------------------------------------------------------------
    FO_clearLocal,    // n       Set locals on dstack to UT_NONE.
    FO_clearLocalOpt, // n       Push options cell and set locals to UT_NONE.
    FO_fetchArg,      //         Push _eval1 result onto dstack.
    FO_litArg,        //         Push current UCell in block onto dstack.
    FO_variant,       // n       Push variant int! onto dstack.
    FO_checkArg,      // t       Check last argument type.
    FO_checkArgMask,  // 2*u32   Check last argument type against mask.
    FO_option,        // n skip  Skip number of instructions if option off.
    FO_nop,           //         Ignore instruction.
    FO_nop2,          // i       Ignore instruction and the next.
    FO_end            //         End function program.
};


typedef struct
{
    UAtom    atom;
    uint16_t n;     // Option number (to be used when table sorted by atom).
}
FuncOption;

#define func_optTable(buf)  ((FuncOption*) (buf->ptr.b + buf->form))


/*
  Convert function argument specification to byte-code program.

  \param blkC       Valid block cell.
  \param argCtx     Context buffer for arguments.  May be zero.
  \param optCtx     Context buffer for options.  May be zero.

  \return Function program buffer id.
*/
static UIndex boron_makeArgProgram( UThread* ut, const UCell* blkC,
                                    UBuffer* argCtx, UBuffer* optCtx,
                                    UCellFunc* fcell )
{
    UBlockIter bi;
    FuncOption options[ MAX_OPT ];
    int localCount = 0;
    int optionCount = 0;
    int optArgCount = 0;
    int prevOptPos = 0;
    UAtom optAtom = 0;
    UBuffer* prog = ur_buffer( BT->tempN );


#define ATOM_LOCAL      UR_ATOM_BAR
#define FO_RESERVE(n)   ur_binReserve( prog, prog->used + n + 1 );
#define FO_EMIT(op)     prog->ptr.b[ prog->used++ ] = op

#define FO_EMIT2(op,data) \
    prog->ptr.b[ prog->used ] = op; \
    prog->ptr.b[ prog->used + 1 ] = data; \
    prog->used += 2;

#define FO_EMIT_R(op) \
    FO_RESERVE(1); \
    FO_EMIT(op)

#define FO_EMIT2_R(op,data) \
    FO_RESERVE(2); \
    FO_EMIT2(op,data)


    ur_binReserve( prog, 16 );
    prog->ptr.b[0] = FO_clearLocal;
    prog->ptr.b[1] = 0;             // localCount set below.
    prog->used = 2;

    ur_blkSlice( ut, &bi, blkC );
    ur_foreach( bi )
    {
        switch( ur_type(bi.it) )
        {
        case UT_DATATYPE:
            if( localCount )
            {
                uint32_t* mask;
                int type = ur_datatype(bi.it);
                if( type < UT_MAX )
                {
                    FO_EMIT2_R( FO_checkArg, type );
                }
                else
                {
                    FO_RESERVE( 12 );
                    switch( prog->used & 3 )    // Pad so masks are aligned.
                    {
                        case 0:
                            FO_EMIT( FO_nop );
                            // Fall through...
                        case 1:
                            FO_EMIT2( FO_nop2, FO_nop );
                            break;
                        case 2:
                            FO_EMIT( FO_nop );
                            break;
                    }
                    FO_EMIT( FO_checkArgMask );
                    assert( (prog->used & 3) == 0 );
                    mask = (uint32_t*) (prog->ptr.b + prog->used);
                    *mask++ = bi.it->datatype.mask0;
                    *mask   = bi.it->datatype.mask1;
                    prog->used += 8;
                }
            }
            break;

        case UT_INT:
            FO_EMIT2_R( FO_variant, ur_int(bi.it) );
            ++localCount;
            break;

        case UT_WORD:
        case UT_LITWORD:
            if( ur_atom(bi.it) < UT_BI_COUNT )
            {
                if( localCount )
                {
                    FO_EMIT2_R( FO_checkArg, (uint8_t) ur_atom(bi.it) );
                }
            }
            else if( ur_atom(bi.it) == ATOM_LOCAL )
            {
                optAtom = ATOM_LOCAL;
                goto close_option;
            }
            else
            {
                if( optAtom )
                {
                    if( optAtom == ATOM_LOCAL )
                    {
                        ++localCount;
                        if( argCtx )
                            ur_ctxAddWordI( argCtx, ur_atom(bi.it) );
                        break;
                    }
                    if( ! optArgCount )
                    {
                        prevOptPos = prog->used;
                        FO_RESERVE( 3 );
                        FO_EMIT( FO_option );
                        FO_EMIT2( optionCount - 1, 0 );
                    }
                    ++optArgCount;
                }
#if 0
                else
                    ++argCount;
#endif
                ++localCount;
                if( argCtx )
                    ur_ctxAddWordI( argCtx, ur_atom(bi.it) );
                FO_EMIT_R( ur_is(bi.it, UT_WORD) ? FO_fetchArg : FO_litArg );
            }
            break;

        case UT_OPTION:
            optAtom = ur_atom(bi.it);
            if( optAtom == UR_ATOM_GHOST )
            {
                ur_setFlags(fcell, FUNC_FLAG_GHOST);
                break;
            }
            options[ optionCount ].atom = optAtom;
            options[ optionCount ].n    = 0;
            ++optionCount;
            if( optCtx )
                ur_ctxAddWordI( optCtx, ur_atom(bi.it) );
close_option:
            if( optArgCount )
            {
                prog->ptr.b[ prevOptPos + 2 ] = optArgCount + 2;
                optArgCount = 0;
            }
            break;
        }
    }

    if( optArgCount )
        prog->ptr.b[ prevOptPos + 2 ] = optArgCount + 2;

    if( optionCount )
    {
        prog->ptr.b[0] = FO_clearLocalOpt;
        ++localCount;
    }
    prog->ptr.b[1] = localCount;

    FO_EMIT( FO_end );      // FO_RESERVE reserves space for end instruction.


    if( (prog->used > 3) || localCount )
    {
        UBuffer* pbuf;
        uint8_t* mem;
        UIndex pbufN;
        int progLen = (prog->used + 3) & ~3;
        int optTableLen = optionCount * sizeof(FuncOption);

        // Finish with prog before ur_genBuffers().
        mem = (uint8_t*) memAlloc( progLen + optTableLen );
        memCpy( mem, prog->ptr.v, prog->used );
        if( optTableLen )
            memCpy( mem + progLen, options, optTableLen );

        ur_genBuffers( ut, 1, &pbufN );
        pbuf = ur_buffer( pbufN );

        pbuf->type  = UT_FUNC;
        pbuf->elemSize = optionCount;
        pbuf->form  = progLen;
        pbuf->flags = 0;
        pbuf->used  = 0;
        pbuf->ptr.v = mem;

        return pbufN;
    }
    return UR_INVALID_BUF;
}


static void _rebindFunc( UThread* ut, UIndex blkN, UIndex new, UIndex old )
{
    UBlockIterM bi;
    int type;

    // We just deep copied so the blocks are in thread storage.
    bi.buf = ur_buffer(blkN);
    bi.it  = bi.buf->ptr.cell;
    bi.end = bi.it + bi.buf->used;

    ur_foreach( bi )
    {
        type = ur_type(bi.it);
        if( ur_isWordType(type) )
        {
            if( (ur_binding(bi.it) == UR_BIND_FUNC) &&
                (bi.it->word.ctx == old) )
            {
                //printf( "KR rebind func %d\n", new );
                bi.it->word.ctx = new;
            }
        }
        else if( ur_isBlockType(type) )
        {
            _rebindFunc( ut, bi.it->series.buf, new, old );
        }
    }
}


void func_copy( UThread* ut, const UCell* from, UCell* res )
{
    UIndex newBodN;
    UIndex fromBodN = ((UCellFunc*) from)->m.f.bodyN;

    //printf( "KR func_copy\n" );
    *res = *from;
    ((UCellFunc*) res)->m.f.bodyN = newBodN = ur_blkClone( ut, fromBodN );

    _rebindFunc( ut, newBodN, newBodN, fromBodN );
}


int func_select( UThread* ut, const UCell* cell, UBlockIter* bi, UCell* res )
{
    const UBuffer* buf;
    const FuncOption* opt;
    const FuncOption* it;
    const FuncOption* end;
    UAtom atom;

    if( ! FCELL->argBufN )
        return ur_error( ut, UR_ERR_SCRIPT,
                         "function has no options to select" );

    buf = ur_bufferE( FCELL->argBufN );
    opt = func_optTable(buf);
    end = opt + buf->elemSize;
    BT->funcOptions = 0;

    for( ; bi->it != bi->end; ++bi->it )
    {
        if( ur_is(bi->it, UT_WORD) )
        {
            atom = ur_atom(bi->it);
            for( it = opt; it != end; ++it )
            {
                if( atom == it->atom )
                    BT->funcOptions |= 1 << (it - opt);
            }
        }
    }

    *res = *cell;
    bi->it = bi->end;
    return UR_OK;
}


extern void block_toString( UThread*, const UCell*, UBuffer*, int );

void func_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    UCell tmp;

    ur_setId(&tmp, UT_BLOCK);
    tmp.series.it = 0;
    tmp.series.end = -1;

    if( FCELL->m.f.sigN )
    {
        ur_strAppendCStr( str, "func " );
        tmp.series.buf = FCELL->m.f.sigN;
        block_toString( ut, &tmp, str, depth );
    }
    else
        ur_strAppendCStr( str, "does " );

    tmp.series.buf = FCELL->m.f.bodyN;
    block_toString( ut, &tmp, str, depth );
}


extern void block_markBuf( UThread* ut, UBuffer* buf );

void func_mark( UThread* ut, UCell* cell )
{
    UIndex n;

    n = FCELL->argBufN;
    if( n > UR_INVALID_BUF )
        ur_markBuffer( ut, n );

    n = FCELL->m.f.bodyN;
    if( n > UR_INVALID_BUF )
    {
        if( ur_markBuffer( ut, n ) )
            block_markBuf( ut, ur_buffer(n) );
    }

    n = FCELL->m.f.sigN;
    if( n > UR_INVALID_BUF )
    {
        if( ur_markBuffer( ut, n ) )
            block_markBuf( ut, ur_buffer(n) );
    }
}


void func_destroy( UBuffer* buf )
{
    if( buf->ptr.v )
    {
        memFree( buf->ptr.v );
        buf->ptr.v = 0; // Urlan wants to see proper cleanup (ur_genBuffers).
    }
    buf->elemSize = 0;
}


void func_toShared( UCell* cell )
{
    UIndex n;

    n = FCELL->argBufN;
    if( n > UR_INVALID_BUF )
        FCELL->argBufN = -n;

    n = FCELL->m.f.bodyN;
    if( n > UR_INVALID_BUF )
        FCELL->m.f.bodyN = -n;

    n = FCELL->m.f.sigN;
    if( n > UR_INVALID_BUF )
        FCELL->m.f.sigN = -n;
}


void func_bind( UThread* ut, UCell* cell, const UBindTarget* bt )
{
    UIndex n = FCELL->m.f.bodyN;
    //printf( "KR func_bind\n" );
    if( n > UR_INVALID_BUF )
    {
        UBuffer* buf = ur_buffer(n);
        ur_bindCells( ut, buf->ptr.cell, buf->ptr.cell + buf->used, bt );
    }
}


//----------------------------------------------------------------------------
// UT_CFUNC


/*
  Installed by boron_threadInit().
*/
void cfunc_recycle2( UThread* ut, int phase )
{
    if( phase == UR_RECYCLE_MARK )
    {
        // Sync data stack used with tos.
        UBuffer* buf = ur_buffer(BT->dstackN);
        buf->used = BT->tos - buf->ptr.cell;

        // Sync frame stack used with tof.
        buf = ur_buffer(BT->fstackN);
        buf->used = BT->tof - ur_ptr(LocalFrame, buf);
    }
}


void cfunc_mark( UThread* ut, UCell* cell )
{
    UIndex n = FCELL->argBufN;
    if( n > UR_INVALID_BUF )
        ur_markBuffer( ut, n );
}


void cfunc_toShared( UCell* cell )
{
    UIndex n = FCELL->argBufN;
    if( n > UR_INVALID_BUF )
        FCELL->argBufN = -n;
}


//----------------------------------------------------------------------------


UDatatype boron_types[] =
{
  {
    "func!",
    unset_make,             unset_make,             func_copy,
    unset_compare,          func_select,
    func_toString,          func_toString,
    unset_recycle,          func_mark,              func_destroy,
    unset_markBuf,          func_toShared,          func_bind
  },
  {
    "cfunc!",
    unset_make,             unset_make,             unset_copy,
    unset_compare,          func_select,
    unset_toString,         unset_toText,
    unset_recycle,          cfunc_mark,             unset_destroy,
    unset_markBuf,          cfunc_toShared,         unset_bind
  },
};


//EOF
