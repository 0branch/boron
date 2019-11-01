/*
  Copyright 2009,2019 Karl Robillard

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdio.h>
#include <string.h>
#include "urlan.h"
#include "urlan_atoms.h"


/*
  Recursively evaluate math expression.

  \param cell   Cell to evaluate.
  \param res    Result.

  \return UR_OK or UR_THROW.
*/
UStatus calc_eval( UThread* ut, UCell* cell, double* res )
{
    switch( ur_type(cell) )
    {
        case UT_WORD:
        {
            const UCell* val = ur_wordCell( ut, cell );
            if( ! val )
                return UR_THROW;
            if( ur_is(val, UT_DOUBLE) )
                *res = ur_double(val);
            else if( ur_is(val, UT_INT) || ur_is(val, UT_CHAR) )
                *res = (double) ur_int(val);
            else
            {
                return ur_error( ut, UR_ERR_SCRIPT, "Invalid word '%s",
                                 ur_wordCStr( cell ) );
            }
        }
            break;

        case UT_DOUBLE:
            *res = ur_double(cell);
            break;

        case UT_INT:
        case UT_CHAR:
            *res = (double) ur_int(cell);
            break;

        case UT_BLOCK:
        case UT_PAREN:
        {
            UBlockIterM bi;
            double num = 0.0;
            double right;

#define RIGHT_VAL \
    if( ++bi.it == bi.end ) \
        return ur_error( ut, UR_ERR_SCRIPT, "Expected operator r-value" ); \
    if( ! calc_eval( ut, bi.it, &right ) ) \
        return UR_THROW;

            if( ! ur_blkSliceM( ut, &bi, cell ) )
                return UR_THROW;
            ur_foreach( bi )
            {
                if( ur_is(bi.it, UT_WORD) )
                {
                    switch( ur_atom(bi.it) )
                    {
                        case UR_ATOM_PLUS:
                            RIGHT_VAL
                            num += right;
                            break;

                        case UR_ATOM_MINUS:
                            RIGHT_VAL
                            num -= right;
                            break;

                        case UR_ATOM_ASTERISK:
                            RIGHT_VAL
                            num *= right;
                            break;

                        case UR_ATOM_SLASH:
                            RIGHT_VAL
                            num /= right;
                            break;

                        default:
                            if( ! calc_eval( ut, bi.it, &num ) )
                                return UR_THROW;
                    }
                }
                else if( ur_is(bi.it, UT_SETWORD) )
                {
                    cell = ur_wordCellM( ut, bi.it );
                    if( ! cell )
                        return UR_THROW;
                    ur_setId( cell, UT_DOUBLE );
                    ur_double(cell) = num;
                }
                else
                {
                    if( ! calc_eval( ut, bi.it, &num ) )
                        return UR_THROW;
                }
            }
            *res = num;
        }
            break;

        default:
            *res = 0.0;
            break;
    }
    return UR_OK;
}


/*
  Evaluate C string.

  \param cmd  String to evaluate.

  \return UR_OK or UR_THROW.
*/
UStatus calc_evalCStr( UThread* ut, const char* cmd, double* result )
{
    UCell cell;
    UIndex blkN;
    UIndex hold;
    UStatus ok;
    int len = strlen( cmd );
    if( len )
    {
        blkN = ur_tokenize( ut, cmd, cmd + len, &cell );
        if( blkN )
        {
            ur_bind(ut, ur_buffer(blkN), ur_threadContext(ut), UR_BIND_THREAD);

            /* Since the program cell is not part of the dataStore,
             * the block must be manually held. */
            hold = ur_hold( blkN );
            ok = calc_eval( ut, &cell, result );
            ur_release( hold );

            return ok;
        }
        return UR_THROW;
    }
    return UR_OK;
}


/*
  Define the words 'pi and 'e.
*/
void defineWords( UThread* ut )
{
    static double constants[2] = { 3.14159265358979, 2.71828182845904 };
    UAtom atoms[2];
    UBuffer* ctx;
    UCell* cell;
    int i;

    ur_internAtoms( ut, "pi e", atoms );

    ctx = ur_threadContext( ut );
    for( i = 0; i < 2; ++i )
    {
        cell = ur_ctxAddWord( ctx, atoms[i] );
        ur_setId( cell, UT_DOUBLE );
        ur_double(cell) = constants[i];
    }
    ur_ctxSort( ctx );
}


int main( int argc, char** argv )
{
    UEnvParameters envParam;
    UThread* ut;
    char cmd[ 2048 ];
    double result;
    (void) argc;
    (void) argv;


    printf( "Urlan Calculator Example %s (%s)\n", UR_VERSION_STR, __DATE__ );

    ut = ur_makeEnv( ur_envParam(&envParam) );
    if( ! ut )
    {
        printf( "ur_makeEnv failed\n" );
        return 255;
    }

    // Create stack to hold exception.
    ur_blkAppendNew( &ut->stack, UT_UNSET );

    defineWords( ut );

    while( 1 )
    {
        printf( ")> " );
        fflush( stdout );   /* Required on Windows. */
        fgets( cmd, sizeof(cmd), stdin ); 

        if( cmd[0] < ' ' )
        {
            printf( "\n" );
        }
        else if( cmd[0] == 'q' )
        {
            break;
        }
        else
        {
            if( calc_evalCStr( ut, cmd, &result ) )
            {
                printf( "= %f\n", result );
            }
            else
            {
                UBuffer str;
                ur_strInit( &str, UR_ENC_UTF8, 0 );

                ur_toText( ut, ur_exception( ut ), &str );
                ur_strTermNull( &str );
                printf( "%s\n", str.ptr.c );

                ur_strFree( &str );
            }
        }
    }

    ur_freeEnv( ut );
    return 0;
}


//EOF
