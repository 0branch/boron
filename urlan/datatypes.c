/*
  Copyright 2009-2013 Karl Robillard

  This file is part of the Urlan datatype system.

  Urlan is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Urlan is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Urlan.  If not, see <http://www.gnu.org/licenses/>.
*/


/** \struct UDatatype
  The UDatatype struct holds methods for a specific class of data.

  When implementing custom types, the unset.h methods can be used.

  \sa USeriesType
*/
/** \var UDatatype::name
  ASCII name of type, ending with '!'.
*/
/** \fn int (*UDatatype::make)(UThread*, const UCell* from, UCell* res)
  Make a new instance of the type.
  From and res are guaranteed to point to different cells.

  Make should perform a shallow copy on any complex types which can be
  nested.

  \param from   Value which describes the new instance.
  \param res    Cell for new instance.

  \return UR_OK/UR_THROW
*/
/** \fn void (*UDatatype::convert)(UThread*, const UCell* from, UCell* res)
  Convert data to a new instance of the type.
  From and res are guaranteed to point to different cells.

  Convert should perform a shallow copy on any complex types which can be
  nested.

  \param from   Value to convert.
  \param res    Cell for new instance.

  \return UR_OK/UR_THROW
*/
/** \fn void (*UDatatype::copy)(UThread*, const UCell* from, UCell* res)
  Make a clone of an existing value.
  From and res are guaranteed to point to different cells.

  \param from   Value to copy.
  \param res    Cell for new instance.
*/
/** \fn int (*UDatatype::compare)(UThread*, const UCell* a, const UCell* b, int test)
  Perform a comparison between cells.

  If the test is UR_COMPARE_SAME, then the type of cells a & b are
  guaranteed to be the same by the caller.  Otherwise, one of the cells
  might not be of the datatype for this compare method.

  When the test is UR_COMPARE_ORDER or UR_COMPARE_ORDER_CASE, then the method
  should return 1, 0, or -1, if cell a is greater than, equal to, or lesser
  than cell b.

  The method should only check for datatypes with a lesser or equal
  ur_type(), as the caller will use the compare method for the higher
  ordered type.

  \param a      First cell.
  \param b      Second cell.
  \param test   Type of comparison to do.

  \return Non-zero if the camparison is true.
*/
/** \fn int (*UDatatype::operate)(UThread*, const UCell* a, const UCell* b, UCell* res, int op)
  Perform an operation on two cells.

  The method should only check for datatypes with a lesser or equal
  ur_type(), as the caller will use the operate method for the higher
  ordered type.

  \param a      First cell.
  \param b      Second cell.
  \param res    Result cell.
  \param op     Type of operation to do.

  \return UR_OK/UR_THROW
*/
/** \fn const UCell* (*UDatatype::select)( UThread*, const UCell* cell, const UCell* sel, UCell* tmp)
  Get the value which a path node references.
  If the selector is invalid, call ur_error() and return 0.

  \param cell   Cell of this datatype.
  \param sel    Selector value.
  \param tmp    Storage for result (if needed).  If used, then return tmp.

  \return Pointer to result cell or 0 if an error is thrown.
*/
/** \fn void (*UDatatype::toString)( UThread*, const UCell* cell, UBuffer* str, int depth )
  Convert cell to its string data representation.

  \param cell   Cell of this datatype.
  \param str    String buffer to append to.
  \param depth  Indentation depth.
*/
/** \fn void (*UDatatype::toText)( UThread*, const UCell* cell, UBuffer* str, int depth )
  Convert cell to its string textual representation.

  \param cell   Cell of this datatype.
  \param str    String buffer to append to.
  \param depth  Indentation depth.
*/
/** \fn void (*UDatatype::recycle)(UThread*, int phase)
  Performs thread global garbage collection duties for the datatype.
  This is called twice from ur_recycle(), first with UR_RECYCLE_MARK, then
  with UR_RECYCLE_SWEEP.
*/
/** \var UrlanRecyclePhase::UR_RECYCLE_MARK
  Phase passed to UDatatype::recycle.
*/
/** \var UrlanRecyclePhase::UR_RECYCLE_SWEEP
  Phase passed to UDatatype::recycle.
*/
/** \fn void (*UDatatype::mark)(UThread*, UCell* cell)
  Responsible for marking any buffers referenced by the cell as used.
  Use ur_markBuffer().
*/
/** \fn void (*UDatatype::destroy)(UBuffer* buf)
  Free any memory or other resources the buffer uses.
*/
/** \fn void (*UDatatype::markBuf)(UThread*, UBuffer* buf)
  Responsible for marking other buffers referenced by this buffer as used.
*/
/** \fn void (*UDatatype::toShared)( UCell* cell )
  Change any buffer ids in the cell so that they reference the shared
  environment (negate the id).
*/
/** \fn void (*UDatatype::bind)( UThread*, UCell* cell, const UBindTarget* bt )
  Bind cell to target.
*/

/** \struct USeriesType
  The USeriesType struct holds extra methods for series datatypes.
*/
/** \fn void (*USeriesType::pick)( const UBuffer* buf, UIndex n, UCell* res )
  Get a single value from the series.

  \param buf    Series buffer to pick from.
  \param n      Zero-based index of element to pick.
  \param res    Result of picked value.
*/
/** \fn void (*USeriesType::poke)( UBuffer* buf, UIndex n, const UCell* val )
  Replace a single value in the series.

  \param buf    Series buffer to modify.
  \param n      Zero-based index of element to replace.
  \param val    Value to replace.
*/
/** \fn int  (*USeriesType::append)( UThread*, UBuffer* buf, const UCell* val )
  Append a value to the series.

  \param buf    Series buffer to append to.
  \param val    Value to append.

  \return UR_OK/UR_THROW
*/
/** \fn int  (*USeriesType::insert)( UThread*, UBuffer* buf, UIndex index,
                                     const UCell* val, UIndex part )
  Insert a value into the series.

  \param buf    Series buffer to insert into.
  \param index  Position in buf.
  \param val    Value to append.
  \param part   Limit number of val elements.
                This will be INT32_MAX by default.

  \return UR_OK/UR_THROW
*/
/** \fn void (*USeriesType::change)( UThread*, USeriesIterM* si,
                                     const UCell* val, UIndex part )
  Change part of the series.

  \param si     Series buffer and change position.  Si.end is ignored.
  \param val    Replacement value.
  \param part   Remove this number of elements and insert replacement.
                If zero then simply overwrite with val.
*/
/** \fn void (*USeriesType::remove)( UThread*, USeriesIterM* si, UIndex part )
  Remove part of the series.

  \param si     Series buffer and position.  Si.end is ignored.
  \param part   Number of element to remove.
*/
/** \fn void (*USeriesType::reverse)( const USeriesIterM* si )
  Reverse series elements.

  \param si     Series buffer and slice to reverse.
*/
/** \fn int (*USeriesType::find)( UThread*, const USeriesIter* si,
                                  const UCell* val, int opt )
  Search for a value in the series.

  \param si     Series iterator.
  \param val    Value to find.
  \param opt    Options.

  \return Zero-based index of val in series or -1 if not found.
*/


#include "urlan_atoms.h"
#include "bignum.h"


//#define ANY3(c,t1,t2,t3)    ((1<<ur_type(c)) & ((1<<t1) | (1<<t2) | (1<<t3)))

#define DT(dt)          (ut->types[ dt ])
#define SERIES_DT(dt)   ((const USeriesType*) (ut->types[ dt ]))

#define block_destroy   ur_arrFree
#define string_destroy  ur_arrFree
#define context_markBuf block_markBuf

void block_markBuf( UThread* ut, UBuffer* buf );


typedef struct
{
    UIndex small;
    UIndex large;
    char   secondLarger;
}
USizeOrder;


void ur_sizeOrder( USizeOrder* ord, UIndex a, UIndex b )
{
    if( a >= b )
    {
        ord->small = b;
        ord->large = a;
        ord->secondLarger = 0;
    }
    else
    {
        ord->small = a;
        ord->large = b;
        ord->secondLarger = 1;
    }
}


//----------------------------------------------------------------------------
// UT_UNSET


int unset_make( UThread* ut, const UCell* from, UCell* res )
{
    (void) ut;
    (void) from;
    ur_setId(res, UT_UNSET);
    return UR_OK;
}

void unset_copy( UThread* ut, const UCell* from, UCell* res )
{
    (void) ut;
    *res = *from;
}

int unset_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    (void) a;
    (void) b;
    (void) test;
    return 0;
}

static const char* operatorNames[] =
{
    "add", "sub", "mul", "div", "mod", "and", "or", "xor"
};

int  unset_operate( UThread* ut, const UCell* a, const UCell* b, UCell* res,
                    int op )
{
    (void) res;
    return ur_error( ut, UR_ERR_SCRIPT, "%s operator is unset for types %s %s",
                     operatorNames[ op ],
                     ur_atomCStr(ut, ur_type(a)),
                     ur_atomCStr(ut, ur_type(b)) );
}

const UCell* unset_select( UThread* ut, const UCell* cell, const UCell* sel,
                           UCell* tmp )
{
    (void) cell;
    (void) sel;
    (void) tmp;
    ur_error( ut, UR_ERR_SCRIPT, "path select is unset for type %s",
              ur_atomCStr(ut, ur_type(cell)) );
    return 0;
}

void unset_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) depth;
    ur_strAppendChar( str, '~' );
    ur_strAppendCStr( str, ur_atomCStr(ut, ur_type(cell)) );
    ur_strAppendChar( str, '~' );
}

void unset_mark( UThread* ut, UCell* cell )
{
    (void) ut;
    (void) cell;
}

void unset_destroy( UBuffer* buf )
{
    (void) buf;
}

void unset_toShared( UCell* cell )
{
    (void) cell;
}

void unset_bind( UThread* ut, UCell* cell, const UBindTarget* bt )
{
    (void) ut;
    (void) cell;
    (void) bt;
}


#define unset_recycle   0
#define unset_markBuf   0


UDatatype dt_unset =
{
    "unset!",
    unset_make,             unset_make,             unset_copy,
    unset_compare,          unset_operate,          unset_select,
    unset_toString,         unset_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


//----------------------------------------------------------------------------
// UT_DATATYPE


/**
  Test if cell is of a certain datatype.

  \param cell       Cell to test.
  \param datatype   Valid cell of type UT_DATATYPE.

  \return Non-zero if cell type matches datatype.
*/
int ur_isDatatype( const UCell* cell, const UCell* datatype )
{
    uint32_t dt = ur_type(cell);
    if( dt < 32 )
        return datatype->datatype.mask0 & (1 << dt);
    else
        return datatype->datatype.mask1 & (1 << (dt - 32));
}


/**
  Initialize cell to be a UT_DATATYPE value of the given type.
*/
void ur_makeDatatype( UCell* cell, int type )
{
    ur_setId(cell, UT_DATATYPE);
    ur_datatype(cell) = type;
    if( type < 32 )
    {
        cell->datatype.mask0 = 1 << type;
        cell->datatype.mask1 = cell->datatype.mask2 = 0;
    }
    else if( type < 64 )
    {
        cell->datatype.mask1 = 1 << (type - 32);
        cell->datatype.mask0 = cell->datatype.mask2 = 0;
    }
    else
    {
        cell->datatype.mask1 = 
        cell->datatype.mask0 = cell->datatype.mask2 = 0xffffffff;
    }
}


/**
  Add type to multi-type UT_DATATYPE cell.
*/
void ur_datatypeAddType( UCell* cell, int type )
{
    uint32_t* mp;
    uint32_t mask;

    if( type < 32 )
    {
        mp = &cell->datatype.mask0;
        mask = 1 << type;
    }
    else if( type < 64 )
    {
        mp = &cell->datatype.mask1;
        mask = 1 << (type - 32);
    }
    else
    {
        mp = &cell->datatype.mask2;
        mask = 1 << (type - 64);
    }

    if( ! (mask & *mp) )
    {
        *mp |= mask;
        cell->datatype.n = UT_TYPEMASK;
    }
}


#if 0
/*
  If cell is any word and it has a datatype name then that type is returned.
  Otherwise the datatype of the cell is returned.
*/
static int _wordType( UThread* ut, const UCell* cell )
{
    int type = ur_type(cell);
    if( ur_isWordType(type) && (ur_atom(cell) < ur_datatypeCount(ut)) )
        type = ur_atom(cell);
    return type;
}
#endif


int datatype_make( UThread* ut, const UCell* from, UCell* res )
{
    (void) ut;
    ur_makeDatatype( res, ur_type(from) );
    return UR_OK;
}


int datatype_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_SAME:
            if( ur_datatype(a) == ur_datatype(b) )
            {
                if( ur_datatype(a) != UT_TYPEMASK )
                    return 1;
                return ((a->datatype.mask0 == b->datatype.mask0) &&
                        (a->datatype.mask1 == b->datatype.mask1));
            }
            break;

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) == ur_type(b) )
            {
                return ((a->datatype.mask0 & b->datatype.mask0) ||
                        (a->datatype.mask1 & b->datatype.mask1));
            }
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            if( ur_type(a) == ur_type(b) )
            {
                if( ur_datatype(a) > ur_datatype(b) )
                    return 1;
                if( ur_datatype(a) < ur_datatype(b) )
                    return -1;
                // Order of two multi-types is undefined.
            }
            break;
    }
    return 0;
}


void datatype_toString(UThread* ut, const UCell* cell, UBuffer* str, int depth)
{
    int dt = ur_datatype(cell);
    (void) depth;
    if( dt < UT_MAX )
    {
        ur_strAppendCStr( str, ur_atomCStr( ut, dt ) );
    }
    else
    {
        uint32_t mask;
        uint32_t dtBits;

        dt = 0;
        dtBits = cell->datatype.mask0;
loop:
        for( mask = 1; dtBits; ++dt, mask <<= 1 )
        {
            if( mask & dtBits )
            {
                dtBits &= ~mask;
                ur_strAppendCStr( str, ur_atomCStr( ut, dt ) );
                ur_strAppendChar( str, '/' );
            }
        }
        if( dt <= 32 )
        {
            if( (dtBits = cell->datatype.mask1) )
            {
                dt = 32;
                goto loop;
            }
        }
        --str->used;    // Remove extra '/'.
    }
}


