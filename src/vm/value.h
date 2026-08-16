#ifndef ARCANA_VALUE_H
#define ARCANA_VALUE_H

#include "../common/arcana_common.h"

/* Forward declaration — full definition in runtime/object.h */
typedef struct ArcObject ArcObject;

/* Tagged runtime value */
typedef enum {
    VAL_NULL,
    VAL_BOOL,
    VAL_I64,
    VAL_F64,
    VAL_OBJ,
} ArcValueTag;

typedef struct {
    ArcValueTag tag;
    union {
        bool        b;
        int64_t     i64;
        double      f64;
        ArcObject*  obj;
    } as;
} ArcValue;

/* --- Value constructors --- */
static inline ArcValue arc_val_null(void)       { return (ArcValue){ .tag = VAL_NULL }; }
static inline ArcValue arc_val_bool(bool v)      { return (ArcValue){ .tag = VAL_BOOL, .as.b = v }; }
static inline ArcValue arc_val_i64(int64_t v)    { return (ArcValue){ .tag = VAL_I64, .as.i64 = v }; }
static inline ArcValue arc_val_f64(double v)     { return (ArcValue){ .tag = VAL_F64, .as.f64 = v }; }
static inline ArcValue arc_val_obj(ArcObject* o) { return (ArcValue){ .tag = VAL_OBJ, .as.obj = o }; }

/* --- Truthiness (obj-aware version needs object.h; defined in value_ops.h) --- */
static inline bool arc_val_is_truthy(ArcValue v) {
    switch (v.tag) {
    case VAL_NULL:  return false;
    case VAL_BOOL:  return v.as.b;
    case VAL_I64:   return v.as.i64 != 0;
    case VAL_F64:   return v.as.f64 != 0.0;
    case VAL_OBJ:   return v.as.obj != NULL;
    }
    return false;
}

/* Equality and printing require object.h — declared here, defined in value_ops.c */
bool arc_val_equal(ArcValue a, ArcValue b);
void arc_val_print(ArcValue v, FILE* out);

#endif /* ARCANA_VALUE_H */
