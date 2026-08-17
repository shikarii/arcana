#include "mir.h"
#include <stdarg.h>
#include <inttypes.h>

/* ── helpers ─────────────────────────────────────────────────────── */

#define INIT_CAP 4

static void block_free(MirBlock* b) {
    for (uint32_t i = 0; i < b->instr_count; ++i) {
        MirInstr* ins = &b->instrs[i];
        if (ins->kind == MIR_OP_CALL)
            ARC_FREE(ins->as.call.args);
        else if (ins->kind == MIR_OP_INTRINSIC)
            ARC_FREE(ins->as.intrinsic.args);
    }
    ARC_FREE(b->instrs);
}

static void func_free(MirFunction* f) {
    for (uint32_t i = 0; i < f->block_count; ++i)
        block_free(&f->blocks[i]);
    ARC_FREE(f->blocks);
}

/* push a validation error */
static void verr(MirValidation* v, uint32_t fi, uint32_t bi,
                 const char* fmt, ...) {
    if (v->count >= v->cap) {
        v->cap = v->cap < 8 ? 8 : v->cap * 2;
        v->errors = ARC_REALLOC(v->errors, MirError, v->cap);
    }
    MirError* e = &v->errors[v->count++];
    e->func_idx  = fi;
    e->block_idx = bi;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->message, sizeof(e->message), fmt, ap);
    va_end(ap);

    v->valid = false;
}

/* ── module init / free ──────────────────────────────────────────── */

void mir_module_init(MirModule* m) {
    m->functions  = NULL;
    m->func_count = 0;
    m->func_cap   = 0;
}

void mir_module_free(MirModule* m) {
    for (uint32_t i = 0; i < m->func_count; ++i)
        func_free(&m->functions[i]);
    ARC_FREE(m->functions);
    m->functions  = NULL;
    m->func_count = 0;
    m->func_cap   = 0;
}

/* ── construction ────────────────────────────────────────────────── */

MirFunction* mir_module_add_func(MirModule* m) {
    if (m->func_count >= m->func_cap) {
        m->func_cap = m->func_cap < INIT_CAP ? INIT_CAP : m->func_cap * 2;
        m->functions = ARC_REALLOC(m->functions, MirFunction, m->func_cap);
    }
    MirFunction* f = &m->functions[m->func_count++];
    memset(f, 0, sizeof(*f));
    f->entry = MIR_BLOCK_NONE;
    return f;
}

MirBlockId mir_func_add_block(MirFunction* f) {
    if (f->block_count >= f->block_cap) {
        f->block_cap = f->block_cap < INIT_CAP ? INIT_CAP : f->block_cap * 2;
        f->blocks = ARC_REALLOC(f->blocks, MirBlock, f->block_cap);
    }
    MirBlockId id = (MirBlockId)f->block_count;
    MirBlock* b = &f->blocks[f->block_count++];
    memset(b, 0, sizeof(*b));
    b->id       = id;
    b->has_term = false;

    /* first block becomes entry by default */
    if (f->entry == MIR_BLOCK_NONE)
        f->entry = id;

    return id;
}

MirInstr* mir_block_add_instr(MirBlock* b) {
    if (b->instr_count >= b->instr_cap) {
        b->instr_cap = b->instr_cap < INIT_CAP ? INIT_CAP : b->instr_cap * 2;
        b->instrs = ARC_REALLOC(b->instrs, MirInstr, b->instr_cap);
    }
    MirInstr* ins = &b->instrs[b->instr_count++];
    memset(ins, 0, sizeof(*ins));
    ins->dest = MIR_TEMP_NONE;
    return ins;
}

MirTemp mir_func_new_temp(MirFunction* f) {
    return f->temp_count++;
}

/* ── pretty-print ────────────────────────────────────────────────── */

