/*
 * vm.c — Arcana virtual machine: init, destroy, dispatch loop.
 *
 * Opcode handler groups live in vm_ops.c to keep each file under
 * the 600-line / 60-line-per-function project limits.
 */

#include "vm_ops.h"
#include <stdarg.h>

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

void vm_error(ArcVm* vm, const char* fmt, ...) {
    vm->error.code = ARC_ERR_RUNTIME;
    vm->error.ip = vm->ip;
    vm->error.func_idx = vm->fp > 0 ? vm->frames[vm->fp - 1].func_idx : 0;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(vm->error.message, sizeof(vm->error.message), fmt, ap);
    va_end(ap);
}

ArcValue vm_const_to_val(ArcGC* gc, const ArcConstant* c) {
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

static ArcStatus vm_exec_call(ArcVm* vm) {
    uint16_t func_idx = vm_read_u16(vm);
    uint8_t argc = vm_read_byte(vm);
    (void)vm_read_byte(vm);
    if (func_idx == 0xFFFF && vm->sp > argc) {
        ArcValue callee_v = vm->stack[vm->sp - argc - 1];
        if (ARC_IS_CLOSURE(callee_v)) func_idx = ARC_AS_CLOSURE(callee_v)->func_idx;
    }
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
    uint32_t base = vm->sp - argc;
    vm->frames[vm->fp++] = (ArcFrame){
        .func_idx = func_idx, .return_ip = vm->ip, .base_slot = base
    };
    for (uint16_t i = argc; i < callee->local_count; i++)
        if (!vm_push(vm, arc_val_null())) return ARC_ERR_RUNTIME;
    vm->ip = callee->code_offset;
    return ARC_OK;
}

static ArcStatus vm_exec_return(ArcVm* vm) {
    ArcValue result;
    if (!vm_pop(vm, &result)) return ARC_ERR_RUNTIME;
    uint32_t base = vm->frames[vm->fp - 1].base_slot;
    ArcObjUpvalue** pp = &vm->open_upvalues;
    while (*pp) {
        if ((*pp)->location >= &vm->stack[base]) {
            ArcObjUpvalue* uv = *pp;
            uv->closed = *uv->location;
            uv->location = &uv->closed;
            *pp = uv->next;
        } else { pp = &(*pp)->next; }
    }
    vm->fp--;
    if (vm->fp == 0) {
        vm->sp = 0;
        vm_push(vm, result);
        vm->halted = true;
        return ARC_OK;
    }
    ArcFrame* frame = &vm->frames[vm->fp];
    vm->sp = frame->base_slot;
    vm->ip = frame->return_ip;
    if (!vm_push(vm, result)) return ARC_ERR_RUNTIME;
    return ARC_OK;
}

ArcStatus vm_exec_exception(ArcVm* vm, uint8_t op) {
    switch (op) {
    case OP_TRY_BEGIN: {
        int32_t offset = vm_read_i32(vm);
        if (vm->handler_count >= ARC_HANDLER_MAX) {
            vm_error(vm, "handler stack overflow"); return ARC_ERR_RUNTIME;
        }
        vm->handlers[vm->handler_count++] = (ArcHandler){
            .catch_ip = (uint32_t)((int32_t)vm->ip + offset),
            .stack_height = vm->sp, .frame_depth = vm->fp
        };
        return ARC_OK;
    }
    case OP_TRY_END:
        if (vm->handler_count == 0) { vm_error(vm, "try_end: no handler to pop"); return ARC_ERR_RUNTIME; }
        vm->handler_count--;
        return ARC_OK;
    case OP_THROW: {
        ArcValue thrown; if (!vm_pop(vm, &thrown)) return ARC_ERR_RUNTIME;
        if (vm->handler_count == 0) { vm_error(vm, "unhandled exception"); return ARC_ERR_RUNTIME; }
        ArcHandler* h = &vm->handlers[--vm->handler_count];
        vm->fp = h->frame_depth;
        vm->sp = h->stack_height;
        vm->ip = h->catch_ip;
        if (!vm_push(vm, thrown)) return ARC_ERR_RUNTIME;
        return ARC_OK;
    }
    default: vm_error(vm, "bad exception op"); return ARC_ERR_RUNTIME;
    }
}

static ArcStatus vm_exec_cast_i64(ArcVm* vm) {
    ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
    switch (v.tag) {
    case VAL_I64:  if (!vm_push(vm, v)) return ARC_ERR_RUNTIME; break;
    case VAL_F64:  if (!vm_push(vm, arc_val_i64((int64_t)v.as.f64))) return ARC_ERR_RUNTIME; break;
    case VAL_BOOL: if (!vm_push(vm, arc_val_i64(v.as.b ? 1 : 0))) return ARC_ERR_RUNTIME; break;
    case VAL_NULL: if (!vm_push(vm, arc_val_i64(0))) return ARC_ERR_RUNTIME; break;
    case VAL_OBJ:
        if (ARC_IS_STRING(v)) {
            ArcObjString* s = ARC_AS_STRING(v);
            char* end; int64_t r = strtoll(s->data, &end, 10);
            if (end == s->data || *end != '\0') { vm_error(vm, "cast_i64: invalid string"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, arc_val_i64(r))) return ARC_ERR_RUNTIME;
        } else { vm_error(vm, "cast_i64: unsupported type"); return ARC_ERR_RUNTIME; }
        break;
    }
    return ARC_OK;
}

static ArcStatus vm_exec_cast_f64(ArcVm* vm) {
    ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
    switch (v.tag) {
    case VAL_F64:  if (!vm_push(vm, v)) return ARC_ERR_RUNTIME; break;
    case VAL_I64:  if (!vm_push(vm, arc_val_f64((double)v.as.i64))) return ARC_ERR_RUNTIME; break;
    case VAL_BOOL: if (!vm_push(vm, arc_val_f64(v.as.b ? 1.0 : 0.0))) return ARC_ERR_RUNTIME; break;
    case VAL_NULL: if (!vm_push(vm, arc_val_f64(0.0))) return ARC_ERR_RUNTIME; break;
    case VAL_OBJ:
        if (ARC_IS_STRING(v)) {
            ArcObjString* s = ARC_AS_STRING(v);
            char* end; double r = strtod(s->data, &end);
            if (end == s->data || *end != '\0') { vm_error(vm, "cast_f64: invalid string"); return ARC_ERR_RUNTIME; }
            if (!vm_push(vm, arc_val_f64(r))) return ARC_ERR_RUNTIME;
        } else { vm_error(vm, "cast_f64: unsupported type"); return ARC_ERR_RUNTIME; }
        break;
    }
    return ARC_OK;
}

static ArcStatus vm_exec_cast_str(ArcVm* vm) {
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
            if (!vm_push(vm, v)) return ARC_ERR_RUNTIME;
        } else {
            const char* tn = arc_obj_type_name(ARC_OBJ_TYPE(v.as.obj));
            int n = snprintf(buf, sizeof(buf), "<%s>", tn);
            ArcObjString* s = arc_obj_string_new(&vm->gc, buf, (uint32_t)n);
            if (!vm_push(vm, arc_val_obj((ArcObject*)s))) return ARC_ERR_RUNTIME;
        }
        break;
    }
    return ARC_OK;
}

