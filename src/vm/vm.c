#include "vm.h"
#include "../runtime/object.h"
#include <stdarg.h>
#include <time.h>

void arc_vm_init(ArcVm* vm, const ArcBytecodeImage* image) {
    memset(vm, 0, sizeof(*vm));
    vm->image = image;
    vm->output = stdout;
    arc_gc_init(&vm->gc);
    vm->open_upvalues = NULL;
}

void arc_vm_destroy(ArcVm* vm) {
    arc_gc_free_all(&vm->gc);
    vm->open_upvalues = NULL;
}

static void vm_error(ArcVm* vm, const char* fmt, ...) {
    vm->error.code = ARC_ERR_RUNTIME;
    vm->error.ip = vm->ip;
    vm->error.func_idx = vm->fp > 0 ? vm->frames[vm->fp - 1].func_idx : 0;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(vm->error.message, sizeof(vm->error.message), fmt, ap);
    va_end(ap);
}

static inline bool vm_push(ArcVm* vm, ArcValue v) {
    if (vm->sp >= ARC_STACK_MAX) {
        vm_error(vm, "stack overflow"); return false;
    }
    vm->stack[vm->sp++] = v;
    return true;
}

static inline bool vm_pop(ArcVm* vm, ArcValue* v) {
    if (vm->sp == 0) {
        vm_error(vm, "stack underflow"); return false;
    }
    *v = vm->stack[--vm->sp];
    return true;
}


static inline uint8_t read_byte(ArcVm* vm) {
    return vm->image->code[vm->ip++];
}

static inline uint16_t read_u16(ArcVm* vm) {
    uint16_t v = arc_read_u16(vm->image->code + vm->ip);
    vm->ip += 2;
    return v;
}

static inline int32_t read_i32(ArcVm* vm) {
    int32_t v = arc_read_i32(vm->image->code + vm->ip);
    vm->ip += 4;
    return v;
}

static ArcValue const_to_val(ArcGC* gc, const ArcConstant* c) {
    switch (c->tag) {
    case ARC_CONST_NULL:   return arc_val_null();
    case ARC_CONST_BOOL:   return arc_val_bool(c->as.b);
    case ARC_CONST_I64:    return arc_val_i64(c->as.i64);
    case ARC_CONST_F64:    return arc_val_f64(c->as.f64);
    case ARC_CONST_STRING: {
        ArcObjString* s = arc_obj_string_new(gc, c->as.str.data, c->as.str.len);
        return arc_val_obj((ArcObject*)s);
    }
    }
    return arc_val_null();
}

/* Binary arithmetic helper */
#define BINARY_OP(vm, op_name, op_sym) do { \
    ArcValue b, a; \
    if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME; \
    if (a.tag == VAL_I64 && b.tag == VAL_I64) { \
        if (!vm_push(vm, arc_val_i64(a.as.i64 op_sym b.as.i64))) return ARC_ERR_RUNTIME; \
    } else if (a.tag == VAL_F64 && b.tag == VAL_F64) { \
        if (!vm_push(vm, arc_val_f64(a.as.f64 op_sym b.as.f64))) return ARC_ERR_RUNTIME; \
    } else { \
        vm_error(vm, "%s: type mismatch", op_name); return ARC_ERR_RUNTIME; \
    } \
} while(0)

#define COMPARE_OP(vm, op_name, op_sym) do { \
    ArcValue b, a; \
    if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME; \
    if (a.tag == VAL_I64 && b.tag == VAL_I64) { \
        if (!vm_push(vm, arc_val_bool(a.as.i64 op_sym b.as.i64))) return ARC_ERR_RUNTIME; \
    } else if (a.tag == VAL_F64 && b.tag == VAL_F64) { \
        if (!vm_push(vm, arc_val_bool(a.as.f64 op_sym b.as.f64))) return ARC_ERR_RUNTIME; \
    } else { \
        vm_error(vm, "%s: type mismatch", op_name); return ARC_ERR_RUNTIME; \
    } \
} while(0)

