#include "gc.h"

void arc_gc_init(ArcGC* gc) {
    gc->objects = NULL;
    gc->bytes_allocated = 0;
    gc->next_gc = ARC_GC_INITIAL_THRESHOLD;
}

ArcObject* arc_gc_alloc(ArcGC* gc, size_t size, ArcObjType type) {
    ArcObject* obj = (ArcObject*)calloc(1, size);
    if (!obj) return NULL;
    obj->type = type;
    obj->marked = false;
    obj->next = gc->objects;
    gc->objects = obj;
    gc->bytes_allocated += size;
    return obj;
}

/* --- Marking --- */

void arc_gc_mark_object(ArcObject* obj) {
    if (!obj || obj->marked) return;
    obj->marked = true;

    /* Trace references within the object */
    switch (obj->type) {
    case OBJ_STRING:
        break;  /* no outgoing references */
    case OBJ_ARRAY: {
        ArcObjArray* arr = (ArcObjArray*)obj;
        for (int32_t i = 0; i < arr->count; i++)
            arc_gc_mark_value(arr->items[i]);
        break;
    }
    case OBJ_MAP: {
        ArcObjMap* map = (ArcObjMap*)obj;
        for (int32_t i = 0; i < map->count; i++) {
            arc_gc_mark_value(map->keys[i]);
            arc_gc_mark_value(map->values[i]);
        }
        break;
    }
    case OBJ_CLOSURE: {
        ArcObjClosure* cl = (ArcObjClosure*)obj;
        for (uint8_t i = 0; i < cl->upvalue_count; i++) {
            if (cl->upvalues[i])
                arc_gc_mark_object((ArcObject*)cl->upvalues[i]);
        }
        break;
    }
    case OBJ_UPVALUE: {
        ArcObjUpvalue* uv = (ArcObjUpvalue*)obj;
        /* Mark the closed-over value (if closed, location == &closed) */
        arc_gc_mark_value(uv->closed);
        /* If still open, the stack slot is already a root */
        break;
    }
    }
}

void arc_gc_mark_value(ArcValue val) {
    if (val.tag == VAL_OBJ && val.as.obj)
        arc_gc_mark_object(val.as.obj);
}

/* --- Sweeping --- */

static void free_object(ArcObject* obj) {
    switch (obj->type) {
    case OBJ_STRING:
        /* ArcObjString uses flexible array member, just free */
        break;
    case OBJ_ARRAY: {
        ArcObjArray* arr = (ArcObjArray*)obj;
        free(arr->items);
        break;
    }
    case OBJ_MAP: {
        ArcObjMap* map = (ArcObjMap*)obj;
        free(map->keys);
        free(map->values);
        break;
    }
    case OBJ_CLOSURE: {
        ArcObjClosure* cl = (ArcObjClosure*)obj;
        free(cl->upvalues);
        break;
    }
    case OBJ_UPVALUE:
        break;
    }
    free(obj);
}

static void sweep(ArcGC* gc) {
    ArcObject** pp = &gc->objects;
    while (*pp) {
        if ((*pp)->marked) {
            (*pp)->marked = false;  /* reset for next cycle */
            pp = &(*pp)->next;
        } else {
            ArcObject* unreached = *pp;
            *pp = unreached->next;
            free_object(unreached);
        }
    }
}

void arc_gc_collect(ArcGC* gc,
                    ArcValue* stack, uint32_t sp,
                    ArcValue* globals, uint16_t global_count,
                    ArcObjUpvalue* open_upvalues) {
    /* Mark roots: stack */
    for (uint32_t i = 0; i < sp; i++)
        arc_gc_mark_value(stack[i]);

    /* Mark roots: globals */
    for (uint16_t i = 0; i < global_count; i++)
        arc_gc_mark_value(globals[i]);

    /* Mark roots: open upvalue chain */
    ArcObjUpvalue* uv = open_upvalues;
    while (uv) {
        arc_gc_mark_object((ArcObject*)uv);
        uv = uv->next;
    }

    /* Sweep */
    sweep(gc);

    /* Adjust threshold */
    gc->next_gc = gc->bytes_allocated * ARC_GC_GROW_FACTOR;
    if (gc->next_gc < ARC_GC_INITIAL_THRESHOLD)
        gc->next_gc = ARC_GC_INITIAL_THRESHOLD;
}

void arc_gc_free_all(ArcGC* gc) {
    ArcObject* obj = gc->objects;
    while (obj) {
        ArcObject* next = obj->next;
        free_object(obj);
        obj = next;
    }
    gc->objects = NULL;
    gc->bytes_allocated = 0;
}
