/*
  Copyright 2009,2012 Karl Robillard

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

  \param pi     Path iterator.
  \param res    Set to value at end of path.

  \return UT_WORD/UT_GETWORD/UR_THROW

  \sa ur_pathCell, ur_wordCell
*/
int ur_pathValue( UThread* ut, UBlockItC* pi, UCell* res )
{
    const UCell* obj = 0;
    const UCell* selector;
    const UCell* (*selectf)( UThread*, const UCell*, const UCell*, UCell* );
    int type;

    if( pi->it == pi->end )
    {
bad_word:
        return ur_error( ut, UR_ERR_SCRIPT,
                         "Path must start with a word!/get-word!");
    }

    type = ur_type(pi->it);
    if( type != UT_WORD && type != UT_GETWORD )
        goto bad_word;

    if( ! (obj = ur_wordCell( ut, pi->it )) )
        return UR_THROW;
    if( ur_is(obj, UT_UNSET) )
    {
        return ur_error( ut, UR_ERR_SCRIPT, "Path word '%s is unset",
                         ur_wordCStr( pi->it ) );
    }

    while( ++pi->it != pi->end )
    {
        // If the select method is NULL, return obj as the result and leave
        // pi->it pointing to the untraversed path segments.
        selectf = ut->types[ ur_type(obj) ]->select;
        if( ! selectf )
            break;

        selector = pi->it;
        if( ur_is(selector, UT_GETWORD) )
        {
            if( ! (selector = ur_wordCell( ut, selector )) )
                return UR_THROW;
        }

        obj = selectf( ut, obj, selector, res );
        if( ! obj )
            return UR_THROW;
    }

    if( obj != res )
        *res = *obj;
    return type;
}


/**
  Get the value which a path refers to.

  \param pc     Valid UT_PATH cell.
  \param res    Set to value at end of path.

  \return UT_WORD/UT_GETWORD/UR_THROW

  \sa ur_pathValue, ur_wordCell
*/
int ur_pathCell( UThread* ut, const UCell* pc, UCell* res )
{
    UBlockItC bi;
    ur_blockItC( ut, pc, &bi );
    return ur_pathValue( ut, &bi, res );
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


extern int coord_poke( UThread*, UCell* cell, int index, const UCell* src );
extern int vec3_poke ( UThread*, UCell* cell, int index, const UCell* src );

/**
  Set path.  This copies src into the cell which the path refers to.

  If any of the path words are unbound (or bound to the shared environment)
  then an error is generated and UR_THROW is returned.

  \param path   Valid path cell.
  \param src    Source value to copy.

  \return UR_OK/UR_THROW
*/
UStatus ur_setPath( UThread* ut, const UCell* path, const UCell* src )
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
                    int index = ur_int(bi.it) - 1;
                    if( ur_isBlockType(t) )
                    {
                        if( ! (buf = ur_bufferSerM(node)) )
                            return UR_THROW;
                        t = node->series.it + index;
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
                                    node->series.it + index, src );
                            return UR_OK;
                        }
                    }
                    else if( t == UT_VEC3 )
                    {
                        return vec3_poke( ut, node, index, src );
                    }
                    else if( t == UT_COORD )
                    {
                        return coord_poke( ut, node, index, src );
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
                    else
                        goto err;
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