static const char* op_name(MirOpKind k) {
    switch (k) {
        case MIR_OP_CONST_INT:   return "const_int";
        case MIR_OP_CONST_FLOAT: return "const_float";
        case MIR_OP_CONST_BOOL:  return "const_bool";
        case MIR_OP_CONST_NULL:  return "const_null";
        case MIR_OP_ADD:         return "add";
        case MIR_OP_SUB:         return "sub";
        case MIR_OP_MUL:         return "mul";
        case MIR_OP_DIV:         return "div";
        case MIR_OP_MOD:         return "mod";
        case MIR_OP_NEG:         return "neg";
        case MIR_OP_NOT:         return "not";
        case MIR_OP_EQ:          return "eq";
        case MIR_OP_NEQ:         return "neq";
        case MIR_OP_LT:          return "lt";
        case MIR_OP_LE:          return "le";
        case MIR_OP_GT:          return "gt";
        case MIR_OP_GE:          return "ge";
        case MIR_OP_LOAD_LOCAL:  return "load_local";
        case MIR_OP_STORE_LOCAL: return "store_local";
        case MIR_OP_CALL:        return "call";
        case MIR_OP_INTRINSIC:   return "intrinsic";
    }
    return "???";
}

/* --- Dump instruction operands by kind --- */
static void dump_instr_operands(const MirInstr* ins, FILE* out) {
    switch (ins->kind) {
        case MIR_OP_CONST_INT:   fprintf(out, "const_int %" PRId64, ins->as.int_val); break;
        case MIR_OP_CONST_FLOAT: fprintf(out, "const_float %g", ins->as.float_val); break;
        case MIR_OP_CONST_BOOL:  fprintf(out, "const_bool %s", ins->as.bool_val ? "true" : "false"); break;
        case MIR_OP_CONST_NULL:  fprintf(out, "const_null"); break;
        case MIR_OP_ADD: case MIR_OP_SUB: case MIR_OP_MUL:
        case MIR_OP_DIV: case MIR_OP_MOD:
        case MIR_OP_EQ:  case MIR_OP_NEQ:
        case MIR_OP_LT:  case MIR_OP_LE:
        case MIR_OP_GT:  case MIR_OP_GE:
            fprintf(out, "%s t%u t%u", op_name(ins->kind), ins->as.binary.lhs, ins->as.binary.rhs);
            break;
        case MIR_OP_NEG: case MIR_OP_NOT:
            fprintf(out, "%s t%u", op_name(ins->kind), ins->as.unary.operand); break;
        case MIR_OP_LOAD_LOCAL:
            fprintf(out, "load_local %u", ins->as.local_idx); break;
        case MIR_OP_STORE_LOCAL:
            fprintf(out, "store_local %u, t%u", ins->as.store.local_idx, ins->as.store.value); break;
        case MIR_OP_CALL:
            fprintf(out, "call func_%u(", ins->as.call.func_idx);
            for (uint8_t i = 0; i < ins->as.call.argc; ++i) {
                if (i > 0) fprintf(out, ", ");
                fprintf(out, "t%u", ins->as.call.args[i]);
            }
            fprintf(out, ")"); break;
        case MIR_OP_INTRINSIC:
            fprintf(out, "intrinsic %u(", ins->as.intrinsic.intrinsic_id);
            for (uint8_t i = 0; i < ins->as.intrinsic.argc; ++i) {
                if (i > 0) fprintf(out, ", ");
                fprintf(out, "t%u", ins->as.intrinsic.args[i]);
            }
            fprintf(out, ")"); break;
    }
}

static void dump_instr(const MirInstr* ins, FILE* out) {
    fprintf(out, "      ");
    if (ins->dest != MIR_TEMP_NONE)
        fprintf(out, "t%u = ", ins->dest);
    dump_instr_operands(ins, out);
    fprintf(out, "\n");
}

