#include "gc.h"    /* includes object.h which includes value.h */

/* --- String --- */

ArcObjString* arc_obj_string_new(ArcGC* gc, const char* data, uint32_t len) {
    ArcObjString* s = (ArcObjString*)arc_gc_alloc(gc, sizeof(ArcObjString) + len + 1, OBJ_STRING);
    if (!s) return NULL;
    s->len = len;
    memcpy(s->data, data, len);
    s->data[len] = '\0';
    return s;
}

ArcObjString* arc_obj_string_concat(ArcGC* gc, ArcObjString* a, ArcObjString* b) {
    uint32_t len = a->len + b->len;
    ArcObjString* s = (ArcObjString*)arc_gc_alloc(gc, sizeof(ArcObjString) + len + 1, OBJ_STRING);
    if (!s) return NULL;
    s->len = len;
    memcpy(s->data, a->data, a->len);
    memcpy(s->data + a->len, b->data, b->len);
    s->data[len] = '\0';
    return s;
}

/* --- Array --- */

ArcObjArray* arc_obj_array_new(ArcGC* gc, int32_t initial_cap) {
    ArcObjArray* arr = (ArcObjArray*)arc_gc_alloc(gc, sizeof(ArcObjArray), OBJ_ARRAY);
    if (!arr) return NULL;
    arr->count = 0;
    arr->cap = initial_cap > 0 ? initial_cap : 8;
    arr->items = (ArcValue*)calloc((size_t)arr->cap, sizeof(ArcValue));
    return arr;
}

void arc_obj_array_push(ArcObjArray* arr, ArcValue val) {
    if (arr->count >= arr->cap) {
        arr->cap = arr->cap < 8 ? 8 : arr->cap * 2;
        arr->items = (ArcValue*)realloc(arr->items, (size_t)arr->cap * sizeof(ArcValue));
    }
    arr->items[arr->count++] = val;
}

/* --- Map (linear scan — adequate for small maps) --- */

ArcObjMap* arc_obj_map_new(ArcGC* gc, int32_t initial_cap) {
    ArcObjMap* map = (ArcObjMap*)arc_gc_alloc(gc, sizeof(ArcObjMap), OBJ_MAP);
    if (!map) return NULL;
    map->count = 0;
    map->cap = initial_cap > 0 ? initial_cap : 8;
    map->keys = (ArcValue*)calloc((size_t)map->cap, sizeof(ArcValue));
    map->values = (ArcValue*)calloc((size_t)map->cap, sizeof(ArcValue));
    return map;
}

bool arc_obj_map_set(ArcObjMap* map, ArcValue key, ArcValue val) {
    /* Check for existing key */
    for (int32_t i = 0; i < map->count; i++) {
        if (arc_val_equal(map->keys[i], key)) {
            map->values[i] = val;
            return true;  /* updated existing */
        }
    }
    /* Insert new */
    if (map->count >= map->cap) {
        map->cap = map->cap < 8 ? 8 : map->cap * 2;
        map->keys = (ArcValue*)realloc(map->keys, (size_t)map->cap * sizeof(ArcValue));
        map->values = (ArcValue*)realloc(map->values, (size_t)map->cap * sizeof(ArcValue));
    }
    map->keys[map->count] = key;
    map->values[map->count] = val;
    map->count++;
    return false;  /* inserted new */
}

bool arc_obj_map_get(ArcObjMap* map, ArcValue key, ArcValue* out) {
    for (int32_t i = 0; i < map->count; i++) {
        if (arc_val_equal(map->keys[i], key)) {
            *out = map->values[i];
            return true;
        }
    }
    return false;
}

/* --- Closure --- */

ArcObjClosure* arc_obj_closure_new(ArcGC* gc, uint16_t func_idx, uint8_t upvalue_count) {
    ArcObjClosure* cl = (ArcObjClosure*)arc_gc_alloc(gc, sizeof(ArcObjClosure), OBJ_CLOSURE);
    if (!cl) return NULL;
    cl->func_idx = func_idx;
    cl->upvalue_count = upvalue_count;
    if (upvalue_count > 0) {
        cl->upvalues = (ArcObjUpvalue**)calloc(upvalue_count, sizeof(ArcObjUpvalue*));
    } else {
        cl->upvalues = NULL;
    }
    return cl;
}

/* --- Upvalue --- */

ArcObjUpvalue* arc_obj_upvalue_new(ArcGC* gc, ArcValue* slot) {
    ArcObjUpvalue* uv = (ArcObjUpvalue*)arc_gc_alloc(gc, sizeof(ArcObjUpvalue), OBJ_UPVALUE);
    if (!uv) return NULL;
    uv->location = slot;
    uv->closed = arc_val_null();
    uv->next = NULL;
    return uv;
}

