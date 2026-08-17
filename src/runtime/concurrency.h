#ifndef ARCANA_CONCURRENCY_H
#define ARCANA_CONCURRENCY_H

#include "../vm/vm.h"
#include "../platform/thread.h"

/* --- Coroutine status --- */
typedef enum {
    CORO_READY,
    CORO_SUSPENDED,
    CORO_RUNNING,
    CORO_DEAD,
} ArcCoroStatus;

/* --- Coroutine: lightweight cooperative execution context --- */
typedef struct {
    ArcObject       obj;
    ArcVm*          vm;       /* own VM instance, shares gc with parent */
    ArcCoroStatus   status;
} ArcObjCoroutine;

/* --- Thread: OS thread with its own VM --- */
typedef struct {
    ArcObject       obj;
    ArcThreadHandle handle;
    ArcVm*          vm;       /* own VM instance, shares gc with parent */
    bool            joined;
} ArcObjThread;

/* --- Mutex: language-level mutual exclusion --- */
typedef struct {
    ArcObject        obj;
    ArcPlatformMutex mtx;
    bool             locked;
} ArcObjMutex;

/* --- Channel: buffered message-passing --- */
typedef struct {
    ArcObject          obj;
    ArcValue*          buffer;
    uint32_t           cap;
    uint32_t           head;
    uint32_t           tail;
    uint32_t           count;
    ArcPlatformMutex   mtx;
    ArcPlatformCondVar not_empty;
    ArcPlatformCondVar not_full;
    bool               closed;
} ArcObjChannel;

/* --- Cast macros (from ArcValue) --- */
#define ARC_AS_COROUTINE(val) ((ArcObjCoroutine*)((val).as.obj))
#define ARC_AS_THREAD(val)    ((ArcObjThread*)((val).as.obj))
#define ARC_AS_MUTEX(val)     ((ArcObjMutex*)((val).as.obj))
#define ARC_AS_CHANNEL(val)   ((ArcObjChannel*)((val).as.obj))

/* --- Constructors --- */
ArcObjCoroutine* arc_obj_coroutine_new(ArcGC* gc, const ArcBytecodeImage* image,
                                       uint16_t func_idx);
ArcObjThread*    arc_obj_thread_new(ArcGC* gc);
ArcObjMutex*     arc_obj_mutex_new(ArcGC* gc);
ArcObjChannel*   arc_obj_channel_new(ArcGC* gc, uint32_t capacity);

/* --- Channel operations --- */
bool arc_channel_send(ArcObjChannel* ch, ArcValue val);
bool arc_channel_recv(ArcObjChannel* ch, ArcValue* out);

/* --- GC helpers (called from gc.c) --- */
void arc_concurrency_mark_obj(ArcObject* obj);
void arc_concurrency_free_obj(ArcObject* obj);

#endif /* ARCANA_CONCURRENCY_H */
