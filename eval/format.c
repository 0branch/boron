/*
  Copyright 2015,2019 Karl Robillard

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


/*-cf-
    format
        fmt     block!
        data
    return: string!
    group: io

    Convert values to formatted string.

    The format specification rules are:
      int!            Field width of next data item.
                      If negative then right align item in field.
      coord!          Field width and limit of next data item.
      char!/string!   Literal output to string.
      'pad char!      Set pad character for following fields.
*/
CFUNC(cfunc_format)
{
    static const char* padStr = "pad";
    UBlockIter fi;
    UBlockIter di;
    USeriesIter si;
    UBuffer tmp;
    UBuffer* str;
    const UCell* data = a1+1;
    UAtom atomPad = 0;
    int pad = ' ';
    int plen;
    int dlen;
    int colWidth;
    int limit;

    ur_makeStringCell( ut, UR_ENC_LATIN1, 32, res );  // gc!

    if( ur_is(data, UT_BLOCK) || ur_is(data, UT_PAREN) )
    {
        UCell* rv = boron_reduceBlock( ut, a2, ur_push(ut, UT_UNSET) ); // gc!
        ur_pop(ut);
        if( ! rv )
            return UR_THROW;
        ur_blkSlice( ut, &di, rv );
    }
    else
    {
        di.it  = data;
        di.end = di.it + 1;;
    }

    ur_blkSlice( ut, &fi, a1 );
    ur_strInit( &tmp, UR_ENC_LATIN1, 0 );
    str = ur_buffer( res->series.buf );

    ur_foreach( fi )
    {
        switch( ur_type(fi.it) )
        {
            case UT_INT:
                colWidth = ur_int(fi.it);
                limit = INT32_MAX;
emit_column:
                if( di.it >= di.end )
                {
                    si.buf = &tmp;          // Required by ur_strAppend.
                    si.it = si.end = 0;
                    dlen = tmp.used = 0;
                }
                else if( ur_isStringType( ur_type(di.it) ) )
                {
                    ur_seriesSlice( ut, &si, di.it++ );
                }
                else
                {
                    si.buf = &tmp;
                    tmp.used = si.it = 0;
                    ur_toText( ut, di.it++, &tmp );
                    si.end = tmp.used;
                }

                dlen = si.end - si.it;
                if( dlen > limit )
                {
                    dlen = limit;
                    si.end = si.it + limit;
                }

                if( colWidth < 0 )
                {
                    for( plen = -colWidth - dlen; plen > 0; --plen )
                        ur_strAppendChar( str, pad );
                    ur_strAppend( str, si.buf, si.it, si.end );
                }
                else
                {
                    ur_strAppend( str, si.buf, si.it, si.end );
                    for( plen = colWidth - dlen; plen > 0; --plen )
                        ur_strAppendChar( str, pad );
                }
                break;

            case UT_COORD:
                colWidth = fi.it->coord.n[0];
                limit    = fi.it->coord.n[1];
                goto emit_column;

            case UT_CHAR:
                ur_strAppendChar( str, ur_int(fi.it) );
                break;

            case UT_STRING:
                ur_seriesSlice( ut, &si, fi.it );
                ur_strAppend( str, si.buf, si.it, si.end );
                break;

            case UT_WORD:
                if( ! atomPad )
                    atomPad = ur_intern( ut, padStr, strlen(padStr) );
                if( ur_atom(fi.it) == atomPad )
                {
                    if( ++fi.it == fi.end )
                        goto cleanup;
                    if( ur_is(fi.it, UT_CHAR) )
                        pad = ur_int(fi.it);
                    else if( ur_is(fi.it, UT_INT) )
                        pad = ur_int(fi.it) + '0';
                }
                break;
        }
    }

cleanup:
    ur_strFree( &tmp );
    return UR_OK;
}


//EOF
