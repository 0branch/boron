/*
  Copyright 2005-2011 Karl Robillard

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


#define _LARGEFILE64_SOURCE     1
#define _FILE_OFFSET_BITS       64      // off_t becomes 8 bytes.

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "os.h"
#include "os_file.h"
#include "boron.h"


#if 0
/*
   change-dir
   (dir -- logic)
*/
UR_CALL_PUB( uc_changeDir )
{
    int logic = 0;
    UString* str = ur_bin( tos );

    UR_CALL_UNUSED_TH;

    ur_termCStr( str );
    if( chdir( str->ptr.c ) == 0 )
        logic = 1;

    ur_initType( tos, UT_LOGIC );
    ur_logic(tos) = logic;
}


/*
   what-dir
   ( -- path)
*/
UR_CALL_PUB( uc_whatDir )
{
    char tmp[ 512 ];

    if( getcwd( tmp, sizeof(tmp) ) )
    {
        int len;
        UIndex binN;
        UString* str;
       
        len = strLen( tmp );
        assert( len > 0 );

        binN = ur_makeBinary( len + 1, &str );
        str->used = len;
        memCpy( str->ptr.c, tmp, len );

        if( tmp[ len - 1 ] != '/' )
        {
            str->ptr.c[ len ] = '/';
            ++str->used;
        }

        UR_S_GROW;
        tos = UR_TOS;
        ur_initString( tos, binN, 0 );  // UT_FILE
    }
    else
    {
        ur_throwErr( UR_ERR_ACCESS, strerror(errno) );
    }
}
#endif


/*
  Returns zero if error.
*/
int ur_fileInfo( const char* path, OSFileInfo* info, int mask )
{
    struct stat buf;
    if( stat( path, &buf ) == -1 )
        return 0;

    if( mask & FI_Size )
        info->size = buf.st_size;

    if( mask & FI_Time )
    {
        info->accessed = (double) buf.st_atime;
        info->modified = (double) buf.st_mtime;
    }

    if( mask & FI_Type )
    {
        if( S_ISREG(buf.st_mode) )
            info->type = FI_File;
        else if( S_ISDIR(buf.st_mode) )
            info->type = FI_Dir;
        else if( S_ISLNK(buf.st_mode) )
            info->type = FI_Link;
#ifdef S_ISSOCK
        else if( S_ISSOCK(buf.st_mode) )
            info->type = FI_Socket;
#endif
        else
            info->type = FI_Other;
    }

    return 1;
}


/*
  Returns 1 if directory, 0 if file or -1 if error.
*/
static int _isDir( const char* path )
{
    struct stat buf;
    if( stat( path, &buf ) == -1 )
        return -1;
    return S_ISDIR(buf.st_mode) ? 1 : 0;
}


/*
  Return UR_OK/UR_THROW.
*/
int ur_makeDir( UThread* ut, const char* path )
{
    int err = mkdir( path, 0755 );
    if( err )
    {
        if( (errno != EEXIST) || (_isDir(path) != 1) )
        {
            ur_error( ut, UR_ERR_ACCESS, strerror/*_r*/(errno) );
            return UR_THROW;
        }
    }
    return UR_OK;
}


double ur_now()
{
    struct timeval tp;
    gettimeofday( &tp, 0 );
    return ((double) tp.tv_sec) + ((double) tp.tv_usec) * 0.000001;
}


/*
  Sets result to block of files or false.
  Returns UR_OK.
*/
int ur_readDir( UThread* ut, const char* filename, UCell* res )
{
    DIR* dir;
    struct dirent* entry;

    dir = opendir( filename );
    if( dir )
    {
        UCell* cell;
        UIndex blkN;
        UIndex hold;
        UIndex strN;

        blkN = ur_makeBlock( ut, 0 );
        hold = ur_hold( blkN );

        while( (entry = readdir( dir )) )
        {
            const char* cp = entry->d_name;
            if( cp[0] == '.' && (cp[1] == '.' || cp[1] == '\0') )
                continue;

            strN = ur_makeStringUtf8( ut, (const uint8_t*) cp,
                                          (const uint8_t*) cp + strlen(cp) );
            cell = ur_blkAppendNew( ur_buffer(blkN), UT_FILE );
            ur_setSeries( cell, strN, 0 );
        }

        closedir( dir );

        ur_release( hold );

        ur_setId(res, UT_BLOCK);
        ur_setSeries(res, blkN, 0);
    }
    else
    {
        ur_setId(res, UT_LOGIC);
        ur_int(res) = 0;
    }
    return UR_OK;
}


#ifdef CONFIG_EXECUTE
static void argumentList( char* cp, char** argv, int maxArg )
{
    int prevWhite = 1;
    int argc = 0;

    while( *cp != '\0' )
    {
        if( *cp == ' ' || *cp == '\t' || *cp == '\n' )
        {
            *cp = '\0';
            prevWhite = 1;
        }
        else if( prevWhite )
        {
            prevWhite = 0;
            argv[ argc ] = cp;
            ++argc;
            if( argc == maxArg )
                goto term;
        }
        ++cp;
    }

term:

    argv[ argc ] = 0;
}