UDatatype dt_datatype =
{
    "datatype!",
    datatype_make,          datatype_make,          unset_copy,
    datatype_compare,       unset_operate,          unset_select,
    datatype_toString,      datatype_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


//----------------------------------------------------------------------------
// UT_NONE


int none_make( UThread* ut, const UCell* from, UCell* res )
{
    (void) ut;
    (void) from;
    ur_setId(res, UT_NONE);
    return UR_OK;
}


int none_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_SAME:
        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            return ur_type(a) == ur_type(b);
    }
    return 0;
}


void none_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) ut;
    (void) cell;
    (void) depth;
    ur_strAppendCStr( str, "none" );
}


UDatatype dt_none =
{
    "none!",
    none_make,              none_make,              unset_copy,
    none_compare,           unset_operate,          unset_select,
    none_toString,          none_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


//----------------------------------------------------------------------------
// UT_LOGIC


int logic_make( UThread* ut, const UCell* from, UCell* res )
{
    (void) ut;
    ur_setId(res, UT_LOGIC);
    switch( ur_type(from) )
    {
        case UT_NONE:
            ur_int(res) = 0;
            break;
        case UT_LOGIC:
        case UT_CHAR:
        case UT_INT:
            ur_int(res) = ur_int(from) ? 1 : 0;
            break;
        case UT_DECIMAL:
            ur_int(res) = ur_decimal(from) ? 1 : 0;
            break;
        case UT_BIGNUM:
        {
            UCell tmp;
            bignum_zero( &tmp );
            ur_int(res) = bignum_equal(from, &tmp) ? 0 : 1;
        }
            break;
        default:
            ur_int(res) = 1;
            break;
    }
    return UR_OK;
}


int logic_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_SAME:
            return ur_int(a) == ur_int(b);

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) == ur_type(b) )
                return ur_int(a) == ur_int(b);
            break;
    }
    return 0;
}


void logic_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) ut;
    (void) depth;
    ur_strAppendCStr( str, ur_int(cell) ? "true" : "false" );
}


int int_operate( UThread* ut, const UCell* a, const UCell* b, UCell*, int );

UDatatype dt_logic =
{
    "logic!",
    logic_make,             logic_make,             unset_copy,
    logic_compare,          int_operate,            unset_select,
    logic_toString,         logic_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind,
};


//----------------------------------------------------------------------------
// UT_CHAR


int char_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_INT) || ur_is(from, UT_CHAR) )
    {
        ur_setId(res, UT_CHAR);
        ur_int(res) = ur_int(from);
        return UR_OK;
    }
    else if( ur_is(from, UT_STRING) )
    {
        USeriesIter si;
        ur_seriesSlice( ut, &si, from );
        SERIES_DT( UT_STRING )->pick( si.buf, si.it, res );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "make char! expected char!/int!/string!" );
}


extern int copyUcs2ToUtf8( uint8_t* dest, const uint16_t* src, int srcLen );
extern char _hexDigits[];

void char_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    char tmp[5];
    char* cstr;
    uint16_t n = ur_int(cell);
    (void) ut;
    (void) depth;

    if( n > 127 )
    {
        if( str->form == UR_ENC_UCS2 )
        {
            uint16_t* cp;
            ur_arrReserve( str, str->used + 3 );
            cp = str->ptr.u16 + str->used;
            *cp++ = '\'';
            *cp++ = n;
            *cp   = '\'';
            str->used += 3;
        }
        else
        {
            uint8_t* cp;
            ur_arrReserve( str, str->used + 9 );
            cp = str->ptr.b + str->used;
            *cp++ = '\'';
            if( str->form == UR_ENC_UTF8 )
            {
                cp += copyUcs2ToUtf8( cp, &n, 1 );
            }
            else
            {
                *cp++ = '^';
                *cp++ = '(';
                if( n & 0xff00 )
                {
                    *cp++ = _hexDigits[ ((n >> 12) & 0xf) ];
                    *cp++ = _hexDigits[ ((n >>  8) & 0xf) ];
                }
                *cp++ = _hexDigits[ ((n >> 4) & 0xf) ];
                *cp++ = _hexDigits[ (n & 0xf) ];
                *cp++ = ')';
            }
            *cp++ = '\'';
            str->used = cp - str->ptr.b;
        }
        return;
    }

    if( n < 16 )
    {
        if( n == '\n' )
            cstr = "'^/'";
        else if( n == '\t' )
            cstr = "'^-'";
        else
        {
            cstr = tmp;
            *cstr++ = '\'';
            *cstr++ = '^';
            n += ((n < 11) ? '0' : ('A' - 10));
            goto close_esc;
        }
    }
    else
    {
        cstr = tmp;
        *cstr++ = '\'';
        if( n == '^' || n == '\'' )
            *cstr++ = '^';
close_esc:
        *cstr++ = n;
        *cstr++ = '\'';
        *cstr = '\0';
        cstr = tmp;
    }
    ur_strAppendCStr( str, cstr );
}


void char_toText( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) ut;
    (void) depth;
    ur_strAppendChar( str, ur_int(cell) );
}


int int_compare( UThread* ut, const UCell* a, const UCell* b, int test );

UDatatype dt_char =
{
    "char!",
    char_make,              char_make,              unset_copy,
    int_compare,            int_operate,            unset_select,
    char_toString,          char_toText,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


//----------------------------------------------------------------------------
// UT_INT


extern int64_t str_toInt64( const char*, const char*, const char** pos );
extern int64_t str_hexToInt64( const char*, const char*, const char** pos );

#define MAKE_NO_UCS2(tn) \
    ur_error(ut,UR_ERR_INTERNAL,"make %s does not handle UCS2 strings",tn)

int int_make( UThread* ut, const UCell* from, UCell* res )
{
    ur_setId(res, UT_INT);
    switch( ur_type(from) )
    {
        case UT_NONE:
            ur_int(res) = 0;
            break;
        case UT_LOGIC:
        case UT_CHAR:
        case UT_INT:
            ur_int(res) = ur_int(from);
            break;
        case UT_DECIMAL:
        case UT_TIME:
        case UT_DATE:
            ur_int(res) = ur_decimal(from);
            break;
        case UT_BIGNUM:
            ur_int(res) = (int32_t) bignum_l(from);
            break;
        case UT_BINARY:
        case UT_STRING:
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, from );
            if( ur_strIsUcs2(si.buf) && ur_is(from, UT_STRING) )
            {
                return MAKE_NO_UCS2( "int!" );
            }
            else
            {
                const char* cp  = si.buf->ptr.c + si.it;
                const char* end = si.buf->ptr.c + si.end;
                if( (si.end - si.it) > 2 && (cp[0] == '0') && (cp[1] == 'x') )
                {
                    ur_int(res) = (int32_t) str_hexToInt64( cp + 2, end, 0 );
                    ur_setFlags(res, UR_FLAG_INT_HEX);
                }
                else
                    ur_int(res) = (int32_t) str_toInt64( cp, end, 0 );
            }
        }
            break;
        default:
            return ur_error( ut, UR_ERR_TYPE,
                "make int! expected number or none!/logic!/char!/string!" );
    }
    return UR_OK;
}


#define MASK_CHAR_INT   ((1 << UT_CHAR) | (1 << UT_INT))
#define ur_isIntType(T) ((1 << T) & MASK_CHAR_INT)

int int_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;

    if( test == UR_COMPARE_SAME )
        return ur_int(a) == ur_int(b);

    if( ur_isIntType( ur_type(a) ) && ur_isIntType( ur_type(b) ) )
    {
        switch( test )
        {
            case UR_COMPARE_EQUAL:
            case UR_COMPARE_EQUAL_CASE:
                return ur_int(a) == ur_int(b);

            case UR_COMPARE_ORDER:
            case UR_COMPARE_ORDER_CASE:
                if( ur_int(a) > ur_int(b) )
                    return 1;
                if( ur_int(a) < ur_int(b) )
                    return -1;
                break;
        }
    }
    return 0;
}


int int_operate( UThread* ut, const UCell* a, const UCell* b, UCell* res,
                 int op )
{
    if( ur_isIntType( ur_type(a) ) && ur_isIntType( ur_type(b) ) )
    {
        ur_setId(res, ur_type(a));
        switch( op )
        {
            case UR_OP_ADD:
                ur_int(res) = ur_int(a) + ur_int(b);
                break;
            case UR_OP_SUB:
                ur_int(res) = ur_int(a) - ur_int(b);
                break;
            case UR_OP_MUL:
                ur_int(res) = ur_int(a) * ur_int(b);
                break;
            case UR_OP_DIV:
                if( ur_int(b) == 0 )
                    goto div_by_zero;
                ur_int(res) = ur_int(a) / ur_int(b);
                break;
            case UR_OP_MOD:
                if( ur_int(b) == 0 )
                    goto div_by_zero;
                ur_int(res) = ur_int(a) % ur_int(b);
                break;
            case UR_OP_AND:
                ur_int(res) = ur_int(a) & ur_int(b);
                break;
            case UR_OP_OR:
                ur_int(res) = ur_int(a) | ur_int(b);
                break;
            case UR_OP_XOR:
                ur_int(res) = ur_int(a) ^ ur_int(b);
                break;
            default:
                return unset_operate( ut, a, b, res, op );
        }
        return UR_OK;
    }
    else if( ur_is(a, UT_LOGIC) || ur_is(b, UT_LOGIC) )
    {
        ur_setId(res, ur_type(a));
        switch( op )
        {
            case UR_OP_AND:
                ur_int(res) = ur_int(a) & ur_int(b);
                break;
            case UR_OP_OR:
                ur_int(res) = ur_int(a) | ur_int(b);
                break;
            case UR_OP_XOR:
                ur_int(res) = ur_int(a) ^ ur_int(b);
                break;
            default:
                return unset_operate( ut, a, b, res, op );
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "int! operator exepected logic!/char!/int!" );

div_by_zero:

    return ur_error( ut, UR_ERR_SCRIPT, "int! divide by zero" );
}


void int_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) ut;
    (void) depth;
    if( ur_flags(cell, UR_FLAG_INT_HEX) )
    {
        ur_strAppendCStr( str, "0x" );
        ur_strAppendHex( str, ur_int(cell), 0 );
    }
    else
        ur_strAppendInt( str, ur_int(cell) );
}


UDatatype dt_int =
{
    "int!",
    int_make,               int_make,               unset_copy,
    int_compare,            int_operate,            unset_select,
    int_toString,           int_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


//----------------------------------------------------------------------------
// UT_DECIMAL


extern double str_toDouble( const char*, const char*, const char** pos );

int decimal_make( UThread* ut, const UCell* from, UCell* res )
{
    ur_setId(res, UT_DECIMAL);
    switch( ur_type(from) )
    {
        case UT_NONE:
            ur_decimal(res) = 0.0;
            break;
        case UT_LOGIC:
        case UT_CHAR:
        case UT_INT:
            ur_decimal(res) = (double) ur_int(from);
            break;
        case UT_DECIMAL:
        case UT_TIME:
        case UT_DATE:
            ur_decimal(res) = ur_decimal(from);
            break;
        case UT_BIGNUM:
            ur_decimal(res) = bignum_d(from);
            break;
        case UT_STRING:
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, from );
            if( ur_strIsUcs2(si.buf) )
                return MAKE_NO_UCS2( "decimal!" );
            else
                ur_decimal(res) = str_toDouble( si.buf->ptr.c + si.it,
                                                si.buf->ptr.c + si.end, 0 );
        }
            break;
        default:
            return ur_error( ut, UR_ERR_TYPE,
                "make decimal! expected number or none!/logic!/char!/string!" );
    }
    return UR_OK;
}


#define MASK_DECIMAL    ((1 << UT_DECIMAL) | (1 << UT_TIME) | (1 << UT_DATE))
#define ur_isDecimalType(T) ((1 << T) & MASK_DECIMAL)

#define FLOAT_EPSILON   (0.00000005960464477539062 * 2.0)

// Compare doubles which may have been floats (e.g. vec3! elements).
static int float_equal( double a, double b )
{
    return a >= (b - FLOAT_EPSILON) && a <= (b + FLOAT_EPSILON);
}


int decimal_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;

    switch( test )
    {
        case UR_COMPARE_SAME:
            return ur_decimal(a) == ur_decimal(b);

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_isDecimalType( ur_type(a) ) )
            {
                if( ur_isDecimalType( ur_type(b) ) )
                    return float_equal( ur_decimal(a), ur_decimal(b) );
                else if( ur_isIntType( ur_type(b) ) )
                    return float_equal( ur_decimal(a), ur_int(b) );
            }
            else
            {
                if( ur_isIntType( ur_type(a) ) )
                    return float_equal( (double) ur_int(a), ur_decimal(b) );
            }
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            if( ur_isDecimalType( ur_type(a) ) )
            {
                if( ur_isDecimalType( ur_type(b) ) )
                {
                    if( ur_decimal(a) > ur_decimal(b) )
                        return 1;
                    if( ur_decimal(a) < ur_decimal(b) )
                        return -1;
                }
                else if( ur_isIntType( ur_type(b) ) )
                {
                    if( ur_decimal(a) > ur_int(b) )
                        return 1;
                    if( ur_decimal(a) < ur_int(b) )
                        return -1;
                }
            }
            else
            {
                if( ur_isIntType( ur_type(a) ) )
                {
                    if( ((double) ur_int(a)) > ur_decimal(b) )
                        return 1;
                    if( ((double) ur_int(a)) < ur_decimal(b) )
                        return -1;
                }
            }
            break;
    }
    return 0;
}


int decimal_operate( UThread* ut, const UCell* a, const UCell* b, UCell* res,
                     int op )
{
    double da;
    double db;
    int t;

    t = ur_type(a);
    if( ur_isDecimalType( t ) )
        da = ur_decimal(a);
    else if( ur_isIntType( t ) )
        da = ur_int(a);
    else
        goto bad_type;

    if( ur_isDecimalType( ur_type(b) ) )
    {
        if( t < UT_DECIMAL )
            t = ur_type(b);
        db = ur_decimal(b);
    }
    else if( ur_isIntType( ur_type(b) ) )
        db = ur_int(b);
    else
        goto bad_type;

    ur_setId(res, t);
    switch( op )
    {
        case UR_OP_ADD:
            ur_decimal(res) = da + db;
            break;
        case UR_OP_SUB:
            ur_decimal(res) = da - db;
            break;
        case UR_OP_MUL:
            ur_decimal(res) = da * db;
            break;
        case UR_OP_DIV:
            if( db == 0.0 )
                goto div_by_zero;
            ur_decimal(res) = da / db;
            break;
        case UR_OP_MOD:
            if( db == 0.0 )
                goto div_by_zero;
            ur_decimal(res) = fmod(da, db);
            break;
        case UR_OP_AND:
        case UR_OP_OR:
        case UR_OP_XOR:
            ur_decimal(res) = 0.0;
            break;
        default:
            return unset_operate( ut, a, b, res, op );
    }
    return UR_OK;

bad_type:

    return ur_error( ut, UR_ERR_TYPE,
             "decimal! operator exepected char!/int!/decimal!/time!/date!" );

div_by_zero:

    return ur_error( ut, UR_ERR_SCRIPT, "decimal! divide by zero" );
}


