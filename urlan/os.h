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
#ifndef _WIN32_WINNT
#define _WIN32_WINNT    0x0403
#endif
#include <windows.h>
#include <direct.h>     // _chdir, _getcwd

#undef small            // Defined in RpcNdr.h (SDK v6.0A)

#ifdef _MSC_VER
#define inline  __inline
#endif

#define vaStrPrint  _vsnprintf

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


#define strPrint    sprintf
#define strNCpy     strncpy
#define strLen      strlen

#define memCpy      memcpy
#define memSet      memset
#define memMove     memmove


#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef UR_CONFIG_EMH
extern void ur_dprint( const char*, ... );
#define dprint      ur_dprint
#else
#define dprint      printf
#endif

#ifdef __cplusplus
}
#endif


#endif  /* OS_H */
