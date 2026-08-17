/*
 * vm_ops.c — Extracted opcode handler groups for the Arcana VM.
 *
 * Each function handles a group of related opcodes and returns ARC_OK
 * to continue or ARC_ERR_RUNTIME on error.  The callers in vm.c
 * propagate errors to the main dispatch loop.
 */

#include "vm_ops.h"
#include "../runtime/object.h"
#include <time.h>

/* ---- Arithmetic: ADD, SUB, MUL, DIV, MOD, NEG ---- */

ArcStatus vm_exec_arithmetic(ArcVm* vm, uint8_t op) {
    ArcValue b, a;
    switch (op) {
    case OP_ADD:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag == VAL_I64 && b.tag == VAL_I64) {
            if (!vm_push(vm, arc_val_i64(a.as.i64 + b.as.i64))) return ARC_ERR_RUNTIME;
        } else if (a.tag == VAL_F64 && b.tag == VAL_F64) {
            if (!vm_push(vm, arc_val_f64(a.as.f64 + b.as.f64))) return ARC_ERR_RUNTIME;
        } else if (ARC_IS_STRING(a) && ARC_IS_STRING(b)) {
            ArcObjString* r = arc_obj_string_concat(&vm->gc, ARC_AS_STRING(a), ARC_AS_STRING(b));
            if (!vm_push(vm, arc_val_obj((ArcObject*)r))) return ARC_ERR_RUNTIME;
        } else { vm_error(vm, "add: type mismatch"); return ARC_ERR_RUNTIME; }
        return ARC_OK;
    case OP_SUB:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag == VAL_I64 && b.tag == VAL_I64) {
            if (!vm_push(vm, arc_val_i64(a.as.i64 - b.as.i64))) return ARC_ERR_RUNTIME;
        } else if (a.tag == VAL_F64 && b.tag == VAL_F64) {
            if (!vm_push(vm, arc_val_f64(a.as.f64 - b.as.f64))) return ARC_ERR_RUNTIME;
        } else { vm_error(vm, "sub: type mismatch"); return ARC_ERR_RUNTIME; }
        return ARC_OK;
    case OP_MUL:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag == VAL_I64 && b.tag == VAL_I64) {
            if (!vm_push(vm, arc_val_i64(a.as.i64 * b.as.i64))) return ARC_ERR_RUNTIME;
        } else if (a.tag == VAL_F64 && b.tag == VAL_F64) {
            if (!vm_push(vm, arc_val_f64(a.as.f64 * b.as.f64))) return ARC_ERR_RUNTIME;
        } else { vm_error(vm, "mul: type mismatch"); return ARC_ERR_RUNTIME; }
        return ARC_OK;
    case OP_DIV:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag == VAL_I64 && b.tag == VAL_I64) {
            if (b.as.i64 == 0) { vm_error(vm, "division by zero"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, arc_val_i64(a.as.i64 / b.as.i64))) return ARC_ERR_RUNTIME;
        } else if (a.tag == VAL_F64 && b.tag == VAL_F64) {
            if (!vm_push(vm, arc_val_f64(a.as.f64 / b.as.f64))) return ARC_ERR_RUNTIME;
        } else { vm_error(vm, "div: type mismatch"); return ARC_ERR_RUNTIME; }
        return ARC_OK;
    case OP_MOD:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag != VAL_I64 || b.tag != VAL_I64) { vm_error(vm, "mod: requires integers"); return ARC_ERR_RUNTIME; }
        if (b.as.i64 == 0) { vm_error(vm, "modulo by zero"); return ARC_ERR_RUNTIME; }
        if (!vm_push(vm, arc_val_i64(a.as.i64 % b.as.i64))) return ARC_ERR_RUNTIME;
        return ARC_OK;
    case OP_NEG: {
        ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
        if (v.tag == VAL_I64) { if (!vm_push(vm, arc_val_i64(-v.as.i64))) return ARC_ERR_RUNTIME; }
        else if (v.tag == VAL_F64) { if (!vm_push(vm, arc_val_f64(-v.as.f64))) return ARC_ERR_RUNTIME; }
        else { vm_error(vm, "neg: type mismatch"); return ARC_ERR_RUNTIME; }
        return ARC_OK;
    }
    default: vm_error(vm, "bad arith op"); return ARC_ERR_RUNTIME;
    }
}

