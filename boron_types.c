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
#define ur_funcBody(c)  ((UCellFunc*) c)->m.f.bodyN
#define ur_funcFunc(c)  ((UCellFunc*) c)->m.func


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
    int optArgs = 0;
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
            if( ur_atom(bi.it) < UT_MAX )
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
                    if( ! optArgs )
                    {
                        ++optArgs;
                        prevOptPos = prog->used;
                        FO_RESERVE( 3 );
                        FO_EMIT( FO_option );
                        FO_EMIT2( optionCount - 1, 0 );
                    }
                }
                ++localCount;
                if( argCtx )
                    ur_ctxAddWordI( argCtx, ur_atom(bi.it) );
                FO_EMIT_R( ur_is(bi.it, UT_WORD) ? FO_fetchArg : FO_litArg );
            }
            break;

        case UT_OPTION:
            assert( optAtom != ATOM_LOCAL );
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
            if( optArgs )
            {
                optArgs = 0;
                prog->ptr.b[ prevOptPos + 2 ] = prog->used - prevOptPos - 1;
            }
            break;
        }
    }

    if( optArgs )
        prog->ptr.b[ prevOptPos + 2 ] = prog->used - prevOptPos - 1;

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
    UIndex fromBodN = ur_funcBody(from);

    //printf( "KR func_copy\n" );
    *res = *from;
    ur_funcBody(res) = newBodN = ur_blkClone( ut, fromBodN );

    _rebindFunc( ut, newBodN, newBodN, fromBodN );
}


int func_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) != ur_type(b) )
                break;
            // Fall through...

        case UR_COMPARE_SAME:
            return ur_funcBody(a) == ur_funcBody(b);
    }
    return 0;
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


int cfunc_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) != ur_type(b) )
                break;
            // Fall through...

        case UR_COMPARE_SAME:
            return ur_funcFunc(a) == ur_funcFunc(b);
    }
    return 0;
}


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
// UT_PORT
/*
  UBuffer members:
    type        UT_PORT
    elemSize    Unused
    form        UR_PORT_SIMPLE, UR_PORT_EXT
    flags       Unused
    used        Unused
    ptr.v       UPortDevice* or UPortDevice**
*/


extern void binary_mark( UThread* ut, UCell* cell );
extern void binary_toShared( UCell* cell );


/**
  Register port device.
  A single device may be added multiple times with different names.

  \param dev    Pointer to UPortDevice.
  \param name   Name of device.
*/
void boron_addPortDevice( UThread* ut, UPortDevice* dev, UAtom name )
{
    UBuffer* ctx = &ut->env->ports;
    int n = ur_ctxAddWordI( ctx, name );
    ((UPortDevice**) ctx->ptr.v)[ n ] = dev;
    ur_ctxSort( ctx );
}


/*
  Called from UPortDevice::open() method if extended data is used.
*/
void boron_extendPort( UBuffer* port, UPortDevice** ext )
{
    port->form = UR_PORT_EXT;
    *ext = port->ptr.v;
    port->ptr.v = ext;
}


static UPortDevice* portLookup( UThread* ut, UAtom name )
{
    UBuffer* ctx = &ut->env->ports;
    int n = ur_ctxLookup( ctx, name );
    if( n < 0 )
        return 0;
    return ((UPortDevice**) ctx->ptr.v)[ n ];
}


int ur_makePort( UThread* ut, UPortDevice* pdev, const UCell* from, UCell* res,
                 int options )
{
    UBuffer* buf;
    UIndex bufN;

    ur_genBuffers( ut, 1, &bufN );
    buf = ur_buffer( bufN );

    buf->type  = UT_PORT;
    buf->form  = UR_PORT_SIMPLE;
    buf->ptr.v = pdev;

    if( ! pdev->open( ut, buf, from, options ) )
    {
        //buf = ur_buffer( bufN );    // Re-aquire
        buf->ptr.v = 0;
        return UR_THROW;
    }

    ur_setId(res, UT_PORT);
    ur_setSeries(res, bufN, 0);
    return UR_OK;
}


int port_make( UThread* ut, const UCell* from, UCell* res )
{
    UPortDevice* pdev = 0;
    UAtom name = 0;

    switch( ur_type(from) )
    {
        case UT_STRING:
        {
            const char* cp;
            const char* url = boron_cstr( ut, from, 0 );

            for( cp = url; *cp; ++cp )
            {
                if( cp[0] == ':' && cp[1] == '/' && cp[2] == '/' )
                {
                    name = ur_internAtom( ut, url, cp );
                    break;
                }
            }
            if( ! name )
                return ur_error( ut, UR_ERR_SCRIPT,
                                 "make port! expected URL" );
            pdev = portLookup( ut, name );
        }
            break;

        case UT_INT:
        case UT_FILE:
            pdev = &port_file;
            break;

        case UT_BLOCK:
        {
            UBlockIter bi;
            ur_blkSlice( ut, &bi, from );
            if( (bi.it == bi.end) || ! ur_is(bi.it, UT_WORD) )
                return ur_error( ut, UR_ERR_SCRIPT,
                                 "make port! expected device word in block" );
            pdev = portLookup( ut, name = ur_atom(bi.it) );
        }
            break;

        default:
            return ur_error( ut, UR_ERR_TYPE,
                             "make port! expected int!/string!/file!/block!" );
    }

    if( ! pdev )
        return ur_error( ut, UR_ERR_SCRIPT, "Port type %s does not exist",
                         ur_atomCStr(ut, name) );

    return ur_makePort( ut, pdev, from, res, 0 );
}


void port_destroy( UBuffer* buf )
{
    if( buf->ptr.v )
    {
        UPortDevice* dev = (buf->form == UR_PORT_SIMPLE) ?
            (UPortDevice*) buf->ptr.v : *((UPortDevice**) buf->ptr.v);
        dev->close( buf );
    }
}


int port_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_SAME:
        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            return a->series.buf == b->series.buf;
#if 0
        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) == ur_type(b) )
            {
                const UBuffer* bufA = ur_bufferSer( a );
                const UBuffer* bufB = ur_bufferSer( b );
                return (bufA->used == bufB->used) &&
                       (bufA->ptr.v == bufB->ptr.v);
            }
            break;
#endif
    }
    return 0;
}


//----------------------------------------------------------------------------


UDatatype boron_types[] =
{
  {
    "func!",
    unset_make,             unset_make,             func_copy,
    func_compare,           unset_operate,          func_select,
    func_toString,          func_toString,
    unset_recycle,          func_mark,              func_destroy,
    unset_markBuf,          func_toShared,          func_bind
  },
  {
    "cfunc!",
    unset_make,             unset_make,             unset_copy,
    cfunc_compare,          unset_operate,          func_select,
    unset_toString,         unset_toText,
    unset_recycle,          cfunc_mark,             unset_destroy,
    unset_markBuf,          cfunc_toShared,         unset_bind
  },
  {
    "port!",
    port_make,              unset_make,             unset_copy,
    port_compare,           unset_operate,          unset_select,
    unset_toString,         unset_toText,
    unset_recycle,          binary_mark,            port_destroy,
    unset_markBuf,          binary_toShared,        unset_bind
  },
};


//EOF
