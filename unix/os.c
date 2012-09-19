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
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "env.h"
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
    int quotes = 0;
    int argc = 0;

    while( *cp != '\0' )
    {
        if( *cp == ' ' || *cp == '\t' || *cp == '\n' )
        {
            if( ! quotes )
            {
                *cp = '\0';
                prevWhite = 1;
            }
        }
        else if( *cp == '"' )
        {
            if( quotes )
                *cp = '\0';
            quotes ^= 1;
        }
        else if( prevWhite )
        {
            prevWhite = 0;
            argv[ argc ] = cp;
            if( ++argc == maxArg )
                goto term;
        }
        ++cp;
    }

term:

    argv[ argc ] = 0;
}


static void _readPipe( int fd, UBuffer* buf )
{
#define BUFSIZE     1024
    int n;

    buf->used = 0;
    if( buf->type == UT_STRING )
    {
        int ucs2 = ur_strIsUcs2(buf);
        ur_arrReserve( buf, BUFSIZE ); 
        while( (n = read( fd, buf->ptr.c + buf->used, BUFSIZE )) > 0 )
        {
            buf->used += ucs2 ? n >> 1 : n;
            ur_arrReserve( buf, buf->used + BUFSIZE ); 
        }
    }
    else
    {
        ur_binReserve( buf, BUFSIZE ); 
        while( (n = read( fd, buf->ptr.c + buf->used, BUFSIZE )) > 0 )
        {
            buf->used += n;
            ur_binReserve( buf, buf->used + BUFSIZE ); 
        }
    }
}


static void _closePipe( int pipe[2] )
{
    close( pipe[0] );
    close( pipe[1] );
}


static int _execIO( UThread* ut, char** argv, UCell* res,
                    const UBuffer* in, UBuffer* out )
{
    pid_t pid;
    int inPipe[2];
    int outPipe[2];

    if( in )
    {
        if( pipe(inPipe) == -1 )
            return ur_error( ut, UR_ERR_INTERNAL, "pipe failed" );
    }
    if( out )
    {
        if( pipe(outPipe) == -1 )
        {
            if( in )
                _closePipe( inPipe );
            return ur_error( ut, UR_ERR_INTERNAL, "pipe failed" );
        }
    }

    pid = fork();
    if( pid == 0 )
    {
        // In child process; invoke command.

        if( in )
        {
            // Set stdin to pipe.
            close( inPipe[1] );
            dup2( inPipe[0], 0 );
            close( inPipe[0] );
        }
        if( out )
        {
            // Set stdout to pipe.
            close( outPipe[0] );
            dup2( outPipe[1], 1 );
            close( outPipe[1] );
        }

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

        int status;

        if( in )
        {
            close( inPipe[0] );
            write( inPipe[1], in->ptr.v,
                   (in->type == UT_STRING) ? in->used * in->elemSize
                                           : in->used );
            close( inPipe[1] );
        }
        if( out )
        {
            close( outPipe[1] );
            _readPipe( outPipe[0], out );
            close( outPipe[0] );
        }

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

        if( in )
            _closePipe( inPipe );
        if( out )
            _closePipe( outPipe );
        return ur_error( ut, UR_ERR_INTERNAL, "fork failed" );
    }
}


#define MAX_SPAWN   4
static pid_t _spawnPids[ MAX_SPAWN ] = { 0, 0, 0, 0 };
static char  _sigchldInstalled = 0;


/*
    Must call wait() or waitpid() when SIGCHLD is received to avoid zombies.
    Using wait() is not an option since it may get the status of a process
    forked by _execIO.
*/
static void _childHandler( int signum, siginfo_t* info, void* context )
{
    int status;
    int i;
    pid_t pid = info->si_pid;

    (void) signum;
    (void) context;

    for( i = 0; i < MAX_SPAWN; ++i )
    {
        if( pid == _spawnPids[i] )
        {
            waitpid( pid, &status, 0 );
            _spawnPids[i] = 0;
            return;
        }
    }
}


extern UPortDevice port_file;
#define FD  used

