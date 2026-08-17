#include "gc.h"

void arc_gc_init(ArcGC* gc) {
    gc->objects = NULL;
    gc->bytes_allocated = 0;
    gc->next_gc = ARC_GC_INITIAL_THRESHOLD;
    gc->stress_mode = false;
    gc->alloc_count = 0;
    gc->collect_count = 0;
    arc_mutex_init(&gc->lock);
}

ArcGC* arc_gc_create(void) {
    ArcGC* gc = (ArcGC*)calloc(1, sizeof(ArcGC));
    if (gc) arc_gc_init(gc);
    return gc;
}

void arc_gc_destroy(ArcGC* gc) {
    if (!gc) return;
    arc_gc_free_all(gc);
    arc_mutex_destroy(&gc->lock);
    free(gc);
}

void arc_gc_set_stress(ArcGC* gc, bool enabled) {
    gc->stress_mode = enabled;
}

ArcObject* arc_gc_alloc(ArcGC* gc, size_t size, ArcObjType type) {
    arc_mutex_lock(&gc->lock);
    gc->alloc_count++;
    ArcObject* obj = (ArcObject*)calloc(1, size);
    if (!obj) { arc_mutex_unlock(&gc->lock); return NULL; }
    obj->type = type;
    obj->marked = false;
    obj->next = gc->objects;
    gc->objects = obj;
    gc->bytes_allocated += size;
    arc_mutex_unlock(&gc->lock);
    return obj;
}

/* Forward declarations — defined in concurrency.c */
void arc_concurrency_mark_obj(ArcObject* obj);
void arc_concurrency_free_obj(ArcObject* obj);

/* --- Marking --- */

void arc_gc_mark_object(ArcObject* obj) {
    if (!obj || obj->marked) return;
    obj->marked = true;

    switch (obj->type) {
    case OBJ_STRING:
    case OBJ_BITVEC:
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
        arc_gc_mark_value(uv->closed);
        break;
    }
    case OBJ_RECORD: {
        ArcObjRecord* rec = (ArcObjRecord*)obj;
        for (uint16_t i = 0; i < rec->field_count; i++)
            arc_gc_mark_value(rec->fields[i]);
        break;
    }
    case OBJ_COROUTINE:
    case OBJ_THREAD:
    case OBJ_MUTEX:
    case OBJ_CHANNEL:
        arc_concurrency_mark_obj(obj);
        break;
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
        break;
    case OBJ_ARRAY:
        free(((ArcObjArray*)obj)->items);
        break;
    case OBJ_MAP:
        free(((ArcObjMap*)obj)->keys);
        free(((ArcObjMap*)obj)->values);
        break;
    case OBJ_CLOSURE:
        free(((ArcObjClosure*)obj)->upvalues);
        break;
    case OBJ_UPVALUE:
        break;
    case OBJ_RECORD:
        free(((ArcObjRecord*)obj)->fields);
        free(((ArcObjRecord*)obj)->field_names);
        break;
    case OBJ_BITVEC:
        free(((ArcObjBitvec*)obj)->bits);
        break;
    case OBJ_COROUTINE:
    case OBJ_THREAD:
    case OBJ_MUTEX:
    case OBJ_CHANNEL:
        arc_concurrency_free_obj(obj);
        break;
    }
    free(obj);
}

static void sweep(ArcGC* gc) {
    ArcObject** pp = &gc->objects;
    while (*pp) {
        if ((*pp)->marked) {
            (*pp)->marked = false;
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
    for (uint32_t i = 0; i < sp; i++)
        arc_gc_mark_value(stack[i]);
    for (uint16_t i = 0; i < global_count; i++)
        arc_gc_mark_value(globals[i]);
    ArcObjUpvalue* uv = open_upvalues;
    while (uv) {
        arc_gc_mark_object((ArcObject*)uv);
        uv = uv->next;
    }
    sweep(gc);
    gc->collect_count++;
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
