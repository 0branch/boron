#ifndef OS_H
#define OS_H
/*
   os.h for Unix
*/


#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>


#define cprint      printf
#ifdef UR_CONFIG_EMH
#define dprint      ur_dprint
#else
#define dprint      printf
#endif
#define vaStrPrint  vsnprintf

#define strPrint    sprintf
#define strNCpy     strncpy
#define strLen      strlen

#define memCpy      memcpy
#define memSet      memset
#define memMove     memmove
#define memSwab     swab

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


typedef pthread_t           OSThread;
typedef pthread_mutex_t     OSMutex;

#define mutexInitF(mh)      (pthread_mutex_init(&mh,0) == -1)
#define mutexFree(mh)       pthread_mutex_destroy(&mh)
#define mutexLock(mh)       pthread_mutex_lock(&mh)
#define mutexUnlock(mh)     pthread_mutex_unlock(&mh)


#ifdef __cplusplus
extern "C" {
#endif

extern long long ur_fileSize( const char* path );
extern void ur_installExceptionHandlers();

#ifdef UR_CONFIG_EMH
extern void ur_dprint( const char*, ... );
#endif

#ifdef __cplusplus
}
#endif


#endif  /* OS_H */