void decimal_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) ut;
    (void) depth;
    ur_strAppendDouble( str, ur_decimal(cell) );
}


UDatatype dt_decimal =
{
    "decimal!",
    decimal_make,           decimal_make,           unset_copy,
    decimal_compare,        decimal_operate,        unset_select,
    decimal_toString,       decimal_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


//----------------------------------------------------------------------------
// UT_BIGNUM


#include "bignum.h"


int bignum_make( UThread* ut, const UCell* from, UCell* res )
{
    switch( ur_type(from) )
    {
        case UT_NONE:
            ur_setId(res, UT_BIGNUM);
            bignum_zero( res );
            break;
        case UT_LOGIC:
        case UT_CHAR:
        case UT_INT:
            ur_setId(res, UT_BIGNUM);
            bignum_seti( res, ur_int(from) );
            break;
        case UT_DECIMAL:
            ur_setId(res, UT_BIGNUM);
            bignum_setd( res, ur_decimal(from) );
            break;
        case UT_BIGNUM:
            *res = *from;
            break;
        case UT_STRING:
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, from );
            ur_setId(res, UT_BIGNUM);
            if( ur_strIsUcs2(si.buf) )
            {
                return MAKE_NO_UCS2( "bignum!" );
            }
            else
            {
                const char* cp = si.buf->ptr.c;
                if( (si.end - si.it) > 2 && (cp[0] == '0') && (cp[1] == 'x') )
                {
                    bignum_setl(res, str_hexToInt64(cp + si.it + 2,
                                                    cp + si.end, 0) );
                    ur_setFlags(res, UR_FLAG_INT_HEX);
                }
                else
                    bignum_setl(res, str_toInt64(cp + si.it,
                                                 cp + si.end, 0) );
            }
        }
            break;
        default:
            return ur_error( ut, UR_ERR_TYPE,
                "make bignum! expected number or none!/logic!/char!/string!" );
    }
    return UR_OK;
}


void bignum_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    int64_t n = bignum_l(cell);
    (void) ut;
    (void) depth;
    if( ur_flags(cell, UR_FLAG_INT_HEX) )
    {
        ur_strAppendCStr( str, "0x" );
        ur_strAppendHex( str, n & 0xffffffff, n >> 32 );
    }
    else
        ur_strAppendInt64( str, n );
}


int bignum_operate( UThread* ut, const UCell* a, const UCell* b, UCell* res,
                    int op )
{
    UCell tmp;

    if( ur_isIntType( ur_type(a) ) )
    {
        bignum_seti( &tmp, ur_int(a) );
        a = &tmp;
    }
    else if( ur_is(a, UT_DECIMAL) )
    {
        bignum_setd( &tmp, ur_decimal(a) );
        a = &tmp;
    }
    else if( ur_isIntType( ur_type(b) ) )
    {
        bignum_seti( &tmp, ur_int(b) );
        b = &tmp;
    }
    else if( ur_is(b, UT_DECIMAL) )
    {
        bignum_setd( &tmp, ur_decimal(b) );
        b = &tmp;
    }
    else if( ! ur_is(a, UT_BIGNUM) || ! ur_is(b, UT_BIGNUM) )
        goto unset;

    ur_setId(res, UT_BIGNUM);
    switch( op )
    {
        case UR_OP_ADD:
            bignum_add( a, b, res );
            return UR_OK;
        case UR_OP_SUB:
            bignum_sub( a, b, res );
            return UR_OK;
        case UR_OP_MUL:
            bignum_mul( a, b, res );
            return UR_OK;
    }

unset:
    return unset_operate( ut, a, b, res, op );
}


UDatatype dt_bignum =
{
    "bignum!",
    bignum_make,            bignum_make,            unset_copy,
    unset_compare,          bignum_operate,         unset_select,
    bignum_toString,        bignum_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


//----------------------------------------------------------------------------
// UT_TIME


double str_toTime( const char* start, const char* end, const char** pos );

int time_make( UThread* ut, const UCell* from, UCell* res )
{
    switch( ur_type(from) )
    {
        case UT_INT:
            ur_setId(res, UT_TIME);
            ur_decimal(res) = (double) ur_int(from);
            break;
        case UT_DECIMAL:
        case UT_TIME:
        case UT_DATE:
            ur_setId(res, UT_TIME);
            ur_decimal(res) = ur_decimal(from);
            break;
        case UT_STRING:
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, from );
            if( ur_strIsUcs2(si.buf) )
            {
                return MAKE_NO_UCS2( "time!" );
            }
            else
            {
                const char* cp  = si.buf->ptr.c;
                ur_setId(res, UT_TIME);
                ur_decimal(res) = str_toTime( cp + si.it, cp + si.end, 0 );
            }
        }
            break;
        default:
            return ur_error( ut, UR_ERR_TYPE,
                "make time! expected int!/decimal!/time!/date!/string!" );
    }
    return UR_OK;
}


int time_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;

    switch( test )
    {
        case UR_COMPARE_SAME:
            return ur_decimal(a) == ur_decimal(b);

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) == ur_type(b) )
                return float_equal( ur_decimal(a), ur_decimal(b) );
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            if( ur_type(a) == ur_type(b) )
            {
                if( ur_decimal(a) > ur_decimal(b) )
                    return 1;
                if( ur_decimal(a) < ur_decimal(b) )
                    return -1;
            }
            break;
    }
    return 0;
}


extern int fpconv_ftoa( double, char* );

void time_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    int seg;
    double n = ur_decimal(cell);
    (void) ut;
    (void) depth;


    if( n < 0.0 )
    {
        n = -n;
        ur_strAppendChar( str, '-' );
    }

    // Hours
    seg = (int) (n / 3600.0);
    if( seg )
        n -= seg * 3600.0;
    ur_strAppendInt( str, seg );
    ur_strAppendChar( str, ':' );

    // Minutes
    seg = (int) (n / 60.0);
    if( seg )
        n -= seg * 60.0;
    if( seg < 10 )
        ur_strAppendChar( str, '0' );
    ur_strAppendInt( str, seg );
    ur_strAppendChar( str, ':' );

    // Seconds
    if( n < 10.0 )
        ur_strAppendChar( str, '0' );

#if 1
    {
    // Limit significant digits and round as ur_strAppendFloat() does but
    // without losing precision through float cast.
    char buf[26];
    int len = fpconv_ftoa( n, buf );
    buf[ len ] = '\0';
    ur_strAppendCStr( str, buf );
    }
#else
    ur_strAppendDouble( str, n );
#endif
}


UDatatype dt_time =
{
    "time!",
    time_make,              time_make,              unset_copy,
    time_compare,           decimal_operate,        unset_select,
    time_toString,          time_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


//----------------------------------------------------------------------------
// UT_DATE


extern void date_toString( UThread*, const UCell*, UBuffer*, int );
extern double ur_stringToDate( const char*, const char*, const char** );


int date_make( UThread* ut, const UCell* from, UCell* res )
{
    switch( ur_type(from) )
    {
        case UT_TIME:
        case UT_DATE:
            ur_setId(res, UT_DATE);
            ur_decimal(res) = ur_decimal(from);
            break;
        case UT_STRING:
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, from );
            if( ur_strIsUcs2(si.buf) )
            {
                return MAKE_NO_UCS2( "date!" );
            }
            else
            {
                const char* cp  = si.buf->ptr.c;
                ur_setId(res, UT_DATE);
                ur_decimal(res) = ur_stringToDate( cp + si.it, cp + si.end, 0 );
            }
        }
            break;
        default:
            return ur_error( ut, UR_ERR_TYPE,
                "make date! expected time!/date!/string!" );
    }
    return UR_OK;
}


int date_operate( UThread* ut, const UCell* a, const UCell* b, UCell* res,
                  int op )
{
    UCell tmp;

    if( ur_is(a, UT_INT) )
    {
        ur_int(&tmp) = ur_int(a);
        a = &tmp;
        goto set_days;
    }
    else if( ur_is(b, UT_INT) )
    {
        ur_int(&tmp) = ur_int(b);
        b = &tmp;
set_days:
        ur_setId(&tmp, UT_INT);
        ur_int(&tmp) *= 86400;
    }
    return decimal_operate( ut, a, b, res, op );
}


UDatatype dt_date =
{
    "date!",
    date_make,              date_make,              unset_copy,
    time_compare,           date_operate,           unset_select,
    date_toString,          date_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


//----------------------------------------------------------------------------
// UT_VEC3


static void vec3_setf( UCell* res, float n )
{
    float* f = res->vec3.xyz;
    f[0] = f[1] = f[2] = n;
}


extern int vector_pickFloatV( const UBuffer*, UIndex n, float* fv, int count );

int vec3_make( UThread* ut, const UCell* from, UCell* res )
{
    ur_setId(res, UT_VEC3);
    switch( ur_type(from) )
    {
        case UT_NONE:
            vec3_setf( res, 0.0f );
            break;
        case UT_LOGIC:
        case UT_INT:
            vec3_setf( res, (float) ur_int(from) );
            break;
        case UT_DECIMAL:
            vec3_setf( res, (float) ur_decimal(from) );
            break;
        case UT_COORD:
            res->vec3.xyz[0] = (float) from->coord.n[0];
            res->vec3.xyz[1] = (float) from->coord.n[1];
            res->vec3.xyz[2] = (from->coord.len > 2) ?
                               (float) from->coord.n[2] : 0.0;
            break;
        case UT_VEC3:
            memCpy( res->vec3.xyz, from->vec3.xyz, 3 * sizeof(float) );
            break;
        case UT_BLOCK:
        {
            UBlockIter bi;
            const UCell* cell;
            float num;
            int len = 0;

            ur_blkSlice( ut, &bi, from );
            ur_foreach( bi )
            {
                if( ur_is(bi.it, UT_WORD) )
                {
                    cell = ur_wordCell( ut, bi.it );
                    if( ! cell )
                        return UR_THROW;
                }
#if 0
                else if( ur_is(bi.it, UT_PATH) )
                {
                    if( ! ur_pathCell( ut, bi.it, res ) )
                        return UR_THROW;
                }
#endif
                else
                {
                    cell = bi.it;
                }

                if( ur_is(cell, UT_INT) )
                    num = (float) ur_int(cell);
                else if( ur_is(cell, UT_DECIMAL) )
                    num = (float) ur_decimal(cell);
                else
                    break;

                res->vec3.xyz[ len ] = num;
                if( ++len == 3 )
                    return UR_OK;
            }
            while( len < 3 )
                res->vec3.xyz[ len++ ] = 0.0f;
        }
            break;
        case UT_VECTOR:
        {
            int len;
            len = vector_pickFloatV( ur_bufferSer(from), from->series.it,
                                     res->vec3.xyz, 3 );
            while( len < 3 )
                res->vec3.xyz[ len++ ] = 0.0f;
        }
            break;
        default:
            return ur_error( ut, UR_ERR_TYPE,
                    "make vec3! expected none!/logic!/int!/decimal!/block!" );
    }
    return UR_OK;
}


void vec3_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) ut;
    for( depth = 0; depth < 3; ++depth )
    {
        if( depth )
            ur_strAppendChar( str, ',' );
        ur_strAppendFloat( str, cell->vec3.xyz[ depth ] );
    }
}


/* index is zero-based */
void vec3_pick( const UCell* cell, int index, UCell* res )
{
    if( (index < 0) || (index >= 3) )
    {
        ur_setId(res, UT_NONE);
    }
    else
    {
        ur_setId(res, UT_DECIMAL);
        ur_decimal(res) = cell->vec3.xyz[ index ];
    }
}


/* index is zero-based */
int vec3_poke( UThread* ut, UCell* cell, int index, const UCell* src )
{
    float num;

    if( (index < 0) || (index >= 3) )
        return ur_error( ut, UR_ERR_SCRIPT, "poke vec3! index out of range" );

    if( ur_is(src, UT_DECIMAL) )
        num = (float) ur_decimal(src);
    else if( ur_is(src, UT_INT) )
        num = (float) ur_int(src);
    else
        return ur_error( ut, UR_ERR_TYPE, "poke vec3! expected int!/decimal!" );

    cell->vec3.xyz[ index ] = num;
    return UR_OK;
}


int vec3_compare( UThread* ut, const UCell* a, const UCell* b, int test )
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
        {
            const float* pa = a->vec3.xyz;
            const float* pb = b->vec3.xyz;
            if( (pa[0] != pb[0]) || (pa[1] != pb[1]) || (pa[2] != pb[2]) )
                return 0;
            return 1;
        }
            break;
    
        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            if( ur_type(a) == ur_type(b) )
            {
                const float* pa = a->vec3.xyz;
                const float* aend = pa + 3;
                const float* pb = b->vec3.xyz;
                while( pa != aend )
                {
                    if( *pa > *pb )
                        return 1;
                    if( *pa < *pb )
                        return -1;
                    ++pa;
                    ++pb;
                }
            }
            break;
    }
    return 0;
}


static const float* _load3f( const UCell* cell, float* tmp )
{
    switch( ur_type(cell) )
    {
        case UT_INT:
            tmp[0] = tmp[1] = tmp[2] = (float) ur_int(cell);
            break;

        case UT_DECIMAL:
            tmp[0] = tmp[1] = tmp[2] = (float) ur_decimal(cell);
            break;

        case UT_COORD:
            tmp[0] = (float) cell->coord.n[0];
            tmp[1] = (float) cell->coord.n[1];
            tmp[2] = (cell->coord.len > 2) ? (float) cell->coord.n[2] : 0.0f;
            break;

        case UT_VEC3:
            return cell->vec3.xyz;

        default:
            return 0;
    }
    return tmp;
}