/* ---- Comparison: EQ, NEQ, LT, LE, GT, GE, NOT ---- */

ArcStatus vm_exec_comparison(ArcVm* vm, uint8_t op) {
    ArcValue b, a;
    switch (op) {
    case OP_EQ:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (!vm_push(vm, arc_val_bool(arc_val_equal(a, b)))) return ARC_ERR_RUNTIME;
        return ARC_OK;
    case OP_NEQ:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (!vm_push(vm, arc_val_bool(!arc_val_equal(a, b)))) return ARC_ERR_RUNTIME;
        return ARC_OK;
    case OP_LT:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag == VAL_I64 && b.tag == VAL_I64) {
            if (!vm_push(vm, arc_val_bool(a.as.i64 < b.as.i64))) return ARC_ERR_RUNTIME;
        } else if (a.tag == VAL_F64 && b.tag == VAL_F64) {
            if (!vm_push(vm, arc_val_bool(a.as.f64 < b.as.f64))) return ARC_ERR_RUNTIME;
        } else { vm_error(vm, "lt: type mismatch"); return ARC_ERR_RUNTIME; }
        return ARC_OK;
    case OP_LE:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag == VAL_I64 && b.tag == VAL_I64) {
            if (!vm_push(vm, arc_val_bool(a.as.i64 <= b.as.i64))) return ARC_ERR_RUNTIME;
        } else if (a.tag == VAL_F64 && b.tag == VAL_F64) {
            if (!vm_push(vm, arc_val_bool(a.as.f64 <= b.as.f64))) return ARC_ERR_RUNTIME;
        } else { vm_error(vm, "le: type mismatch"); return ARC_ERR_RUNTIME; }
        return ARC_OK;
    case OP_GT:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag == VAL_I64 && b.tag == VAL_I64) {
            if (!vm_push(vm, arc_val_bool(a.as.i64 > b.as.i64))) return ARC_ERR_RUNTIME;
        } else if (a.tag == VAL_F64 && b.tag == VAL_F64) {
            if (!vm_push(vm, arc_val_bool(a.as.f64 > b.as.f64))) return ARC_ERR_RUNTIME;
        } else { vm_error(vm, "gt: type mismatch"); return ARC_ERR_RUNTIME; }
        return ARC_OK;
    case OP_GE:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag == VAL_I64 && b.tag == VAL_I64) {
            if (!vm_push(vm, arc_val_bool(a.as.i64 >= b.as.i64))) return ARC_ERR_RUNTIME;
        } else if (a.tag == VAL_F64 && b.tag == VAL_F64) {
            if (!vm_push(vm, arc_val_bool(a.as.f64 >= b.as.f64))) return ARC_ERR_RUNTIME;
        } else { vm_error(vm, "ge: type mismatch"); return ARC_ERR_RUNTIME; }
        return ARC_OK;
    case OP_NOT: {
        ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
        if (!vm_push(vm, arc_val_bool(!arc_val_is_truthy(v)))) return ARC_ERR_RUNTIME;
        return ARC_OK;
    }
    default: vm_error(vm, "bad cmp op"); return ARC_ERR_RUNTIME;
    }
}

/* ---- String ops: STR_LEN, STR_SLICE, STR_INDEX ---- */

ArcStatus vm_exec_string_ops(ArcVm* vm, uint8_t op) {
    switch (op) {
    case OP_STR_LEN: {
        ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
        if (!ARC_IS_STRING(v)) { vm_error(vm, "str_len: not a string"); return ARC_ERR_RUNTIME; }
        if (!vm_push(vm, arc_val_i64(ARC_AS_STRING(v)->len))) return ARC_ERR_RUNTIME;
        return ARC_OK;
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
        return ARC_OK;
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
        return ARC_OK;
    }
    default: vm_error(vm, "bad string op"); return ARC_ERR_RUNTIME;
    }
}

/* ---- Bitwise: BIT_AND, BIT_OR, BIT_XOR, BIT_NOT, SHL, SHR ---- */

