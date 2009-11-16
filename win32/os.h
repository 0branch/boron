#ifndef OS_H
#define OS_H
/*
   os.h for Win32
*/


#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Must define _WIN32_WINNT to use InitializeCriticalSectionAndSpinCount
#define _WIN32_WINNT    0x0403
// ws2 header included here for uc_wait because it must come before windows.h
#include <winsock2.h>
#include <windows.h>


#ifdef _MSC_VER
#define inline  __inline
#endif


#define cprint      printf
#ifdef UR_CONFIG_EMH
#define dprint      ur_dprint
#else
#define dprint      printf
#endif
#define vaStrPrint  _vsnprintf

#define strPrint    sprintf
#define strNCpy     strncpy
#define strLen      strlen

#define memCpy      memcpy
#define memSet      memset
#define memMove     memmove
#define memSwab     _swab

#ifdef TRACK_MALLOC
void* memAlloc( size_t );
void* memRealloc( void*, size_t );
void  memFree( void* );
void  memReport( int verbose );
#else
#define memAlloc    malloc
#define memRealloc  realloc
#define memFree     free
#endif


typedef FILE*   FileHandle;
typedef char*   FileMode;

#define FILE_READ               "r"
#define FILE_READ_BINARY        "rb"
#define FILE_WRITE              "w"
#define FILE_WRITE_BINARY       "wb"
#define FILE_APPEND             "a"
#define FILE_APPEND_BINARY      "ab"

#define fileOpen(fn,mode)       fopen(fn,mode)
#define fileClose(fh)           fclose(fh)
#define fileRead(fh,buf,len)    fread(buf,1,len,fh)
#define fileWrite(fh,buf,len)   fwrite(buf,1,len,fh)
#define fileSeek(fh,off)        fseek(fh,off,SEEK_SET)
#define fileError(fh)           ferror(fh)


typedef HANDLE              OSThread;
typedef CRITICAL_SECTION    OSMutex;

#define mutexInitF(mh) \
    (InitializeCriticalSectionAndSpinCount(&mh,0x80000400) == 0)
#define mutexFree(mh)       DeleteCriticalSection(&mh)
#define mutexLock(mh)       EnterCriticalSection(&mh)
#define mutexUnlock(mh)     LeaveCriticalSection(&mh)


#ifdef __cplusplus
extern "C" {
#endif

extern __int64 ur_fileSize( const char* path );
extern void ur_installExceptionHandlers();

#ifdef UR_CONFIG_EMH
extern void ur_dprint( const char*, ... );
#endif

#ifdef __cplusplus
}
#endif


#endif  /* OS_H */
