#ifndef ARCANA_GC_H
#define ARCANA_GC_H

#include "object.h"

#define ARC_GC_INITIAL_THRESHOLD 1024  /* bytes before first GC */
#define ARC_GC_GROW_FACTOR       2

/* Garbage collector state */
struct ArcGC {
    ArcObject*  objects;            /* head of all allocated objects */
    size_t      bytes_allocated;
    size_t      next_gc;            /* trigger threshold */
};

/* Initialize / tear down */
void arc_gc_init(ArcGC* gc);
void arc_gc_free_all(ArcGC* gc);

/* Allocate an object (adds to object chain) */
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
