/*
  Copyright 2009,2011,2017 Karl Robillard

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
#include <string.h>
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


// Create args block for any command line parameters.
void createArgs( UThread* ut, int argc, char** argv )
{
    UCell* cell;
    UBuffer* blk;
    const uint8_t* cp;
    UIndex* bufN;
    UIndex* bi;
    int i, n;

    cell = ur_ctxAddWord( ur_threadContext(ut), ur_intern(ut, "args", 4) );
    if( argc > 0 )
    {
        n = argc + 1;
        bufN = bi = (UIndex*) malloc( sizeof(UIndex) * n );
        if( bufN )
        {
            blk = ur_genBuffers( ut, n, bufN );
            ur_blkInit( blk, UT_BLOCK, argc );
            blk->used = argc;
            ur_initSeries( cell, UT_BLOCK, *bi++ );

            cell = blk->ptr.cell;
            for( i = 0; i < argc; ++cell, ++bi, ++i )
            {
                ur_initSeries( cell, UT_STRING, *bi );

                cp = (const uint8_t*) argv[i];
                ur_strInitUtf8( ur_buffer(*bi), cp, cp + strlen((char*)cp) );
            }

            free( bufN );
            return;
        }
    }
    ur_setId(cell, UT_NONE);
}


UIndex handleException( UThread* ut, UBuffer* str, int* rc )
{
    const UCell* res = ur_exception( ut );
    *rc = 1;
    if( ur_is(res, UT_WORD) )
    {
        switch( ur_atom(res) )
        {
            case UR_ATOM_QUIT:
                /*
                res = boron_result( ut );
                if( ur_is(res, UT_INT) )
                    *rc = ur_int(res) & 0xff;
                */
                return UR_ATOM_QUIT;

            case UR_ATOM_HALT:
                printf( "**halt\n" );
                return UR_ATOM_HALT;

            default:
                printf( "**unhandled exception %s\n", ur_wordCStr( res ) );
                break;
        }
    }
    else
    {
        str->used = 0;
        ur_toText( ut, res, str );
        ur_strTermNull( str );

        puts( str->ptr.c );
    }
    return 0;
}


int main( int argc, char** argv )
{
    char* cmd = 0;
    UThread* ut;
    UBuffer rstr;
    UCell* res;
    int fileN = 0;
    int returnCode = 0;
    int i;
    char promptDisabled = 0;
    char secure = 1;


    // Parse arguments.
    for( i = 1; i < argc; ++i )
    {
        const char* arg = argv[i];
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


    {
    UEnvParameters param;
    ut = boron_makeEnv( boron_envParam(&param) );
    }
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

    if( fileN > 0 )
    {
        UStatus ok;

        createArgs( ut, argc - 1 - fileN, argv + 1 + fileN );

        res = ut->stack.ptr.cell + ut->stack.used - 1;
        if( ! boron_load( ut, argv[fileN], res ) )
            goto exception;

        ok = boron_doBlock( ut, res, ur_push(ut, UT_UNSET) );
        ur_pop(ut);
        if( ! ok )
            goto exception;
    }
    else if( fileN < 0 )
    {
        fileN = -fileN;
        createArgs( ut, argc - 1 - fileN, argv + 1 + fileN );
        res = boron_evalUtf8( ut, argv[fileN], -1 );
        if( ! res )
        {
exception:
            if( handleException( ut, &rstr, &returnCode ) == UR_ATOM_HALT )
                goto prompt;
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
                res = boron_evalUtf8( ut, cmd, -1 );
                if( res )
                {
                    if( ur_is(res, UT_UNSET) ||
                        ur_is(res, UT_CONTEXT) ) //||
                        //ur_is(res, UT_FUNC) )
                        continue;

                    rstr.used = 0;
                    ur_toStr( ut, res, &rstr, 0 );
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
                    if( handleException( ut, &rstr, &returnCode ) == UR_ATOM_QUIT )
                        goto cleanup;
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
    return returnCode;
}


/*EOF*/