ArcStatus arc_vm_run(ArcVm* vm) {
    if (vm->image->functions.count == 0) {
        vm_error(vm, "no functions"); return ARC_ERR_RUNTIME;
    }

    /* Set up initial frame for function 0 (main) */
    const ArcFuncRecord* main_fn = &vm->image->functions.funcs[0];
    vm->frames[0] = (ArcFrame){
        .func_idx = 0,
        .return_ip = 0,
        .base_slot = 0
    };
    vm->fp = 1;
    vm->ip = main_fn->code_offset;

    /* Reserve space for locals */
    vm->sp = main_fn->local_count;
    for (uint16_t i = 0; i < main_fn->local_count; i++)
        vm->stack[i] = arc_val_null();

    while (!vm->halted) {
        if (vm->ip >= vm->image->code_len) {
            vm_error(vm, "ip out of bounds"); return ARC_ERR_RUNTIME;
        }

        uint32_t instr_ip = vm->ip;
        uint8_t op = read_byte(vm);

        if (vm->trace) {
            fprintf(vm->output ? vm->output : stdout,
                    "  [%04u] %-16s sp=%u\n", instr_ip, arc_op_mnemonic(op), vm->sp);
        }

        switch (op) {
        case OP_CONST: {
            uint16_t idx = read_u16(vm);
            if (idx >= vm->image->constants.count) {
                vm_error(vm, "invalid constant index %u", idx); return ARC_ERR_RUNTIME;
            }
            if (!vm_push(vm, const_to_val(&vm->gc, &vm->image->constants.entries[idx])))
                return ARC_ERR_RUNTIME;
            break;
        }

        case OP_POP: {
            ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
            break;
        }

        case OP_DUP: {
            if (vm->sp == 0) { vm_error(vm, "dup: empty stack"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, vm->stack[vm->sp - 1])) return ARC_ERR_RUNTIME;
            break;
        }

        case OP_LOAD_LOCAL: {
            uint16_t slot = read_u16(vm);
            uint32_t base = vm->frames[vm->fp - 1].base_slot;
            if (!vm_push(vm, vm->stack[base + slot])) return ARC_ERR_RUNTIME;
            break;
        }

        case OP_STORE_LOCAL: {
            uint16_t slot = read_u16(vm);
            ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
            uint32_t base = vm->frames[vm->fp - 1].base_slot;
            vm->stack[base + slot] = v;
            break;
        }

        case OP_LOAD_GLOBAL: {
            uint16_t idx = read_u16(vm);
            if (idx >= vm->global_count) {
                /* Auto-extend globals to cover this index */
                while (vm->global_count <= idx && vm->global_count < ARC_GLOBALS_MAX)
                    vm->globals[vm->global_count++] = arc_val_null();
            }
            if (idx >= ARC_GLOBALS_MAX) {
                vm_error(vm, "global index %u out of bounds", idx); return ARC_ERR_RUNTIME;
            }
            if (!vm_push(vm, vm->globals[idx])) return ARC_ERR_RUNTIME;
            break;
        }

        case OP_STORE_GLOBAL: {
            uint16_t idx = read_u16(vm);
            ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
            if (idx >= ARC_GLOBALS_MAX) {
                vm_error(vm, "global index %u out of bounds", idx); return ARC_ERR_RUNTIME;
            }
            vm->globals[idx] = v;
            if (idx >= vm->global_count) vm->global_count = idx + 1;
            break;
        }

        case OP_ADD: {
            ArcValue b, a;
            if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
            if (a.tag == VAL_I64 && b.tag == VAL_I64) {
                if (!vm_push(vm, arc_val_i64(a.as.i64 + b.as.i64))) return ARC_ERR_RUNTIME;
            } else if (a.tag == VAL_F64 && b.tag == VAL_F64) {
                if (!vm_push(vm, arc_val_f64(a.as.f64 + b.as.f64))) return ARC_ERR_RUNTIME;
            } else if (ARC_IS_STRING(a) && ARC_IS_STRING(b)) {
                ArcObjString* result = arc_obj_string_concat(&vm->gc,
                    ARC_AS_STRING(a), ARC_AS_STRING(b));
                if (!vm_push(vm, arc_val_obj((ArcObject*)result))) return ARC_ERR_RUNTIME;
            } else {
                vm_error(vm, "add: type mismatch"); return ARC_ERR_RUNTIME;
            }
            break;
        }
        case OP_SUB: BINARY_OP(vm, "sub", -); break;
        case OP_MUL: BINARY_OP(vm, "mul", *); break;
        case OP_DIV: {
            ArcValue b, a;
            if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
            if (a.tag == VAL_I64 && b.tag == VAL_I64) {
                if (b.as.i64 == 0) { vm_error(vm, "division by zero"); return ARC_ERR_RUNTIME; }
                if (!vm_push(vm, arc_val_i64(a.as.i64 / b.as.i64))) return ARC_ERR_RUNTIME;
            } else if (a.tag == VAL_F64 && b.tag == VAL_F64) {
                if (!vm_push(vm, arc_val_f64(a.as.f64 / b.as.f64))) return ARC_ERR_RUNTIME;
            } else {
                vm_error(vm, "div: type mismatch"); return ARC_ERR_RUNTIME;
            }
            break;
        }
        case OP_MOD: {
            ArcValue b, a;
            if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
            if (a.tag != VAL_I64 || b.tag != VAL_I64) {
                vm_error(vm, "mod: requires integers"); return ARC_ERR_RUNTIME;
            }
            if (b.as.i64 == 0) { vm_error(vm, "modulo by zero"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, arc_val_i64(a.as.i64 % b.as.i64))) return ARC_ERR_RUNTIME;
            break;
        }
        case OP_NEG: {
            ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
            if (v.tag == VAL_I64) { if (!vm_push(vm, arc_val_i64(-v.as.i64))) return ARC_ERR_RUNTIME; }
            else if (v.tag == VAL_F64) { if (!vm_push(vm, arc_val_f64(-v.as.f64))) return ARC_ERR_RUNTIME; }
            else { vm_error(vm, "neg: type mismatch"); return ARC_ERR_RUNTIME; }
            break;
        }

        /* --- String ops --- */
        case OP_STR_LEN: {
            ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
            if (!ARC_IS_STRING(v)) { vm_error(vm, "str_len: not a string"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, arc_val_i64(ARC_AS_STRING(v)->len))) return ARC_ERR_RUNTIME;
            break;
        }
        case OP_STR_SLICE: {
            ArcValue vend, vstart, vstr;
            if (!vm_pop(vm, &vend) || !vm_pop(vm, &vstart) || !vm_pop(vm, &vstr)) return ARC_ERR_RUNTIME;
            if (!ARC_IS_STRING(vstr) || vstart.tag != VAL_I64 || vend.tag != VAL_I64) {
                vm_error(vm, "str_slice: type mismatch"); return ARC_ERR_RUNTIME;
            }
            ArcObjString* s = ARC_AS_STRING(vstr);
            int64_t start = vstart.as.i64, end = vend.as.i64;
            if (start < 0) start = 0;
            if (end > (int64_t)s->len) end = s->len;
            if (start >= end) {
                ArcObjString* empty = arc_obj_string_new(&vm->gc, "", 0);
                if (!vm_push(vm, arc_val_obj((ArcObject*)empty))) return ARC_ERR_RUNTIME;
            } else {
                ArcObjString* sub = arc_obj_string_new(&vm->gc, s->data + start, (uint32_t)(end - start));
                if (!vm_push(vm, arc_val_obj((ArcObject*)sub))) return ARC_ERR_RUNTIME;
            }
            break;
        }
        case OP_STR_INDEX: {
            ArcValue vidx, vstr;
            if (!vm_pop(vm, &vidx) || !vm_pop(vm, &vstr)) return ARC_ERR_RUNTIME;
            if (!ARC_IS_STRING(vstr) || vidx.tag != VAL_I64) {
                vm_error(vm, "str_index: type mismatch"); return ARC_ERR_RUNTIME;
            }
            ArcObjString* s = ARC_AS_STRING(vstr);
            int64_t idx = vidx.as.i64;
            if (idx < 0 || idx >= (int64_t)s->len) {
                vm_error(vm, "str_index: index %lld out of bounds (len=%u)", (long long)idx, s->len);
                return ARC_ERR_RUNTIME;
            }
            ArcObjString* ch = arc_obj_string_new(&vm->gc, s->data + idx, 1);
            if (!vm_push(vm, arc_val_obj((ArcObject*)ch))) return ARC_ERR_RUNTIME;
            break;
        }

        /* --- Bitwise ops (i64 only) --- */
        case OP_BIT_AND: {
            ArcValue b, a; if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
            if (a.tag != VAL_I64 || b.tag != VAL_I64) { vm_error(vm, "bit_and: requires integers"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, arc_val_i64(a.as.i64 & b.as.i64))) return ARC_ERR_RUNTIME;
            break;
        }
        case OP_BIT_OR: {
            ArcValue b, a; if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
            if (a.tag != VAL_I64 || b.tag != VAL_I64) { vm_error(vm, "bit_or: requires integers"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, arc_val_i64(a.as.i64 | b.as.i64))) return ARC_ERR_RUNTIME;
            break;
        }
        case OP_BIT_XOR: {
            ArcValue b, a; if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
            if (a.tag != VAL_I64 || b.tag != VAL_I64) { vm_error(vm, "bit_xor: requires integers"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, arc_val_i64(a.as.i64 ^ b.as.i64))) return ARC_ERR_RUNTIME;
            break;
        }
        case OP_BIT_NOT: {
            ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
            if (v.tag != VAL_I64) { vm_error(vm, "bit_not: requires integer"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, arc_val_i64(~v.as.i64))) return ARC_ERR_RUNTIME;
            break;
        }
        case OP_SHL: {
            ArcValue b, a; if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
            if (a.tag != VAL_I64 || b.tag != VAL_I64) { vm_error(vm, "shl: requires integers"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, arc_val_i64(a.as.i64 << (b.as.i64 & 63)))) return ARC_ERR_RUNTIME;
            break;
        }
        case OP_SHR: {
            ArcValue b, a; if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
            if (a.tag != VAL_I64 || b.tag != VAL_I64) { vm_error(vm, "shr: requires integers"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, arc_val_i64((int64_t)((uint64_t)a.as.i64 >> (b.as.i64 & 63))))) return ARC_ERR_RUNTIME;
            break;
        }

        /* --- Type casts --- */
        case OP_CAST_I64: {
            ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
            switch (v.tag) {
            case VAL_I64:  { if (!vm_push(vm, v)) return ARC_ERR_RUNTIME; break; }
            case VAL_F64:  { if (!vm_push(vm, arc_val_i64((int64_t)v.as.f64))) return ARC_ERR_RUNTIME; break; }
            case VAL_BOOL: { if (!vm_push(vm, arc_val_i64(v.as.b ? 1 : 0))) return ARC_ERR_RUNTIME; break; }
            case VAL_NULL: { if (!vm_push(vm, arc_val_i64(0))) return ARC_ERR_RUNTIME; break; }
            case VAL_OBJ:
                if (ARC_IS_STRING(v)) {
                    ArcObjString* s = ARC_AS_STRING(v);
                    char* end; int64_t r = strtoll(s->data, &end, 10);
                    if (end == s->data || *end != '\0') { vm_error(vm, "cast_i64: invalid string"); return ARC_ERR_RUNTIME; }
                    if (!vm_push(vm, arc_val_i64(r))) return ARC_ERR_RUNTIME;
                } else { vm_error(vm, "cast_i64: unsupported type"); return ARC_ERR_RUNTIME; }
                break;
            }
            break;
        }
        case OP_CAST_F64: {
            ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
            switch (v.tag) {
            case VAL_F64:  { if (!vm_push(vm, v)) return ARC_ERR_RUNTIME; break; }
            case VAL_I64:  { if (!vm_push(vm, arc_val_f64((double)v.as.i64))) return ARC_ERR_RUNTIME; break; }
            case VAL_BOOL: { if (!vm_push(vm, arc_val_f64(v.as.b ? 1.0 : 0.0))) return ARC_ERR_RUNTIME; break; }
            case VAL_NULL: { if (!vm_push(vm, arc_val_f64(0.0))) return ARC_ERR_RUNTIME; break; }
            case VAL_OBJ:
                if (ARC_IS_STRING(v)) {
                    ArcObjString* s = ARC_AS_STRING(v);
                    char* end; double r = strtod(s->data, &end);
                    if (end == s->data || *end != '\0') { vm_error(vm, "cast_f64: invalid string"); return ARC_ERR_RUNTIME; }
                    if (!vm_push(vm, arc_val_f64(r))) return ARC_ERR_RUNTIME;
                } else { vm_error(vm, "cast_f64: unsupported type"); return ARC_ERR_RUNTIME; }
                break;
            }
            break;
        }
        case OP_CAST_STR: {
            ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
            char buf[64];
            switch (v.tag) {
            case VAL_NULL: {
                ArcObjString* s = arc_obj_string_new(&vm->gc, "null", 4);
                if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                break;
            }
            case VAL_BOOL: {
                const char* t = v.as.b ? "true" : "false";
                ArcObjString* s = arc_obj_string_new(&vm->gc, t, (uint32_t)strlen(t));
                if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                break;
            }
            case VAL_I64: {
                int n = snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i64);
                ArcObjString* s = arc_obj_string_new(&vm->gc, buf, (uint32_t)n);
                if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                break;
            }
            case VAL_F64: {
                int n = snprintf(buf, sizeof(buf), "%g", v.as.f64);
                ArcObjString* s = arc_obj_string_new(&vm->gc, buf, (uint32_t)n);
                if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                break;
            }
            case VAL_OBJ:
                if (ARC_IS_STRING(v)) {
                    if (!vm_push(vm, v)) return ARC_ERR_RUNTIME;  /* already a string */
                } else {
                    const char* tn = arc_obj_type_name(ARC_OBJ_TYPE(v.as.obj));
                    int n = snprintf(buf, sizeof(buf), "<%s>", tn);
                    ArcObjString* s = arc_obj_string_new(&vm->gc, buf, (uint32_t)n);
                    if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                }
                break;
            }
            break;
        }

        /* --- Collections --- */
        case OP_ARRAY_NEW: {
            uint16_t count = read_u16(vm);
            ArcObjArray* arr = arc_obj_array_new(&vm->gc, count > 0 ? count : 8);
            /* Pop items in reverse, then reverse the array to maintain order */
            for (uint16_t i = 0; i < count; i++) {
                ArcValue item; if (!vm_pop(vm, &item)) return ARC_ERR_RUNTIME;
                arc_obj_array_push(arr, item);
            }
            /* Reverse to restore original order (items were popped LIFO) */
            for (int32_t i = 0, j = arr->count - 1; i < j; i++, j--) {
                ArcValue tmp = arr->items[i]; arr->items[i] = arr->items[j]; arr->items[j] = tmp;
            }
            if (!vm_push(vm, arc_val_obj((ArcObject*)arr))) return ARC_ERR_RUNTIME;
            break;
        }
        case OP_INDEX_GET: {
            ArcValue key, container;
            if (!vm_pop(vm, &key) || !vm_pop(vm, &container)) return ARC_ERR_RUNTIME;
            if (ARC_IS_ARRAY(container)) {
                if (key.tag != VAL_I64) { vm_error(vm, "index_get: array index must be integer"); return ARC_ERR_RUNTIME; }
                ArcObjArray* arr = ARC_AS_ARRAY(container);
                int64_t idx = key.as.i64;
                if (idx < 0 || idx >= arr->count) { vm_error(vm, "index_get: index out of bounds"); return ARC_ERR_RUNTIME; }
                if (!vm_push(vm, arr->items[idx])) return ARC_ERR_RUNTIME;
            } else if (ARC_IS_MAP(container)) {
                ArcObjMap* map = ARC_AS_MAP(container);
                ArcValue val;
                if (arc_obj_map_get(map, key, &val)) {
                    if (!vm_push(vm, val)) return ARC_ERR_RUNTIME;
                } else {
                    if (!vm_push(vm, arc_val_null())) return ARC_ERR_RUNTIME;
                }
            } else if (ARC_IS_STRING(container)) {
                /* String indexing returns single-char string */
                if (key.tag != VAL_I64) { vm_error(vm, "index_get: string index must be integer"); return ARC_ERR_RUNTIME; }
                ArcObjString* s = ARC_AS_STRING(container);
                int64_t idx = key.as.i64;
                if (idx < 0 || idx >= (int64_t)s->len) { vm_error(vm, "index_get: string index out of bounds"); return ARC_ERR_RUNTIME; }
                ArcObjString* ch = arc_obj_string_new(&vm->gc, s->data + idx, 1);
                if (!vm_push(vm, arc_val_obj((ArcObject*)ch))) return ARC_ERR_RUNTIME;
            } else {
                vm_error(vm, "index_get: not indexable"); return ARC_ERR_RUNTIME;
            }
            break;
        }
        case OP_INDEX_SET: {
            ArcValue val, key, container;
            if (!vm_pop(vm, &val) || !vm_pop(vm, &key) || !vm_pop(vm, &container)) return ARC_ERR_RUNTIME;
            if (ARC_IS_ARRAY(container)) {
                if (key.tag != VAL_I64) { vm_error(vm, "index_set: array index must be integer"); return ARC_ERR_RUNTIME; }
                ArcObjArray* arr = ARC_AS_ARRAY(container);
                int64_t idx = key.as.i64;
                if (idx < 0 || idx >= arr->count) { vm_error(vm, "index_set: index out of bounds"); return ARC_ERR_RUNTIME; }
                arr->items[idx] = val;
            } else if (ARC_IS_MAP(container)) {
                arc_obj_map_set(ARC_AS_MAP(container), key, val);
            } else {
                vm_error(vm, "index_set: not indexable"); return ARC_ERR_RUNTIME;
            }
            break;
        }
        case OP_LENGTH: {
            ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
            if (ARC_IS_STRING(v)) {
                if (!vm_push(vm, arc_val_i64(ARC_AS_STRING(v)->len))) return ARC_ERR_RUNTIME;
            } else if (ARC_IS_ARRAY(v)) {
                if (!vm_push(vm, arc_val_i64(ARC_AS_ARRAY(v)->count))) return ARC_ERR_RUNTIME;
            } else if (ARC_IS_MAP(v)) {
                if (!vm_push(vm, arc_val_i64(ARC_AS_MAP(v)->count))) return ARC_ERR_RUNTIME;
            } else {
                vm_error(vm, "length: not a container"); return ARC_ERR_RUNTIME;
            }
            break;
        }
        case OP_MAP_NEW: {
            uint16_t pair_count = read_u16(vm);
            ArcObjMap* map = arc_obj_map_new(&vm->gc, pair_count > 0 ? pair_count : 8);
            /* Pop key-value pairs (value first, then key — reverse order) */
            for (uint16_t i = 0; i < pair_count; i++) {
                ArcValue val, key;
                if (!vm_pop(vm, &val) || !vm_pop(vm, &key)) return ARC_ERR_RUNTIME;
                arc_obj_map_set(map, key, val);
            }
            if (!vm_push(vm, arc_val_obj((ArcObject*)map))) return ARC_ERR_RUNTIME;
            break;
        }

        case OP_EQ: {
            ArcValue b, a;
            if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
            if (!vm_push(vm, arc_val_bool(arc_val_equal(a, b)))) return ARC_ERR_RUNTIME;
            break;
        }
        case OP_NEQ: {
            ArcValue b, a;
            if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
            if (!vm_push(vm, arc_val_bool(!arc_val_equal(a, b)))) return ARC_ERR_RUNTIME;
            break;
        }
        case OP_LT:  COMPARE_OP(vm, "lt", <); break;
        case OP_LE:  COMPARE_OP(vm, "le", <=); break;
        case OP_GT:  COMPARE_OP(vm, "gt", >); break;
        case OP_GE:  COMPARE_OP(vm, "ge", >=); break;

        case OP_NOT: {
            ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
            if (!vm_push(vm, arc_val_bool(!arc_val_is_truthy(v)))) return ARC_ERR_RUNTIME;
            break;
        }

        case OP_JUMP: {
            int32_t offset = read_i32(vm);
            vm->ip = (uint32_t)((int32_t)vm->ip + offset);
            break;
        }
        case OP_JUMP_IF_FALSE: {
            int32_t offset = read_i32(vm);
            ArcValue cond; if (!vm_pop(vm, &cond)) return ARC_ERR_RUNTIME;
            if (!arc_val_is_truthy(cond)) vm->ip = (uint32_t)((int32_t)vm->ip + offset);
            break;
        }
        case OP_JUMP_IF_TRUE: {
            int32_t offset = read_i32(vm);
            ArcValue cond; if (!vm_pop(vm, &cond)) return ARC_ERR_RUNTIME;
            if (arc_val_is_truthy(cond)) vm->ip = (uint32_t)((int32_t)vm->ip + offset);
            break;
        }

        /* --- Exception handling --- */
        case OP_TRY_BEGIN: {
            int32_t offset = read_i32(vm);
            if (vm->handler_count >= ARC_HANDLER_MAX) {
                vm_error(vm, "handler stack overflow"); return ARC_ERR_RUNTIME;
            }
            vm->handlers[vm->handler_count++] = (ArcHandler){
                .catch_ip = (uint32_t)((int32_t)vm->ip + offset),
                .stack_height = vm->sp,
                .frame_depth = vm->fp
            };
            break;
        }
        case OP_TRY_END: {
            if (vm->handler_count == 0) {
                vm_error(vm, "try_end: no handler to pop"); return ARC_ERR_RUNTIME;
            }
            vm->handler_count--;
            break;
        }
        case OP_THROW: {
            ArcValue thrown; if (!vm_pop(vm, &thrown)) return ARC_ERR_RUNTIME;
            if (vm->handler_count == 0) {
                vm_error(vm, "unhandled exception");
                return ARC_ERR_RUNTIME;
            }
            ArcHandler* h = &vm->handlers[--vm->handler_count];
            /* Unwind call stack to handler's frame depth */
            vm->fp = h->frame_depth;
            vm->sp = h->stack_height;
            vm->ip = h->catch_ip;
            /* Push the thrown value as the caught exception */
            if (!vm_push(vm, thrown)) return ARC_ERR_RUNTIME;
            break;
        }

        case OP_CALL: {
            uint16_t func_idx = read_u16(vm);
            uint8_t argc = read_byte(vm);
            (void)read_byte(vm); /* padding byte to fill 4 operand bytes */

            if (func_idx >= vm->image->functions.count) {
                vm_error(vm, "invalid function index %u", func_idx); return ARC_ERR_RUNTIME;
            }
            if (vm->fp >= ARC_FRAMES_MAX) {
                vm_error(vm, "call stack overflow"); return ARC_ERR_RUNTIME;
            }

            const ArcFuncRecord* callee = &vm->image->functions.funcs[func_idx];
            if (argc != callee->arity) {
                vm_error(vm, "arity mismatch: expected %u, got %u", callee->arity, argc);
                return ARC_ERR_RUNTIME;
            }

            /* Arguments are on the stack. Set up new frame. */
            uint32_t base = vm->sp - argc;

            /* Push frame */
            vm->frames[vm->fp++] = (ArcFrame){
                .func_idx = func_idx,
                .return_ip = vm->ip,
                .base_slot = base
            };

            /* Extend stack for additional locals beyond params */
            for (uint16_t i = argc; i < callee->local_count; i++) {
                if (!vm_push(vm, arc_val_null())) return ARC_ERR_RUNTIME;
            }

            vm->ip = callee->code_offset;
            break;
        }

        case OP_RETURN: {
            ArcValue result;
            if (!vm_pop(vm, &result)) return ARC_ERR_RUNTIME;

            vm->fp--;
            if (vm->fp == 0) {
                /* Returning from main */
                vm->sp = 0;
                vm_push(vm, result);
                vm->halted = true;
                break;
            }

            ArcFrame* frame = &vm->frames[vm->fp];
            vm->sp = frame->base_slot;
            vm->ip = frame->return_ip;
            if (!vm_push(vm, result)) return ARC_ERR_RUNTIME;
            break;
        }

        /* --- Closures --- */
        case OP_CLOSURE: {
            uint16_t func_idx = read_u16(vm);
            if (func_idx >= vm->image->functions.count) {
                vm_error(vm, "closure: invalid function index %u", func_idx); return ARC_ERR_RUNTIME;
            }
            ArcObjClosure* cl = arc_obj_closure_new(&vm->gc, func_idx, 0);
            if (!vm_push(vm, arc_val_obj((ArcObject*)cl))) return ARC_ERR_RUNTIME;
            break;
        }
        case OP_GET_UPVAL: {
            (void)read_u16(vm); /* upvalue index — consume operand */
            vm_error(vm, "get_upvalue: not yet fully wired"); return ARC_ERR_RUNTIME;
        }
        case OP_SET_UPVAL: {
            (void)read_u16(vm); /* upvalue index — consume operand */
            vm_error(vm, "set_upvalue: not yet fully wired"); return ARC_ERR_RUNTIME;
        }
        case OP_CLOSE_UPVAL: {
            /* Close the upvalue at the top of stack */
            /* TODO: walk open_upvalues chain, close any pointing at sp-1 */
            break;
        }

        case OP_INTRINSIC: {
            uint16_t id = read_u16(vm);
            uint8_t argc = read_byte(vm);
            (void)read_byte(vm); /* padding */

            switch (id) {
            case ARC_INTRINSIC_PRINT: {
                FILE* out = vm->output ? vm->output : stdout;
                for (uint8_t i = 0; i < argc; i++) {
                    ArcValue v = vm->stack[vm->sp - argc + i];
                    if (i > 0) fprintf(out, " ");
                    arc_val_print(v, out);
                }
                fprintf(out, "\n");
                vm->sp -= argc;
                break;
            }
            case ARC_INTRINSIC_CLOCK: {
                double t = (double)clock() / (double)CLOCKS_PER_SEC;
                if (!vm_push(vm, arc_val_f64(t))) return ARC_ERR_RUNTIME;
                break;
            }
            case ARC_INTRINSIC_TYPE: {
                if (argc != 1) { vm_error(vm, "type: expects 1 argument"); return ARC_ERR_RUNTIME; }
                ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
                const char* tn;
                switch (v.tag) {
                case VAL_NULL: tn = "null"; break;
                case VAL_BOOL: tn = "bool"; break;
                case VAL_I64:  tn = "i64"; break;
                case VAL_F64:  tn = "f64"; break;
                case VAL_OBJ:  tn = arc_obj_type_name(ARC_OBJ_TYPE(v.as.obj)); break;
                default:       tn = "unknown"; break;
                }
                ArcObjString* s = arc_obj_string_new(&vm->gc, tn, (uint32_t)strlen(tn));
                if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                break;
            }
            case ARC_INTRINSIC_ASSERT: {
                if (argc != 1) { vm_error(vm, "assert: expects 1 argument"); return ARC_ERR_RUNTIME; }
                ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
                if (!arc_val_is_truthy(v)) {
                    vm_error(vm, "assertion failed"); return ARC_ERR_RUNTIME;
                }
                if (!vm_push(vm, arc_val_null())) return ARC_ERR_RUNTIME;
                break;
            }
            case ARC_INTRINSIC_TOSTRING: {
                if (argc != 1) { vm_error(vm, "tostring: expects 1 argument"); return ARC_ERR_RUNTIME; }
                ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
                char buf[64];
                switch (v.tag) {
                case VAL_NULL: {
                    ArcObjString* s = arc_obj_string_new(&vm->gc, "null", 4);
                    if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                    break;
                }
                case VAL_BOOL: {
                    const char* t = v.as.b ? "true" : "false";
                    ArcObjString* s = arc_obj_string_new(&vm->gc, t, (uint32_t)strlen(t));
                    if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                    break;
                }
                case VAL_I64: {
                    int n = snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i64);
                    ArcObjString* s = arc_obj_string_new(&vm->gc, buf, (uint32_t)n);
                    if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                    break;
                }
                case VAL_F64: {
                    int n = snprintf(buf, sizeof(buf), "%g", v.as.f64);
                    ArcObjString* s = arc_obj_string_new(&vm->gc, buf, (uint32_t)n);
                    if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                    break;
                }
                case VAL_OBJ:
                    if (ARC_IS_STRING(v)) { if (!vm_push(vm, v)) return ARC_ERR_RUNTIME; }
                    else {
                        const char* t = arc_obj_type_name(ARC_OBJ_TYPE(v.as.obj));
                        int n = snprintf(buf, sizeof(buf), "<%s>", t);
                        ArcObjString* s = arc_obj_string_new(&vm->gc, buf, (uint32_t)n);
                        if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                    }
                    break;
                }
                break;
            }
            case ARC_INTRINSIC_INPUT: {
                /* Read a line from stdin */
                char line[1024];
                if (fgets(line, sizeof(line), stdin)) {
                    size_t len = strlen(line);
                    if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
                    ArcObjString* s = arc_obj_string_new(&vm->gc, line, (uint32_t)len);
                    if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
                } else {
                    if (!vm_push(vm, arc_val_null())) return ARC_ERR_RUNTIME;
                }
                break;
            }
            case ARC_INTRINSIC_LEN: {
                if (argc != 1) { vm_error(vm, "len: expects 1 argument"); return ARC_ERR_RUNTIME; }
                ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
                if (ARC_IS_STRING(v)) {
                    if (!vm_push(vm, arc_val_i64(ARC_AS_STRING(v)->len))) return ARC_ERR_RUNTIME;
                } else if (ARC_IS_ARRAY(v)) {
                    if (!vm_push(vm, arc_val_i64(ARC_AS_ARRAY(v)->count))) return ARC_ERR_RUNTIME;
                } else if (ARC_IS_MAP(v)) {
                    if (!vm_push(vm, arc_val_i64(ARC_AS_MAP(v)->count))) return ARC_ERR_RUNTIME;
                } else {
                    vm_error(vm, "len: not a container"); return ARC_ERR_RUNTIME;
                }
                break;
            }
            case ARC_INTRINSIC_PUSH: {
                if (argc != 2) { vm_error(vm, "push: expects 2 arguments"); return ARC_ERR_RUNTIME; }
                ArcValue val = vm->stack[vm->sp - 1];
                ArcValue arr_v = vm->stack[vm->sp - 2];
                vm->sp -= 2;
                if (!ARC_IS_ARRAY(arr_v)) { vm_error(vm, "push: first arg must be array"); return ARC_ERR_RUNTIME; }
                arc_obj_array_push(ARC_AS_ARRAY(arr_v), val);
                if (!vm_push(vm, arc_val_null())) return ARC_ERR_RUNTIME;
                break;
            }
            case ARC_INTRINSIC_KEYS: {
                if (argc != 1) { vm_error(vm, "keys: expects 1 argument"); return ARC_ERR_RUNTIME; }
                ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
                if (!ARC_IS_MAP(v)) { vm_error(vm, "keys: not a map"); return ARC_ERR_RUNTIME; }
                ArcObjMap* map = ARC_AS_MAP(v);
                ArcObjArray* arr = arc_obj_array_new(&vm->gc, map->count);
                for (int32_t i = 0; i < map->count; i++)
                    arc_obj_array_push(arr, map->keys[i]);
                if (!vm_push(vm, arc_val_obj((ArcObject*)arr))) return ARC_ERR_RUNTIME;
                break;
            }
            default:
                vm_error(vm, "unknown intrinsic %u", id); return ARC_ERR_RUNTIME;
            }
            break;
        }

        case OP_HALT:
            vm->halted = true;
            break;

        default:
            vm_error(vm, "unknown opcode 0x%02X at %u", op, instr_ip);
            return ARC_ERR_RUNTIME;
        }
    }
    return ARC_OK;
}

ArcValue arc_vm_result(const ArcVm* vm) {
    if (vm->sp > 0) return vm->stack[vm->sp - 1];
    return arc_val_null();
}
