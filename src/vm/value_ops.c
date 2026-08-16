#include "value.h"
#include "../runtime/object.h"

bool arc_val_equal(ArcValue a, ArcValue b) {
    if (a.tag != b.tag) return false;
    switch (a.tag) {
    case VAL_NULL:  return true;
    case VAL_BOOL:  return a.as.b == b.as.b;
    case VAL_I64:   return a.as.i64 == b.as.i64;
    case VAL_F64:   return a.as.f64 == b.as.f64;
    case VAL_OBJ:   return arc_obj_equal(a.as.obj, b.as.obj);
    }
    return false;
}

void arc_val_print(ArcValue v, FILE* out) {
    switch (v.tag) {
    case VAL_NULL:  fprintf(out, "null"); break;
    case VAL_BOOL:  fprintf(out, "%s", v.as.b ? "true" : "false"); break;
    case VAL_I64:   fprintf(out, "%lld", (long long)v.as.i64); break;
    case VAL_F64:   fprintf(out, "%g", v.as.f64); break;
    case VAL_OBJ:
        if (v.as.obj) arc_obj_print(v.as.obj, out);
        else fprintf(out, "null");
        break;
    }
}