/* --- Record --- */

ArcObjRecord* arc_obj_record_new(ArcGC* gc, const char* type_name, uint16_t field_count) {
    ArcObjRecord* rec = (ArcObjRecord*)arc_gc_alloc(gc, sizeof(ArcObjRecord), OBJ_RECORD);
    if (!rec) return NULL;
    rec->type_name = type_name;
    rec->field_count = 0;
    rec->field_cap = field_count > 0 ? field_count : 4;
    rec->fields = (ArcValue*)calloc(rec->field_cap, sizeof(ArcValue));
    rec->field_names = (const char**)calloc(rec->field_cap, sizeof(const char*));
    return rec;
}

bool arc_obj_record_set(ArcObjRecord* rec, const char* field, ArcValue val) {
    /* Update existing */
    for (uint16_t i = 0; i < rec->field_count; i++) {
        if (strcmp(rec->field_names[i], field) == 0) {
            rec->fields[i] = val;
            return true;
        }
    }
    /* Insert new */
    if (rec->field_count >= rec->field_cap) {
        rec->field_cap = rec->field_cap < 4 ? 4 : rec->field_cap * 2;
        rec->fields = (ArcValue*)realloc(rec->fields, rec->field_cap * sizeof(ArcValue));
        rec->field_names = (const char**)realloc(rec->field_names, rec->field_cap * sizeof(const char*));
    }
    rec->field_names[rec->field_count] = field;
    rec->fields[rec->field_count] = val;
    rec->field_count++;
    return false;
}

bool arc_obj_record_get(ArcObjRecord* rec, const char* field, ArcValue* out) {
    for (uint16_t i = 0; i < rec->field_count; i++) {
        if (strcmp(rec->field_names[i], field) == 0) {
            *out = rec->fields[i];
            return true;
        }
    }
    return false;
}

/* --- Printing --- */

void arc_obj_print(ArcObject* obj, FILE* out) {
    switch (obj->type) {
    case OBJ_STRING:
        fprintf(out, "%s", ((ArcObjString*)obj)->data);
        break;
    case OBJ_ARRAY: {
        ArcObjArray* arr = (ArcObjArray*)obj;
        fprintf(out, "[");
        for (int32_t i = 0; i < arr->count; i++) {
            if (i > 0) fprintf(out, ", ");
            arc_val_print(arr->items[i], out);
        }
        fprintf(out, "]");
        break;
    }
    case OBJ_MAP: {
        ArcObjMap* map = (ArcObjMap*)obj;
        fprintf(out, "{");
        for (int32_t i = 0; i < map->count; i++) {
            if (i > 0) fprintf(out, ", ");
            arc_val_print(map->keys[i], out);
            fprintf(out, ": ");
            arc_val_print(map->values[i], out);
        }
        fprintf(out, "}");
        break;
    }
    case OBJ_CLOSURE:
        fprintf(out, "<closure %u>", ((ArcObjClosure*)obj)->func_idx);
        break;
    case OBJ_UPVALUE:
        fprintf(out, "<upvalue>");
        break;
    case OBJ_RECORD: {
        ArcObjRecord* rec = (ArcObjRecord*)obj;
        fprintf(out, "%s{", rec->type_name ? rec->type_name : "record");
        for (uint16_t i = 0; i < rec->field_count; i++) {
            if (i > 0) fprintf(out, ", ");
            fprintf(out, "%s: ", rec->field_names[i]);
            arc_val_print(rec->fields[i], out);
        }
        fprintf(out, "}");
        break;
    }
    }
}

/* --- Equality --- */

bool arc_obj_equal(ArcObject* a, ArcObject* b) {
    if (a == b) return true;
    if (a->type != b->type) return false;
    switch (a->type) {
    case OBJ_STRING: {
        ArcObjString* sa = (ArcObjString*)a;
        ArcObjString* sb = (ArcObjString*)b;
        return sa->len == sb->len && memcmp(sa->data, sb->data, sa->len) == 0;
    }
    default:
        return false;  /* reference equality for other types */
    }
}

/* --- Type name --- */

const char* arc_obj_type_name(ArcObjType type) {
    switch (type) {
    case OBJ_STRING:  return "string";
    case OBJ_ARRAY:   return "array";
    case OBJ_MAP:     return "map";
    case OBJ_CLOSURE: return "closure";
    case OBJ_UPVALUE: return "upvalue";
    case OBJ_RECORD:  return "record";
    }
    return "unknown";
}
