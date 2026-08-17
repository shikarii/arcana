#include "disassembler.h"
#include "opcodes.h"

static void print_const(const ArcConstPool* pool, uint16_t idx, FILE* out) {
    if (idx >= pool->count) { fprintf(out, "???"); return; }
    const ArcConstant* c = &pool->entries[idx];
    switch (c->tag) {
    case ARC_CONST_NULL:   fprintf(out, "null"); break;
    case ARC_CONST_BOOL:   fprintf(out, "%s", c->as.b ? "true" : "false"); break;
    case ARC_CONST_I64:    fprintf(out, "%lld", (long long)c->as.i64); break;
    case ARC_CONST_F64:    fprintf(out, "%g", c->as.f64); break;
    case ARC_CONST_STRING: fprintf(out, "\"%s\"", c->as.str.data); break;
    }
}

/* --- Print operand details for a single instruction --- */
static void print_operand(const ArcBytecodeImage* img, uint8_t op,
                           uint32_t ip, FILE* out) {
    switch (op) {
    case OP_CONST: {
        uint16_t idx = arc_read_u16(img->code + ip);
        fprintf(out, "#%u  ; ", idx);
        print_const(&img->constants, idx, out);
        break;
    }
    case OP_LOAD_LOCAL: case OP_STORE_LOCAL:
        fprintf(out, "%u", arc_read_u16(img->code + ip)); break;
    case OP_LOAD_GLOBAL: case OP_STORE_GLOBAL:
        fprintf(out, "%u", arc_read_u16(img->code + ip)); break;
    case OP_JUMP: case OP_JUMP_IF_FALSE: case OP_JUMP_IF_TRUE: {
        int32_t offset = arc_read_i32(img->code + ip);
        uint32_t target = (uint32_t)((int32_t)(ip + 4) + offset);
        fprintf(out, "%+d -> %u", offset, target);
        break;
    }
    case OP_CALL: {
        uint16_t fi = arc_read_u16(img->code + ip);
        uint8_t argc = img->code[ip + 2];
        fprintf(out, "func=%u argc=%u", fi, argc);
        break;
    }
    case OP_INTRINSIC: {
        uint16_t id = arc_read_u16(img->code + ip);
        uint8_t argc = img->code[ip + 2];
        const char* iname = "???";
        if (id == ARC_INTRINSIC_PRINT) iname = "print";
        else if (id == ARC_INTRINSIC_CLOCK) iname = "clock";
        fprintf(out, "%s argc=%u", iname, argc);
        break;
    }
    default: break;
    }
}

void arc_disassemble_func(const ArcBytecodeImage* img, uint16_t func_idx, FILE* out) {
    const ArcFuncRecord* fn = &img->functions.funcs[func_idx];
    uint16_t ni = fn->name_const_idx;
    const char* name = "???";
    if (ni < img->constants.count && img->constants.entries[ni].tag == ARC_CONST_STRING)
        name = img->constants.entries[ni].as.str.data;

    fprintf(out, ".func %s arity=%u locals=%u max_stack=%u\n",
            name, fn->arity, fn->local_count, fn->max_stack);

    uint32_t ip = fn->code_offset;
    uint32_t end = fn->code_offset + fn->code_length;

    while (ip < end) {
        uint32_t start = ip;
        uint8_t op = img->code[ip++];
        int ob = arc_op_operand_bytes(op);
        if (ob < 0) {
            fprintf(out, "  %04u  %02X            ???\n", start, op);
            break;
        }
        fprintf(out, "  %04u  %02X", start, op);
        for (int i = 0; i < ob; i++) fprintf(out, " %02X", img->code[ip + i]);
        for (int i = ob; i < 4; i++) fprintf(out, "   ");
        fprintf(out, "  %-16s", arc_op_mnemonic(op));
        print_operand(img, op, ip, out);
        fprintf(out, "\n");
        ip += (uint32_t)ob;
    }
    fprintf(out, ".end\n\n");
}

void arc_disassemble(const ArcBytecodeImage* img, FILE* out) {
    fprintf(out, "; Arcana bytecode v%d.%d\n", ARC_VERSION_MAJOR, ARC_VERSION_MINOR);
    fprintf(out, "; constants: %u, functions: %u, code: %u bytes\n\n",
            img->constants.count, img->functions.count, img->code_len);

    /* Print constant pool */
    for (uint16_t i = 0; i < img->constants.count; i++) {
        fprintf(out, ".const #%u  ", i);
        print_const(&img->constants, i, out);
        fprintf(out, "\n");
    }
    if (img->constants.count > 0) fprintf(out, "\n");

    /* Print each function */
    for (uint16_t i = 0; i < img->functions.count; i++)
        arc_disassemble_func(img, i, out);
}
