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


typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint16_t argProgOffset;
    UIndex   argProgN;      // Same location as UCellSeries buf.
    union {
        int (*func)( UThread*, UCell*, UCell* );
#ifdef CONFIG_ASSEMBLE
        jit_function_t jitFunc;
#endif
        /*
        struct
        {
            UIndex bodyN;
            UIndex sigN;
        } f;
        */
    } m;
}
UCellFunc;

#define FUNC_FLAG_GHOST     1
#define FCELL  ((UCellFunc*) cell)
#define ur_funcBody(c)  (c)->series.buf


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
            if( (ur_binding(bi.it) == BOR_BIND_FUNC) &&
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


extern void block_toString( UThread*, const UCell*, UBuffer*, int );

void func_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    UCell tmp;
    const UBuffer* blk = ur_bufferSer(cell);

    if( cell->series.it )   // Argument program prelude
    {
        ur_strAppendCStr( str, "func " );
        block_toString( ut, blk->ptr.cell + 1, str, depth );
    }
    else
        ur_strAppendCStr( str, "does " );

    ur_setId(&tmp, UT_BLOCK);
    ur_setSeries(&tmp, cell->series.buf, cell->series.it);
    block_toString( ut, &tmp, str, depth );
}


extern void block_markBuf( UThread* ut, UBuffer* buf );

void func_mark( UThread* ut, UCell* cell )
{
    UIndex n = ur_funcBody(cell);
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
    UIndex n = ur_funcBody(cell);
    if( n > UR_INVALID_BUF )
        cell->series.buf = -n;
}


void func_bind( UThread* ut, UCell* cell, const UBindTarget* bt )
{
    UIndex n = ur_funcBody(cell);
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
            return a->series.buf == b->series.buf;
    }
    return 0;
}


void cfunc_mark( UThread* ut, UCell* cell )
{
    UIndex n = FCELL->argProgN;
    if( n > UR_INVALID_BUF )
        ur_markBuffer( ut, n );
}


