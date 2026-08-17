#include "thread.h"

#ifdef _WIN32
/* ================================================================
 * Windows implementation (Win32 API)
 * ================================================================ */

/* Trampoline: bridge void*(*)(void*) to DWORD WINAPI (*)(LPVOID) */
typedef struct {
    ArcThreadFunc fn;
    void*         arg;
    void*         result;
} ArcWinTrampoline;

static DWORD WINAPI arc_win_thread_entry(LPVOID data) {
    ArcWinTrampoline* t = (ArcWinTrampoline*)data;
    t->result = t->fn(t->arg);
    return 0;
}

bool arc_thread_create(ArcThreadHandle* out, ArcThreadFunc fn, void* arg) {
    ArcWinTrampoline* t = (ArcWinTrampoline*)malloc(sizeof(ArcWinTrampoline));
    if (!t) return false;
    t->fn = fn;
    t->arg = arg;
    t->result = NULL;
    *out = CreateThread(NULL, 0, arc_win_thread_entry, t, 0, NULL);
    if (!*out) { free(t); return false; }
    return true;
}

void* arc_thread_join(ArcThreadHandle handle) {
    WaitForSingleObject(handle, INFINITE);
    CloseHandle(handle);
    return NULL;
}

void arc_mutex_init(ArcPlatformMutex* m)    { InitializeCriticalSection(m); }
void arc_mutex_destroy(ArcPlatformMutex* m) { DeleteCriticalSection(m); }
void arc_mutex_lock(ArcPlatformMutex* m)    { EnterCriticalSection(m); }
void arc_mutex_unlock(ArcPlatformMutex* m)  { LeaveCriticalSection(m); }

bool arc_mutex_trylock(ArcPlatformMutex* m) {
    return TryEnterCriticalSection(m) != 0;
}

void arc_condvar_init(ArcPlatformCondVar* cv)    { InitializeConditionVariable(cv); }
void arc_condvar_destroy(ArcPlatformCondVar* cv) { (void)cv; /* no-op on Windows */ }

void arc_condvar_wait(ArcPlatformCondVar* cv, ArcPlatformMutex* m) {
    SleepConditionVariableCS(cv, m, INFINITE);
}

void arc_condvar_signal(ArcPlatformCondVar* cv)    { WakeConditionVariable(cv); }
void arc_condvar_broadcast(ArcPlatformCondVar* cv) { WakeAllConditionVariable(cv); }

#else
/* ================================================================
 * POSIX implementation (pthreads)
 * ================================================================ */

bool arc_thread_create(ArcThreadHandle* out, ArcThreadFunc fn, void* arg) {
    return pthread_create(out, NULL, fn, arg) == 0;
}

void* arc_thread_join(ArcThreadHandle handle) {
    void* ret = NULL;
    pthread_join(handle, &ret);
    return ret;
}

void arc_mutex_init(ArcPlatformMutex* m)    { pthread_mutex_init(m, NULL); }
void arc_mutex_destroy(ArcPlatformMutex* m) { pthread_mutex_destroy(m); }
void arc_mutex_lock(ArcPlatformMutex* m)    { pthread_mutex_lock(m); }
void arc_mutex_unlock(ArcPlatformMutex* m)  { pthread_mutex_unlock(m); }

bool arc_mutex_trylock(ArcPlatformMutex* m) {
    return pthread_mutex_trylock(m) == 0;
}

void arc_condvar_init(ArcPlatformCondVar* cv)    { pthread_cond_init(cv, NULL); }
void arc_condvar_destroy(ArcPlatformCondVar* cv) { pthread_cond_destroy(cv); }

void arc_condvar_wait(ArcPlatformCondVar* cv, ArcPlatformMutex* m) {
    pthread_cond_wait(cv, m);
}

void arc_condvar_signal(ArcPlatformCondVar* cv)    { pthread_cond_signal(cv); }
void arc_condvar_broadcast(ArcPlatformCondVar* cv) { pthread_cond_broadcast(cv); }

#endif