ArcStatus vm_exec_cast(ArcVm* vm, uint8_t op) {
    switch (op) {
    case OP_CAST_I64: return vm_exec_cast_i64(vm);
    case OP_CAST_F64: return vm_exec_cast_f64(vm);
    case OP_CAST_STR: return vm_exec_cast_str(vm);
    default: vm_error(vm, "bad cast op"); return ARC_ERR_RUNTIME;
    }
}

static ArcStatus vm_exec_stack_local_global(ArcVm* vm, uint8_t op) {
    switch (op) {
    case OP_CONST: {
        uint16_t idx = vm_read_u16(vm);
        if (idx >= vm->image->constants.count) {
            vm_error(vm, "invalid constant index %u", idx); return ARC_ERR_RUNTIME;
        }
        if (!vm_push(vm, vm_const_to_val(&vm->gc, &vm->image->constants.entries[idx])))
            return ARC_ERR_RUNTIME;
        return ARC_OK;
    }
    case OP_POP: { ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME; return ARC_OK; }
    case OP_DUP:
        if (vm->sp == 0) { vm_error(vm, "dup: empty stack"); return ARC_ERR_RUNTIME; }
        if (!vm_push(vm, vm->stack[vm->sp - 1])) return ARC_ERR_RUNTIME;
        return ARC_OK;
    case OP_LOAD_LOCAL: {
        uint16_t slot = vm_read_u16(vm);
        uint32_t base = vm->frames[vm->fp - 1].base_slot;
        if (!vm_push(vm, vm->stack[base + slot])) return ARC_ERR_RUNTIME;
        return ARC_OK;
    }
    case OP_STORE_LOCAL: {
        uint16_t slot = vm_read_u16(vm);
        ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
        vm->stack[vm->frames[vm->fp - 1].base_slot + slot] = v;
        return ARC_OK;
    }
    case OP_LOAD_GLOBAL: {
        uint16_t idx = vm_read_u16(vm);
        if (idx >= vm->global_count) {
            while (vm->global_count <= idx && vm->global_count < ARC_GLOBALS_MAX)
                vm->globals[vm->global_count++] = arc_val_null();
        }
        if (idx >= ARC_GLOBALS_MAX) {
            vm_error(vm, "global index %u out of bounds", idx); return ARC_ERR_RUNTIME;
        }
        if (!vm_push(vm, vm->globals[idx])) return ARC_ERR_RUNTIME;
        return ARC_OK;
    }
    case OP_STORE_GLOBAL: {
        uint16_t idx = vm_read_u16(vm);
        ArcValue v; if (!vm_pop(vm, &v)) return ARC_ERR_RUNTIME;
        if (idx >= ARC_GLOBALS_MAX) {
            vm_error(vm, "global index %u out of bounds", idx); return ARC_ERR_RUNTIME;
        }
        vm->globals[idx] = v;
        if (idx >= vm->global_count) vm->global_count = idx + 1;
        return ARC_OK;
    }
    default: vm_error(vm, "bad stack/local/global op"); return ARC_ERR_RUNTIME;
    }
}

