/*
  Copyright 2005-2009 Karl Robillard

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
#include "urlan.h"


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


/**
  Returns -1 if not present.
*/
long long ur_fileSize( const char* path )
{
    struct stat buf;
    if( stat( path, &buf ) == -1 )
        return -1;
    return buf.st_size;
}


/**
  Returns 1 if directory, 0 if file or -1 if error.
*/
int ur_isDir( const char* path )
{
    struct stat buf;
    if( stat( path, &buf ) == -1 )
        return -1;
    return S_ISDIR(buf.st_mode) ? 1 : 0;
}


#if 0
/**
  Returns 0 if result set to file modification time or -1 if error.
*/
int ur_fileModified( const char* path, UCell* res )
{
    struct stat buf;
    if( stat( path, &buf ) == -1 )
        return -1;

    ur_initType( res, UT_TIME );
    ur_seconds(res) = (double) buf.st_mtime;
    return 0;
}


/*
  (filename -- status)

  Result is block of [type-word byte-size modified-time]
*/
UR_CALL_PUB( uc_file_info )
{
    struct stat buf;
    UCell* cell;
    UBlock* blk;
    UString* str;
    UIndex blkN;
    UAtom atom;

    if( ur_is(tos, UT_STRING) )
    {
        str = ur_bin( tos );
        ur_termCStr( str );

        if( stat( str->ptr.c + tos->series.it, &buf ) == -1 )
        {
            ur_throwErr( UR_ERR_ACCESS, strerror(errno) );
            return;
        }

        blkN = ur_makeBlock( 3 );
        blk = ur_blockPtr( blkN );

        // Should put filename as first cell?

        cell = ur_appendCell(blk, UT_WORD);
        if( S_ISREG(buf.st_mode) )
            atom = ur_intern( "file", 4 );
        else if( S_ISDIR(buf.st_mode) )
            atom = ur_intern( "dir", 3 );
        else if( S_ISLNK(buf.st_mode) )
            atom = ur_intern( "link", 4 );
#ifdef S_ISSOCK
        else if( S_ISSOCK(buf.st_mode) )
            atom = ur_intern( "socket", 6 );
#endif
        else
            atom = ur_intern( "other", 5 );
        ur_setUnbound(cell, atom);

        cell = ur_appendCell(blk, UT_INT);
        ur_int(cell) = buf.st_size;

        cell = ur_appendCell(blk, UT_TIME);
        ur_seconds(cell) = (double) buf.st_mtime;


        ur_initType(tos, UT_BLOCK);
        ur_setSeries(tos, blkN, 0 );
    }
    /*
    else if( ur_is(tos, UT_PORT) )
    {
        if( fstat( path, &buf ) == -1 )
    }
    */
}
#endif


