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

#define MAX_CODE_LEN 65536

/* --- Validate operands for a single instruction --- */
static void verify_operands(ArcVerifyResult* result, const ArcBytecodeImage* img,
                             const ArcFuncRecord* fn, uint16_t fi,
                             uint32_t instr_start, uint8_t op, uint32_t ip) {
    switch (op) {
    case OP_CONST: {
        uint16_t idx = arc_read_u16(img->code + ip);
        if (idx >= img->constants.count)
            verr(result, fi, instr_start, "invalid constant index %u", idx);
        break;
    }
    case OP_LOAD_LOCAL:
    case OP_STORE_LOCAL: {
        uint16_t slot = arc_read_u16(img->code + ip);
        if (slot >= fn->local_count)
            verr(result, fi, instr_start, "local slot %u >= local_count %u", slot, fn->local_count);
        break;
    }
    case OP_CALL:
    case OP_THREAD_SPAWN:
    case OP_INTRINSIC: {
        uint16_t idx = arc_read_u16(img->code + ip);
        if ((op == OP_CALL || op == OP_THREAD_SPAWN) && idx >= img->functions.count)
            verr(result, fi, instr_start, "invalid function index %u", idx);
        if (op == OP_INTRINSIC && idx >= ARC_INTRINSIC_COUNT)
            verr(result, fi, instr_start, "invalid intrinsic id %u", idx);
        break;
    }
    case OP_JUMP:
    case OP_JUMP_IF_FALSE:
    case OP_JUMP_IF_TRUE: {
        int32_t offset = arc_read_i32(img->code + ip);
        uint32_t target = (uint32_t)((int32_t)(ip + 4) + offset);
        uint32_t end = fn->code_offset + fn->code_length;
        if (target < fn->code_offset || target > end)
            verr(result, fi, instr_start, "jump target %u out of function bounds [%u,%u]",
                 target, fn->code_offset, end);
        break;
    }
    default: break;
    }
}

/* --- Record expected stack height at jump targets --- */
static void record_jump_heights(ArcVerifyResult* result, const ArcFuncRecord* fn,
                                 const ArcBytecodeImage* img, uint16_t fi,
                                 int16_t* height_at, uint8_t op,
                                 uint32_t ip, int stack_height,
                                 int op_bytes, uint32_t instr_start,
                                 int* stack_out) {
    int post_height = stack_height;
    if (op == OP_JUMP || op == OP_JUMP_IF_FALSE || op == OP_JUMP_IF_TRUE) {
        int32_t offset = arc_read_i32(img->code + ip);
        uint32_t target = (uint32_t)((int32_t)(ip + 4) + offset);
        uint32_t end = fn->code_offset + fn->code_length;
        if (target >= fn->code_offset && target < end) {
            uint32_t rel_target = target - fn->code_offset;
            if (height_at[rel_target] == -2) {
                height_at[rel_target] = (int16_t)post_height;
            } else if (height_at[rel_target] != post_height) {
                verr(result, fi, instr_start,
                     "stack height mismatch at jump target %u: expected %d, got %d",
                     target, height_at[rel_target], post_height);
            }
        }
    }

    if (op == OP_JUMP || op == OP_HALT || op == OP_RETURN) {
        uint32_t next_rel = (ip + (uint32_t)op_bytes) - fn->code_offset;
        if (next_rel < fn->code_length && height_at[next_rel] != -2)
            *stack_out = height_at[next_rel];
        else if (next_rel < fn->code_length)
            *stack_out = 0;
        else
            *stack_out = stack_height;
    } else {
        *stack_out = stack_height;
    }
}

/* --- Verify a single function --- */
static void verify_function(ArcVerifyResult* result, const ArcBytecodeImage* img,
                              uint16_t fi) {
    const ArcFuncRecord* fn = &img->functions.funcs[fi];

    if (fn->code_offset + fn->code_length > img->code_len) {
        verr(result, fi, 0, "function code extends beyond code section");
        return;
    }

    int16_t* height_at = NULL;
    if (fn->code_length > 0 && fn->code_length <= MAX_CODE_LEN) {
        height_at = (int16_t*)calloc(fn->code_length, sizeof(int16_t));
        for (uint32_t i = 0; i < fn->code_length; i++) height_at[i] = -2;
    }

    uint32_t ip = fn->code_offset;
    uint32_t end = fn->code_offset + fn->code_length;
    int stack_height = 0;
    bool has_halt_or_return = false;

    while (ip < end) {
        uint32_t instr_start = ip;
        uint32_t rel_ip = ip - fn->code_offset;

        /* Check stack consistency at join points */
        if (height_at && rel_ip < fn->code_length) {
            if (height_at[rel_ip] == -2) {
                height_at[rel_ip] = (int16_t)stack_height;
            } else if (height_at[rel_ip] != stack_height) {
                verr(result, fi, instr_start,
                     "stack height mismatch at join: expected %d, got %d",
                     height_at[rel_ip], stack_height);
                stack_height = height_at[rel_ip];
            }
        }

        uint8_t op = img->code[ip++];
        int op_bytes = arc_op_operand_bytes(op);
        if (op_bytes < 0) {
            verr(result, fi, instr_start, "unknown opcode 0x%02X", op);
            break;
        }
        if (ip + (uint32_t)op_bytes > end) {
            verr(result, fi, instr_start, "instruction truncated");
            break;
        }

        verify_operands(result, img, fn, fi, instr_start, op, ip);

        /* Simulate stack effects */
        int pops = arc_op_pops(op);
        int pushes = arc_op_pushes(op);
        if (pops >= 0) {
            if (pops == -1) {
                uint8_t argc = img->code[ip + 2];
                stack_height -= argc;
            } else {
                stack_height -= pops;
            }
        }
        if (stack_height < 0) {
            verr(result, fi, instr_start, "stack underflow (height %d)", stack_height);
            stack_height = 0;
        }
        if (pushes >= 0) stack_height += pushes;
        if (op == OP_HALT || op == OP_RETURN) has_halt_or_return = true;

        if (height_at)
            record_jump_heights(result, fn, img, fi, height_at, op,
                                ip, stack_height, op_bytes, instr_start, &stack_height);

        ip += (uint32_t)op_bytes;
    }

    if (!has_halt_or_return)
        verr(result, fi, fn->code_offset, "function has no halt or return");

    free(height_at);
}

ArcVerifyResult arc_verify(const ArcBytecodeImage* img) {
    ArcVerifyResult result = { .valid = true, .error_count = 0 };

    if (img->functions.count == 0) {
        verr(&result, 0, 0, "no functions defined");
        return result;
    }

    for (uint16_t fi = 0; fi < img->functions.count; fi++)
        verify_function(&result, img, fi);

    return result;
}
