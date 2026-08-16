#include "verifier.h"
#include <stdarg.h>

static void verr(ArcVerifyResult* r, uint16_t fi, uint32_t off, const char* fmt, ...) {
    if (r->error_count >= ARC_VERIFY_MAX_ERRORS) return;
    ArcVerifyError* e = &r->errors[r->error_count++];
    e->func_idx = fi;
    e->offset = off;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->message, sizeof(e->message), fmt, ap);
    va_end(ap);
    r->valid = false;
}

ArcVerifyResult arc_verify(const ArcBytecodeImage* img) {
    ArcVerifyResult result = { .valid = true, .error_count = 0 };

    if (img->functions.count == 0) {
        verr(&result, 0, 0, "no functions defined");
        return result;
    }

    for (uint16_t fi = 0; fi < img->functions.count; fi++) {
        const ArcFuncRecord* fn = &img->functions.funcs[fi];

        /* Check code bounds */
        if (fn->code_offset + fn->code_length > img->code_len) {
            verr(&result, fi, 0, "function code extends beyond code section");
            continue;
        }

        /* Walk instructions and simulate stack height */
        uint32_t ip = fn->code_offset;
        uint32_t end = fn->code_offset + fn->code_length;
        int stack_height = 0;
        int max_stack = 0;
        bool has_halt_or_return = false;

        while (ip < end) {
            uint32_t instr_start = ip;
            uint8_t op = img->code[ip++];
            int op_bytes = arc_op_operand_bytes(op);

            if (op_bytes < 0) {
                verr(&result, fi, instr_start, "unknown opcode 0x%02X", op);
                break;
            }

            if (ip + (uint32_t)op_bytes > end) {
                verr(&result, fi, instr_start, "instruction truncated");
                break;
            }

            /* Validate operands */
            switch (op) {
            case OP_CONST: {
                uint16_t idx = arc_read_u16(img->code + ip);
                if (idx >= img->constants.count)
                    verr(&result, fi, instr_start, "invalid constant index %u", idx);
                break;
            }
            case OP_LOAD_LOCAL:
            case OP_STORE_LOCAL: {
                uint16_t slot = arc_read_u16(img->code + ip);
                if (slot >= fn->local_count)
                    verr(&result, fi, instr_start, "local slot %u >= local_count %u", slot, fn->local_count);
                break;
            }
            case OP_CALL:
            case OP_INTRINSIC: {
                uint16_t idx = arc_read_u16(img->code + ip);
                if (op == OP_CALL && idx >= img->functions.count)
                    verr(&result, fi, instr_start, "invalid function index %u", idx);
                if (op == OP_INTRINSIC && idx >= ARC_INTRINSIC_COUNT)
                    verr(&result, fi, instr_start, "invalid intrinsic id %u", idx);
                break;
            }
            case OP_JUMP:
            case OP_JUMP_IF_FALSE:
            case OP_JUMP_IF_TRUE: {
                int32_t offset = arc_read_i32(img->code + ip);
                uint32_t target = (uint32_t)((int32_t)(ip + 4) + offset);
                if (target < fn->code_offset || target > end)
                    verr(&result, fi, instr_start, "jump target %u out of function bounds [%u,%u]",
                         target, fn->code_offset, end);
                break;
            }
            default: break;
            }

            /* Simulate stack effects */
            int pops = arc_op_pops(op);
            int pushes = arc_op_pushes(op);

            if (pops >= 0) {
                if (pops == -1) {
                    /* Variable: CALL/INTRINSIC pops argc values */
                    uint8_t argc = img->code[ip + 2];
                    stack_height -= argc;
                } else {
                    stack_height -= pops;
                }
            }
            if (stack_height < 0) {
                verr(&result, fi, instr_start, "stack underflow (height %d)", stack_height);
                stack_height = 0;
            }
            if (pushes >= 0) stack_height += pushes;
            if (stack_height > max_stack) max_stack = stack_height;

            if (op == OP_HALT || op == OP_RETURN) has_halt_or_return = true;

            ip += (uint32_t)op_bytes;
        }

        if (!has_halt_or_return)
            verr(&result, fi, fn->code_offset, "function has no halt or return");
    }

    return result;
}