#define OPER_V3(OP) \
    res->vec3.xyz[0] = va[0] OP vb[0]; \
    res->vec3.xyz[1] = va[1] OP vb[1]; \
    res->vec3.xyz[2] = va[2] OP vb[2]

int vec3_operate( UThread* ut, const UCell* a, const UCell* b, UCell* res,
                  int op )
{
    float tmp[ 3 ];
    const float* va;
    const float* vb;

    va = _load3f( a, tmp );
    if( ! va )
        goto bad_type;
    vb = _load3f( b, tmp );
    if( ! vb )
        goto bad_type;

    ur_setId(res, UT_VEC3);
    switch( op )
    {
        case UR_OP_ADD:
            OPER_V3( + );
            break;
        case UR_OP_SUB:
            OPER_V3( - );
            break;
        case UR_OP_MUL:
            OPER_V3( * );
            break;
        case UR_OP_DIV:
            OPER_V3( / );
            break;
        default:
            return unset_operate( ut, a, b, res, op );
    }
    return UR_OK;

bad_type:

    return ur_error( ut, UR_ERR_TYPE,
                     "vec3! operator exepected int!/decimal!/coord!/vec3!" );
}


static
const UCell* vec3_select( UThread* ut, const UCell* cell, const UCell* sel,
                          UCell* tmp )
{
    if( ur_is(sel, UT_INT) )
    {
        vec3_pick( cell, ur_int(sel) - 1, tmp );
        return tmp;
    }
    ur_error( ut, UR_ERR_SCRIPT, "vec3 select expected int!" );
    return 0;
}


UDatatype dt_vec3 =
{
    "vec3!",
    vec3_make,              vec3_make,              unset_copy,
    vec3_compare,           vec3_operate,           vec3_select,
    vec3_toString,          vec3_toString,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
};


//----------------------------------------------------------------------------
// UT_WORD


int word_makeType( UThread* ut, const UCell* from, UCell* res, int ntype )
{
    UAtom atom;
    int type = ur_type(from);

    if( ur_isWordType( type ) )
    {
        *res = *from;
        ur_type(res) = ntype;
        return UR_OK;
    }
    else if( type == UT_STRING )
    {
        USeriesIter si;

        ur_seriesSlice( ut, &si, from );
        if( si.buf->form == UR_ENC_LATIN1 )
        {
            atom = ur_internAtom( ut, si.buf->ptr.c + si.it,
                                      si.buf->ptr.c + si.end );
        }
        else
        {
            UBuffer tmp;
            ur_strInit( &tmp, UR_ENC_LATIN1, 0 );
            ur_strAppend( &tmp, si.buf, si.it, si.end );
            atom = ur_internAtom( ut, tmp.ptr.c, tmp.ptr.c + tmp.used );
            ur_strFree( &tmp );
        }
        if( atom == UR_INVALID_ATOM )
            return UR_THROW;
set_atom:
        ur_setId(res, ntype);
        ur_setWordUnbound(res, atom);
        return UR_OK;
    }
    else if( type == UT_DATATYPE )
    {
        atom = ur_datatype(from);
        if( atom < UT_MAX )
            goto set_atom;
    }
    return ur_error( ut, UR_ERR_TYPE, "make word! expected word!/string!" );
}


int word_make( UThread* ut, const UCell* from, UCell* res )
{
    return word_makeType( ut, from, res, UT_WORD );
}


/*
  Returns atom (if cell is any word), datatype atom (if cell is a simple
  datatype), or -1.
*/
static int word_atomOrType( const UCell* cell )
{
    int type = ur_type(cell);
    if( ur_isWordType(type) )
        return ur_atom(cell);
    if( type == UT_DATATYPE )
    {
        type = ur_datatype(cell);
        if( type < UT_MAX )
            return type;
    }
    return -1;
}


int compare_ic_uint8_t( const uint8_t*, const uint8_t*,
                        const uint8_t*, const uint8_t* );

int word_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    switch( test )
    {
        case UR_COMPARE_SAME:
            return ((ur_atom(a) == ur_atom(b)) &&
                    (ur_binding(a) == ur_binding(b)) &&
                    (a->word.ctx == b->word.ctx));

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
        {
            int atomA = word_atomOrType( a );
            if( (atomA > -1) && (atomA == word_atomOrType(b)) )
                return 1;
        }
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            if( ur_type(a) == ur_type(b) )
            {
#define ATOM_LEN(str)   strLen((const char*) str)
                const uint8_t* strA = (const uint8_t*) ur_wordCStr(a);
                const uint8_t* strB = (const uint8_t*) ur_wordCStr(b);
                int (*func)(const uint8_t*, const uint8_t*,
                            const uint8_t*, const uint8_t* );
                func = (test == UR_COMPARE_ORDER) ? compare_ic_uint8_t
                                                  : compare_uint8_t;
                return func( strA, strA + ATOM_LEN(strA),
                             strB, strB + ATOM_LEN(strB) );
            }
            break;
    }
    return 0;
}


void word_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) depth;
    ur_strAppendCStr( str, ur_wordCStr( cell ) );
}


void word_mark( UThread* ut, UCell* cell )
{
    if( ur_binding(cell) == UR_BIND_THREAD )
    {
        UIndex n = cell->word.ctx;
        if( ur_markBuffer( ut, n ) )
            context_markBuf( ut, ur_buffer(n) );
    }
}


void word_toShared( UCell* cell )
{
    if( ur_binding(cell) == UR_BIND_THREAD )
    {
        ur_setBinding( cell, UR_BIND_ENV );
        cell->word.ctx = -cell->word.ctx;
    }
#if 1
    // FIXME: The core library should have no knowledge of other binding types.
    else if( ur_binding(cell) >= UR_BIND_USER ) // UR_BIND_FUNC, UR_BIND_OPTION
        cell->word.ctx = -cell->word.ctx;
#endif
}


UDatatype dt_word =
{
    "word!",
    word_make,              word_make,              unset_copy,
    word_compare,           unset_operate,          unset_select,
    word_toString,          word_toString,
    unset_recycle,          word_mark,              unset_destroy,
    unset_markBuf,          word_toShared,          unset_bind
};


//----------------------------------------------------------------------------
// UT_LITWORD


int litword_make( UThread* ut, const UCell* from, UCell* res )
{
    return word_makeType( ut, from, res, UT_LITWORD );
}


void litword_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) depth;
    ur_strAppendChar( str, '\'' );
    ur_strAppendCStr( str, ur_wordCStr( cell ) );
}


UDatatype dt_litword =
{
    "lit-word!",
    litword_make,           litword_make,           unset_copy,
    word_compare,           unset_operate,          unset_select,
    litword_toString,       word_toString,
    unset_recycle,          word_mark,              unset_destroy,
    unset_markBuf,          word_toShared,          unset_bind
};


//----------------------------------------------------------------------------
// UT_SETWORD


int setword_make( UThread* ut, const UCell* from, UCell* res )
{
    return word_makeType( ut, from, res, UT_SETWORD );
}


void setword_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) depth;
    ur_strAppendCStr( str, ur_wordCStr( cell ) );
    ur_strAppendChar( str, ':' );
}


UDatatype dt_setword =
{
    "set-word!",
    setword_make,           setword_make,           unset_copy,
    word_compare,           unset_operate,          unset_select,
    setword_toString,       word_toString,
    unset_recycle,          word_mark,              unset_destroy,
    unset_markBuf,          word_toShared,          unset_bind
};


//----------------------------------------------------------------------------
// UT_GETWORD


int getword_make( UThread* ut, const UCell* from, UCell* res )
{
    return word_makeType( ut, from, res, UT_GETWORD );
}


void getword_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) depth;
    ur_strAppendChar( str, ':' );
    ur_strAppendCStr( str, ur_wordCStr( cell ) );
}


UDatatype dt_getword =
{
    "get-word!",
    getword_make,           getword_make,           unset_copy,
    word_compare,           unset_operate,          unset_select,
    getword_toString,       word_toString,
    unset_recycle,          word_mark,              unset_destroy,
    unset_markBuf,          word_toShared,          unset_bind
};


//----------------------------------------------------------------------------
// UT_OPTION


void option_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    (void) depth;
    ur_strAppendChar( str, '/' );
    ur_strAppendCStr( str, ur_wordCStr( cell ) );
}


UDatatype dt_option =
{
    "option!",
    unset_make,             unset_make,             unset_copy,
    word_compare,           unset_operate,          unset_select,
    option_toString,        option_toString,
    unset_recycle,          word_mark,              unset_destroy,
    unset_markBuf,          word_toShared,          unset_bind
};


//----------------------------------------------------------------------------
// UT_BINARY


void binary_copy( UThread* ut, const UCell* from, UCell* res )
{
    UBinaryIter bi;
    UIndex n;
    int len;

    ur_binSlice( ut, &bi, from );
    len = bi.end - bi.it;
    n = ur_makeBinary( ut, len );       // Invalidates bi.buf.
    if( len )
        ur_binAppendData( ur_buffer(n), bi.it, len );

    ur_setId( res, ur_type(from) );     // Handle binary! & bitset!
    ur_setSeries( res, n, 0 );
}


int binary_make( UThread* ut, const UCell* from, UCell* res )
{
    int type = ur_type(from);
    if( type == UT_INT )
    {
        ur_makeBinaryCell( ut, ur_int(from), res );
        return UR_OK;
    }
    else if( type == UT_BINARY )
    {
        binary_copy( ut, from, res );
        return UR_OK;
    }
    else if( ur_isStringType(type) || (type == UT_VECTOR) )
    {
        USeriesIter si;
        UBuffer* bin;

        bin = ur_makeBinaryCell( ut, 0, res );
        ur_seriesSlice( ut, &si, from );
        ur_binAppendArray( bin, &si );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "make binary! expected int!/binary!/string!/file!" );
}

 
int binary_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    switch( test )
    {
        case UR_COMPARE_SAME:
            return ((a->series.buf == b->series.buf) &&
                    (a->series.it == b->series.it) &&
                    (a->series.end == b->series.end));

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ! ur_is(a, UT_BINARY) || ! ur_is(b, UT_BINARY) )
                break;
            if( (a->series.buf == b->series.buf) &&
                (a->series.it == b->series.it) &&
                (a->series.end == b->series.end) )
                return 1;
            {
            USeriesIter ai;
            USeriesIter bi;

            ur_seriesSlice( ut, &ai, a );
            ur_seriesSlice( ut, &bi, b );

            if( (ai.end - ai.it) == (bi.end - bi.it) )
            {
                const uint8_t* pos;
                const uint8_t* end = bi.buf->ptr.b + bi.end;
                pos = match_pattern_8( ai.buf->ptr.b + ai.it,
                            ai.buf->ptr.b + ai.end,
                            bi.buf->ptr.b + bi.it, end );
                return pos == end;
            }
            }
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            if( ur_is(a, UT_BINARY) && ur_is(b, UT_BINARY) )
            {
                USeriesIter ai;
                USeriesIter bi;

                ur_seriesSlice( ut, &ai, a );
                ur_seriesSlice( ut, &bi, b );

                return compare_uint8_t( ai.buf->ptr.b + ai.it,
                                        ai.buf->ptr.b + ai.end,
                                        bi.buf->ptr.b + bi.it,
                                        bi.buf->ptr.b + bi.end );
            }
            break;
    }
    return 0;
}


static const char* binaryEncStart[ UR_BENC_COUNT ] = { "#{", "2#{", "64#{" };

void binary_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    UBinaryIter bi;
    (void) depth;

    ur_binSlice( ut, &bi, cell );
    ur_strAppendCStr( str, binaryEncStart[ bi.buf->form ] );
    ur_strAppendBinary( str, bi.it, bi.end, bi.buf->form );
    ur_strAppendChar( str, '}' );
}


void binary_mark( UThread* ut, UCell* cell )
{
    UIndex n = cell->series.buf;
    if( n > UR_INVALID_BUF )    // Also acts as (! ur_isShared(n))
        ur_markBuffer( ut, n );
}


void binary_toShared( UCell* cell )
{
    UIndex n = cell->series.buf;
    if( n > UR_INVALID_BUF )
        cell->series.buf = -n;
}


void binary_pick( const UBuffer* buf, UIndex n, UCell* res )
{
    if( n > -1 && n < buf->used )
    {
        ur_setId(res, UT_INT);
        ur_int(res) = buf->ptr.b[ n ];
    }
    else
        ur_setId(res, UT_NONE);
}


void binary_poke( UBuffer* buf, UIndex n, const UCell* val )
{
    if( n > -1 && n < buf->used )
    {
        if( ur_is(val, UT_CHAR) || ur_is(val, UT_INT) )
            buf->ptr.b[ n ] = ur_int(val);
    }
}