static ArcStatus vm_exec_branch(ArcVm* vm, uint8_t op) {
    int32_t offset = vm_read_i32(vm);
    switch (op) {
    case OP_JUMP:
        vm->ip = (uint32_t)((int32_t)vm->ip + offset); return ARC_OK;
    case OP_JUMP_IF_FALSE: {
        ArcValue cond; if (!vm_pop(vm, &cond)) return ARC_ERR_RUNTIME;
        if (!arc_val_is_truthy(cond)) vm->ip = (uint32_t)((int32_t)vm->ip + offset);
        return ARC_OK;
    }
    case OP_JUMP_IF_TRUE: {
        ArcValue cond; if (!vm_pop(vm, &cond)) return ARC_ERR_RUNTIME;
        if (arc_val_is_truthy(cond)) vm->ip = (uint32_t)((int32_t)vm->ip + offset);
        return ARC_OK;
    }
    default: vm_error(vm, "bad branch op"); return ARC_ERR_RUNTIME;
    }
}

static ArcStatus vm_setup_main(ArcVm* vm) {
    if (vm->image->functions.count == 0) {
        vm_error(vm, "no functions"); return ARC_ERR_RUNTIME;
    }
    const ArcFuncRecord* main_fn = &vm->image->functions.funcs[0];
    vm->frames[0] = (ArcFrame){ .func_idx = 0, .return_ip = 0, .base_slot = 0 };
    vm->fp = 1;
    vm->ip = main_fn->code_offset;
    vm->sp = main_fn->local_count;
    for (uint16_t i = 0; i < main_fn->local_count; i++)
        vm->stack[i] = arc_val_null();
    return ARC_OK;
}

static ArcStatus vm_dispatch(ArcVm* vm, uint8_t op, uint32_t instr_ip) {
    switch (op) {
    case OP_CONST: case OP_POP: case OP_DUP:
    case OP_LOAD_LOCAL: case OP_STORE_LOCAL:
    case OP_LOAD_GLOBAL: case OP_STORE_GLOBAL:
        return vm_exec_stack_local_global(vm, op);
    case OP_ADD: case OP_SUB: case OP_MUL:
    case OP_DIV: case OP_MOD: case OP_NEG:
        return vm_exec_arithmetic(vm, op);
    case OP_STR_LEN: case OP_STR_SLICE: case OP_STR_INDEX:
        return vm_exec_string_ops(vm, op);
    case OP_BIT_AND: case OP_BIT_OR: case OP_BIT_XOR:
    case OP_BIT_NOT: case OP_SHL: case OP_SHR:
        return vm_exec_bitwise(vm, op);
    case OP_CAST_I64: case OP_CAST_F64: case OP_CAST_STR:
        return vm_exec_cast(vm, op);
    case OP_EQ: case OP_NEQ: case OP_LT:
    case OP_LE: case OP_GT: case OP_GE: case OP_NOT:
        return vm_exec_comparison(vm, op);
    case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_JUMP_IF_TRUE:
        return vm_exec_branch(vm, op);
    case OP_TRY_BEGIN: case OP_TRY_END: case OP_THROW:
        return vm_exec_exception(vm, op);
    case OP_CALL:   return vm_exec_call(vm);
    case OP_RETURN: return vm_exec_return(vm);
    case OP_CLOSURE: case OP_GET_UPVAL:
    case OP_SET_UPVAL: case OP_CLOSE_UPVAL:
        return vm_exec_closure(vm, op);
    case OP_ARRAY_NEW: case OP_INDEX_GET: case OP_INDEX_SET:
    case OP_LENGTH: case OP_MAP_NEW:
        return vm_exec_collections(vm, op);
    case OP_INTRINSIC: return vm_exec_intrinsic(vm);
    case OP_HALT: vm->halted = true; return ARC_OK;
    default:
        vm_error(vm, "unknown opcode 0x%02X at %u", op, instr_ip);
        return ARC_ERR_RUNTIME;
    }
}

ArcStatus arc_vm_run(ArcVm* vm) {
    ArcStatus st = vm_setup_main(vm);
    if (st != ARC_OK) return st;
    while (!vm->halted) {
        if (vm->ip >= vm->image->code_len) {
            vm_error(vm, "ip out of bounds"); return ARC_ERR_RUNTIME;
        }
        uint32_t instr_ip = vm->ip;
        uint8_t op = vm_read_byte(vm);
        if (vm->trace) {
            fprintf(vm->output ? vm->output : stdout,
                    "  [%04u] %-16s sp=%u\n", instr_ip, arc_op_mnemonic(op), vm->sp);
        }
        st = vm_dispatch(vm, op, instr_ip);
        if (st != ARC_OK) return st;
    }
    return ARC_OK;
}

ArcValue arc_vm_result(const ArcVm* vm) {
    if (vm->sp > 0) return vm->stack[vm->sp - 1];
    return arc_val_null();
}
