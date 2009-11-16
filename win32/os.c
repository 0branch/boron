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


#include <direct.h>
#include <io.h>
#include <sys/timeb.h>
#include <time.h>
#include "os.h"
#include "urlan.h"

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
#endif


/**
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


/**
  Returns 1 if directory, 0 if file or -1 if error.
*/
int ur_isDir( const char* path )
{
    DWORD attr;
    attr = GetFileAttributes( path );
    if( attr == INVALID_FILE_ATTRIBUTES )
        return -1;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}


#if 0
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


/*
  (filename -- status)

  Result is block of [type-word byte-size modified-time]
*/
UR_CALL_PUB( uc_file_info )
{
    UCell* cell;
    UBlock* blk;
    UString* str;
    UIndex blkN;
    UAtom atom;

    if( ur_is(tos, UT_STRING) )
    {
        WIN32_FIND_DATA info;
        HANDLE fh;

        str = ur_bin( tos );
        ur_termCStr( str );

        fh = FindFirstFile( str->ptr.c + tos->series.it, &info );
        if( fh == INVALID_HANDLE_VALUE )
        {
            ur_throwErr( UR_ERR_ACCESS, "FindFirstFile" );
            return;
        }
        FindClose( fh );

        blkN = ur_makeBlock( 3 );
        blk = ur_blockPtr( blkN );

        cell = ur_appendCell(blk, UT_WORD);
        if( info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            atom = ur_intern( "dir", 3 );
        else
            atom = ur_intern( "file", 4 );
        ur_setUnbound(cell, atom);

        cell = ur_appendCell(blk, UT_INT);
        ur_int(cell) = info.nFileSizeLow;
        // info.nFileSizeHigh;

        cell = ur_appendCell(blk, UT_TIME);
        ur_seconds(cell) = ft_to_seconds( &info.ftLastWriteTime );

        ur_initType(tos, UT_BLOCK);
        ur_setSeries(tos, blkN, 0 );
    }
}
#endif


/*
  Return UR_OK/UR_THROW.
*/
int ur_makeDir( UThread* ut, const char* path )
{
    if( ! CreateDirectory( path, NULL ) )
    {
        DWORD err;
        err = GetLastError();
        if( (err != ERROR_FILE_EXISTS) || (ur_isDir(path) != 1) )
        {
            ur_throwErr( UR_ERR_ACCESS, "CreateDirectory error (%d)", err );
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


#if 0
void ur_readDir( UThread* ut, const char* filename, UCell* res )
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
           
        blkN = ur_makeBlock( 0 );
        hold = ur_holdBlock( blkN );

        do
        {
            const char* cp = fileinfo.name;
            if( cp[0] == '.' && (cp[1] == '.' || cp[1] == '\0') )
                continue;

            // Mark cell as unset in case GC is called by ur_makeString.
            cell = ur_appendCell( ur_blockPtr( blkN ), UT_UNSET );
            ur_makeString( cell, fileinfo.name, -1 );
        }
        while( _findnext( handle, &fileinfo ) != -1 );

        _findclose( handle );

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
#if 0
static void _callOutput( UThread* ut, UCell* tos )
{
#define BUFSIZE     256
    UString* str;
    char* cp;
    HANDLE childStdOutR;
    HANDLE childStdOutW;
    DWORD nr;
    SECURITY_ATTRIBUTES sec;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;


    str = ur_bin( tos );
    ur_termCStr( str );
    cp = str->ptr.c + tos->series.it;


    sec.nLength              = sizeof(SECURITY_ATTRIBUTES); 
    sec.lpSecurityDescriptor = NULL;
    sec.bInheritHandle       = TRUE; 

    if( ! CreatePipe( &childStdOutR, &childStdOutW, &sec, 0 ) ) 
    {
        ur_throwErr( UR_ERR_INTERNAL, "CreatePipe failed\n" );
        return;
    }

    SetHandleInformation( childStdOutR, HANDLE_FLAG_INHERIT, 0 );

    ZeroMemory( &si, sizeof(si) );
    si.cb         = sizeof(si);
    si.hStdError  = childStdOutW;
    si.hStdOutput = childStdOutW;
    si.hStdInput  = NULL;
    si.dwFlags    = STARTF_USESTDHANDLES;

    ZeroMemory( &pi, sizeof(pi) );

    // Start the child process. 
    if( ! CreateProcess( NULL,   // No module name (use command line). 
        TEXT(cp),         // Command line. 
        NULL,             // Process handle not inheritable. 
        NULL,             // Thread handle not inheritable. 
        TRUE,             // Handle inheritance. 
        DETACHED_PROCESS, // Prevent pop-up of MSDOS windows.
        NULL,             // Use parent's environment block. 
        NULL,             // Use parent's starting directory. 
        &si,              // Pointer to STARTUPINFO structure.
        &pi )             // Pointer to PROCESS_INFORMATION structure.
    )
    {
        CloseHandle( childStdOutW );
        CloseHandle( childStdOutR );
        ur_throwErr( UR_ERR_INTERNAL,
                     "CreateProcess failed (%d).\n", GetLastError() );
        return;
    }

    CloseHandle( childStdOutW );

    str = ur_binPtr(REF_CALL_OUT);
    str->used = 0;
    ur_arrayReserve( str, 1, BUFSIZE ); 

    while( 1 )
    {
        if( ReadFile( childStdOutR, str->ptr.c + str->used, BUFSIZE,
                      &nr, NULL ) == 0 )
        {
            // GetLastError();
            break;
        }
        if( nr == 0 )
            break;

        str->used += nr;
        orArrayReserve( str, 1, str->used + BUFSIZE ); 
    }

    // MSDN example does not CloseHandle(childStdOutR);

    {
    DWORD code;

    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

    if( GetExitCodeProcess( pi.hProcess, &code ) )
    {
        ur_initType( tos, UT_INT );
        ur_int(tos) = code;
    }
    else
    {
        ur_setNone(tos);
    }
    }

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
}
#endif


static void _callSimple( UThread* ut, UCell* tos )
{
    UString* str;
    char* cp;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    str = ur_bin( tos );
    ur_termCStr( str );
    cp  = str->ptr.c + tos->series.it;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);

    ZeroMemory( &pi, sizeof(pi) );

    // Start the child process. 
    if( ! CreateProcess( NULL,   // No module name (use command line). 
        TEXT(cp),         // Command line. 
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
        ur_throwErr( UR_ERR_INTERNAL,
                     "CreateProcess failed (%d).\n", GetLastError() );
        return;
    }

    ur_setNone(tos);

    //if( orRefineSet(REF_CALL_WAIT) )
    {
        DWORD code;

        // Wait until child process exits.
        WaitForSingleObject( pi.hProcess, INFINITE );

        if( GetExitCodeProcess( pi.hProcess, &code ) )
        {
            ur_initType( tos, UT_INT );
            ur_int(tos) = code;
        }
    }

    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
}


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
#endif


void ur_installExceptionHandlers() {}
#endif


/*EOF*/
