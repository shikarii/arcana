#include "vm.h"
#include <stdarg.h>

void arc_vm_init(ArcVm* vm, const ArcBytecodeImage* image) {
    memset(vm, 0, sizeof(*vm));
    vm->image = image;
    vm->output = stdout;
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

static inline ArcValue vm_peek(ArcVm* vm) {
    return vm->stack[vm->sp - 1];
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

static ArcValue const_to_val(const ArcConstant* c) {
    switch (c->tag) {
    case ARC_CONST_NULL:   return arc_val_null();
    case ARC_CONST_BOOL:   return arc_val_bool(c->as.b);
    case ARC_CONST_I64:    return arc_val_i64(c->as.i64);
    case ARC_CONST_F64:    return arc_val_f64(c->as.f64);
    case ARC_CONST_STRING: return arc_val_null(); /* strings not yet runtime values */
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
            if (!vm_push(vm, const_to_val(&vm->image->constants.entries[idx])))
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

        case OP_ADD: BINARY_OP(vm, "add", +); break;
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