int binary_append( UThread* ut, UBuffer* buf, const UCell* val )
{
    int vt = ur_type(val);

    if( (vt == UT_BINARY) || ur_isStringType(vt) )
    {
        USeriesIter si;
        int len;

        ur_seriesSlice( ut, &si, val );
        len = si.end - si.it;
        if( len )
        {
            if( (vt != UT_BINARY) && ur_strIsUcs2(si.buf) )
            {
                len *= 2;
                si.it *= 2;
            }
            ur_binAppendData( buf, si.buf->ptr.b + si.it, len );
        }
        return UR_OK;
    }
    else if( (vt == UT_CHAR) || (vt == UT_INT) )
    {
        ur_binReserve( buf, buf->used + 1 );
        buf->ptr.b[ buf->used++ ] = ur_int(val);
        return UR_OK;
    }
    else if( vt == UT_BLOCK )
    {
        UBlockIter bi;
        ur_blkSlice( ut, &bi, val );
        ur_foreach( bi )
        {
            if( ! binary_append( ut, buf, bi.it ) )
                return UR_THROW;
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                 "append binary! expected char!/int!/binary!/string!/block!" );
}


int binary_insert( UThread* ut, UBuffer* buf, UIndex index,
                   const UCell* val, UIndex part )
{
    int vt = ur_type(val);

    if( (vt == UT_BINARY) || ur_isStringType(vt) )
    {
        USeriesIter si;
        int len;

        ur_seriesSlice( ut, &si, val );
        len = si.end - si.it;
        if( len > part )
            len = part;
        if( len )
        {
            if( (vt != UT_BINARY) && ur_strIsUcs2(si.buf) )
            {
                len *= 2;
                si.it *= 2;
            }

            ur_binExpand( buf, index, len );
            if( si.buf == buf )
                ur_seriesSlice( ut, &si, val );     // Re-aquire si.buf->ptr.

            memCpy( buf->ptr.b + index, si.buf->ptr.b + si.it, len );
        }
        return UR_OK;
    }
    else if( (vt == UT_CHAR) || (vt == UT_INT) )
    {
        ur_binExpand( buf, index, 1 );
        buf->ptr.b[ index ] = ur_int(val);
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "insert binary! expected char!/int!/binary!/string!" );
}


int binary_change( UThread* ut, USeriesIterM* si, const UCell* val,
                   UIndex part )
{
    int type = ur_type(val);
    if( type == UT_CHAR || type == UT_INT )
    {
        UBuffer* buf = si->buf;
        if( si->it == buf->used )
            ur_binReserve( buf, ++buf->used );
        buf->ptr.b[ si->it++ ] = ur_int(val);
        if( part > 1 )
            ur_binErase( buf, si->it, part - 1 );
        return UR_OK;
    }
    else if( type == UT_BINARY )
    {
        UBinaryIter ri;
        UBuffer* buf;
        UIndex newUsed;
        int rlen;

        ur_binSlice( ut, &ri, val );
        rlen = ri.end - ri.it;
        if( rlen > 0 )
        {
            buf = si->buf;
            if( part > 0 )
            {
                if( part > rlen )
                {
                    ur_binErase( buf, si->it, part - rlen );
                    newUsed = (buf->used < rlen) ? rlen : buf->used;
                }
                else 
                {
                    if( part < rlen )
                        ur_binExpand( buf, si->it, rlen - part );
                    newUsed = buf->used;
                }
            }
            else
            {
                newUsed = si->it + rlen;
                if( newUsed < buf->used )
                    newUsed = buf->used;
            }

            // TODO: Handle overwritting self when buf is val.

            buf->used = si->it;
            ur_binAppendData( buf, ri.it, rlen );
            si->it = buf->used;
            buf->used = newUsed;
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "change binary! expected char!/int!/binary!" );
}


void binary_remove( UThread* ut, USeriesIterM* si, UIndex part )
{
    (void) ut;
    ur_binErase( si->buf, si->it, (part > 0) ? part : 1 );
}


void binary_reverse( const USeriesIterM* si )
{
    uint8_t* it = si->buf->ptr.b;
    reverse_uint8_t( it + si->it, it + si->end );
}


int binary_find( UThread* ut, const USeriesIter* si, const UCell* val, int opt )
{
    const UBuffer* buf = si->buf;
    const uint8_t* it = buf->ptr.b;
    const uint8_t* ba = it + si->it;
    const uint8_t* bb = it + si->end;
    int vt = ur_type(val);

    if( (vt == UT_CHAR) || (vt == UT_INT) )
    {
        if( opt & UR_FIND_LAST )
            it = find_last_uint8_t( ba, bb, ur_int(val) );
        else
            it = find_uint8_t( ba, bb, ur_int(val) );
check_find:
        if( it )
            return it - buf->ptr.b;
    }
    else if( ur_isStringType( vt ) || (vt == UT_BINARY) )
    {
        USeriesIter siV;
        const uint8_t* itV;

        ur_seriesSlice( ut, &siV, val );

        if( (vt != UT_BINARY) && ur_strIsUcs2(siV.buf) )
            return -1;      // TODO: Handle ucs2.

        // TODO: Implement UR_FIND_LAST.
        itV = siV.buf->ptr.b;
        it = find_pattern_8( ba, bb, itV + siV.it, itV + siV.end );
        goto check_find;
    }
    else if( vt == UT_BITSET )
    {
        const UBuffer* bbuf = ur_bufferSer(val);
        if( opt & UR_FIND_LAST )
            it = find_last_charset_uint8_t( ba, bb, bbuf->ptr.b, bbuf->used );
        else
            it = find_charset_uint8_t( ba, bb, bbuf->ptr.b, bbuf->used );
        goto check_find;
    }
    return -1;
}


int binary_operate( UThread* ut, const UCell* a, const UCell* b, UCell* res,
                    int op )
{
    if( ur_type(a) == ur_type(b) )
    {
        switch( op )
        {
            /*
            case UR_OP_ADD:
            case UR_OP_SUB:
            case UR_OP_MUL:
            case UR_OP_DIV:
            case UR_OP_MOD:
            */

            case UR_OP_AND:
            case UR_OP_OR:
            case UR_OP_XOR:
            {
                UBinaryIter biA;
                UBinaryIter biB;
                UBuffer* bin;
                uint8_t* bp;
                USizeOrder ord;

                bin = ur_makeBinaryCell( ut, 0, res );
                ur_type(res) = ur_type(a);

                ur_binSlice( ut, &biA, a );
                ur_binSlice( ut, &biB, b );
                ur_sizeOrder( &ord, biA.end - biA.it, biB.end - biB.it );

                if( ord.large )
                {
                    ur_binExpand( bin, 0, ord.large );
                    bp = bin->ptr.b;
                    ord.large -= ord.small;     // Large is now remainder.
                    switch( op )
                    {
                        case UR_OP_AND:
                            while( ord.small-- )
                                *bp++ = *biA.it++ & *biB.it++;
                            memSet( bp, 0, ord.large );
                            break;

                        case UR_OP_OR:
                            while( ord.small-- )
                                *bp++ = *biA.it++ | *biB.it++;
                            goto copy_remain;

                        case UR_OP_XOR:
                            while( ord.small-- )
                                *bp++ = *biA.it++ ^ *biB.it++;
copy_remain:
                            memCpy( bp, ord.secondLarger ? biB.it : biA.it,
                                    ord.large );
                            break;
                    }
                }
            }
                return UR_OK;
        }
    }
    return unset_operate( ut, a, b, res, op );
}


const UCell* binary_select( UThread* ut, const UCell* cell, const UCell* sel,
                            UCell* tmp )
{
    if( ur_is(sel, UT_INT) )
    {
        const UBuffer* buf = ur_bufferSer(cell);
        binary_pick( buf, cell->series.it + ur_int(sel) - 1, tmp );
        return tmp;
    }
    ur_error( ut, UR_ERR_SCRIPT, "binary select expected int!" );
    return 0;
}


USeriesType dt_binary =
{
    {
    "binary!",
    binary_make,            binary_make,            binary_copy,
    binary_compare,         binary_operate,         binary_select,
    binary_toString,        binary_toString,
    unset_recycle,          binary_mark,            ur_binFree,
    unset_markBuf,          binary_toShared,        unset_bind
    },
    binary_pick,            binary_poke,            binary_append,
    binary_insert,          binary_change,          binary_remove,
    binary_reverse,         binary_find
};


//----------------------------------------------------------------------------
// UT_BITSET


UBuffer* ur_makeBitsetCell( UThread* ut, int bitCount, UCell* res )
{
    UBuffer* buf;
    int bytes = (bitCount + 7) / 8;

    buf = ur_makeBinaryCell( ut, bytes, res );
    ur_type(res) = UT_BITSET;

    buf->used = bytes;
    memSet( buf->ptr.b, 0, buf->used );

    return buf;
}


#define setBit(mem,n)       (mem[(n)>>3] |= 1<<((n)&7))
#define bitIsSet(mem,n)     (mem[(n)>>3] & 1<<((n)&7))

int bitset_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_INT) )
    {
        ur_makeBitsetCell( ut, ur_int(from), res );
        return UR_OK;
    }
    else if( ur_is(from, UT_CHAR) )
    {
        int n = ur_int(from);
        UBuffer* buf = ur_makeBitsetCell( ut, n + 1, res );
        setBit( buf->ptr.b, n );
        return UR_OK;
    }
    else if( ur_is(from, UT_BINARY) )
    {
        binary_copy( ut, from, res );
        ur_type(res) = UT_BITSET;
        return UR_OK;
    }
    else if( ur_is(from, UT_STRING) )
    {
        uint8_t* bits;
        UBinaryIter si;
        int n;

        bits = ur_makeBitsetCell( ut, 256, res )->ptr.b;

        ur_binSlice( ut, &si, from );
        if( si.buf->form != UR_ENC_LATIN1 )
        {
            return ur_error( ut, UR_ERR_INTERNAL,
                    "FIXME: make bitset! only handles Latin-1 strings" );
        }

        ur_foreach( si )
        {
            n = *si.it;
            setBit( bits, n );
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "make bitset! expected int!/char!/binary!/string!" );
}


int bitset_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    switch( test )
    {
        case UR_COMPARE_SAME:
            return a->series.buf == b->series.buf;

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ! ur_is(a, UT_BITSET) || ! ur_is(b, UT_BITSET) )
                break;
            if( a->series.buf == b->series.buf )
                return 1;
            {
            USizeOrder ord;
            const UBuffer* ba = ur_bufferSer(a);
            const UBuffer* bb = ur_bufferSer(b);

            ur_sizeOrder( &ord, ba->used, bb->used );
            if( ord.small )
            {
                const uint8_t* pos;
                const uint8_t* end = bb->ptr.b + ord.small;
                pos = match_pattern_8( ba->ptr.b, ba->ptr.b + ord.small,
                                       bb->ptr.b, end );
                if( pos != end )
                    return 0;

                pos = ord.secondLarger ? bb->ptr.b : ba->ptr.b;
                end = pos + ord.large;
                pos += ord.small;
                while( pos != end )
                {
                    if( *pos++ )
                        return 0;
                }
            }
            }
            return 1;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            break;
    }
    return 0;
}


void bitset_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    const UBuffer* buf = ur_bufferSer(cell);
    (void) depth;

    // Using "make bitset!" requires evaluation to re-load.
    // Maybe bitset! should have it's own syntax?

    ur_strAppendCStr( str, "make bitset! #{" );
    ur_strAppendBinary( str, buf->ptr.b, buf->ptr.b + buf->used, UR_BENC_16 );
    ur_strAppendChar( str, '}' );
}


void bitset_reverse( const USeriesIterM* si )
{
    (void) si;
}


int bitset_find( UThread* ut, const USeriesIter* si, const UCell* val, int opt )
{
    const UBuffer* buf = si->buf;
    int vt = ur_type(val);
    int n;
    (void) opt;

    if( (vt == UT_CHAR) || (vt == UT_INT) )
    {
        n = ur_int(val);
        if( ((n >> 3) < buf->used) && bitIsSet( buf->ptr.b, n ) )
            return n;
    }
    else if( vt == UT_BLOCK )       // Succeeds if all bits are found.
    {
        UBlockIter bi;
        n = -1;
        ur_blkSlice( ut, &bi, val );
        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_CHAR) || ur_is(bi.it, UT_INT) )
            {
                n = ur_int(bi.it);
                if( ((n >> 3) >= buf->used) || ! bitIsSet( buf->ptr.b, n ) )
                    return -1;
            }
        }
        return n;
    }
    return -1;
}


USeriesType dt_bitset =
{
    {
    "bitset!",
    bitset_make,            bitset_make,            binary_copy,
    bitset_compare,         binary_operate,         unset_select,
    bitset_toString,        bitset_toString,
    unset_recycle,          binary_mark,            ur_binFree,
    unset_markBuf,          binary_toShared,        unset_bind
    },
    binary_pick,            binary_poke,            binary_append,
    binary_insert,          binary_change,          binary_remove,
    bitset_reverse,         bitset_find
};


//----------------------------------------------------------------------------
// UT_STRING


void string_copy( UThread* ut, const UCell* from, UCell* res )
{
    USeriesIter si;
    UBuffer* buf;
    int len;

    ur_seriesSlice( ut, &si, from );
    len = si.end - si.it;
    // Make invalidates si.buf.
    buf = ur_makeStringCell( ut, si.buf->form, len, res );
    if( len )
        ur_strAppend( buf, ur_bufferSer(from), si.it, si.end );
}


static void setStringCell( UCell* cell, UIndex bufN )
{
    ur_setId( cell, UT_STRING );
    ur_setSeries( cell, bufN, 0 );
}


int string_convert( UThread* ut, const UCell* from, UCell* res )
{
    int type = ur_type(from);
    if( ur_isStringType(type) )
    {
        string_copy( ut, from, res );
    }
    else if( type == UT_BINARY )
    {
        UBinaryIter bi;
        UIndex n;

        ur_binSlice( ut, &bi, from );
        n = ur_makeStringUtf8( ut, bi.it, bi.end );

        setStringCell( res, n );
    }
    else
    {
        DT( type )->toString( ut, from,
                              ur_makeStringCell(ut, UR_ENC_LATIN1, 0, res), 0 );
    }
    return UR_OK;
}


int string_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_INT) )
    {
        ur_makeStringCell( ut, UR_ENC_LATIN1, ur_int(from), res );
        return UR_OK;
    }
    return string_convert( ut, from, res );
}

 
#define COMPARE_IC(T) \
int compare_ic_ ## T( const T* it, const T* end, \
        const T* itB, const T* endB ) { \
    int ca, cb; \
    int lenA = end - it; \
    int lenB = endB - itB; \
    while( it < end && itB < endB ) { \
        ca = ur_charLowercase( *it++ ); \
        cb = ur_charLowercase( *itB++ ); \
        if( ca > cb ) \
            return 1; \
        if( ca < cb ) \
            return -1; \
    } \
    if( lenA > lenB ) \
        return 1; \
    if( lenA < lenB ) \
        return -1; \
    return 0; \
}

COMPARE_IC(uint8_t)
COMPARE_IC(uint16_t)


