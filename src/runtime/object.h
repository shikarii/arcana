#ifndef ARCANA_OBJECT_H
#define ARCANA_OBJECT_H

#include "../vm/value.h"

/* Forward declaration */
typedef struct ArcGC ArcGC;

/* --- Object type tags --- */
typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_MAP,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_RECORD,
    OBJ_BITVEC,
    OBJ_COROUTINE,
    OBJ_THREAD,
    OBJ_MUTEX,
    OBJ_CHANNEL,
} ArcObjType;

/* --- Common object header --- */
struct ArcObject {
    ArcObjType  type;
    bool        marked;
    ArcObject*  next;       /* intrusive linked list for GC sweep */
};

/* --- Concrete object types --- */

typedef struct {
    ArcObject obj;
    uint32_t  len;
    char      data[];       /* flexible array member */
} ArcObjString;

typedef struct {
    ArcObject  obj;
    ArcValue*  items;
    int32_t    count;
    int32_t    cap;
} ArcObjArray;

typedef struct {
    ArcObject  obj;
    ArcValue*  keys;
    ArcValue*  values;
    int32_t    count;
    int32_t    cap;
} ArcObjMap;

typedef struct ArcObjUpvalue {
    ArcObject             obj;
    ArcValue*             location;   /* points to stack slot or &closed */
    ArcValue              closed;     /* captured value after scope exits */
    struct ArcObjUpvalue* next;       /* open upvalue chain */
} ArcObjUpvalue;

typedef struct {
    ArcObject       obj;
    uint16_t        func_idx;
    ArcObjUpvalue** upvalues;
    uint8_t         upvalue_count;
} ArcObjClosure;

typedef struct {
    ArcObject  obj;
    const char* type_name;    /* record type name (e.g. "Point") */
    ArcValue*  fields;        /* field values array */
    const char** field_names; /* field name array (parallel to fields) */
    uint16_t   field_count;
    uint16_t   field_cap;
} ArcObjRecord;

typedef struct {
    ArcObject  obj;
    uint8_t*   bits;        /* packed bit storage */
    uint32_t   bit_count;   /* number of bits */
} ArcObjBitvec;

/* --- Type checking macros --- */
#define ARC_IS_OBJ(val)      ((val).tag == VAL_OBJ)
#define ARC_OBJ_TYPE(obj)    (((ArcObject*)(obj))->type)
#define ARC_IS_STRING(val)   (ARC_IS_OBJ(val) && ARC_OBJ_TYPE((val).as.obj) == OBJ_STRING)
#define ARC_IS_ARRAY(val)    (ARC_IS_OBJ(val) && ARC_OBJ_TYPE((val).as.obj) == OBJ_ARRAY)
#define ARC_IS_MAP(val)      (ARC_IS_OBJ(val) && ARC_OBJ_TYPE((val).as.obj) == OBJ_MAP)
#define ARC_IS_CLOSURE(val)  (ARC_IS_OBJ(val) && ARC_OBJ_TYPE((val).as.obj) == OBJ_CLOSURE)
#define ARC_IS_RECORD(val)   (ARC_IS_OBJ(val) && ARC_OBJ_TYPE((val).as.obj) == OBJ_RECORD)
#define ARC_IS_BITVEC(val)   (ARC_IS_OBJ(val) && ARC_OBJ_TYPE((val).as.obj) == OBJ_BITVEC)
#define ARC_IS_COROUTINE(val) (ARC_IS_OBJ(val) && ARC_OBJ_TYPE((val).as.obj) == OBJ_COROUTINE)
#define ARC_IS_THREAD(val)    (ARC_IS_OBJ(val) && ARC_OBJ_TYPE((val).as.obj) == OBJ_THREAD)
#define ARC_IS_MUTEX(val)     (ARC_IS_OBJ(val) && ARC_OBJ_TYPE((val).as.obj) == OBJ_MUTEX)
#define ARC_IS_CHANNEL(val)   (ARC_IS_OBJ(val) && ARC_OBJ_TYPE((val).as.obj) == OBJ_CHANNEL)

/* --- Cast macros (from ArcValue) --- */
#define ARC_AS_STRING(val)   ((ArcObjString*)((val).as.obj))
#define ARC_AS_ARRAY(val)    ((ArcObjArray*)((val).as.obj))
#define ARC_AS_MAP(val)      ((ArcObjMap*)((val).as.obj))
#define ARC_AS_CLOSURE(val)  ((ArcObjClosure*)((val).as.obj))
#define ARC_AS_UPVALUE(val)  ((ArcObjUpvalue*)((val).as.obj))
#define ARC_AS_RECORD(val)   ((ArcObjRecord*)((val).as.obj))
#define ARC_AS_BITVEC(val)   ((ArcObjBitvec*)((val).as.obj))

/* --- Object constructors (allocate via GC) --- */
ArcObjString*  arc_obj_string_new(ArcGC* gc, const char* data, uint32_t len);
ArcObjString*  arc_obj_string_concat(ArcGC* gc, ArcObjString* a, ArcObjString* b);
ArcObjArray*   arc_obj_array_new(ArcGC* gc, int32_t initial_cap);
void           arc_obj_array_push(ArcObjArray* arr, ArcValue val);
ArcObjMap*     arc_obj_map_new(ArcGC* gc, int32_t initial_cap);
bool           arc_obj_map_set(ArcObjMap* map, ArcValue key, ArcValue val);
bool           arc_obj_map_get(ArcObjMap* map, ArcValue key, ArcValue* out);
ArcObjClosure* arc_obj_closure_new(ArcGC* gc, uint16_t func_idx, uint8_t upvalue_count);
ArcObjUpvalue* arc_obj_upvalue_new(ArcGC* gc, ArcValue* slot);
ArcObjRecord*  arc_obj_record_new(ArcGC* gc, const char* type_name, uint16_t field_count);
bool           arc_obj_record_set(ArcObjRecord* rec, const char* field, ArcValue val);
bool           arc_obj_record_get(ArcObjRecord* rec, const char* field, ArcValue* out);
ArcObjBitvec*  arc_obj_bitvec_new(ArcGC* gc, uint32_t bit_count);
uint32_t       arc_obj_bitvec_popcount(ArcObjBitvec* bv);

/* --- Object utilities --- */
void        arc_obj_print(ArcObject* obj, FILE* out);
bool        arc_obj_equal(ArcObject* a, ArcObject* b);
const char* arc_obj_type_name(ArcObjType type);

#endif /* ARCANA_OBJECT_H */