void cfunc_toShared( UCell* cell )
{
    UIndex n = FCELL->argProgN;
    if( n > UR_INVALID_BUF )
        FCELL->argProgN = -n;
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

/** \struct UPortDevice
  The UPortDevice struct holds methods for a class of input/ouput device.
*/
/** \enum PortForm
  These are the UBuffer::form values for port buffers.
*/
/** \var PortForm::UR_PORT_SIMPLE
  Denotes that UBuffer::ptr is the UPortDevice pointer.
*/
/** \var PortForm::UR_PORT_EXT
  Denotes that UBuffer::ptr points to extension data and that the first
  member is the UPortDevice pointer.
*/

/** \fn int (*UPortDevice::open)(UThread*, const UPortDevice* pdev, const UCell* from, int opt, UCell* res)
  Create and open a new port.

  \param pdev   This device class.
  \param from   Specification of port.
  \param opt    Port options.
  \param res    Cell for new port.

  \return UR_OK/UR_THROW.

  Open will normally call boron_makePort() to generate a port buffer and
  set res.
*/
/** \fn void (*UPortDevice::close)(UBuffer* pbuf)
  Close port.

  \param pbuf   Buffer created by UPortDevice::open.
*/
/** \fn int (*UPortDevice::read)(UThread*, UBuffer* pbuf, UCell* dest, int len)
  Read data from port.
  If UPortDevice::defaultReadLen is greater than zero then dest will
  reference a UT_BINARY or UT_STRING buffer with enough memory reserved for
  len bytes.

  \param pbuf   Port buffer created by UPortDevice::open.
  \param dest   Destination buffer or result.
  \param len    Number of bytes to read into dest.

  \return UR_OK/UR_THROW
*/
/** \fn int (*UPortDevice::write)(UThread*, UBuffer* pbuf, const UCell* data)
  Write data to port.

  \param pbuf   Buffer created by UPortDevice::open.
  \param data   Data to write.

  \return UR_OK/UR_THROW
*/
/** \fn int (*UPortDevice::seek)(UThread*, UBuffer* pbuf, UCell* pos, int where)
  Seek to position.

  \param pbuf   Buffer created by UPortDevice::open.
  \param pos    New position relative to where.
  \param where  SEEK_SET, SEEK_END, or SEEK_CUR.

  \return UR_OK/UR_THROW

  If the device cannot seek then call ur_error().
*/
/** \var int UPortDevice::defaultReadLen
  Number of bytes to read if script does not specify a length.
  This should be set to zero if UPortDevice::read does not expect a UT_BINARY
  or UT_STRING buffer.
*/


extern void binary_mark( UThread* ut, UCell* cell );
extern void binary_toShared( UCell* cell );


/**
  Register port device.
  A single device struct may be added multiple times with different names.

  \param dev    Pointer to UPortDevice.
  \param name   Name of device.
*/
void boron_addPortDevice( UThread* ut, const UPortDevice* dev, UAtom name )
{
    UBuffer* ctx = &BENV->ports;
    int n = ur_ctxAddWordI( ctx, name );
    ((const UPortDevice**) ctx->ptr.v)[ n ] = dev;
    ur_ctxSort( ctx );
}


static UPortDevice* portLookup( UThread* ut, UAtom name )
{
    UBuffer* ctx = &BENV->ports;
    int n = ur_ctxLookup( ctx, name );
    if( n < 0 )
        return 0;
    return ((UPortDevice**) ctx->ptr.v)[ n ];
}


/**
  Create port buffer.

  \param pdev   Device pointer.
  \param ext    Pointer to port extension data or zero.
  \param res    Result port cell.

  \return UBuffer with type, form, and ptr initialized.

  This is usually called from inside a UPortDevice::open method.

  The UBuffer::type is set to UT_PORT.

  If ext is zero then the UBuffer::form is set to UR_PORT_SIMPLE and the
  UBuffer::ptr member is set to pdev.  If ext is non-zero then the form is set
  to UR_PORT_EXT, ptr is set to ext, and pdev is copied to the start
  of ext.
*/
UBuffer* boron_makePort( UThread* ut, const UPortDevice* pdev, void* ext,
                         UCell* res )
{
    UBuffer* buf;
    UIndex bufN;

    buf = ur_genBuffers( ut, 1, &bufN );

    buf->type = UT_PORT;
    if( ext )
    {
        buf->form  = UR_PORT_EXT;
        buf->ptr.v = ext;
        *((const UPortDevice**) ext) = pdev;
    }
    else
    {
        buf->form  = UR_PORT_SIMPLE;
        buf->ptr.v = (void*) pdev;
    }

    ur_initSeries(res, UT_PORT, bufN);
    return buf;
}


/*
  \param opt   PortOpenOptions
*/
int port_makeOpt( UThread* ut, const UCell* from, int opt, UCell* res )
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
                    if( name == UR_INVALID_ATOM )
                        return UR_THROW;
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

    return pdev->open( ut, pdev, from, opt, res );
}


int port_make( UThread* ut, const UCell* from, UCell* res )
{
    return port_makeOpt( ut, from, 0, res );
}


void port_destroy( UBuffer* buf )
{
    if( buf->ptr.v )
    {
        UPortDevice* dev = (buf->form == UR_PORT_SIMPLE) ?
            (UPortDevice*) buf->ptr.v : *((UPortDevice**) buf->ptr.v);
        dev->close( buf );
        buf->ptr.v = 0;
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
            if( ur_type(a) == ur_type(b) )
                return a->series.buf == b->series.buf;
            break;
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


const UDatatype boron_types[] =
{
  {
    "func!",
    unset_make,             unset_make,             func_copy,
    func_compare,           unset_operate,          unset_select,
    func_toString,          func_toString,
    unset_recycle,          func_mark,              func_destroy,
    unset_markBuf,          func_toShared,          func_bind
  },
  {
    "cfunc!",
    unset_make,             unset_make,             unset_copy,
    cfunc_compare,          unset_operate,          unset_select,
    unset_toString,         unset_toText,
    unset_recycle,          cfunc_mark,             unset_destroy,
    unset_markBuf,          cfunc_toShared,         unset_bind
  },
  // CONFIG_ASSEMBLE
  {
    "afunc!",
    unset_make,             unset_make,             unset_copy,
    cfunc_compare,          unset_operate,          unset_select,
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