int string_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    switch( test )
    {
        case UR_COMPARE_SAME:
            return ((a->series.buf == b->series.buf) &&
                    (a->series.it == b->series.it) &&
                    (a->series.end == b->series.end));

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ! ur_isStringType(ur_type(a)) || ! ur_isStringType(ur_type(b)) )
                break;
            if( (a->series.buf == b->series.buf) &&
                (a->series.it == b->series.it) &&
                (a->series.end == b->series.end) )
                return 1;
            {
            USeriesIter ai;
            USeriesIter bi;
            int len;

            ur_seriesSlice( ut, &ai, a );
            ur_seriesSlice( ut, &bi, b );
            len = ai.end - ai.it;

            if( (bi.end - bi.it) == len )
            {
                if( (len == 0) ||
                    (ur_strMatch( &ai, &bi, (test == UR_COMPARE_EQUAL_CASE) )
                        == len ) )
                    return 1;
            }
            }
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            if( ! ur_isStringType(ur_type(a)) || ! ur_isStringType(ur_type(b)) )
                break;
            {
            USeriesIter ai;
            USeriesIter bi;

            ur_seriesSlice( ut, &ai, a );
            ur_seriesSlice( ut, &bi, b );

            if( ai.buf->elemSize != bi.buf->elemSize )
                return 0;       // TODO: Handle all different encodings.

            if( ur_strIsUcs2(ai.buf) )
            {
                int (*func)(const uint16_t*, const uint16_t*,
                            const uint16_t*, const uint16_t* );
                func = (test == UR_COMPARE_ORDER) ? compare_ic_uint16_t
                                                  : compare_uint16_t;
                return func( ai.buf->ptr.u16 + ai.it,
                             ai.buf->ptr.u16 + ai.end,
                             bi.buf->ptr.u16 + bi.it,
                             bi.buf->ptr.u16 + bi.end );
            }
            else
            {
                int (*func)(const uint8_t*, const uint8_t*,
                             const uint8_t*, const uint8_t* );
                func = (test == UR_COMPARE_ORDER) ? compare_ic_uint8_t
                                                  : compare_uint8_t;
                return func( ai.buf->ptr.b + ai.it,
                             ai.buf->ptr.b + ai.end,
                             bi.buf->ptr.b + bi.it,
                             bi.buf->ptr.b + bi.end );
            }
            }
    }
    return 0;
}


// Newline (\n) or double quote (").
//                                        0x04
static uint8_t _strLongChars[5] = { 0x00, 0x00, 0x00, 0x00, 0x04 };

static uint8_t _strEscapeChars[16] = {
   0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x28
};


static inline int nibbleToChar( int n )
{
    return (n < 10) ? n + '0' : n + 'A' - 10;
}

void string_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    const int longLen = 40;
    int len;
    int quote;
    UIndex escPos;
    USeriesIter si;

    ur_seriesSlice( ut, &si, cell );
    len = si.end - si.it;

    if( len < 1 )
    {
        ur_strAppendCStr( str, "\"\"" );
        return;
    }

    if( (len > longLen) ||
        (ur_strFindChars( si.buf, si.it, si.end,
                          _strLongChars, sizeof(_strLongChars) ) > -1) )
    {
        ur_strAppendChar( str, '{' );
        quote = '}';
    }
    else
    {
        ur_strAppendChar( str, '"' );
        quote = '"';
    }

    while( 1 )
    {
        escPos = ur_strFindChars( si.buf, si.it, si.end,
                                  _strEscapeChars, sizeof(_strEscapeChars) );
        if( escPos < 0 )
        {
            ur_strAppend( str, si.buf, si.it, si.end );
            break;
        }

        if( escPos != si.it )
            ur_strAppend( str, si.buf, si.it, escPos );
        si.it = escPos + 1;

        depth = ur_strIsUcs2(si.buf) ? si.buf->ptr.u16[ escPos ]
                                     : si.buf->ptr.b[ escPos ];
        switch( depth )
        {
           case '\t':
                ur_strAppendCStr( str, "^-" );
                break;

            case '\n':
                if( quote == '"' )
                    ur_strAppendCStr( str, "^/" );
                else
                    ur_strAppendChar( str, '\n' );
                break;

            case '^':
                ur_strAppendCStr( str, "^^" );
                break;

            case '{':
                if( quote == '"' )
                    ur_strAppendChar( str, '{' );
                else
                    ur_strAppendCStr( str, "^{" );
                break;

            case '}':
                if( quote == '"' )
                    ur_strAppendChar( str, '}' );
                else
                    ur_strAppendCStr( str, "^}" );
                break;

            default:
                ur_strAppendChar( str, '^' );
                ur_strAppendChar( str, nibbleToChar( depth ) );
                break;
        }
    }

    ur_strAppendChar( str, quote );
}


void string_toText( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    const UBuffer* ss = ur_bufferSer(cell);
    (void) depth;
    ur_strAppend( str, ss, cell->series.it,
                  (cell->series.end > -1) ? cell->series.end : ss->used );
}


void string_pick( const UBuffer* buf, UIndex n, UCell* res )
{
    if( n > -1 && n < buf->used )
    {
        ur_setId(res, UT_CHAR);
        ur_int(res) = ur_strIsUcs2(buf) ? buf->ptr.u16[ n ]
                                        : buf->ptr.b[ n ];
    }
    else
        ur_setId(res, UT_NONE);
}


void string_poke( UBuffer* buf, UIndex n, const UCell* val )
{
    if( n > -1 && n < buf->used )
    {
        if( ur_is(val, UT_CHAR) || ur_is(val, UT_INT) )
        {
            if( ur_strIsUcs2(buf) )
                buf->ptr.u16[ n ] = ur_int(val);
            else
                buf->ptr.b[ n ] = ur_int(val);
        }
    }
}


int string_append( UThread* ut, UBuffer* buf, const UCell* val )
{
    int type = ur_type(val);

    if( ur_isStringType(type) )
    {
        USeriesIter si;
        ur_seriesSlice( ut, &si, val );
        ur_strAppend( buf, si.buf, si.it, si.end );
    }
    else if( type == UT_CHAR )
    {
        ur_strAppendChar( buf, ur_int(val) );
    }
    else if( type == UT_BLOCK )
    {
        UBlockIter bi;
        const UDatatype** dt = ut->types;
        ur_blkSlice( ut, &bi, val );
        ur_foreach( bi )
        {
            dt[ ur_type(bi.it) ]->toText( ut, bi.it, buf, 0 );
            //ur_toText( ut, bi.it, str );
        }
    }
    else
    {
        DT( type )->toText( ut, val, buf, 0 );
    }
    return UR_OK;
}


int string_insert( UThread* ut, UBuffer* buf, UIndex index,
                   const UCell* val, UIndex part )
{
    int type = ur_type(val);

    if( ur_isStringType(type) )
    {
        USeriesIter si;
        UIndex saveUsed;
        int len;

        ur_seriesSlice( ut, &si, val );
        len = si.end - si.it;
        if( len > part )
            len = part;
        if( len )
        {
            ur_arrExpand( buf, index, len );

            saveUsed = buf->used;
            buf->used = index;
            ur_strAppend( buf, si.buf, si.it, si.it + len );
            buf->used = saveUsed;
        }
        return UR_OK;
    }
    else if( type == UT_CHAR )
    {
        ur_arrExpand( buf, index, 1 );
        if( ur_strIsUcs2(buf) )
            buf->ptr.u16[ index ] = ur_int(val);
        else
            buf->ptr.c[ index ] = ur_int(val);
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "insert string! expected char!/string!" );
}


/*
  \param si     String to change.
  \param ri     Replacement string.
  \param part   Replace this many characters, regardless of the length of ri.

  \return  si->it is placed at end of change and si->buf.used may be modified.
*/
static void ur_strChange( USeriesIterM* si, USeriesIter* ri, UIndex part )
{
    UBuffer* buf;
    UIndex newUsed;
    int rlen = ri->end - ri->it;

    if( rlen > 0 )
    {
        buf = si->buf;
        if( part > 0 )
        {
            if( part > rlen )
            {
                ur_arrErase( buf, si->it, part - rlen );
                newUsed = (buf->used < rlen) ? rlen : buf->used;
            }
            else
            {
                if( part < rlen )
                    ur_arrExpand( buf, si->it, rlen - part );
                newUsed = buf->used;
            }
        }
        else
        {
            newUsed = si->it + rlen;
            if( newUsed < buf->used )
                newUsed = buf->used;
        }

        // TODO: Handle overwritting self when buf is ri->buf.

        buf->used = si->it;
        ur_strAppend( buf, ri->buf, ri->it, ri->end );
        si->it = buf->used;
        buf->used = newUsed;
    }
}


int string_change( UThread* ut, USeriesIterM* si, const UCell* val,
                   UIndex part )
{
    USeriesIter siV;
    int type = ur_type(val);

    if( type == UT_CHAR )
    {
        UBuffer* buf = si->buf;
        if( si->it == buf->used )
            ur_arrReserve( buf, ++buf->used );

        if( ur_strIsUcs2(buf) )
            buf->ptr.u16[ si->it ] = ur_int(val);
        else
            buf->ptr.b[ si->it ] = ur_int(val);
        ++si->it;

        if( part > 1 )
            ur_arrErase( buf, si->it, part - 1 );
    }
    else if( ur_isStringType(type) )
    {
        ur_seriesSlice( ut, &siV, val );
        ur_strChange( si, &siV, part );
    }
    else
    {
        UBuffer tmp;

        ur_strInit( &tmp, UR_ENC_LATIN1, 0 );
        DT( type )->toString( ut, val, &tmp, 0 );

        siV.buf = &tmp;
        siV.it  = 0;
        siV.end = tmp.used;

        ur_strChange( si, &siV, part );
        ur_strFree( &tmp );
    }
    return UR_OK;
}


void string_remove( UThread* ut, USeriesIterM* si, UIndex part )
{
    (void) ut;
    ur_arrErase( si->buf, si->it, (part > 0) ? part : 1 );
}


void string_reverse( const USeriesIterM* si )
{
    const UBuffer* buf = si->buf;
    assert( buf->form != UR_ENC_UTF8 );
    if( ur_strIsUcs2(buf) )
        reverse_uint16_t( buf->ptr.u16 + si->it, buf->ptr.u16 + si->end );
    else
        reverse_uint8_t( buf->ptr.b + si->it, buf->ptr.b + si->end );
}


int string_find( UThread* ut, const USeriesIter* si, const UCell* val, int opt )
{
    const UBuffer* buf = si->buf;

    switch( ur_type(val) )
    {
        case UT_CHAR:
            return ur_strFindChar( buf, si->it, si->end, ur_int(val), opt );

        case UT_BINARY:
        case UT_STRING:
        case UT_FILE:
        {
            USeriesIter pi;
            ur_seriesSlice( ut, &pi, val );
            if( opt & UR_FIND_LAST )
                return -1;      // TODO: Implement UR_FIND_LAST.
            return ur_strFind( si, &pi, opt & UR_FIND_CASE );
        }

        case UT_BITSET:
        {
            const UBuffer* bbuf = ur_bufferSer(val);
            if( opt & UR_FIND_LAST )
                return ur_strFindCharsRev( buf, si->it, si->end,
                                           bbuf->ptr.b, bbuf->used );
            else
                return ur_strFindChars( buf, si->it, si->end,
                                        bbuf->ptr.b, bbuf->used );
        }
    }
    return -1;
}


const UCell* string_select( UThread* ut, const UCell* cell, const UCell* sel,
                            UCell* tmp )
{
    if( ur_is(sel, UT_INT) )
    {
        const UBuffer* buf = ur_bufferSer(cell);
        string_pick( buf, cell->series.it + ur_int(sel) - 1, tmp );
        return tmp;
    }
    ur_error( ut, UR_ERR_SCRIPT, "string select expected int!" );
    return 0;
}


USeriesType dt_string =
{
    {
    "string!",
    string_make,            string_convert,         string_copy,
    string_compare,         unset_operate,          string_select,
    string_toString,        string_toText,
    unset_recycle,          binary_mark,            string_destroy,
    unset_markBuf,          binary_toShared,        unset_bind
    },
    string_pick,            string_poke,            string_append,
    string_insert,          string_change,          string_remove,
    string_reverse,         string_find
};


//----------------------------------------------------------------------------
// UT_FILE


int file_make( UThread* ut, const UCell* from, UCell* res )
{
    int ok = string_make( ut, from, res );
    if( ok )
        ur_type(res) = UT_FILE;
    return ok;
}


int file_convert( UThread* ut, const UCell* from, UCell* res )
{
    int ok = string_convert( ut, from, res );
    if( ok )
        ur_type(res) = UT_FILE;
    return ok;
}


void file_copy( UThread* ut, const UCell* from, UCell* res )
{
    string_copy( ut, from, res );
    ur_type(res) = UT_FILE;
}


// "()[]; "
static uint8_t _fileQuoteChars[12] =
{
    0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x28
};

void file_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    USeriesIter si;
    (void) depth;

    ur_seriesSlice( ut, &si, cell );

    if( ur_strFindChars( si.buf, si.it, si.end, _fileQuoteChars,
                         sizeof(_fileQuoteChars) ) > -1 )
    {
        ur_strAppendCStr( str, "%\"" );
        ur_strAppend( str, si.buf, si.it, si.end );
        ur_strAppendChar( str, '"' );
    }
    else
    {
        ur_strAppendChar( str, '%' );
        ur_strAppend( str, si.buf, si.it, si.end );
    }
}


USeriesType dt_file =
{
    {
    "file!",
    file_make,              file_convert,           file_copy,
    string_compare,         unset_operate,          string_select,
    file_toString,          string_toText,
    unset_recycle,          binary_mark,            string_destroy,
    unset_markBuf,          binary_toShared,        unset_bind
    },
    string_pick,            string_poke,            string_append,
    string_insert,          string_change,          string_remove,
    string_reverse,         string_find
};


//----------------------------------------------------------------------------
// UT_BLOCK


void block_copy( UThread* ut, const UCell* from, UCell* res )
{
    UBlockIter bi;
    UBuffer* buf;
    int len;

    ur_blkSlice( ut, &bi, from );
    len = bi.end - bi.it;
    // Make invalidates bi.buf.
    buf = ur_makeBlockCell( ut, ur_type(from), len, res );
    if( len )
        ur_blkAppendCells( buf, bi.it, len );
}


int block_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_INT) )
    {
        ur_makeBlockCell( ut, UT_BLOCK, ur_int(from), res );
        return UR_OK;
    }
    else if( ur_is(from, UT_STRING) )
    {
        USeriesIter si;
        ur_seriesSlice( ut, &si, from );
        if( si.it == si.end )
        {
            ur_makeBlockCell( ut, UT_BLOCK, 0, res );
            return UR_OK;
        }
        else if( (si.buf->elemSize == 1) )
        {
            if( ur_tokenizeType( ut, si.buf->form,
                                 si.buf->ptr.c + si.it,
                                 si.buf->ptr.c + si.end, res ) )
                return UR_OK;
        }
        else
        {
            UBuffer tmp;
            UIndex n;
            ur_strInit( &tmp, UR_ENC_UTF8, 0 );
            ur_strAppend( &tmp, si.buf, si.it, si.end );
            n = ur_tokenizeType( ut, UR_ENC_UTF8, tmp.ptr.c,
                                 tmp.ptr.c + tmp.used, res );
            ur_strFree( &tmp );
            if( n )
                return UR_OK;
        }
        return UR_THROW;
    }
    else if( ur_isBlockType( ur_type(from) )  )
    {
        block_copy( ut, from, res );
        ur_type(res) = UT_BLOCK;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "make block! expected int!/string!/block!" );
}


