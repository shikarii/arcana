#ifndef ARCANA_VALUE_H
#define ARCANA_VALUE_H

#include "../common/arcana_common.h"

/* Tagged runtime value — explicit tags for v0 clarity */
typedef enum {
    VAL_NULL,
    VAL_BOOL,
    VAL_I64,
    VAL_F64,
    VAL_STRING,
} ArcValueTag;

/* Immutable heap-allocated string */
typedef struct ArcString {
    uint32_t len;
    uint32_t refcount;
    char     data[];  /* flexible array member */
} ArcString;

static inline ArcString* arc_string_new(const char* s, uint32_t len) {
    ArcString* str = (ArcString*)calloc(1, sizeof(ArcString) + len + 1);
    str->len = len; str->refcount = 1;
    memcpy(str->data, s, len); str->data[len] = '\0';
    return str;
}
static inline void arc_string_retain(ArcString* s) { if (s) s->refcount++; }
static inline void arc_string_release(ArcString* s) {
    if (s && --s->refcount == 0) free(s);
}

typedef struct {
    ArcValueTag tag;
    union {
        bool       b;
        int64_t    i64;
        double     f64;
        ArcString* str;
    } as;
} ArcValue;

static inline ArcValue arc_val_null(void)       { return (ArcValue){ .tag = VAL_NULL }; }
static inline ArcValue arc_val_bool(bool v)      { return (ArcValue){ .tag = VAL_BOOL, .as.b = v }; }
static inline ArcValue arc_val_i64(int64_t v)    { return (ArcValue){ .tag = VAL_I64, .as.i64 = v }; }
static inline ArcValue arc_val_f64(double v)     { return (ArcValue){ .tag = VAL_F64, .as.f64 = v }; }
static inline ArcValue arc_val_string(ArcString* s) { return (ArcValue){ .tag = VAL_STRING, .as.str = s }; }

static inline bool arc_val_is_truthy(ArcValue v) {
    switch (v.tag) {
    case VAL_NULL:   return false;
    case VAL_BOOL:   return v.as.b;
    case VAL_I64:    return v.as.i64 != 0;
    case VAL_F64:    return v.as.f64 != 0.0;
    case VAL_STRING: return v.as.str && v.as.str->len > 0;
    }
    return false;
}

static inline bool arc_val_equal(ArcValue a, ArcValue b) {
    if (a.tag != b.tag) return false;
    switch (a.tag) {
    case VAL_NULL:   return true;
    case VAL_BOOL:   return a.as.b == b.as.b;
    case VAL_I64:    return a.as.i64 == b.as.i64;
    case VAL_F64:    return a.as.f64 == b.as.f64;
    case VAL_STRING:
        if (a.as.str == b.as.str) return true;
        if (a.as.str->len != b.as.str->len) return false;
        return memcmp(a.as.str->data, b.as.str->data, a.as.str->len) == 0;
    }
    return false;
}

static inline void arc_val_print(ArcValue v, FILE* out) {
    switch (v.tag) {
    case VAL_NULL:   fprintf(out, "null"); break;
    case VAL_BOOL:   fprintf(out, "%s", v.as.b ? "true" : "false"); break;
    case VAL_I64:    fprintf(out, "%lld", (long long)v.as.i64); break;
    case VAL_F64:    fprintf(out, "%g", v.as.f64); break;
    case VAL_STRING: fprintf(out, "%s", v.as.str ? v.as.str->data : ""); break;
    }
}

#endif /* ARCANA_VALUE_H */