static int _execOutput( UThread* ut, char** argv, UCell* res, UBuffer* buf )
{
#define BUFSIZE     512
    int pid;
    int pfd[2];

    if( pipe(pfd) == -1 )
        return ur_error( ut, UR_ERR_INTERNAL, "pipe failed" );

    pid = fork();
    if( pid == 0 )
    {
        // In child process; invoke command.

        close( pfd[0] );
        dup2( pfd[1], 1 );
        close( pfd[1] );

        if( execvp( argv[0], argv ) == -1 )
        {
            perror( "execvp" );
            _exit( 255 );
        }
        return 0;
    }
    else if( pid > 0 )
    {
        // In parent process.

        int n;
        int status;

        close( pfd[1] );

        buf->used = 0;
        if( buf->type == UT_STRING )
        {
            int ucs2 = ur_strIsUcs2(buf);
            ur_arrReserve( buf, BUFSIZE ); 
            while( (n = read( pfd[0], buf->ptr.c + buf->used, BUFSIZE )) > 0 )
            {
                buf->used += ucs2 ? n >> 1 : n;
                ur_arrReserve( buf, buf->used + BUFSIZE ); 
            }
        }
        else
        {
            ur_binReserve( buf, BUFSIZE ); 
            while( (n = read( pfd[0], buf->ptr.c + buf->used, BUFSIZE )) > 0 )
            {
                buf->used += n;
                ur_binReserve( buf, buf->used + BUFSIZE ); 
            }
        }

        close( pfd[0] );

        waitpid( pid, &status, 0 );
        if( WIFEXITED(status) )
        {
            ur_setId(res, UT_INT);
            ur_int(res) = WEXITSTATUS(status);
            return UR_OK;
        }
        ur_setId(res, UT_NONE);
        return UR_OK;
    }
    else
    {
        // fork failed.
        close( pfd[0] );
        close( pfd[1] );
        return ur_error( ut, UR_ERR_INTERNAL, "fork failed" );
    }
}


#ifdef EXECUTE_WAIT
static int _asyncProcCount = 0;
#endif

static int _execSimple( UThread* ut, char**argv, UCell* res )
{
    int pid;
    int status;
#ifdef EXECUTE_WAIT
    int waitRef;

    waitRef = orRefineSet(REF_CALL_WAIT);
    if( ! waitRef )
    {
        // Need to call wait() when SIGCHLD is received to avoid zombies.
        /*
         FIXME: Using _asyncProcCount may cause the signal handler to get
         the status if call/wait is called while an asynchronous process is
         still running.
        */
        _asyncProcCount++;
    }
#endif

    pid = fork();
    if( pid == 0 )
    {
        // In child process; invoke command.

        if( execvp( argv[0], argv ) == -1 )
        {
            perror( "execvp" );
            _exit( 255 );
        }
        return 0;
    }
    else if( pid > 0 )
    {
        // In parent process.

#ifdef EXECUTE_WAIT
        if( waitRef )
#endif
        {
            waitpid( pid, &status, 0 );
            if( WIFEXITED(status) )
            {
                ur_setId(res, UT_INT);
                ur_int(res) = WEXITSTATUS( status );
                return UR_OK;
            }
        }
        ur_setId(res, UT_NONE);
        return UR_OK;
    }
    else
    {
        // fork failed.
#ifdef EXECUTE_WAIT
        if( ! waitRef )
            --_asyncProcCount;
#endif
        return ur_error( ut, UR_ERR_INTERNAL, "fork failed" );
    }
}


/*-cf-
    execute
        command     string!
        /out        Store output of command.
            buffer  binary!/string!
    return: return status of command
    group: os

    Runs an external program.
*/
CFUNC_PUB( cfunc_execute )
{
    char* argv[10];

    if( ! ur_is(a1, UT_STRING) )
    {
        ur_setId(res, UT_LOGIC);
        ur_int(res) = 0;
        return UR_OK;
    }

    // Create arg list before fork to avoid any possible copy-on-write overhead.
    argumentList( boron_cstr( ut, a1, 0 ), argv, 9 );

    if( CFUNC_OPTIONS & 1 )
    {
        UBuffer* buf;
        int type = ur_type(a1 + 1);

        if( type != UT_BINARY && type != UT_STRING )
            return ur_error( ut, UR_ERR_TYPE,
                             "execute expected binary!/string! output" );
        if( ! (buf = ur_bufferSerM(a1 + 1)) )
            return UR_THROW;

        return _execOutput( ut, argv, res, buf );
    }
    return _execSimple( ut, argv, res );
}


#ifdef EXECUTE_WAIT
/* Cleans up after callSimple() */
static void child_handler( int sig )
{
    (void) sig;

    if( _asyncProcCount )
    {
        int status;
        wait( &status );
        --_asyncProcCount;
    }
}


void ur_installExceptionHandlers()
{
    /* NOTE: Cannot use this if using Qt's QProcess (it uses SIGCHLD) */
    struct sigaction childSA;

    childSA.sa_handler = child_handler;
    childSA.sa_flags   = SA_NOCLDSTOP;
    sigemptyset( &childSA.sa_mask );

    sigaction( SIGCHLD, &childSA, NULL );
}
#endif
#endif


/*EOF*/
