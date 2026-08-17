#ifndef ARCANA_GC_H
#define ARCANA_GC_H

#include "object.h"
#include "../platform/thread.h"

#define ARC_GC_INITIAL_THRESHOLD 1024  /* bytes before first GC */
#define ARC_GC_GROW_FACTOR       2

/* Garbage collector state.
 * Shared between VMs via pointer — coroutines and threads use the
 * same GC instance so objects can cross execution boundaries. */
struct ArcGC {
    ArcObject*       objects;        /* head of all allocated objects */
    size_t           bytes_allocated;
    size_t           next_gc;        /* trigger threshold */
    bool             stress_mode;    /* if true, collect on every allocation */
    uint32_t         alloc_count;    /* total allocations since init */
    uint32_t         collect_count;  /* total collections since init */
    ArcPlatformMutex lock;           /* protects object chain for threads */
};

/* Create / destroy a heap-allocated GC (shared via pointer) */
ArcGC* arc_gc_create(void);
void   arc_gc_destroy(ArcGC* gc);

/* Initialize stack-allocated GC (tests, single-thread use) */
void arc_gc_init(ArcGC* gc);
void arc_gc_free_all(ArcGC* gc);

/* Enable/disable stress mode (GC on every allocation) */
void arc_gc_set_stress(ArcGC* gc, bool enabled);

/* Allocate an object (thread-safe — takes lock) */
ArcObject* arc_gc_alloc(ArcGC* gc, size_t size, ArcObjType type);

/* Mark-sweep collection.
 * Caller must provide roots: stack values and globals.
 * open_upvalues is the head of the VM's open upvalue chain. */
void arc_gc_collect(ArcGC* gc,
                    ArcValue* stack, uint32_t sp,
                    ArcValue* globals, uint16_t global_count,
                    ArcObjUpvalue* open_upvalues);

/* Mark a single value as reachable (for external root marking) */
void arc_gc_mark_value(ArcValue val);
void arc_gc_mark_object(ArcObject* obj);

#endif /* ARCANA_GC_H */
