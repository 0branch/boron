/*
  Copyright 2009 Karl Robillard

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


#include "urlan.h"


/** \defgroup dt_path Datatype Path
  \ingroup urlan
  @{
*/ 


/**
  Get the value which a path refers to.

  \param pc     Valid UT_PATH cell.
  \param res    Set to value at end of path.

  \return UT_WORD/UT_GETWORD/UR_THROW
*/
int ur_pathCell( UThread* ut, const UCell* pc, UCell* res )
{
    UBlockIter bi;
    const UCell* node = 0;
    const UCell* selector;
    int type;

    ur_blkSlice( ut, &bi, pc );

    if( bi.it == bi.end )
    {
bad_word:
        return ur_error( ut, UR_ERR_SCRIPT,
                         "First path node must be a word!/get-word!");
    }

    type = ur_type(bi.it);
    if( type != UT_WORD && type != UT_GETWORD )
        goto bad_word;

    if( ! (node = ur_wordCell( ut, bi.it )) )
        return UR_THROW;
    if( ur_is(node, UT_UNSET) )
    {
        return ur_error( ut, UR_ERR_SCRIPT, "Path word '%s is unset",
                         ur_wordCStr( bi.it ) );
    }

    while( ++bi.it != bi.end )
    {
        if( ur_is(bi.it, UT_GETWORD) )
        {
            if( ! (selector = ur_wordCell( ut, bi.it )) )
                return UR_THROW;
        }
        else
        {
            selector = bi.it;
        }

        node = ut->types[ ur_type(node) ]->select( ut, node, selector, res );
        if( ! node )
            return UR_THROW;
    }
    if( node != res )
        *res = *node;
    return type;
}


/*
  Returns zero if word not found.
*/
static UCell* ur_blkSelectWord( UBuffer* buf, UAtom atom )
{
    // Should this work on cell and use UBlockIter?
    UCell* it  = buf->ptr.cell;
    UCell* end = it + buf->used;
    while( it != end )
    {
        // Checking atom first would be faster (it will fail more often and
        // is a quicker check), but the atom field may not be intialized
        // memory so memory checkers will report an error.
        if( ur_isWordType( ur_type(it) ) && (ur_atom(it) == atom) )
        {
            if( ++it == end )
                return 0;
            return it;
        }
        ++it;
    }
    return 0;
}


/**
  Set path.  This copies src into the cell which the path refers to.

  If any of the path words are unbound (or bound to the shared environment)
  then an error is generated and UR_THROW is returned.

  \param path   Valid path cell.
  \param src    Source value to copy.

  \return UR_OK/UR_THROW
*/
int ur_setPath( UThread* ut, const UCell* path, const UCell* src )
{
    UBlockIterM bi;
    UBuffer* buf;
    UCell* node = 0;

    ur_blkSliceM( ut, &bi, path );

    ur_foreach( bi )
    {
        switch( ur_type(bi.it) )
        {
            case UT_INT:
                if( node )
                {
                    int t = ur_type(node);
                    if( ur_isBlockType(t) )
                    {
                        if( ! (buf = ur_bufferSerM(node)) )
                            return UR_THROW;
                        t = node->series.it + ur_int(bi.it) - 1;
                        if( t > -1 && t < buf->used )
                        {
                            node = buf->ptr.cell + t;
                            break;
                        }
                    }
                    else if( ur_isSeriesType(t) )
                    {
                        if( ! (buf = ur_bufferSerM(node)) )
                            return UR_THROW;
                        if( bi.it + 1 == bi.end )
                        {
                            ((USeriesType*) ut->types[ t ])->poke( buf,
                                    node->series.it + ur_int(bi.it) - 1, src );
                            return UR_OK;
                        }
                    }
                }
                goto err;

            case UT_WORD:
                if( node )
                {
                    if( ur_is(node, UT_CONTEXT) )
                    {
                        int i;
                        if( ! (buf = ur_bufferSerM(node)) )
                            return UR_THROW;
                        i = ur_ctxLookup( buf, ur_atom(bi.it) );
                        if( i < 0 )
                            goto err;
                        node = buf->ptr.cell + i;
                    }
                    else if( ur_is(node, UT_BLOCK) )
                    {
                        if( ! (buf = ur_bufferSerM(node)) )
                            return UR_THROW;
                        node = ur_blkSelectWord( buf, ur_atom(bi.it) );
                        if( ! node )
                            goto err;
                    }
                }
                else
                {
                    if( ! (node = ur_wordCellM( ut, bi.it )) )
                        return UR_THROW;
                    if( ur_is(node, UT_UNSET) )
                    {
                        return ur_error( ut, UR_ERR_SCRIPT,
                                "Path word '%s is unset",
                                ur_wordCStr( bi.it ) );
                    }
                }
                break;

            case UT_GETWORD:
                //break;

            default:
                goto err;
        }
    }

    if( node )
        *node = *src;
    return UR_OK;

err:
    return ur_error( ut, UR_ERR_SCRIPT, "Invalid path! node" );
}


/** @} */ 


//EOF
