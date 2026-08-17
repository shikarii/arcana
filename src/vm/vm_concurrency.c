/*
 * vm_concurrency.c — Concurrency opcode handlers for the Arcana VM.
 *
 * Handles: coroutines, threads, mutexes, channels.
 */

#include "vm_ops.h"
#include "../runtime/concurrency.h"

/* --- Coroutines --- */

static ArcStatus vm_exec_coro_new(ArcVm* vm) {
    uint16_t func_idx = vm_read_u16(vm);
    if (func_idx >= vm->image->functions.count) {
        vm_error(vm, "coro_new: invalid function index %u", func_idx);
        return ARC_ERR_RUNTIME;
    }
    ArcObjCoroutine* coro = arc_obj_coroutine_new(
        vm->gc, vm->image, func_idx);
    if (!coro || !coro->vm) {
        vm_error(vm, "coro_new: allocation failed");
        return ARC_ERR_RUNTIME;
    }
    if (!vm_push(vm, arc_val_obj((ArcObject*)coro))) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

static ArcStatus vm_exec_coro_resume(ArcVm* vm) {
    ArcValue send_val, coro_val;
    if (!vm_pop(vm, &send_val) || !vm_pop(vm, &coro_val))
        return ARC_ERR_RUNTIME;
    if (!ARC_IS_COROUTINE(coro_val)) {
        vm_error(vm, "coro_resume: not a coroutine");
        return ARC_ERR_RUNTIME;
    }
    ArcObjCoroutine* coro = ARC_AS_COROUTINE(coro_val);
    if (coro->status == CORO_DEAD) {
        vm_error(vm, "coro_resume: coroutine is dead");
        return ARC_ERR_RUNTIME;
    }
    /* Push send_val onto coroutine stack if suspended (not first resume) */
    if (coro->status == CORO_SUSPENDED)
        vm_push(coro->vm, send_val);
    coro->status = CORO_RUNNING;
    ArcStatus st = arc_vm_continue(coro->vm);
    if (st == ARC_YIELD) {
        coro->status = CORO_SUSPENDED;
        if (!vm_push(vm, coro->vm->yielded)) return ARC_ERR_RUNTIME;
    } else if (st == ARC_OK) {
        coro->status = CORO_DEAD;
        if (!vm_push(vm, arc_vm_result(coro->vm))) return ARC_ERR_RUNTIME;
    } else {
        coro->status = CORO_DEAD;
        vm_error(vm, "coro_resume: coroutine error: %s", coro->vm->error.message);
        return ARC_ERR_RUNTIME;
    }
    return ARC_OK;
}

static ArcStatus vm_exec_coro_yield(ArcVm* vm) {
    ArcValue val;
    if (!vm_pop(vm, &val)) return ARC_ERR_RUNTIME;
    vm->yielded = val;
    return ARC_YIELD;
}

static ArcStatus vm_exec_coro_status(ArcVm* vm) {
    ArcValue coro_val;
    if (!vm_pop(vm, &coro_val)) return ARC_ERR_RUNTIME;
    if (!ARC_IS_COROUTINE(coro_val)) {
        vm_error(vm, "coro_status: not a coroutine"); return ARC_ERR_RUNTIME;
    }
    const char* s;
    switch (ARC_AS_COROUTINE(coro_val)->status) {
    case CORO_READY:     s = "ready"; break;
    case CORO_RUNNING:   s = "running"; break;
    case CORO_SUSPENDED: s = "suspended"; break;
    case CORO_DEAD:      s = "dead"; break;
    default:             s = "unknown"; break;
    }
    ArcObjString* str = arc_obj_string_new(vm->gc, s, (uint32_t)strlen(s));
    if (!vm_push(vm, arc_val_obj((ArcObject*)str))) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

/* --- Threads --- */

static void* thread_entry(void* arg) {
    ArcObjThread* t = (ArcObjThread*)arg;
    arc_vm_continue(t->vm);
    return NULL;
}

static ArcStatus vm_exec_thread_spawn(ArcVm* vm) {
    uint16_t func_idx = vm_read_u16(vm);
    if (func_idx >= vm->image->functions.count) {
        vm_error(vm, "thread_spawn: invalid function index %u", func_idx);
        return ARC_ERR_RUNTIME;
    }
    ArcObjThread* t = arc_obj_thread_new(vm->gc);
    if (!t) { vm_error(vm, "thread_spawn: allocation failed"); return ARC_ERR_RUNTIME; }
    /* Set up thread's VM with shared GC */
    t->vm = (ArcVm*)calloc(1, sizeof(ArcVm));
    if (!t->vm) { vm_error(vm, "thread_spawn: vm alloc failed"); return ARC_ERR_RUNTIME; }
    t->vm->image = vm->image;
    t->vm->gc = vm->gc;
    t->vm->owns_gc = false;
    t->vm->output = vm->output;
    /* Set up initial frame */
    const ArcFuncRecord* fn = &vm->image->functions.funcs[func_idx];
    t->vm->frames[0] = (ArcFrame){
        .func_idx = func_idx, .return_ip = 0, .base_slot = 0
    };
    t->vm->fp = 1;
    t->vm->ip = fn->code_offset;
    t->vm->sp = fn->local_count;
    for (uint16_t i = 0; i < fn->local_count; i++)
        t->vm->stack[i] = arc_val_null();
    if (!arc_thread_create(&t->handle, thread_entry, t)) {
        vm_error(vm, "thread_spawn: failed to create thread");
        return ARC_ERR_RUNTIME;
    }
    if (!vm_push(vm, arc_val_obj((ArcObject*)t))) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

static ArcStatus vm_exec_thread_join(ArcVm* vm) {
    ArcValue t_val;
    if (!vm_pop(vm, &t_val)) return ARC_ERR_RUNTIME;
    if (!ARC_IS_THREAD(t_val)) {
        vm_error(vm, "thread_join: not a thread"); return ARC_ERR_RUNTIME;
    }
    ArcObjThread* t = ARC_AS_THREAD(t_val);
    if (t->joined) {
        vm_error(vm, "thread_join: already joined"); return ARC_ERR_RUNTIME;
    }
    arc_thread_join(t->handle);
    t->joined = true;
    if (!vm_push(vm, arc_vm_result(t->vm))) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

/* --- Mutexes --- */

static ArcStatus vm_exec_mutex_new(ArcVm* vm) {
    ArcObjMutex* m = arc_obj_mutex_new(vm->gc);
    if (!m) { vm_error(vm, "mutex_new: allocation failed"); return ARC_ERR_RUNTIME; }
    if (!vm_push(vm, arc_val_obj((ArcObject*)m))) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

static ArcStatus vm_exec_mutex_lock(ArcVm* vm) {
    ArcValue m_val;
    if (!vm_pop(vm, &m_val)) return ARC_ERR_RUNTIME;
    if (!ARC_IS_MUTEX(m_val)) {
        vm_error(vm, "mutex_lock: not a mutex"); return ARC_ERR_RUNTIME;
    }
    ArcObjMutex* m = ARC_AS_MUTEX(m_val);
    arc_mutex_lock(&m->mtx);
    m->locked = true;
    return ARC_OK;
}

static ArcStatus vm_exec_mutex_unlock(ArcVm* vm) {
    ArcValue m_val;
    if (!vm_pop(vm, &m_val)) return ARC_ERR_RUNTIME;
    if (!ARC_IS_MUTEX(m_val)) {
        vm_error(vm, "mutex_unlock: not a mutex"); return ARC_ERR_RUNTIME;
    }
    ArcObjMutex* m = ARC_AS_MUTEX(m_val);
    m->locked = false;
    arc_mutex_unlock(&m->mtx);
    return ARC_OK;
}

/* --- Channels --- */

static ArcStatus vm_exec_chan_new(ArcVm* vm) {
    uint16_t cap = vm_read_u16(vm);
    ArcObjChannel* ch = arc_obj_channel_new(vm->gc, cap > 0 ? cap : 1);
    if (!ch) { vm_error(vm, "chan_new: allocation failed"); return ARC_ERR_RUNTIME; }
    if (!vm_push(vm, arc_val_obj((ArcObject*)ch))) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

static ArcStatus vm_exec_chan_send(ArcVm* vm) {
    ArcValue val, ch_val;
    if (!vm_pop(vm, &val) || !vm_pop(vm, &ch_val)) return ARC_ERR_RUNTIME;
    if (!ARC_IS_CHANNEL(ch_val)) {
        vm_error(vm, "chan_send: not a channel"); return ARC_ERR_RUNTIME;
    }
    if (!arc_channel_send(ARC_AS_CHANNEL(ch_val), val)) {
        vm_error(vm, "chan_send: channel closed"); return ARC_ERR_RUNTIME;
    }
    return ARC_OK;
}

static ArcStatus vm_exec_chan_recv(ArcVm* vm) {
    ArcValue ch_val;
    if (!vm_pop(vm, &ch_val)) return ARC_ERR_RUNTIME;
    if (!ARC_IS_CHANNEL(ch_val)) {
        vm_error(vm, "chan_recv: not a channel"); return ARC_ERR_RUNTIME;
    }
    ArcValue result;
    if (!arc_channel_recv(ARC_AS_CHANNEL(ch_val), &result)) {
        if (!vm_push(vm, arc_val_null())) return ARC_ERR_RUNTIME;
    } else {
        if (!vm_push(vm, result)) return ARC_ERR_RUNTIME;
    }
    return ARC_OK;
}

/* --- Main dispatcher --- */

ArcStatus vm_exec_concurrency(ArcVm* vm, uint8_t op) {
    switch (op) {
    case OP_CORO_NEW:      return vm_exec_coro_new(vm);
    case OP_CORO_RESUME:   return vm_exec_coro_resume(vm);
    case OP_CORO_YIELD:    return vm_exec_coro_yield(vm);
    case OP_CORO_STATUS:   return vm_exec_coro_status(vm);
    case OP_THREAD_SPAWN:  return vm_exec_thread_spawn(vm);
    case OP_THREAD_JOIN:   return vm_exec_thread_join(vm);
    case OP_MUTEX_NEW:     return vm_exec_mutex_new(vm);
    case OP_MUTEX_LOCK:    return vm_exec_mutex_lock(vm);
    case OP_MUTEX_UNLOCK:  return vm_exec_mutex_unlock(vm);
    case OP_CHAN_NEW:       return vm_exec_chan_new(vm);
    case OP_CHAN_SEND:      return vm_exec_chan_send(vm);
    case OP_CHAN_RECV:      return vm_exec_chan_recv(vm);
    default:
        vm_error(vm, "bad concurrency op 0x%02X", op);
        return ARC_ERR_RUNTIME;
    }
}
