#ifndef ARCANA_THREAD_H
#define ARCANA_THREAD_H

#include "../common/arcana_common.h"

/*
 * Cross-platform threading primitives.
 *
 * Wraps pthreads (Unix) and Win32 threads (Windows) behind a
 * uniform C API for the Arcana VM's concurrency layer.
 */

#ifdef _WIN32
#include <windows.h>
typedef HANDLE              ArcThreadHandle;
typedef CRITICAL_SECTION    ArcPlatformMutex;
typedef CONDITION_VARIABLE  ArcPlatformCondVar;
#else
#include <pthread.h>
typedef pthread_t           ArcThreadHandle;
typedef pthread_mutex_t     ArcPlatformMutex;
typedef pthread_cond_t      ArcPlatformCondVar;
#endif

/* Thread entry point: void* fn(void* arg) */
typedef void* (*ArcThreadFunc)(void* arg);

/* --- Thread --- */
bool   arc_thread_create(ArcThreadHandle* out, ArcThreadFunc fn, void* arg);
void*  arc_thread_join(ArcThreadHandle handle);

/* --- Mutex --- */
void arc_mutex_init(ArcPlatformMutex* m);
void arc_mutex_destroy(ArcPlatformMutex* m);
void arc_mutex_lock(ArcPlatformMutex* m);
void arc_mutex_unlock(ArcPlatformMutex* m);
bool arc_mutex_trylock(ArcPlatformMutex* m);

/* --- Condition variable --- */
void arc_condvar_init(ArcPlatformCondVar* cv);
void arc_condvar_destroy(ArcPlatformCondVar* cv);
void arc_condvar_wait(ArcPlatformCondVar* cv, ArcPlatformMutex* m);
void arc_condvar_signal(ArcPlatformCondVar* cv);
void arc_condvar_broadcast(ArcPlatformCondVar* cv);

#endif /* ARCANA_THREAD_H */
