/*
 * concurrency.c — Concurrency object constructors, GC helpers, channel ops.
 */

#include "concurrency.h"

/* --- Coroutine --- */

ArcObjCoroutine* arc_obj_coroutine_new(ArcGC* gc, const ArcBytecodeImage* image,
                                       uint16_t func_idx) {
    ArcObjCoroutine* coro = (ArcObjCoroutine*)arc_gc_alloc(
        gc, sizeof(ArcObjCoroutine), OBJ_COROUTINE);
    if (!coro) return NULL;
    coro->vm = (ArcVm*)calloc(1, sizeof(ArcVm));
    if (!coro->vm) return coro;
    coro->vm->image = image;
    coro->vm->gc = gc;
    coro->vm->owns_gc = false;
    coro->vm->output = stdout;
    /* Set up initial frame for the coroutine function */
    if (func_idx < image->functions.count) {
        const ArcFuncRecord* fn = &image->functions.funcs[func_idx];
        coro->vm->frames[0] = (ArcFrame){
            .func_idx = func_idx, .return_ip = 0, .base_slot = 0
        };
        coro->vm->fp = 1;
        coro->vm->ip = fn->code_offset;
        coro->vm->sp = fn->local_count;
        for (uint16_t i = 0; i < fn->local_count; i++)
            coro->vm->stack[i] = arc_val_null();
    }
    coro->status = CORO_READY;
    return coro;
}

/* --- Thread --- */

ArcObjThread* arc_obj_thread_new(ArcGC* gc) {
    ArcObjThread* t = (ArcObjThread*)arc_gc_alloc(
        gc, sizeof(ArcObjThread), OBJ_THREAD);
    if (!t) return NULL;
    t->vm = NULL;
    t->joined = false;
    return t;
}

/* --- Mutex --- */

ArcObjMutex* arc_obj_mutex_new(ArcGC* gc) {
    ArcObjMutex* m = (ArcObjMutex*)arc_gc_alloc(
        gc, sizeof(ArcObjMutex), OBJ_MUTEX);
    if (!m) return NULL;
    arc_mutex_init(&m->mtx);
    m->locked = false;
    return m;
}

/* --- Channel --- */

ArcObjChannel* arc_obj_channel_new(ArcGC* gc, uint32_t capacity) {
    ArcObjChannel* ch = (ArcObjChannel*)arc_gc_alloc(
        gc, sizeof(ArcObjChannel), OBJ_CHANNEL);
    if (!ch) return NULL;
    ch->cap = capacity > 0 ? capacity : 1;
    ch->buffer = (ArcValue*)calloc(ch->cap, sizeof(ArcValue));
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    ch->closed = false;
    arc_mutex_init(&ch->mtx);
    arc_condvar_init(&ch->not_empty);
    arc_condvar_init(&ch->not_full);
    return ch;
}

bool arc_channel_send(ArcObjChannel* ch, ArcValue val) {
    arc_mutex_lock(&ch->mtx);
    while (ch->count >= ch->cap && !ch->closed)
        arc_condvar_wait(&ch->not_full, &ch->mtx);
    if (ch->closed) { arc_mutex_unlock(&ch->mtx); return false; }
    ch->buffer[ch->tail] = val;
    ch->tail = (ch->tail + 1) % ch->cap;
    ch->count++;
    arc_condvar_signal(&ch->not_empty);
    arc_mutex_unlock(&ch->mtx);
    return true;
}

bool arc_channel_recv(ArcObjChannel* ch, ArcValue* out) {
    arc_mutex_lock(&ch->mtx);
    while (ch->count == 0 && !ch->closed)
        arc_condvar_wait(&ch->not_empty, &ch->mtx);
    if (ch->count == 0) { arc_mutex_unlock(&ch->mtx); return false; }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->cap;
    ch->count--;
    arc_condvar_signal(&ch->not_full);
    arc_mutex_unlock(&ch->mtx);
    return true;
}

/* --- GC: Mark reachable objects inside concurrency types --- */

void arc_concurrency_mark_obj(ArcObject* obj) {
    switch (obj->type) {
    case OBJ_COROUTINE: {
        ArcObjCoroutine* coro = (ArcObjCoroutine*)obj;
        if (!coro->vm) break;
        for (uint32_t i = 0; i < coro->vm->sp; i++)
            arc_gc_mark_value(coro->vm->stack[i]);
        for (uint16_t i = 0; i < coro->vm->global_count; i++)
            arc_gc_mark_value(coro->vm->globals[i]);
        arc_gc_mark_value(coro->vm->yielded);
        break;
    }
    case OBJ_THREAD: {
        ArcObjThread* t = (ArcObjThread*)obj;
        if (!t->vm) break;
        for (uint32_t i = 0; i < t->vm->sp; i++)
            arc_gc_mark_value(t->vm->stack[i]);
        for (uint16_t i = 0; i < t->vm->global_count; i++)
            arc_gc_mark_value(t->vm->globals[i]);
        break;
    }
    case OBJ_CHANNEL: {
        ArcObjChannel* ch = (ArcObjChannel*)obj;
        for (uint32_t i = 0; i < ch->count; i++) {
            uint32_t idx = (ch->head + i) % ch->cap;
            arc_gc_mark_value(ch->buffer[idx]);
        }
        break;
    }
    case OBJ_MUTEX:
        break; /* no outgoing references */
    default:
        break;
    }
}

/* --- GC: Free concurrency object internals --- */

void arc_concurrency_free_obj(ArcObject* obj) {
    switch (obj->type) {
    case OBJ_COROUTINE:
        free(((ArcObjCoroutine*)obj)->vm);
        break;
    case OBJ_THREAD: {
        ArcObjThread* t = (ArcObjThread*)obj;
        free(t->vm);
        break;
    }
    case OBJ_MUTEX:
        arc_mutex_destroy(&((ArcObjMutex*)obj)->mtx);
        break;
    case OBJ_CHANNEL: {
        ArcObjChannel* ch = (ArcObjChannel*)obj;
        arc_mutex_destroy(&ch->mtx);
        arc_condvar_destroy(&ch->not_empty);
        arc_condvar_destroy(&ch->not_full);
        free(ch->buffer);
        break;
    }
    default:
        break;
    }
}