int block_convert( UThread* ut, const UCell* from, UCell* res )
{
    int type = ur_type(from);

    if( type == UT_STRING )
    {
        return block_make( ut, from, res );
    }
    else if( ur_isBlockType( type )  )
    {
        block_copy( ut, from, res );
        ur_type(res) = UT_BLOCK;
    }
    else
    {
        UBuffer* blk = ur_makeBlockCell( ut, UT_BLOCK, 1, res );
        ur_blkPush( blk, from );
    }
    return UR_OK;
}


int block_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    switch( test )
    {
        case UR_COMPARE_SAME:
            return ((a->series.buf == b->series.buf) &&
                    (a->series.it == b->series.it) &&
                    (a->series.end == b->series.end));

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) != ur_type(b) )
                break;
            if( (a->series.buf == b->series.buf) &&
                (a->series.it == b->series.it) &&
                (a->series.end == b->series.end) )
                return 1;
            {
            UBlockIter ai;
            UBlockIter bi;
            const UDatatype** dt;
            int t;

            ur_blkSlice( ut, &ai, a );
            ur_blkSlice( ut, &bi, b );

            if( (ai.end - ai.it) == (bi.end - bi.it) )
            {
                dt = ut->types;
                ur_foreach( ai )
                {
                    t = ur_type(ai.it);
                    if( t < ur_type(bi.it) )
                        t = ur_type(bi.it);
                    if( ! dt[ t ]->compare( ut, ai.it, bi.it, test ) )
                        return 0;
                    ++bi.it;
                }
                return 1;
            }
            }
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            break;
    }
    return 0;
}


#define BLOCK_OP_INT(OP) \
    ur_foreach(bi) { \
        if( ur_isIntType(ur_type(bi.it)) ) \
            n = n OP ur_int(bi.it); \
        else if( ur_isDecimalType(ur_type(bi.it)) ) \
            n = n OP (int) ur_decimal(bi.it); \
    }

#define BLOCK_OP_DEC(OP) \
    ur_foreach(bi) { \
        if( ur_isDecimalType(ur_type(bi.it)) ) \
            n = n OP ur_decimal(bi.it); \
        else if( ur_isIntType(ur_type(bi.it)) ) \
            n = n OP (double) ur_int(bi.it); \
    }

int block_operate( UThread* ut, const UCell* a, const UCell* b, UCell* res,
                   int op )
{
    UBlockIter bi;

    if( ur_isIntType( ur_type(a) ) )
    {
        int n = ur_int(a);
        ur_blkSlice( ut, &bi, b );
        switch( op )
        {
            case UR_OP_ADD:
                BLOCK_OP_INT( + )
                break;
            case UR_OP_SUB:
                BLOCK_OP_INT( - )
                break;
            case UR_OP_MUL:
                BLOCK_OP_INT( * )
                break;
            case UR_OP_AND:
                BLOCK_OP_INT( & )
                break;
            case UR_OP_OR:
                BLOCK_OP_INT( | )
                break;
            case UR_OP_XOR:
                BLOCK_OP_INT( ^ )
                break;
            default:
                return unset_operate( ut, a, b, res, op );
        }
        ur_setId(res, ur_type(a));
        ur_int(res) = n;
        return UR_OK;
    }
    else if( ur_isDecimalType( ur_type(a) ) )
    {
        double n = ur_decimal(a);
        ur_blkSlice( ut, &bi, b );
        switch( op )
        {
            case UR_OP_ADD:
                BLOCK_OP_DEC( + )
                break;
            case UR_OP_SUB:
                BLOCK_OP_DEC( - )
                break;
            case UR_OP_MUL:
                BLOCK_OP_DEC( * )
                break;
            default:
                return unset_operate( ut, a, b, res, op );
        }
        ur_setId(res, ur_type(a));
        ur_decimal(res) = n;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "block! operator exepected char!/int!/decimal!" );
}


/*
  If depth is -1 then the outermost pair of braces will be omitted.
*/
void block_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    UBlockIter bi;
    const UCell* start;
    int brace = 0;

    if( depth > -1 )
    {
        switch( ur_type(cell) )
        {
            case UT_BLOCK:
                ur_strAppendChar( str, '[' );
                brace = ']';
                break;
            case UT_PAREN:
                ur_strAppendChar( str, '(' );
                brace = ')';
                break;
#ifdef UR_CONFIG_MACROS
            case UT_MACRO:
                ur_strAppendCStr( str, "^(" );
                brace = ')';
                break;
#endif
        }
    }

    ur_blkSlice( ut, &bi, cell );
    start = bi.it;

    ++depth;
    ur_foreach( bi )
    {
        if( bi.it->id.flags & UR_FLAG_SOL )
        {
            ur_strAppendChar( str, '\n' );
            ur_strAppendIndent( str, depth );
        }
        else if( bi.it != start )
        {
            ur_strAppendChar( str, ' ' );
        }
        ur_toStr( ut, bi.it, str, depth );
    }
    --depth;

    if( (start != bi.end) && (start->id.flags & UR_FLAG_SOL) )
    {
        ur_strAppendChar( str, '\n' );
        if( brace )
            ur_strAppendIndent( str, depth );
    }

    if( brace )
        ur_strAppendChar( str, brace );
}


void block_toText( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    UBlockIter bi;
    const UCell* start;
    (void) depth;

    ur_blkSlice( ut, &bi, cell );
    start = bi.it;

    ur_foreach( bi )
    {
        if( bi.it != start )
            ur_strAppendChar( str, ' ' );
        ur_toText( ut, bi.it, str );
    }
}


void block_markBuf( UThread* ut, UBuffer* buf )
{
    int t;
    UCell* it  = buf->ptr.cell;
    UCell* end = it + buf->used;
    while( it != end )
    {
        t = ur_type(it);
        if( t >= UT_REFERENCE_BUF )
        {
            DT( t )->mark( ut, it );
        }
        ++it;
    }
}


void block_mark( UThread* ut, UCell* cell )
{
    UIndex n = cell->series.buf;
    if( n > UR_INVALID_BUF )    // Also acts as (! ur_isShared(n))
    {
        if( ur_markBuffer( ut, n ) )
            block_markBuf( ut, ur_buffer(n) );
    }
}


void block_toShared( UCell* cell )
{
    UIndex n = cell->series.buf;
    if( n > UR_INVALID_BUF )
        cell->series.buf = -n;
}


void block_pick( const UBuffer* buf, UIndex n, UCell* res )
{
    if( n > -1 && n < buf->used )
        *res = buf->ptr.cell[ n ];
    else
        ur_setId(res, UT_NONE);
}


void block_poke( UBuffer* buf, UIndex n, const UCell* val )
{
    if( n > -1 && n < buf->used )
        buf->ptr.cell[ n ] = *val;
}


int block_append( UThread* ut, UBuffer* buf, const UCell* val )
{
    if( ur_is(val, UT_BLOCK) || ur_is(val, UT_PAREN) )
    {
        UBlockIter bi;
        ur_blkSlice( ut, &bi, val );
        if( bi.buf == buf )
        {
            // If appending to self then this makes sure the source
            // cells pointer does not change in ur_blkAppendCells().
            ur_arrReserve( buf, buf->used + (bi.end - bi.it) );
            ur_blkSlice( ut, &bi, val );
        }
        ur_blkAppendCells( buf, bi.it, bi.end - bi.it );
    }
    else
    {
        ur_blkPush( buf, val );
    }
    return UR_OK;
}


int block_insert( UThread* ut, UBuffer* buf, UIndex index,
                  const UCell* val, UIndex part )
{
    if( ur_is(val, UT_BLOCK) || ur_is(val, UT_PAREN) )
    {
        UBlockIter bi;
        int len;

        ur_blkSlice( ut, &bi, val );
        len = bi.end - bi.it;
        if( len > part )
            len = part;
        if( len > 0 )
        {
            if( bi.buf == buf )
            {
                // Inserting into self.
                UIndex it = bi.it - buf->ptr.cell;
                ur_arrExpand( buf, index, len );
                if( it != index )
                {
                    memCpy( buf->ptr.cell + index, buf->ptr.cell + it,
                            len * sizeof(UCell) );
                }
            }
            else
            {
                ur_blkInsert( buf, index, bi.it, len );
            }
        }
    }
    else
    {
        ur_blkInsert( buf, index, val, 1 );
    }
    return UR_OK;
}


int block_change( UThread* ut, USeriesIterM* si, const UCell* val,
                  UIndex part )
{
    if( ur_isBlockType( ur_type(val) ) )
    {
        UBlockIter ri;
        UBuffer* buf;
        UIndex newUsed;
        int rlen;

        ur_blkSlice( ut, &ri, val );
        rlen = ri.end - ri.it;
        if( rlen > 0 )
        {
            buf = si->buf;
            if( part > 0 )
            {
                if( part > rlen )
                {
                    ur_arrErase( buf, si->it, part - rlen );
                    newUsed = (buf->used < rlen) ? rlen : buf->used;
                }
                else 
                {
                    if( part < rlen )
                        ur_arrExpand( buf, si->it, rlen - part );
                    newUsed = buf->used;
                }
            }
            else
            {
                newUsed = si->it + rlen;
                if( newUsed < buf->used )
                    newUsed = buf->used;
            }

            // TODO: Handle overwritting self when buf is val.

            buf->used = si->it;
            ur_blkAppendCells( buf, ri.it, rlen );
            si->it = buf->used;
            buf->used = newUsed;
        }
    }
    else
    {
        UBuffer* buf = si->buf;
        if( si->it == buf->used )
            ur_arrReserve( buf, ++buf->used );
        buf->ptr.cell[ si->it++ ] = *val;
        if( part > 1 )
            ur_arrErase( buf, si->it, part - 1 );
    }
    return UR_OK;
}


void block_remove( UThread* ut, USeriesIterM* si, UIndex part )
{
    (void) ut;
    ur_arrErase( si->buf, si->it, (part > 0) ? part : 1 );
}


void block_reverse( const USeriesIterM* si )
{
    if( si->end > si->it )
    {
        UCell tmp;
        UCell* it  = si->buf->ptr.cell + si->it;
        UCell* end = si->buf->ptr.cell + si->end;

        while( it < --end )
        {
            tmp = *it;
            *it++ = *end;
            *end = tmp;
        }
    }
}


int block_find( UThread* ut, const USeriesIter* si, const UCell* val, int opt )
{
    UBlockIter bi;
    const UBuffer* buf = si->buf;
    int (*equal)(UThread*, const UCell*, const UCell*) =
        (opt & UR_FIND_CASE) ? ur_equalCase : ur_equal;

    bi.it  = buf->ptr.cell + si->it;
    bi.end = buf->ptr.cell + si->end;

    if( opt & UR_FIND_LAST )
    {
        while( bi.it != bi.end )
        {
            --bi.end;
            if( equal( ut, val, bi.end ) )
                return bi.end - buf->ptr.cell;
        }
    }
    else
    {
        ur_foreach( bi )
        {
            if( equal( ut, val, bi.it ) )
                return bi.it - buf->ptr.cell;
        }
    }
    return -1;
}


const UCell* block_select( UThread* ut, const UCell* cell, const UCell* sel,
                           UCell* tmp )
{
    const UBuffer* buf = ur_bufferSer(cell);

    if( ur_is(sel, UT_INT) )
    {
        //block_pick( buf, cell->series.it + ur_int(sel) - 1, tmp );
        int n = cell->series.it + ur_int(sel) - 1;
        if( n > -1 && n < buf->used )
            return buf->ptr.cell + n;
none:
        ur_setId(tmp, UT_NONE);
        return tmp;
    }
    else if( ur_is(sel, UT_WORD) )
    {
        UBlockIter wi;
        UAtom atom = ur_atom(sel);
        ur_blkSlice( ut, &wi, cell );
        ur_foreach( wi )
        {
            // Checking atom first would be faster (it will fail more often
            // and is a quicker check), but the atom field may not be
            // intialized memory so memory checkers will report an error.
            if( ur_isWordType( ur_type(wi.it) ) && (ur_atom(wi.it) == atom) )
            {
                if( ++wi.it == wi.end )
                    goto none;
                return wi.it;
            }
        }
    }
    ur_error( ut, UR_ERR_SCRIPT, "block select expected int!/word!" );
    return 0;
}


USeriesType dt_block =
{
    {
    "block!",
    block_make,             block_convert,          block_copy,
    block_compare,          block_operate,          block_select,
    block_toString,         block_toText,
    unset_recycle,          block_mark,             block_destroy,
    block_markBuf,          block_toShared,         unset_bind
    },
    block_pick,             block_poke,             block_append,
    block_insert,           block_change,           block_remove,
    block_reverse,          block_find
};


//----------------------------------------------------------------------------
// UT_PAREN


int paren_make( UThread* ut, const UCell* from, UCell* res )
{
    int ok = block_make( ut, from, res );
    if( ok )
        ur_type(res) = UT_PAREN;
    return ok;
}


int paren_convert( UThread* ut, const UCell* from, UCell* res )
{
    int ok = block_convert( ut, from, res );
    if( ok )
        ur_type(res) = UT_PAREN;
    return ok;
}


USeriesType dt_paren =
{
    {
    "paren!",
    paren_make,             paren_convert,          block_copy,
    block_compare,          unset_operate,          block_select,
    block_toString,         block_toString,
    unset_recycle,          block_mark,             block_destroy,
    block_markBuf,          block_toShared,         unset_bind
    },
    block_pick,             block_poke,             block_append,
    block_insert,           block_change,           block_remove,
    block_reverse,          block_find
};


//----------------------------------------------------------------------------
// UT_PATH