ArcStatus vm_exec_bitwise(ArcVm* vm, uint8_t op) {
    ArcValue b, a;
    switch (op) {
    case OP_BIT_AND:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag != VAL_I64 || b.tag != VAL_I64) { vm_error(vm, "bit_and: requires integers"); return ARC_ERR_RUNTIME; }
        if (!vm_push(vm, arc_val_i64(a.as.i64 & b.as.i64))) return ARC_ERR_RUNTIME;
        return ARC_OK;
    case OP_BIT_OR:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag != VAL_I64 || b.tag != VAL_I64) { vm_error(vm, "bit_or: requires integers"); return ARC_ERR_RUNTIME; }
        if (!vm_push(vm, arc_val_i64(a.as.i64 | b.as.i64))) return ARC_ERR_RUNTIME;
        return ARC_OK;
    case OP_BIT_XOR:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag != VAL_I64 || b.tag != VAL_I64) { vm_error(vm, "bit_xor: requires integers"); return ARC_ERR_RUNTIME; }
        if (!vm_push(vm, arc_val_i64(a.as.i64 ^ b.as.i64))) return ARC_ERR_RUNTIME;
        return ARC_OK;
    case OP_BIT_NOT: {
        ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
        if (v.tag != VAL_I64) { vm_error(vm, "bit_not: requires integer"); return ARC_ERR_RUNTIME; }
        if (!vm_push(vm, arc_val_i64(~v.as.i64))) return ARC_ERR_RUNTIME;
        return ARC_OK;
    }
    case OP_SHL:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag != VAL_I64 || b.tag != VAL_I64) { vm_error(vm, "shl: requires integers"); return ARC_ERR_RUNTIME; }
        if (!vm_push(vm, arc_val_i64(a.as.i64 << (b.as.i64 & 63)))) return ARC_ERR_RUNTIME;
        return ARC_OK;
    case OP_SHR:
        if (!vm_pop(vm, &b) || !vm_pop(vm, &a)) return ARC_ERR_RUNTIME;
        if (a.tag != VAL_I64 || b.tag != VAL_I64) { vm_error(vm, "shr: requires integers"); return ARC_ERR_RUNTIME; }
        if (!vm_push(vm, arc_val_i64((int64_t)((uint64_t)a.as.i64 >> (b.as.i64 & 63))))) return ARC_ERR_RUNTIME;
        return ARC_OK;
    default: vm_error(vm, "bad bit op"); return ARC_ERR_RUNTIME;
    }
}

/* ---- Collections: ARRAY_NEW, INDEX_GET, INDEX_SET, LENGTH, MAP_NEW ---- */

static ArcStatus vm_exec_array_new(ArcVm* vm) {
    uint16_t count = vm_read_u16(vm);
    ArcObjArray* arr = arc_obj_array_new(&vm->gc, count > 0 ? count : 8);
    for (uint16_t i = 0; i < count; i++) {
        ArcValue item; if (!vm_pop(vm, &item)) return ARC_ERR_RUNTIME;
        arc_obj_array_push(arr, item);
    }
    for (int32_t i = 0, j = arr->count - 1; i < j; i++, j--) {
        ArcValue tmp = arr->items[i]; arr->items[i] = arr->items[j]; arr->items[j] = tmp;
    }
    if (!vm_push(vm, arc_val_obj((ArcObject*)arr))) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

static ArcStatus vm_exec_index_get(ArcVm* vm) {
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
        if (arc_obj_map_get(map, key, &val)) { if (!vm_push(vm, val)) return ARC_ERR_RUNTIME; }
        else { if (!vm_push(vm, arc_val_null())) return ARC_ERR_RUNTIME; }
    } else if (ARC_IS_STRING(container)) {
        if (key.tag != VAL_I64) { vm_error(vm, "index_get: string index must be integer"); return ARC_ERR_RUNTIME; }
        ArcObjString* s = ARC_AS_STRING(container);
        int64_t idx = key.as.i64;
        if (idx < 0 || idx >= (int64_t)s->len) { vm_error(vm, "index_get: string index out of bounds"); return ARC_ERR_RUNTIME; }
        ArcObjString* ch = arc_obj_string_new(&vm->gc, s->data + idx, 1);
        if (!vm_push(vm, arc_val_obj((ArcObject*)ch))) return ARC_ERR_RUNTIME;
    } else { vm_error(vm, "index_get: not indexable"); return ARC_ERR_RUNTIME; }
    return ARC_OK;
}