/*
  Return UR_OK/UR_THROW.
*/
int ur_makeDir( UThread* ut, const char* path )
{
    int err = mkdir( path, 0755 );
    if( err )
    {
        if( (errno != EEXIST) || (ur_isDir(path) != 1) )
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


#if 0
/**
  Sets result.
*/
void ur_readDir( UThread* ut, const char* filename, UCell* res )
{
    DIR* dir;
    struct dirent* entry;

    dir = opendir( filename );
    if( dir )
    {
        UCell* cell;
        UIndex blkN;
        UIndex hold;

        blkN = ur_makeBlock( 0 );
        hold = ur_holdBlock( blkN );

        while( (entry = readdir( dir )) )
        {
            const char* cp = entry->d_name;
            if( cp[0] == '.' && (cp[1] == '.' || cp[1] == '\0') )
                continue;

            // Mark cell as unset in case GC is called by ur_makeString.
            cell = ur_appendCell( ur_blockPtr( blkN ), UT_UNSET );
            ur_makeString( cell, cp, -1 );
        }

        closedir( dir );

        ur_release( hold );

        ur_initType(res, UT_BLOCK);
        ur_setSeries(res, blkN, 0);
    }
    else
    {
        ur_setFalse(res);
    }
}


#ifdef UR_CONFIG_OS_RUN
static void argumentList( UString* str, int si, char** argv, int maxArg )
{
    char* cp;
    int prevWhite;
    int argc;

    ur_termCStr( str );
    cp = str->ptr.c + si;

    argc = 0;
    prevWhite = 1;
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


#if 0
static void _callOutput( UThread* ut, UCell* tos )
{
#define BUFSIZE     512
    int pid;
    int pfd[2];
    char* argv[10];

    if( pipe(pfd) == -1 )
    {
        ur_throwErr( UR_ERR_INTERNAL, "pipe failed" );
        return;
    }

    // Create arg list before fork to avoid any possible copy-on-write overhead.
    argumentList( ur_bin(tos), tos->series.it, argv, 9 );

    pid = fork();
    if( pid == 0 )
    {
        // In child process; invoke command.

        close( pfd[0] );
        dup2( pfd[1], 1 );
        close( pfd[1] );

        if( execvp( argv[0], argv ) == -1 )
        {
            perror( "uc_system_run execvp" );
            _exit( 255 );
        }
    }
    else if( pid > 0 )
    {
        // In parent process.

        int n;
        int status;
        OString* str;

        str = ur_binPtr(REF_CALL_OUT);
        str->used = 0;
        ur_arrayReserve( str, 1, BUFSIZE ); 

        close( pfd[1] );
        while( (n = read( pfd[0], str->ptr.c + str->used, BUFSIZE )) > 0 )
        {
            str->used += n;
            ur_arrayReserve( str, 1, str->used + BUFSIZE ); 
        }
        close( pfd[0] );

        waitpid( pid, &status, 0 );
        if( WIFEXITED(status) )
        {
            ur_initType( tos, UT_INT );
            ur_int(tos) = WEXITSTATUS(status);
            return;
        }
        ur_setNone(tos);
    }
    else
    {
        // fork failed.
        close( pfd[0] );
        close( pfd[1] );
        ur_throwErr( UR_ERR_INTERNAL, "fork failed" );
    }
}
#endif


static int _asyncProcCount = 0;

static void _callSimple( UThread* ut, UCell* tos )
{
    int pid;
    int status;
    int waitRef;
    char* argv[10];

#if 1
    waitRef = 1;
#else
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

    // Create arg list before fork to avoid any possible copy-on-write overhead.
    argumentList( ur_bin(tos), tos->series.it, argv, 9 );

    pid = fork();
    if( pid == 0 )
    {
        // In child process; invoke command.

        if( execvp( argv[0], argv ) == -1 )
        {
            perror( "uc_system_run execvp" );
            _exit( 255 );
        }
    }
    else if( pid > 0 )
    {
        // In parent process.

        if( waitRef )
        {
            waitpid( pid, &status, 0 );
            if( WIFEXITED(status) )
            {
                ur_initType( tos, UT_INT );
                ur_int(tos) = WEXITSTATUS( status );
                return;
            }
        }
        ur_setNone(tos);
    }
    else
    {
        // fork failed.
        ur_throwErr( UR_ERR_INTERNAL, "fork failed" );
        if( ! waitRef )
            --_asyncProcCount;
    }
}


// (command -- status)
UR_CALL_PUB( uc_system_run )
{
    if( ! ur_is(tos, UT_STRING) )
    {
        ur_setFalse( tos );
        return;
    }

    /*
    if( orRefineSet( REF_CALL_OUTPUT ) )
    {
        _callOutput( ut, tos );
    }
    else
    */
    {
        _callSimple( ut, tos );
    }
}


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
#endif


#if 0
void kill_handler( int sig )
{
    (void) sig;
    printf( "User Abort\n" );
}

void fpe_handler( int sig )
{
    (void) sig;
    printf( "Math Error\n" );
    exit(-1);
}
#endif


void ur_installExceptionHandlers()
{
#ifdef UR_CONFIG_OS_RUN
    /* NOTE: Cannot use this if using Qt's QProcess (it uses SIGCHLD) */
    struct sigaction childSA;

    childSA.sa_handler = child_handler;
    childSA.sa_flags   = SA_NOCLDSTOP;
    sigemptyset( &childSA.sa_mask );

    sigaction( SIGCHLD, &childSA, NULL );
#endif

#if 0
    struct sigaction action;
    struct sigaction faction;

    action.sa_handler = kill_handler;
    action.sa_flags   = 0;
    sigemptyset( &action.sa_mask );

    sigaction( SIGTERM, &action, NULL );    // kill
    sigaction( SIGINT,  &action, NULL );    // Ctrl-C

    faction.sa_handler = fpe_handler;
    faction.sa_flags   = 0;
    sigemptyset( &faction.sa_mask );

    sigaction( SIGFPE,  &faction, NULL );
#endif

#if 0
    //double f = 0.0;
    //printf( "%g\n", 2.0 / f );
    int n = 0;
    printf( "%d\n", 2 / n );
#endif
}
#endif


/*EOF*/
