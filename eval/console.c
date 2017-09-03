/*
  Copyright 2009,2011 Karl Robillard

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
#include <stdlib.h>
#ifdef _WIN32
#include <winsock2.h>
#define setenv(name,val,over)   SetEnvironmentVariable(name, val)
#endif
#ifdef CONFIG_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif
#ifdef CONFIG_LINENOISE
#include "linenoise.h"
#define readline    linenoise
#define add_history linenoiseHistoryAdd
#endif
#include "boron.h"
#include "urlan_atoms.h"
#include "str.h"
#ifdef BORON_GL
#include "boron-gl.h"
#define boron_makeEnv   boron_makeEnvGL
#define boron_freeEnv   boron_freeEnvGL
#define APPNAME     "Boron-GL"
#else
#define APPNAME     "Boron"
#endif


#define PROMPT      ")> "
#define CMD_SIZE    2048
#define PRINT_MAX   156
#ifndef CONFIG_READLINE
#define ESC         27
#endif


void usage( const char* arg0 )
{
    printf( APPNAME " %s (%s)\n\n", BORON_VERSION_STR, __DATE__ );
    printf( "Usage: %s [options] [script] [arguments]\n\n", arg0 );
    printf( "Options:\n"
#ifdef BORON_GL
            "  -a      Disable audio\n"
#endif
            "  -e exp  Evaluate expression\n"
            "  -h      Show this help and exit\n"
            "  -p      Disable prompt and exit on exception\n"
            "  -s      Disable security\n"
          );
}


int denyAccess( UThread* ut, const char* msg )
{
    (void) ut;
    (void) msg;
    return UR_ACCESS_DENY;
}


int requestAccess( UThread* ut, const char* msg )
{
    char answer[8];
    (void) ut;

    printf( "%s? (y/n/a) ", msg );
    fflush( stdout );   /* Required on Windows. */
    fgets( answer, sizeof(answer), stdin );

    if( *answer == 'y' )
        return UR_ACCESS_ALLOW;
    if( *answer == 'a' )
        return UR_ACCESS_ALWAYS;
    return UR_ACCESS_DENY;
}


void reportError( UThread* ut, UCell* err, UBuffer* str )
{
    str->used = 0;
    ur_toText( ut, err, str );
    ur_strTermNull( str );

    puts( str->ptr.c );
}


int main( int argc, char** argv )
{
    char* cmd = 0;
    UThread* ut;
    UBuffer rstr;
    UCell* val;
    int fileN = 0;
    int ret = 0;
    char promptDisabled = 0;
    char secure = 1;


    // Parse arguments.
    if( argc > 1 )
    {
        int i;
        char* arg;

        for( i = 1; i < argc; ++i )
        {
            arg = argv[i];
            if( arg[0] == '-' )
            {
                // Handle concatenated option characters.  This is useful for
                // shell sha-bang invocation which may only pass one argument.
                for( ++arg; *arg != '\0'; ++arg )
                {
                    switch( *arg )
                    {
#ifdef BORON_GL
                        case 'a':
                            setenv( "BORON_GL_AUDIO", "0", 1 );
                            break;
#endif
                        case 'e':
                            if( ++i >= argc )
                                goto usage_err;
                            fileN = -i;
                            i = argc;
                            break;

                        case 'h':
                            usage( argv[0] );
                            return 0;

                        case 'p':
                            promptDisabled = 1;
                            break;

                        case 's':
                            secure = 0;
                            break;

                        default:
usage_err:
                            usage( argv[0] );
                            return 64;      // EX_USAGE
                    }
                }
            }
            else
            {
                fileN = i;
                break;
            }
        }
    }


    ut = boron_makeEnv( 0, 0 );
    if( ! ut )
    {
        puts( "boron_makeEnv failed" );
        return 70;      // EX_SOFTWARE
    }
    ur_freezeEnv( ut );

    if( secure )
        boron_setAccessFunc( ut, promptDisabled ? denyAccess : requestAccess );

    ur_strInit( &rstr, UR_ENC_UTF8, 0 );

#ifdef _WIN32
    {
    WORD wsver;
    WSADATA wsdata;
    wsver = MAKEWORD( 2, 2 );
    WSAStartup( wsver, &wsdata );
    }
#endif

    if( fileN )
    {
        char* pos;
        int expression;

        if( fileN < 0 )
        {
            fileN = -fileN;
            expression = 1;
        }
        else
            expression = 0;

        pos = cmd = malloc( CMD_SIZE );
        cmd[ CMD_SIZE - 1 ] = -1;

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
            *pos++ = '\n';
        }
        else
        {
            pos = str_copy( pos, "args: none\n" );
        }

        if( expression )
        {
            pos = str_copy( pos, argv[fileN] );
        }
        else
        {
            pos = str_copy( pos, "do load {" );
            pos = str_copy( pos, argv[fileN] );
            *pos++ = '}';
        }

        assert( cmd[ CMD_SIZE - 1 ] == -1 && "cmd buffer overflow" );

        if( ! boron_doCStr( ut, cmd, pos - cmd ) )
        {
            UCell* ex = boron_exception( ut );
            if( ur_is(ex, UT_ERROR) )
            {
                reportError( ut, ex, &rstr );
                if( promptDisabled )
                    ret = 1;
                else
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
                }
            }
        }
    }
    else
    {
        printf( APPNAME " %s (%s)\n", BORON_VERSION_STR, __DATE__ );

prompt:

        if( promptDisabled )
            goto cleanup;

#ifdef CONFIG_READLINE
        rl_bind_key( '\t', rl_insert );     // Disable tab completion.
#elif ! defined(CONFIG_LINENOISE)
        if( ! cmd )
            cmd = malloc( CMD_SIZE );
#endif

        while( 1 )
        {
#if defined(CONFIG_READLINE) || defined(CONFIG_LINENOISE)
            free( cmd );
            cmd = readline( PROMPT );
            if( ! cmd || ! *cmd )
               continue;
            add_history( cmd );
#else
            printf( PROMPT );
            fflush( stdout );   /* Required on Windows. */
            fgets( cmd, CMD_SIZE, stdin ); 
#endif
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
                    val = boron_result( ut );

                    if( ur_is(val, UT_UNSET) ||
                        ur_is(val, UT_CONTEXT) ) //||
                        //ur_is(val, UT_FUNC) )
                        continue;

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
                                printf( "**unhandled exception %s\n",
                                        ur_wordCStr( ex ) );
                                break;
                        }
                    }
                    boron_reset( ut );
                }
            }
        }
    }

cleanup:

    ur_strFree( &rstr );
    boron_freeEnv( ut );

#ifdef _WIN32
    WSACleanup();
#endif

    free( cmd );
    return ret;

quit:

    val = boron_result( ut );
    if( ur_is(val, UT_INT) )
        ret = ur_int(val) & 0xff;
    goto cleanup;
}


/*EOF*/
