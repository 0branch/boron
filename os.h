#ifndef OS_H
#define OS_H
/*
   Boron operating system interface.
*/


#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __sun__
#include <inttypes.h>
#else
#include <stdint.h>
#endif


#ifdef _WIN32

// Must define _WIN32_WINNT to use InitializeCriticalSectionAndSpinCount
#define _WIN32_WINNT    0x0403
// ws2 header included here for uc_wait because it must come before windows.h
#include <winsock2.h>
#include <windows.h>

#ifdef _MSC_VER
#define inline  __inline
#endif

#define vaStrPrint  _vsnprintf
#define memSwab     _swab

typedef HANDLE              OSThread;
typedef CRITICAL_SECTION    OSMutex;
typedef CONDITION_VARIABLE  OSCond;

#define mutexInitF(mh) \
    (InitializeCriticalSectionAndSpinCount(&mh,0x80000400) == 0)
#define mutexFree(mh)       DeleteCriticalSection(&mh)
#define mutexLock(mh)       EnterCriticalSection(&mh)
#define mutexUnlock(mh)     LeaveCriticalSection(&mh)
#define condInit(cond)      InitializeConditionVariable(&cond)
#define condFree(cond)
#define condWaitF(cond,mh)  (! SleepConditionVariableCS(&cond,&mh,INFINITE))
#define condSignal(cond)    WakeConditionVariable(&cond)

#else

#include <pthread.h>
#include <unistd.h>

#define vaStrPrint  vsnprintf
#define memSwab     swab

typedef pthread_t           OSThread;
typedef pthread_mutex_t     OSMutex;
typedef pthread_cond_t      OSCond;

#define mutexInitF(mh)      (pthread_mutex_init(&mh,0) == -1)
#define mutexFree(mh)       pthread_mutex_destroy(&mh)
#define mutexLock(mh)       pthread_mutex_lock(&mh)
#define mutexUnlock(mh)     pthread_mutex_unlock(&mh)
#define condInit(cond)      pthread_cond_init(&cond,0)
#define condFree(cond)      pthread_cond_destroy(&cond)
#define condWaitF(cond,mh)  pthread_cond_wait(&cond,&mh)
#define condSignal(cond)    pthread_cond_signal(&cond)

#endif


#define cprint      printf
#ifdef UR_CONFIG_EMH
#define dprint      ur_dprint
#else
#define dprint      printf
#endif

#define strPrint    sprintf
#define strNCpy     strncpy
#define strLen      strlen

#define memCpy      memcpy
#define memSet      memset
#define memMove     memmove


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


enum OSFileInfoMask
{
    FI_Size = 0x01,
    FI_Time = 0x02,
    FI_Type = 0x04
};


enum OSFileType
{
    FI_File,
    FI_Link,
    FI_Dir,
    FI_Socket,
    FI_Other
};


typedef struct
{
    int64_t size;
    double  accessed;
    double  modified;
    uint8_t type;
}
OSFileInfo;


#ifdef __cplusplus
extern "C" {
#endif

extern int  ur_fileInfo( const char* path, OSFileInfo* info, int mask );
extern void ur_installExceptionHandlers();
extern double ur_now();

#ifdef UR_CONFIG_EMH
extern void ur_dprint( const char*, ... );
#endif

#ifdef __cplusplus
}
#endif


#endif  /* OS_H */