static int path_makeT( UThread* ut, const UCell* from, UCell* res, int ptype )
{
    int typeMask = 1 << ur_type(from);
    if( typeMask & ((1<<UT_BLOCK) | (1<<UT_PAREN)) )
    {
        UBlockIter bi;
        UBuffer* blk;

        ur_blkSlice( ut, &bi, from );
        // Make invalidates bi.buf.
        blk = ur_makeBlockCell( ut, ptype, bi.end - bi.it, res );

        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_WORD) || ur_is(bi.it, UT_INT) )
                ur_blkPush( blk, bi.it );
        }
        return UR_OK;
    }
    else if( typeMask & ((1<<UT_PATH) | (1<<UT_LITPATH) | (1<<UT_SETPATH)) )
    {
        block_copy( ut, from, res );
        ur_type(res) = ptype;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "make path! expected block!" );
}


int path_make( UThread* ut, const UCell* from, UCell* res )
{
    return path_makeT( ut, from, res, UT_PATH );
}


void path_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    UBlockIter bi;
    const UCell* start;

    ur_blkSlice( ut, &bi, cell );
    start = bi.it;

    if( ur_is(cell, UT_LITPATH) )
        ur_strAppendChar( str, '\'' );

    ur_foreach( bi )
    {
        if( bi.it != start )
            ur_strAppendChar( str, '/' );
        ur_toStr( ut, bi.it, str, depth );
    }

    if( ur_is(cell, UT_SETPATH) )
        ur_strAppendChar( str, ':' );
}


USeriesType dt_path =
{
    {
    "path!",
    path_make,              path_make,              block_copy,
    block_compare,          unset_operate,          block_select,
    path_toString,          path_toString,
    unset_recycle,          block_mark,             block_destroy,
    block_markBuf,          block_toShared,         unset_bind
    },
    block_pick,             block_poke,             block_append,
    block_insert,           block_change,           block_remove,
    block_reverse,          block_find
};


//----------------------------------------------------------------------------
// UT_LITPATH


int litpath_make( UThread* ut, const UCell* from, UCell* res )
{
    return path_makeT( ut, from, res, UT_LITPATH );
}


USeriesType dt_litpath =
{
    {
    "lit-path!",
    litpath_make,           litpath_make,           block_copy,
    block_compare,          unset_operate,          block_select,
    path_toString,          path_toString,
    unset_recycle,          block_mark,             block_destroy,
    block_markBuf,          block_toShared,         unset_bind
    },
    block_pick,             block_poke,             block_append,
    block_insert,           block_change,           block_remove,
    block_reverse,          block_find
};


//----------------------------------------------------------------------------
// UT_SETPATH


int setpath_make( UThread* ut, const UCell* from, UCell* res )
{
    return path_makeT( ut, from, res, UT_SETPATH );
}


USeriesType dt_setpath =
{
    {
    "set-path!",
    setpath_make,           setpath_make,           block_copy,
    block_compare,          unset_operate,          block_select,
    path_toString,          path_toString,
    unset_recycle,          block_mark,             block_destroy,
    block_markBuf,          block_toShared,         unset_bind
    },
    block_pick,             block_poke,             block_append,
    block_insert,           block_change,           block_remove,
    block_reverse,          block_find
};


//----------------------------------------------------------------------------
// UT_CONTEXT


int context_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_BLOCK) )
    {
        UBlockIterM bi;
        UBuffer* ctx;

        ctx = ur_makeContextCell( ut, 0, res );     // gc!

        if( ! ur_blkSliceM( ut, &bi, from ) )
            return UR_THROW;

        ur_ctxSetWords( ctx, bi.it, bi.end );
        ur_ctxSort( ctx );
        ur_bind( ut, bi.buf, ctx, UR_BIND_SELF );
        return UR_OK;
    }
    else if( ur_is(from, UT_CONTEXT) )
    {
        ur_ctxClone( ut, ur_bufferSer(from), res );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "make context! expected block!/context!");
}


void context_copy( UThread* ut, const UCell* from, UCell* res )
{
    ur_ctxClone( ut, ur_bufferSer(from), res );
}


int context_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_SAME:
            return (a->series.buf == b->series.buf);

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) != ur_type(b) )
                break;
            if( a->series.buf == b->series.buf )
                return 1;
            // TODO: Compare words and values.
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            break;
    }
    return 0;
}


/*
   \ctx        Pointer to a valid and sorted context.
   \ctxN       The buffer of ctx (required for binding the words).
   \param res  Set to a block of words in the context (bound to the context).
*/
void _contextWords( UThread* ut, const UBuffer* ctx, UIndex ctxN, UCell* res )
{
    UBlockIterM di;
    UAtom* ait;
    UAtom* atoms;
    int bindType;
    UIndex used = ctx->used;

    di.buf = ur_makeBlockCell( ut, UT_BLOCK, used, res );
    di.it  = di.buf->ptr.cell;
    di.end = di.it + used;

    ctx = ur_bufferE(ctxN);     // Re-aquire.
    atoms = ait = ((UAtom*) di.end) - used;
    ur_ctxWordAtoms( ctx, atoms );

    if( ctxN == UR_INVALID_BUF )
        bindType = UR_BIND_UNBOUND;
    else
        bindType = ur_isShared(ctxN) ? UR_BIND_ENV : UR_BIND_THREAD;

    ur_foreach( di )
    {
        ur_setId(di.it, UT_WORD);
        ur_setBinding( di.it, bindType );
        di.it->word.ctx   = ctxN;
        di.it->word.index = ait - atoms;
        di.it->word.atom  = *ait++;
    }

    di.buf->used = used;
}


const UCell* context_select( UThread* ut, const UCell* cell, const UCell* sel,
                             UCell* tmp )
{
    const UBuffer* ctx;

    if( (ctx = ur_sortedContext( ut, cell )) )
    {
        if( ur_is(sel, UT_WORD) )
        {
            int i = ur_ctxLookup( ctx, ur_atom(sel) );
            if( i >= 0 )
                return ur_ctxCell(ctx, i);
            if( ur_atom(sel) == UR_ATOM_SELF )
            {
                *tmp = *cell;
                return tmp;
            }
            ur_error( ut, UR_ERR_SCRIPT, "context has no word '%s",
                      ur_wordCStr(sel) );
        }
#if 0
        else if( ur_is(sel, UT_LITWORD) )   // Deprecated
        {
            if( ur_atom(sel) == UR_ATOM_WORDS )
            {
                _contextWords( ut, ctx, cell->context.buf, tmp );
                return tmp;
            }
        }
#endif
        else
        {
            ur_error( ut, UR_ERR_SCRIPT,
                      "context select expected word!/lit-word!" );
        }
    }
    return 0;
}


static void context_print( UThread* ut, const UBuffer* buf, UBuffer* str,
                           int depth )
{
#define ASTACK_SIZE 8
    union {
        UAtom* heap;
        UAtom  stack[ ASTACK_SIZE ];
    } atoms;
    UAtom* ait;
    int alloced;
    const UCell* it  = buf->ptr.cell;
    const UCell* end = it + buf->used;

    // Get word atoms in order.
    if( buf->used > ASTACK_SIZE )
    {
        alloced = 1;
        atoms.heap = ait = (UAtom*) memAlloc( sizeof(UAtom) * buf->used );
        ur_ctxWordAtoms( buf, atoms.heap );
    }
    else
    {
        alloced = 0;
        ur_ctxWordAtoms( buf, ait = atoms.stack );
    }

    while( it != end )
    {
        ur_strAppendIndent( str, depth );
        ur_strAppendCStr( str, ur_atomCStr( ut, *ait++ ) );
        ur_strAppendCStr( str, ": " );
        DT( ur_type(it) )->toString( ut, it, str, depth );
        ur_strAppendChar( str, '\n' );
        ++it;
    }

    if( alloced )
        memFree( atoms.heap );
}


#define ur_ctxRecursion(buf)    (buf)->elemSize

#define ur_printRecurseEnd(cell,ctxb) \
    if( ! ur_isShared(cell->context.buf) ) \
        ur_ctxRecursion((UBuffer*) ctxb) = 0

static
const UBuffer* ur_printRecurse( UThread* ut, const UCell* cell, UBuffer* str )
{
    const UBuffer* buf = ur_bufferSer( cell );

    // Recursion on shared buffers is not handled.
    if( ur_isShared(cell->series.buf) )
        return buf;

    if( ur_ctxRecursion(buf) )
    {
        unset_toString( ut, cell, str, 0 );
        return 0;
    }

    ur_ctxRecursion((UBuffer*) buf) = 1;
    return buf;
}


void context_toText( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    const UBuffer* buf = ur_printRecurse( ut, cell, str );
    if( buf )
    {
        context_print( ut, buf, str, depth );
        ur_printRecurseEnd( cell, buf );
    }
}


/*
  If depth is -1 then the context word and braces will be omitted.
*/
void context_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    if( depth < 0 )
    {
        context_toText( ut, cell, str, 0 );
    }
    else
    {
        const UBuffer* buf = ur_printRecurse( ut, cell, str );
        if( buf )
        {
            ur_strAppendCStr( str, "context [\n" );
            context_print( ut, buf, str, depth + 1 );
            ur_strAppendIndent( str, depth );
            ur_strAppendCStr( str, "]" );
            ur_printRecurseEnd( cell, buf );
        }
    }
}


void context_destroy( UBuffer* buf )
{
    ur_ctxFree( buf );
}


UDatatype dt_context =
{
    "context!",
    context_make,           context_make,           context_copy,
    context_compare,        unset_operate,          context_select,
    context_toString,       context_toText,
    unset_recycle,          block_mark,             context_destroy,
    context_markBuf,        block_toShared,         unset_bind
};


//----------------------------------------------------------------------------
// UT_ERROR


int error_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_STRING) )
    {
        ur_setId(res, UT_ERROR);
        res->error.exType     = UR_ERR_SCRIPT;
        res->error.messageStr = from->series.buf;
        res->error.traceBlk   = UR_INVALID_BUF;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "make error! expected string! message" );
}


int error_compare( UThread* ut, const UCell* a, const UCell* b, int test )
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
            if( a->error.exType == b->error.exType &&
                a->error.messageStr == b->error.messageStr &&
                a->error.traceBlk == b->error.traceBlk )
                return 1;
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            if( ur_type(a) == ur_type(b) )
            {
                if( a->error.exType > b->error.exType )
                    return 1;
                if( a->error.exType < b->error.exType )
                    return -1;
                {
                UCell strA, strB;

                setStringCell( &strA, a->error.messageStr );
                setStringCell( &strB, b->error.messageStr );

                return string_compare( ut, &strA, &strB, test );
                }
            }
            break;
    }
    return 0;
}


static const char* errorTypeStr[] =
{
    "Datatype",
    "Script",
    "Syntax",
    "Access",
    "Internal"
};


static void _lineToString( UThread* ut, const UCell* bc, UBuffer* str )
{
    UBlockIter bi;
    const UCell* start;
    UIndex fstart;


    // Specialized version of ur_blkSlice() to get pointers even if 
    // bi.it is at bi.end.  Changing ur_blkSlice to do this would slow it
    // down with extra checks to validate that series.it < buf->used.

    bi.buf = ur_bufferSer(bc);
    if( ! bi.buf->ptr.cell || ! bi.buf->used )
        return;
    {
        UIndex end = bi.buf->used;
        if( (bc->series.end > -1) && (bc->series.end < end) )
            end = bc->series.end;
        if( end < bc->series.it )
            end = bc->series.it;
        bi.it  = bi.buf->ptr.cell + bc->series.it;
        bi.end = bi.buf->ptr.cell + end;
    }
    start = bi.it;
    if( bi.it == bi.end )
        --start;

    // Set end to newline after bc->series.it.
    if( bi.it != bi.end )
    {
        ++bi.it;
        ur_foreach( bi )
        {
            if( ur_flags(bi.it, UR_FLAG_SOL) )
                break;
        }
        bi.end = bi.it;
    }

    // Set start to newline at or before bc->series.it.
    while( start != bi.buf->ptr.cell )
    {
        if( ur_flags(start, UR_FLAG_SOL) )
            break;
        --start;
    }
    bi.it = start;

    // Convert to string without any open/close braces.
    ur_foreach( bi )
    {
        if( bi.it != start )
            ur_strAppendChar( str, ' ' );
        fstart = str->used;
        ur_toStr( ut, bi.it, str, 0 );
        if( ur_is(bi.it, UT_BLOCK) || ur_is(bi.it, UT_PAREN) )
        {
            fstart = ur_strFindChar( str, fstart, str->used, '\n', 0 );
            if( fstart > -1 )
                str->used = fstart;
        }
    }
}


void error_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    uint16_t et = cell->error.exType;
    const UBuffer* msg;
    (void) depth;

    if( et < UR_ERR_COUNT )
    {
        ur_strAppendCStr( str, errorTypeStr[ et ] );
        ur_strAppendCStr( str, " Error: " );
    }
    else
    {
        ur_strAppendCStr( str, "Error " );
        ur_strAppendInt( str, et );
        ur_strAppendCStr( str, ": " );
    }

    msg = ur_buffer( cell->error.messageStr );
    ur_strAppend( str, msg, 0, msg->used );

    if( cell->error.traceBlk > UR_INVALID_BUF )
    {
        UBlockIter bi;

        bi.buf = ur_buffer( cell->error.traceBlk );
        bi.it  = bi.buf->ptr.cell;
        bi.end = bi.it + bi.buf->used;

        if( bi.buf->used )
        {
            ur_strAppendCStr( str, "\nTrace:" );
            ur_foreach( bi )
            {
                ur_strAppendCStr( str, "\n -> " );
                _lineToString( ut, bi.it, str );
            }
        }
    }
}


void error_mark( UThread* ut, UCell* cell )
{
    UIndex n;
   
    ur_markBuffer( ut, cell->error.messageStr );

    n = cell->error.traceBlk;
    if( n > UR_INVALID_BUF )
    {
        if( ur_markBuffer( ut, n ) )
            block_markBuf( ut, ur_buffer(n) );
    }
}


void error_toShared( UCell* cell )
{
    UIndex n;
    n = cell->error.messageStr;
    if( n > UR_INVALID_BUF )
        cell->error.messageStr = -n;
    n = cell->error.traceBlk;
    if( n > UR_INVALID_BUF )
        cell->error.traceBlk = -n;
}


UDatatype dt_error =
{
    "error!",
    error_make,             error_make,             unset_copy,
    error_compare,          unset_operate,          unset_select,
    error_toString,         error_toString,
    unset_recycle,          error_mark,             unset_destroy,
    unset_markBuf,          error_toShared,         unset_bind
};


//EOF
