/*
 * vm_ops.h — Internal header shared between vm.c and vm_ops.c.
 *
 * Exposes:
 *   1. Stack / read helpers (inline — used by both TUs)
 *   2. Opcode group handlers called from the dispatch loop
 *
 * NOT part of the public API — consumers include vm.h only.
 */

#ifndef ARCANA_VM_OPS_H
#define ARCANA_VM_OPS_H

#include "vm.h"
#include "../runtime/object.h"
#include <stdarg.h>

/* ---- Error reporting (defined in vm.c, used by vm_ops.c) ---- */
void vm_error(ArcVm* vm, const char* fmt, ...);

/* ---- Stack helpers ---- */

static inline bool vm_push(ArcVm* vm, ArcValue v) {
    if (vm->sp >= ARC_STACK_MAX) { vm_error(vm, "stack overflow"); return false; }
    vm->stack[vm->sp++] = v;
    return true;
}

static inline bool vm_pop(ArcVm* vm, ArcValue* v) {
    if (vm->sp == 0) { vm_error(vm, "stack underflow"); return false; }
    *v = vm->stack[--vm->sp];
    return true;
}

/* ---- Bytecode read helpers ---- */

static inline uint8_t vm_read_byte(ArcVm* vm) {
    return vm->image->code[vm->ip++];
}

static inline uint16_t vm_read_u16(ArcVm* vm) {
    uint16_t v = arc_read_u16(vm->image->code + vm->ip);
    vm->ip += 2;
    return v;
}

static inline int32_t vm_read_i32(ArcVm* vm) {
    int32_t v = arc_read_i32(vm->image->code + vm->ip);
    vm->ip += 4;
    return v;
}

/* ---- Constant helper (defined in vm.c) ---- */
ArcValue vm_const_to_val(ArcGC* gc, const ArcConstant* c);

/* ---- Opcode group handlers (defined in vm_ops.c) ---- */

ArcStatus vm_exec_arithmetic(ArcVm* vm, uint8_t op);
ArcStatus vm_exec_comparison(ArcVm* vm, uint8_t op);
ArcStatus vm_exec_string_ops(ArcVm* vm, uint8_t op);
ArcStatus vm_exec_bitwise(ArcVm* vm, uint8_t op);
ArcStatus vm_exec_cast(ArcVm* vm, uint8_t op);
ArcStatus vm_exec_collections(ArcVm* vm, uint8_t op);
ArcStatus vm_exec_closure(ArcVm* vm, uint8_t op);
ArcStatus vm_exec_exception(ArcVm* vm, uint8_t op);
ArcStatus vm_exec_intrinsic(ArcVm* vm);

#endif /* ARCANA_VM_OPS_H */