/*
    NOTE: Cannot use _execSpawn if using Qt's QProcess (it uses SIGCHLD).
*/
static int _execSpawn( UThread* ut, char** argv, int port, UCell* res )
{
    UEnv* env = ut->env;
    pid_t pid;
    int pslot;
    int outPipe[2];


    if( port )
    {
        if( pipe(outPipe) == -1 )
            return ur_error( ut, UR_ERR_INTERNAL, "pipe failed" );
    }

    LOCK_GLOBAL

    if( ! _sigchldInstalled )
    {
        struct sigaction childSA;

        _sigchldInstalled = 1;

        childSA.sa_sigaction = _childHandler;
        childSA.sa_flags     = SA_SIGINFO | SA_NOCLDSTOP;
        sigemptyset( &childSA.sa_mask );

        sigaction( SIGCHLD, &childSA, NULL );
    }

    for( pslot = 0; pslot < MAX_SPAWN; ++pslot )
    {
        if( ! _spawnPids[ pslot ] )
        {
            // Using PID 1 as placeholder till fork returns the actual PID.
            _spawnPids[ pslot ] = 1;
            break;
        }
    }

    UNLOCK_GLOBAL

    if( pslot == MAX_SPAWN )
    {
        if( port )
            _closePipe( outPipe );
        return ur_error( ut, UR_ERR_INTERNAL,
                         "execute only handles %d spawn", MAX_SPAWN );
    }

    pid = fork();
    if( pid == 0 )
    {
        // In child process; invoke command.

        close( STDIN_FILENO );

        if( port )
        {
            // Redirect stdout & stderr to pipe.
            close( outPipe[0] );
            dup2( outPipe[1], STDOUT_FILENO );
            dup2( outPipe[1], STDERR_FILENO );
            close( outPipe[1] );
        }
        else
        {
            int nul = open( "/dev/null", O_WRONLY );
            dup2( nul, STDOUT_FILENO );
            close( nul );

            // Don't change stderr so perror() will be seen.
        }

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

        // NOTE: If somehow the child exits and _childHandler is called
        //       before _spawnPids is set here then we'll get a zombie and
        //       the pslot will remain forever.

        _spawnPids[ pslot ] = pid;

        if( port )
        {
            UBuffer* pbuf;

            close( outPipe[1] );

            // file_open
            pbuf = boron_makePort( ut, &port_file, 0, res );
            pbuf->FD = outPipe[0];
        }
        else
        {
            ur_setId(res, UT_INT);
            ur_int(res) = pid;
        }
        return UR_OK;
    }
    else
    {
        // fork failed.

        if( port )
            _closePipe( outPipe );
        return ur_error( ut, UR_ERR_INTERNAL, "fork failed" );
    }
}


/*-cf-
    execute
        command     string!
        /in         Provide data via stdin.
            input   binary!/string!
        /out        Store output of command.
            output  binary!/string!
        /spawn      Run command asynchronously.
        /port       Return port to read spawn output.
    return: int! status of command or spawn port!
    group: os

    Runs an external program.

    If /spawn is used then /in and /out are ignored.
*/
CFUNC_PUB( cfunc_execute )
{
#define OPT_EXECUTE_IN      0x01
#define OPT_EXECUTE_OUT     0x02
#define OPT_EXECUTE_SPAWN   0x04
#define OPT_EXECUTE_PORT    0x08
    char* argv[10];
    const UBuffer* in = 0;
    UBuffer* out = 0;
    int type;
    uint32_t opt = CFUNC_OPTIONS;


    if( ! ur_is(a1, UT_STRING) )
    {
        ur_setId(res, UT_LOGIC);
        ur_int(res) = 0;
        return UR_OK;
    }

    // Create arg list before fork to avoid any possible copy-on-write overhead.
    argumentList( boron_cstr( ut, a1, 0 ), argv, 9 );

    if( opt & OPT_EXECUTE_SPAWN )
        return _execSpawn( ut, argv, opt & OPT_EXECUTE_PORT, res );

    if( opt & OPT_EXECUTE_IN )
    {
        type = ur_type(a1 + 1);
        if( type != UT_BINARY && type != UT_STRING )
            return ur_error( ut, UR_ERR_TYPE,
                             "execute expected binary!/string! input" );
        in = ur_bufferSer(a1 + 1);
    }

    if( opt & OPT_EXECUTE_OUT )
    {
        type = ur_type(a1 + 2);
        if( type != UT_BINARY && type != UT_STRING )
            return ur_error( ut, UR_ERR_TYPE,
                             "execute expected binary!/string! output" );
        if( ! (out = ur_bufferSerM(a1 + 2)) )
            return UR_THROW;
    }

    return _execIO( ut, argv, res, in, out );
}
#endif


/*
   nanosleep with interruption handling.
*/
static void _nsleep( const struct timespec* st )
{
    int rval;
    struct timespec tv = *st;
    do
    {
        rval = nanosleep( &tv, &tv );
    }
    while( (rval != 0) && (errno == EINTR) );
}


/*-cf-
    sleep
        time    int!/decimal!/time!
    return: unset!

    Delay for a number of seconds.
*/
CFUNC_PUB( cfunc_sleep )
{
    struct timespec stime;

    switch( ur_type(a1) )
    {
        case UT_INT:
            stime.tv_sec  = ur_int(a1);
            stime.tv_nsec = 0;
            break;

        case UT_DECIMAL:
        case UT_TIME:
        {
            double t = ur_decimal(a1);
            stime.tv_sec  = (time_t) t;
            stime.tv_nsec = (long) ((t - floor(t)) * 1000000000.0);
        }
            break;

        default:
            return ur_error( ut, UR_ERR_TYPE,
                             "sleep expected int!/decimal!/time!" );
    }

    _nsleep( &stime );
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*EOF*/