static void dump_term(const MirTerminator* t, FILE* out) {
    fprintf(out, "      ");
    switch (t->kind) {
        case MIR_TERM_GOTO:
            fprintf(out, "goto block_%u", t->as.target);
            break;
        case MIR_TERM_BRANCH:
            fprintf(out, "branch t%u -> block_%u, block_%u",
                    t->as.branch.cond,
                    t->as.branch.then_block,
                    t->as.branch.else_block);
            break;
        case MIR_TERM_RETURN:
            if (t->as.return_val != MIR_TEMP_NONE)
                fprintf(out, "return t%u", t->as.return_val);
            else
                fprintf(out, "return");
            break;
        case MIR_TERM_HALT:
            fprintf(out, "halt");
            break;
    }
    fprintf(out, "\n");
}

void mir_dump_func(const MirFunction* f, FILE* out) {
    fprintf(out, "func %s(arity=%u, locals=%u, temps=%u):\n",
            f->name, f->arity, f->local_count, f->temp_count);

    for (uint32_t i = 0; i < f->block_count; ++i) {
        const MirBlock* b = &f->blocks[i];
        fprintf(out, "  block_%u:\n", b->id);

        for (uint32_t j = 0; j < b->instr_count; ++j)
            dump_instr(&b->instrs[j], out);

        if (b->has_term)
            dump_term(&b->term, out);
    }

    fprintf(out, "\n");
}

void mir_dump(const MirModule* m, FILE* out) {
    for (uint32_t i = 0; i < m->func_count; ++i)
        mir_dump_func(&m->functions[i], out);
}

/* ── validation ──────────────────────────────────────────────────── */

static bool block_id_valid(const MirFunction* f, MirBlockId id) {
    return id < f->block_count;
}

/* --- Validate terminator targets for a single block --- */
static void validate_block_term(const MirFunction* f, const MirBlock* b,
                                 uint32_t fi, uint32_t bi, MirValidation* v) {
    if (!b->has_term) {
        verr(v, fi, bi, "func '%s' block_%u: missing terminator", f->name, b->id);
        return;
    }
    switch (b->term.kind) {
        case MIR_TERM_GOTO:
            if (!block_id_valid(f, b->term.as.target))
                verr(v, fi, bi, "func '%s' block_%u: goto target block_%u out of range",
                     f->name, b->id, b->term.as.target);
            break;
        case MIR_TERM_BRANCH:
            if (!block_id_valid(f, b->term.as.branch.then_block))
                verr(v, fi, bi, "func '%s' block_%u: branch then target block_%u out of range",
                     f->name, b->id, b->term.as.branch.then_block);
            if (!block_id_valid(f, b->term.as.branch.else_block))
                verr(v, fi, bi, "func '%s' block_%u: branch else target block_%u out of range",
                     f->name, b->id, b->term.as.branch.else_block);
            break;
        case MIR_TERM_RETURN:
        case MIR_TERM_HALT:
            break;
    }
}

MirValidation mir_validate(const MirModule* m) {
    MirValidation v;
    memset(&v, 0, sizeof(v));
    v.valid = true;

    for (uint32_t fi = 0; fi < m->func_count; ++fi) {
        const MirFunction* f = &m->functions[fi];

        if (f->block_count == 0) {
            verr(&v, fi, 0, "func '%s': no blocks", f->name);
            continue;
        }
        if (!block_id_valid(f, f->entry))
            verr(&v, fi, 0, "func '%s': entry block %u out of range", f->name, f->entry);

        for (uint32_t i = 0; i < f->block_count; ++i) {
            for (uint32_t j = i + 1; j < f->block_count; ++j) {
                if (f->blocks[i].id == f->blocks[j].id)
                    verr(&v, fi, i, "func '%s': duplicate block ID %u at indices %u and %u",
                         f->name, f->blocks[i].id, i, j);
            }
        }

        for (uint32_t bi = 0; bi < f->block_count; ++bi)
            validate_block_term(f, &f->blocks[bi], fi, bi, &v);
    }

    return v;
}

void mir_validation_free(MirValidation* v) {
    ARC_FREE(v->errors);
    v->errors = NULL;
    v->count  = 0;
    v->cap    = 0;
    v->valid  = true;
}