static ArcStatus vm_exec_index_set(ArcVm* vm) {
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
    } else { vm_error(vm, "index_set: not indexable"); return ARC_ERR_RUNTIME; }
    return ARC_OK;
}

static ArcStatus vm_exec_length(ArcVm* vm) {
    ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
    if (ARC_IS_STRING(v)) { if (!vm_push(vm, arc_val_i64(ARC_AS_STRING(v)->len))) return ARC_ERR_RUNTIME; }
    else if (ARC_IS_ARRAY(v)) { if (!vm_push(vm, arc_val_i64(ARC_AS_ARRAY(v)->count))) return ARC_ERR_RUNTIME; }
    else if (ARC_IS_MAP(v)) { if (!vm_push(vm, arc_val_i64(ARC_AS_MAP(v)->count))) return ARC_ERR_RUNTIME; }
    else { vm_error(vm, "length: not a container"); return ARC_ERR_RUNTIME; }
    return ARC_OK;
}

static ArcStatus vm_exec_map_new(ArcVm* vm) {
    uint16_t pair_count = vm_read_u16(vm);
    ArcObjMap* map = arc_obj_map_new(&vm->gc, pair_count > 0 ? pair_count : 8);
    for (uint16_t i = 0; i < pair_count; i++) {
        ArcValue val, key;
        if (!vm_pop(vm, &val) || !vm_pop(vm, &key)) return ARC_ERR_RUNTIME;
        arc_obj_map_set(map, key, val);
    }
    if (!vm_push(vm, arc_val_obj((ArcObject*)map))) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

ArcStatus vm_exec_collections(ArcVm* vm, uint8_t op) {
    switch (op) {
    case OP_ARRAY_NEW: return vm_exec_array_new(vm);
    case OP_INDEX_GET: return vm_exec_index_get(vm);
    case OP_INDEX_SET: return vm_exec_index_set(vm);
    case OP_LENGTH:    return vm_exec_length(vm);
    case OP_MAP_NEW:   return vm_exec_map_new(vm);
    default: vm_error(vm, "bad collection op"); return ARC_ERR_RUNTIME;
    }
}

/* ---- Closures: CLOSURE, GET_UPVAL, SET_UPVAL, CLOSE_UPVAL ---- */

static ArcStatus vm_exec_closure_new(ArcVm* vm) {
    uint16_t func_idx = vm_read_u16(vm);
    if (func_idx >= vm->image->functions.count) {
        vm_error(vm, "closure: invalid function index %u", func_idx); return ARC_ERR_RUNTIME;
    }
    const ArcFuncRecord* fn = &vm->image->functions.funcs[func_idx];
    ArcObjClosure* cl = arc_obj_closure_new(&vm->gc, func_idx, fn->upvalue_count);
    for (uint8_t i = 0; i < fn->upvalue_count; i++) {
        const ArcUpvalueDesc* desc = &fn->upvalues[i];
        if (desc->is_local) {
            uint32_t base = vm->frames[vm->fp - 1].base_slot;
            ArcValue* slot = &vm->stack[base + desc->index];
            ArcObjUpvalue* uv = vm->open_upvalues;
            while (uv && uv->location != slot) uv = uv->next;
            if (!uv) {
                uv = arc_obj_upvalue_new(&vm->gc, slot);
                uv->next = vm->open_upvalues;
                vm->open_upvalues = uv;
            }
            cl->upvalues[i] = uv;
        } else {
            ArcFrame* frame = &vm->frames[vm->fp - 1];
            ArcValue callee_v = vm->stack[frame->base_slot > 0 ? frame->base_slot - 1 : 0];
            if (ARC_IS_CLOSURE(callee_v)) {
                ArcObjClosure* encl = ARC_AS_CLOSURE(callee_v);
                if (desc->index < encl->upvalue_count) cl->upvalues[i] = encl->upvalues[desc->index];
            }
        }
    }
    if (!vm_push(vm, arc_val_obj((ArcObject*)cl))) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

static ArcStatus vm_exec_get_upval(ArcVm* vm) {
    uint16_t idx = vm_read_u16(vm);
    ArcFrame* frame = &vm->frames[vm->fp - 1];
    ArcValue callee_v = vm->stack[frame->base_slot > 0 ? frame->base_slot - 1 : 0];
    if (!ARC_IS_CLOSURE(callee_v)) { vm_error(vm, "get_upvalue: not in a closure"); return ARC_ERR_RUNTIME; }
    ArcObjClosure* cl = ARC_AS_CLOSURE(callee_v);
    if (idx >= cl->upvalue_count || !cl->upvalues[idx]) {
        vm_error(vm, "get_upvalue: invalid index %u", idx); return ARC_ERR_RUNTIME;
    }
    if (!vm_push(vm, *cl->upvalues[idx]->location)) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

static ArcStatus vm_exec_set_upval(ArcVm* vm) {
    uint16_t idx = vm_read_u16(vm);
    ArcValue val; if (!vm_pop(vm, &val)) return ARC_ERR_RUNTIME;
    ArcFrame* frame = &vm->frames[vm->fp - 1];
    ArcValue callee_v = vm->stack[frame->base_slot > 0 ? frame->base_slot - 1 : 0];
    if (!ARC_IS_CLOSURE(callee_v)) { vm_error(vm, "set_upvalue: not in a closure"); return ARC_ERR_RUNTIME; }
    ArcObjClosure* cl = ARC_AS_CLOSURE(callee_v);
    if (idx >= cl->upvalue_count || !cl->upvalues[idx]) {
        vm_error(vm, "set_upvalue: invalid index %u", idx); return ARC_ERR_RUNTIME;
    }
    *cl->upvalues[idx]->location = val;
    return ARC_OK;
}

static ArcStatus vm_exec_close_upval(ArcVm* vm) {
    if (vm->sp == 0) return ARC_OK;
    ArcValue* slot = &vm->stack[vm->sp - 1];
    ArcObjUpvalue** pp = &vm->open_upvalues;
    while (*pp) {
        if ((*pp)->location >= slot) {
            ArcObjUpvalue* uv = *pp;
            uv->closed = *uv->location;
            uv->location = &uv->closed;
            *pp = uv->next;
        } else { pp = &(*pp)->next; }
    }
    return ARC_OK;
}

ArcStatus vm_exec_closure(ArcVm* vm, uint8_t op) {
    switch (op) {
    case OP_CLOSURE:    return vm_exec_closure_new(vm);
    case OP_GET_UPVAL:  return vm_exec_get_upval(vm);
    case OP_SET_UPVAL:  return vm_exec_set_upval(vm);
    case OP_CLOSE_UPVAL:return vm_exec_close_upval(vm);
    default: vm_error(vm, "bad closure op"); return ARC_ERR_RUNTIME;
    }
}

/* ---- Intrinsics: nested switch on intrinsic IDs ---- */

static ArcStatus vm_intr_print(ArcVm* vm, uint8_t argc) {
    FILE* out = vm->output ? vm->output : stdout;
    for (uint8_t i = 0; i < argc; i++) {
        ArcValue v = vm->stack[vm->sp - argc + i];
        if (i > 0) fprintf(out, " ");
        arc_val_print(v, out);
    }
    fprintf(out, "\n");
    vm->sp -= argc;
    return ARC_OK;
}

static ArcStatus vm_intr_type(ArcVm* vm) {
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
    return ARC_OK;
}

static ArcStatus vm_intr_tostring(ArcVm* vm) {
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
    return ARC_OK;
}

static ArcStatus vm_intr_len(ArcVm* vm) {
    ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
    if (ARC_IS_STRING(v)) { if (!vm_push(vm, arc_val_i64(ARC_AS_STRING(v)->len))) return ARC_ERR_RUNTIME; }
    else if (ARC_IS_ARRAY(v)) { if (!vm_push(vm, arc_val_i64(ARC_AS_ARRAY(v)->count))) return ARC_ERR_RUNTIME; }
    else if (ARC_IS_MAP(v)) { if (!vm_push(vm, arc_val_i64(ARC_AS_MAP(v)->count))) return ARC_ERR_RUNTIME; }
    else { vm_error(vm, "len: not a container"); return ARC_ERR_RUNTIME; }
    return ARC_OK;
}

static ArcStatus vm_intr_push(ArcVm* vm) {
    ArcValue val = vm->stack[vm->sp - 1];
    ArcValue arr_v = vm->stack[vm->sp - 2];
    vm->sp -= 2;
    if (!ARC_IS_ARRAY(arr_v)) { vm_error(vm, "push: first arg must be array"); return ARC_ERR_RUNTIME; }
    arc_obj_array_push(ARC_AS_ARRAY(arr_v), val);
    if (!vm_push(vm, arc_val_null())) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

static ArcStatus vm_intr_keys(ArcVm* vm) {
    ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
    if (!ARC_IS_MAP(v)) { vm_error(vm, "keys: not a map"); return ARC_ERR_RUNTIME; }
    ArcObjMap* map = ARC_AS_MAP(v);
    ArcObjArray* arr = arc_obj_array_new(&vm->gc, map->count);
    for (int32_t i = 0; i < map->count; i++)
        arc_obj_array_push(arr, map->keys[i]);
    if (!vm_push(vm, arc_val_obj((ArcObject*)arr))) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

ArcStatus vm_exec_intrinsic(ArcVm* vm) {
    uint16_t id = vm_read_u16(vm);
    uint8_t argc = vm_read_byte(vm);
    (void)vm_read_byte(vm); /* padding */
    switch (id) {
    case ARC_INTRINSIC_PRINT: return vm_intr_print(vm, argc);
    case ARC_INTRINSIC_CLOCK: {
        double t = (double)clock() / (double)CLOCKS_PER_SEC;
        if (!vm_push(vm, arc_val_f64(t))) return ARC_ERR_RUNTIME;
        return ARC_OK;
    }
    case ARC_INTRINSIC_TYPE:
        if (argc != 1) { vm_error(vm, "type: expects 1 argument"); return ARC_ERR_RUNTIME; }
        return vm_intr_type(vm);
    case ARC_INTRINSIC_ASSERT: {
        if (argc != 1) { vm_error(vm, "assert: expects 1 argument"); return ARC_ERR_RUNTIME; }
        ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
        if (!arc_val_is_truthy(v)) { vm_error(vm, "assertion failed"); return ARC_ERR_RUNTIME; }
        if (!vm_push(vm, arc_val_null())) return ARC_ERR_RUNTIME;
        return ARC_OK;
    }
    case ARC_INTRINSIC_TOSTRING:
        if (argc != 1) { vm_error(vm, "tostring: expects 1 argument"); return ARC_ERR_RUNTIME; }
        return vm_intr_tostring(vm);
    case ARC_INTRINSIC_INPUT: {
        char line[1024];
        if (fgets(line, sizeof(line), stdin)) {
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
            ArcObjString* s = arc_obj_string_new(&vm->gc, line, (uint32_t)len);
            if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
        } else { if (!vm_push(vm, arc_val_null())) return ARC_ERR_RUNTIME; }
        return ARC_OK;
    }
    case ARC_INTRINSIC_LEN:
        if (argc != 1) { vm_error(vm, "len: expects 1 argument"); return ARC_ERR_RUNTIME; }
        return vm_intr_len(vm);
    case ARC_INTRINSIC_PUSH:
        if (argc != 2) { vm_error(vm, "push: expects 2 arguments"); return ARC_ERR_RUNTIME; }
        return vm_intr_push(vm);
    case ARC_INTRINSIC_KEYS:
        if (argc != 1) { vm_error(vm, "keys: expects 1 argument"); return ARC_ERR_RUNTIME; }
        return vm_intr_keys(vm);
    default: vm_error(vm, "unknown intrinsic %u", id); return ARC_ERR_RUNTIME;
    }
}
