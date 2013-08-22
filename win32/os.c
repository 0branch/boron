/*
  Copyright 2005-2013 Karl Robillard

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


#include <direct.h>
#include <io.h>
#include <sys/timeb.h>
#include <time.h>
#include "os.h"
#include "os_file.h"
#include "boron.h"

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES     ((DWORD)-1)
#endif


#if 0
/*
   change-dir
   (dir -- logic)
*/
UR_CALL_PUB( uc_changeDir )
{
    int logic = 0;
    UString* str = ur_bin( tos );

    ur_termCStr(str);
    if( _chdir( str->ptr.c ) == 0 )
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

    if( _getcwd( tmp, sizeof(tmp) ) )
    {
        int len;
        UIndex binN;
        UString* str;
       
        len = strLen( tmp );
        assert( len > 0 );

        binN = ur_makeBinary( len + 1, &str );
        str->used = len;
        memCpy( str->ptr.c, tmp, len );

        if( tmp[ len - 1 ] != '\\' )
        {
            str->ptr.c[ len ] = '\\';
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


/*
  Returns -1 if not present.
*/
int64_t ur_fileSize( const char* path )
{
    LARGE_INTEGER lsize;
    HANDLE fh;
    BOOL ok;
#if _MSC_VER < 1300
    DWORD loword;
    DWORD hiword;
#endif

    // FIXME: CreateFile will fail if another process has locked the file.

    fh = CreateFile( path, GENERIC_READ, FILE_SHARE_READ, NULL,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if( fh == INVALID_HANDLE_VALUE )
        return -1;
#if _MSC_VER < 1300
    loword = GetFileSize( fh, &hiword );
    if( loword != INVALID_FILE_SIZE )
    {
        lsize.LowPart  = loword;
        lsize.HighPart = hiword;
        ok = 1;
    }
    else
        ok = 0;
#else 
    ok = GetFileSizeEx( fh, &lsize );
#endif
    CloseHandle( fh );
    if( ! ok )
        return -1;
    //return lsize.LowPart;
    return lsize.QuadPart;
}
#endif


/*
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&time, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

    // Build a string showing the date and time.
    wsprintf(lpszString, TEXT("%02d/%02d/%d  %02d:%02d"),
             stLocal.wMonth, stLocal.wDay, stLocal.wYear,
             stLocal.wHour, stLocal.wMinute);
*/

// FILETIME hold a 64-bit value representing the number of 100-nanosecond
// intervals since January 1, 1601 (UTC).

static double ft_to_seconds( const FILETIME* ft )
{
    double sec;
    // FIXME
    // Convert to unix time (since 1969)?
    sec  = ((double) ft->dwHighDateTime) * 16777215.0;
    sec += ((double) ft->dwLowDateTime) / 256.0;
    //return sec;
    return 0.0;
}


#if 0
/**
  Returns 0 if result set to file modification time or -1 if error.
*/
int ur_fileModified( const char* path, UCell* res )
{
    FILETIME time;
    HANDLE fh;
    BOOL ok;

    // FIXME: CreateFile will fail if another process has locked the file.

    fh = CreateFile( path, GENERIC_READ, FILE_SHARE_READ, NULL,
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if( fh == INVALID_HANDLE_VALUE )
        return -1;
    ok = GetFileTime( fh, NULL, NULL, &time );
    CloseHandle( fh );
    if( ! ok )
        return -1;

    ur_initType( res, UT_TIME );
    ur_seconds(res) = ft_to_seconds( &time );
    return 0;
}
#endif


/*
  Returns zero if error.
*/
int ur_fileInfo( const char* path, OSFileInfo* info, int mask )
{
    WIN32_FIND_DATA data;
    HANDLE fh;

    fh = FindFirstFile( path, &data );
    if( fh == INVALID_HANDLE_VALUE )
        return 0;
    FindClose( fh );

    if( mask & FI_Size )
    {
        info->size = data.nFileSizeLow;
        if( data.nFileSizeHigh )
            info->size += data.nFileSizeHigh * (((int64_t) MAXDWORD)+1);
    }

    if( mask & FI_Time )
    {
        info->accessed = ft_to_seconds( &data.ftLastAccessTime );
        info->modified = ft_to_seconds( &data.ftLastWriteTime );
    }

    if( mask & FI_Type )
    {
        if( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            info->type = FI_Dir;
        else
            info->type = FI_File;
    }

    return 1;
}


/*
  Returns 1 if directory, 0 if file or -1 if error.
*/
static int _isDir( const char* path )
{
    DWORD attr;
    attr = GetFileAttributes( path );
    if( attr == INVALID_FILE_ATTRIBUTES )
        return -1;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}


/*
  Return UR_OK/UR_THROW.
*/
int ur_makeDir( UThread* ut, const char* path )
{
    if( ! CreateDirectory( path, NULL ) )
    {
        DWORD err;
        err = GetLastError();
        if( (err != ERROR_FILE_EXISTS) || (_isDir(path) != 1) )
        {
            ur_error( ut, UR_ERR_ACCESS, "CreateDirectory error (%d)", err );
            return UR_THROW;
        }
    }
    return UR_OK;
}


double ur_now()
{
    struct _timeb tb;
    _ftime( &tb );
    return tb.time + (tb.millitm * 0.001);
}


int ur_readDir( UThread* ut, const char* filename, UCell* res )
{
    char filespec[ _MAX_PATH ];
    struct _finddata_t fileinfo;
    long handle;

    // Look for all files.  We ensure there is a slash before the wildcard.
    // It's OK if the path is already terminated with a slash - multiple
    // slashes are filtered by _findfirst.

    strcpy( filespec, filename );
    strcat( filespec, "\\*.*" );

    handle = _findfirst( filespec, &fileinfo );
    if( handle != -1 )
    {
        UCell* cell;
        UIndex blkN;
        UIndex hold;
        UIndex strN;
           
        blkN = ur_makeBlock( ut, 0 );
        hold = ur_hold( blkN );

        do
        {
            const char* cp = fileinfo.name;
            if( cp[0] == '.' && (cp[1] == '.' || cp[1] == '\0') )
                continue;

            strN = ur_makeStringUtf8( ut, cp, cp + strlen(cp) );
            cell = ur_blkAppendNew( ur_buffer(blkN), UT_FILE );
            ur_setSeries( cell, strN, 0 );
        }
        while( _findnext( handle, &fileinfo ) != -1 );

        _findclose( handle );

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
static void _readPipe( HANDLE fd, UBuffer* buf )
{
#define BUFSIZE     1024
    DWORD nr;

    buf->used = 0;
    if( buf->type == UT_STRING )
    {
        int ucs2 = ur_strIsUcs2(buf);
        ur_arrReserve( buf, BUFSIZE );
        while( 1 )
        {
            if( ReadFile(fd, buf->ptr.c + buf->used, BUFSIZE, &nr, NULL) == 0 )
                break;  // GetLastError();
            if( nr == 0 )
                break;
            buf->used += ucs2 ? nr >> 1 : nr;
            ur_arrReserve( buf, buf->used + BUFSIZE ); 
        }
    }
    else
    {
        ur_binReserve( buf, BUFSIZE );
        while( 1 )
        {
            if( ReadFile(fd, buf->ptr.c + buf->used, BUFSIZE, &nr, NULL) == 0 )
                break;  // GetLastError();
            if( nr == 0 )
                break;
            buf->used += nr;
            ur_binReserve( buf, buf->used + BUFSIZE ); 
        }
    }
}


static int _execIO( UThread* ut, char* cmd, UCell* res,
                    const UBuffer* in, UBuffer* out )
{
    HANDLE childStdInR;
    HANDLE childStdInW;
    HANDLE childStdOutR;
    HANDLE childStdOutW;
    SECURITY_ATTRIBUTES sec;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD flags = 0;


    sec.nLength              = sizeof(SECURITY_ATTRIBUTES); 
    sec.lpSecurityDescriptor = NULL;
    sec.bInheritHandle       = TRUE; 

    if( in )
    {
        if( ! CreatePipe( &childStdInR, &childStdInW, &sec, 0 ) ) 
            return ur_error( ut, UR_ERR_INTERNAL, "CreatePipe failed\n" );
        // Child does not inherit our end of pipe.
        SetHandleInformation( childStdInW, HANDLE_FLAG_INHERIT, 0 );
    }

    if( out )
    {
        if( ! CreatePipe( &childStdOutR, &childStdOutW, &sec, 0 ) ) 
        {
            if( in )
            {
                CloseHandle( childStdInW );
                CloseHandle( childStdInR );
            }
            return ur_error( ut, UR_ERR_INTERNAL, "CreatePipe failed\n" );
        }
        // Child does not inherit our end of pipe.
        SetHandleInformation( childStdOutR, HANDLE_FLAG_INHERIT, 0 );
        flags = DETACHED_PROCESS;
    }

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    if( in || out )
    {
        if( out )
        {
            si.hStdError = si.hStdOutput = childStdOutW;
        }
        else
        {
            si.hStdError  = GetStdHandle( STD_ERROR_HANDLE );
            si.hStdOutput = GetStdHandle( STD_OUTPUT_HANDLE );
        }
        si.hStdInput = in ? childStdInR : NULL;
        si.dwFlags   = STARTF_USESTDHANDLES;
    }

    ZeroMemory( &pi, sizeof(pi) );

    // Start the child process. 
    if( ! CreateProcess( NULL,   // No module name (use command line). 
        TEXT(cmd),        // Command line. 
        NULL,             // Process handle not inheritable. 
        NULL,             // Thread handle not inheritable. 
        TRUE,             // Handle inheritance. 
        flags,            // Creation flags.
        NULL,             // Use parent's environment block. 
        NULL,             // Use parent's starting directory. 
        &si,              // Pointer to STARTUPINFO structure.
        &pi )             // Pointer to PROCESS_INFORMATION structure.
    )
    {
        if( in )
        {
            CloseHandle( childStdInW );
            CloseHandle( childStdInR );
        }
        if( out )
        {
            CloseHandle( childStdOutW );
            CloseHandle( childStdOutR );
        }
        return ur_error( ut, UR_ERR_INTERNAL,
                         "CreateProcess failed (%d).\n", GetLastError() );
    }

    if( in )
    {
        DWORD nw;
        CloseHandle( childStdInR );
        WriteFile( childStdInW, in->ptr.v,
                   (in->type == UT_STRING) ? in->used * in->elemSize : in->used,
                   &nw, NULL );
        CloseHandle( childStdInW );
    }

    if( out )
    {
        CloseHandle( childStdOutW );
        _readPipe( childStdOutR, out );
        CloseHandle( childStdOutR );
    }

    {
    DWORD code;

    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

    if( GetExitCodeProcess( pi.hProcess, &code ) )
    {
        ur_setId(res, UT_INT);
        ur_int(res) = code;
    }
    else
    {
        ur_setId(res, UT_NONE);
    }
    }

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    return UR_OK;
}


static int _execSpawn( UThread* ut, char* cmd, UCell* res )
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);

    ZeroMemory( &pi, sizeof(pi) );

    // Start the child process. 
    if( ! CreateProcess( NULL,   // No module name (use command line). 
        TEXT(cmd),        // Command line. 
        NULL,             // Process handle not inheritable. 
        NULL,             // Thread handle not inheritable. 
        FALSE,            // Set handle inheritance to FALSE. 
        0,                // No creation flags. 
        NULL,             // Use parent's environment block. 
        NULL,             // Use parent's starting directory. 
        &si,              // Pointer to STARTUPINFO structure.
        &pi )             // Pointer to PROCESS_INFORMATION structure.
    )
    {
        return ur_error( ut, UR_ERR_INTERNAL,
                         "CreateProcess failed (%d).\n", GetLastError() );
    }

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );

    ur_setId(res, UT_INT);
    ur_int(res) = pi.dwProcessId;
    return UR_OK;
}


/*
  execute command /in input /out output /spawn
*/
CFUNC_PUB( cfunc_execute )
{
#define OPT_EXECUTE_IN      0x01
#define OPT_EXECUTE_OUT     0x02
#define OPT_EXECUTE_SPAWN   0x04
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

    if( opt & OPT_EXECUTE_SPAWN )
        return _execSpawn( ut, boron_cstr( ut, a1, 0 ), res );

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

    return _execIO( ut, boron_cstr( ut, a1, 0 ), res, in, out );
}
#endif


CFUNC_PUB( cfunc_sleep )
{
    DWORD msec;

    switch( ur_type(a1) )
    {
        case UT_INT:
            msec = ur_int(a1) * 1000;
            break;

        case UT_DECIMAL:
        case UT_TIME:
            msec = (DWORD) (ur_decimal(a1) * 1000.0);
            break;

        default:
            return ur_error( ut, UR_ERR_TYPE,
                             "sleep expected int!/decimal!/time!" );
    }

    Sleep( msec );
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


#ifdef CONFIG_EXECUTE
/*
  with-flock file body /nowait
*/
CFUNC_PUB( cfunc_with_flock )
{
#define OPT_FLOCK_NOWAIT   0x01
#define LOCK_ALL    ULONG_MAX
    OVERLAPPED over;
    HANDLE fh;
    int ok = UR_OK;
    DWORD oper = LOCKFILE_EXCLUSIVE_LOCK;
    const char* file = boron_cstr( ut, a1, 0 );

    if( ! boron_requestAccess( ut, "Lock file \"%s\"", file ) )
        return UR_THROW;

    fh = CreateFile( file, GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                     OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if( fh == INVALID_HANDLE_VALUE )
    {
        return ur_error( ut, UR_ERR_ACCESS, "could not open file %s (%d)",
                         file, GetLastError() );
    }

    if( CFUNC_OPTIONS & OPT_FLOCK_NOWAIT )
        oper |= LOCKFILE_FAIL_IMMEDIATELY;

    memset( &over, 0, sizeof(over) );

    if( LockFileEx( fh, oper, 0, LOCK_ALL, LOCK_ALL, &over ) )
    {
        ok = boron_doBlock( ut, a1 + 1, res );
        UnlockFileEx( fh, 0, LOCK_ALL, LOCK_ALL, &over );
    }
    else
    {
        ur_setId(res, UT_LOGIC);
        ur_int(res) = 0;
    }
    CloseHandle( fh );
    return ok;
}
#endif


/*EOF*/
