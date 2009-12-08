/*
  Copyright 2009 Karl Robillard

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


#include <assert.h>
#include <stdio.h>
#include "boron.h"
#include "urlan_atoms.h"
#include "str.h"


#define APPNAME     "Boron"

#define ESC         27
#define PRINT_MAX   156


void usage( const char* arg0 )
{
    printf( APPNAME " %s (%s)\n\n", UR_VERSION_STR, __DATE__ );
    printf( "Usage: %s [options] [script] [arguments]\n\n", arg0 );
    printf( "Options:\n"
          //"  -s      Disable security\n"
            "  -e exp  Evaluate expression\n"
            "  -h      Show this help and exit\n"
          );
}


void reportError( UThread* ut, UCell* err, UBuffer* str )
{
    str->used = 0;
    ur_toText( ut, err, str );
    ur_strTermNull( str );

    printf( str->ptr.c );
    putchar( '\n' );
}


int main( int argc, char** argv )
{
    char cmd[ 2048 ];
    UThread* ut;
    UBuffer rstr;
    int fileN = 0;
    int ret = 0;


    ut = boron_makeEnv();
    if( ! ut )
    {
        printf( "boron_makeEnv failed\n" );
        return 70;      // EX_SOFTWARE
    }

    ur_freezeEnv( ut );


    if( argc > 1 )
    {
        int i;
        char* arg;

        for( i = 1; i < argc; ++i )
        {
            arg = argv[i];
            if( arg[0] == '-' )
            {
                switch( arg[1] )
                {
                    case 'e':
                        if( ++i >= argc )
                            goto usage_err;
                        fileN = -i;
                        i = argc;
                        break;
#if 0
                    case 's':
                        ur_disable( ut, UR_ENV_SECURE );
                        break;
#endif
                    case 'h':
                        usage( argv[0] );
                        return 0;

                    default:
usage_err:
                        usage( argv[0] );
                        return 64;      // EX_USAGE
                }
            }
            else
            {
                fileN = i;
                break;
            }
        }
    }

    ur_strInit( &rstr, UR_ENC_UTF8, 0 );

    if( fileN )
    {
        char* pos;
        int exp;

        if( fileN < 0 )
        {
            fileN = -fileN;
            exp = 1;
        }
        else
            exp = 0;

        pos = cmd;
        cmd[ sizeof(cmd) - 1 ] = -1;

        // Create args block for any command line parameters.
        if( (argc - fileN) > 1 )
        {
            int i;
            pos = str_copy( pos, "args: [" );
            for( i = fileN + 1; i < argc; ++i )
            {
                *pos++ = '"';
                pos = str_copy( pos, argv[i] );
                *pos++ = '"';
                *pos++ = ' ';
            }
            *pos++ = ']';
        }
        else
        {
            pos = str_copy( pos, "args: none " );
        }

        if( exp )
        {
            pos = str_copy( pos, argv[fileN] );
        }
        else
        {
            pos = str_copy( pos, "do load {" );
            pos = str_copy( pos, argv[fileN] );
            *pos++ = '}';
        }

        assert( cmd[ sizeof(cmd) - 1 ] == -1 && "cmd buffer overflow" );

        if( ! boron_doCStr( ut, cmd, pos - cmd ) )
        {
            UCell* ex = boron_exception( ut );
            if( ur_is(ex, UT_ERROR) )
            {
                reportError( ut, ex, &rstr );
                goto prompt;
            }
            else if( ur_is(ex, UT_WORD) )
            {
                switch( ur_atom(ex) ) 
                {
                    case UR_ATOM_QUIT:
                        goto quit;

                    case UR_ATOM_HALT:
                        goto prompt;
                        break;
                }
            }
        }
    }
    else
    {
        printf( APPNAME " %s (%s)\n", UR_VERSION_STR, __DATE__ );

prompt:

        while( 1 )
        {
            printf( ")> " );
            fflush( stdout );   /* Required on Windows. */
            fgets( cmd, sizeof(cmd), stdin ); 
#if 0
            {
                char* cp = cmd;
                while( *cp != '\n' )
                    printf( " %d", (int) *cp++ );
                printf( "\n" );
            }
#endif

            if( cmd[0] == ESC )
            {
                // Up   27 91 65
                // Down 27 91 66
                printf( "\n" );
            }
            else if( cmd[0] != '\n' )
            {
#if 0
                if( cmd[0] == 'q' )
                    goto quit;
#endif
                if( boron_doCStr( ut, cmd, -1 ) )
                {
                    UCell* val = boron_result( ut );

                    if( ur_is(val, UT_UNSET) ||
                        ur_is(val, UT_CONTEXT) ) //||
                        //ur_is(val, UT_FUNC) )
                        goto prompt;

                    rstr.used = 0;
                    ur_toStr( ut, val, &rstr, 0 );
                    if( rstr.ptr.c )
                    {
                        ur_strTermNull( &rstr );
                        if( rstr.used > PRINT_MAX )
                        {
                            char* cp = str_copy( rstr.ptr.c + PRINT_MAX - 4,
                                                 "..." );
                            *cp = '\0';
                        }
                        printf( "== %s\n", rstr.ptr.c );
                    }
                }
                else
                {
                    UCell* ex = boron_exception( ut );
                    if( ur_is(ex, UT_ERROR) )
                    {
                        reportError( ut, ex, &rstr );
                    }
                    else if( ur_is(ex, UT_WORD) )
                    {
                        switch( ur_atom(ex) ) 
                        {
                            case UR_ATOM_QUIT:
                                goto quit;

                            case UR_ATOM_HALT:
                                printf( "**halt\n" );
                                break;

                            default:
                                printf( "**unhandled excepetion %s\n",
                                        ur_atomCStr(ut,ur_atom(ex)) );
                                break;
                        }
                    }
                    boron_reset( ut );
                }
            }
        }
    }

quit:

    ur_strFree( &rstr );
    boron_freeEnv( ut );

    return ret;
}


/*EOF*/
