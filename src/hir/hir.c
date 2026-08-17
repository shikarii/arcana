/*
 * hir.c -- HIR (High-level Intermediate Representation) implementation
 *
 * Provides allocation, pretty-printing, validation, and cleanup for the
 * Arcana HIR tree structure defined in hir.h.
 */

#include "hir.h"

#include <inttypes.h>   /* PRId64 */
#include <stdarg.h>     /* va_list, va_start, va_end */

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static void hir_block_init(HirBlock* b) {
    b->stmts = NULL;
    b->count = 0;
    b->cap   = 0;
}

/* Forward declarations for recursive freeing */
static void hir_block_free(HirBlock* b);

static bool hir_expr_is_binary(HirExprKind k) {
    switch (k) {
        case HIR_ADD: case HIR_SUB: case HIR_MUL: case HIR_DIV: case HIR_MOD:
        case HIR_EQ:  case HIR_NEQ: case HIR_LT:  case HIR_LE:
        case HIR_GT:  case HIR_GE:
            return true;
        default:
            return false;
    }
}

static bool hir_expr_is_unary(HirExprKind k) {
    return k == HIR_NEG || k == HIR_NOT;
}

/* ------------------------------------------------------------------ */
/*  Recursive freeing                                                   */
/* ------------------------------------------------------------------ */

void hir_expr_free(HirExpr* e) {
    if (!e) return;

    if (hir_expr_is_binary(e->kind)) {
        hir_expr_free(e->as.binary.lhs);
        hir_expr_free(e->as.binary.rhs);
    } else if (hir_expr_is_unary(e->kind)) {
        hir_expr_free(e->as.unary.operand);
    } else if (e->kind == HIR_CALL) {
        for (uint8_t i = 0; i < e->as.call.argc; i++) {
            hir_expr_free(e->as.call.args[i]);
        }
        ARC_FREE(e->as.call.args);
    } else if (e->kind == HIR_INTRINSIC) {
        for (uint8_t i = 0; i < e->as.intrinsic.argc; i++) {
            hir_expr_free(e->as.intrinsic.args[i]);
        }
        ARC_FREE(e->as.intrinsic.args);
    }
    /* CONST_INT, CONST_FLOAT, CONST_BOOL, CONST_NULL, VAR_LOAD, POISON: no children */

    ARC_FREE(e);
}

static void hir_stmt_free(HirStmt* s) {
    switch (s->kind) {
        case HIR_STMT_EXPR:
            hir_expr_free(s->as.expr);
            break;
        case HIR_STMT_LET:
            hir_expr_free(s->as.let.value);
            break;
        case HIR_STMT_ASSIGN:
            hir_expr_free(s->as.assign.value);
            break;
        case HIR_STMT_RETURN:
            hir_expr_free(s->as.ret.value);
            break;
        case HIR_STMT_IF:
            hir_expr_free(s->as.if_stmt.cond);
            hir_block_free(&s->as.if_stmt.then_block);
            hir_block_free(&s->as.if_stmt.else_block);
            break;
        case HIR_STMT_WHILE:
            hir_expr_free(s->as.while_stmt.cond);
            hir_block_free(&s->as.while_stmt.body);
            break;
        case HIR_STMT_PRINT:
            hir_expr_free(s->as.print.value);
            break;
    }
}

static void hir_block_free(HirBlock* b) {
    for (uint32_t i = 0; i < b->count; i++) {
        hir_stmt_free(&b->stmts[i]);
    }
    ARC_FREE(b->stmts);
    b->stmts = NULL;
    b->count = 0;
    b->cap   = 0;
}

/* ------------------------------------------------------------------ */
/*  Module init / free                                                  */
/* ------------------------------------------------------------------ */

void hir_module_init(HirModule* m) {
    m->functions  = NULL;
    m->func_count = 0;
    m->func_cap   = 0;
    hir_block_init(&m->main_body);
    m->output_node = ARC_INVALID_ID;
}

void hir_module_free(HirModule* m) {
    for (uint32_t i = 0; i < m->func_count; i++) {
        hir_block_free(&m->functions[i].body);
    }
    ARC_FREE(m->functions);
    hir_block_free(&m->main_body);

    m->functions  = NULL;
    m->func_count = 0;
    m->func_cap   = 0;
}

/* ------------------------------------------------------------------ */
/*  Allocation helpers                                                  */
/* ------------------------------------------------------------------ */

HirExpr* hir_expr_new(HirExprKind kind, ArcElementId src) {
    HirExpr* e = ARC_ALLOC(HirExpr, 1);
    assert(e && "hir_expr_new: allocation failed");
    e->kind      = kind;
    e->source_id = src;
    /* calloc zeroes the union, so pointers are NULL, ints are 0 */
    return e;
}

HirStmt* hir_block_add(HirBlock* b, HirStmtKind kind, ArcElementId src) {
    if (b->count >= b->cap) {
        b->cap = b->cap < 8 ? 8 : b->cap * 2;
        b->stmts = ARC_REALLOC(b->stmts, HirStmt, b->cap);
        assert(b->stmts && "hir_block_add: allocation failed");
        /* Zero the newly allocated region */
        memset(&b->stmts[b->count], 0,
               (b->cap - b->count) * sizeof(HirStmt));
    }
    HirStmt* s = &b->stmts[b->count++];
    s->kind      = kind;
    s->source_id = src;
    return s;
}

/* ------------------------------------------------------------------ */
/*  Pretty-print                                                        */
/* ------------------------------------------------------------------ */

static void hir_print_indent(FILE* out, int indent) {
    for (int i = 0; i < indent; i++) {
        fputc(' ', out);
    }
}

static const char* hir_expr_kind_name(HirExprKind k) {
    switch (k) {
        case HIR_CONST_INT:   return "const_int";
        case HIR_CONST_FLOAT: return "const_float";
        case HIR_CONST_BOOL:  return "const_bool";
        case HIR_CONST_NULL:  return "const_null";
        case HIR_ADD: return "add"; case HIR_SUB: return "sub";
        case HIR_MUL: return "mul"; case HIR_DIV: return "div";
        case HIR_MOD: return "mod";
        case HIR_NEG: return "neg"; case HIR_NOT: return "not";
        case HIR_EQ:  return "eq";  case HIR_NEQ: return "neq";
        case HIR_LT:  return "lt";  case HIR_LE:  return "le";
        case HIR_GT:  return "gt";  case HIR_GE:  return "ge";
        case HIR_VAR_LOAD:   return "var";
        case HIR_CALL:       return "call";
        case HIR_INTRINSIC:  return "intrinsic";
        case HIR_POISON:     return "poison";
        default:             return "?unknown?";
    }
}

/* --- Dump a literal, var_load, or poison leaf expression --- */
static void hir_dump_leaf(const HirExpr* e, FILE* out) {
    switch (e->kind) {
        case HIR_CONST_INT:   fprintf(out, "(const_int %" PRId64 ")", e->as.int_val); break;
        case HIR_CONST_FLOAT: fprintf(out, "(const_float %g)", e->as.float_val); break;
        case HIR_CONST_BOOL:  fprintf(out, "(const_bool %s)", e->as.bool_val ? "true" : "false"); break;
        case HIR_CONST_NULL:  fprintf(out, "(const_null)"); break;
        case HIR_VAR_LOAD:    fprintf(out, "v%u", (unsigned)e->as.var_idx); break;
        case HIR_POISON:      fprintf(out, "<poison>"); break;
        default:              fprintf(out, "(?%s?)", hir_expr_kind_name(e->kind)); break;
    }
}

/* --- Dump a call or intrinsic expression with arguments --- */
static void hir_dump_call(const HirExpr* e, FILE* out, int indent) {
    if (e->kind == HIR_CALL) {
        fprintf(out, "(call f%u", (unsigned)e->as.call.func_idx);
        for (uint8_t i = 0; i < e->as.call.argc; i++) {
            fputc(' ', out);
            hir_dump_expr(e->as.call.args[i], out, indent);
        }
    } else {
        fprintf(out, "(intrinsic #%u", (unsigned)e->as.intrinsic.intrinsic_id);
        for (uint8_t i = 0; i < e->as.intrinsic.argc; i++) {
            fputc(' ', out);
            hir_dump_expr(e->as.intrinsic.args[i], out, indent);
        }
    }
    fputc(')', out);
}

void hir_dump_expr(const HirExpr* e, FILE* out, int indent) {
    if (!e) { fprintf(out, "(null)"); return; }

    if (hir_expr_is_binary(e->kind)) {
        fprintf(out, "(%s ", hir_expr_kind_name(e->kind));
        hir_dump_expr(e->as.binary.lhs, out, indent);
        fputc(' ', out);
        hir_dump_expr(e->as.binary.rhs, out, indent);
        fputc(')', out);
    } else if (hir_expr_is_unary(e->kind)) {
        fprintf(out, "(%s ", hir_expr_kind_name(e->kind));
        hir_dump_expr(e->as.unary.operand, out, indent);
        fputc(')', out);
    } else if (e->kind == HIR_CALL || e->kind == HIR_INTRINSIC) {
        hir_dump_call(e, out, indent);
    } else {
        hir_dump_leaf(e, out);
    }
}

static void hir_dump_stmt(const HirStmt* s, FILE* out, int indent) {
    hir_print_indent(out, indent);

    switch (s->kind) {
        case HIR_STMT_EXPR:
            hir_dump_expr(s->as.expr, out, indent);
            fputc('\n', out);
            break;

        case HIR_STMT_LET:
            fprintf(out, "let v%u = ", (unsigned)s->as.let.var_idx);
            hir_dump_expr(s->as.let.value, out, indent);
            fputc('\n', out);
            break;

        case HIR_STMT_ASSIGN:
            fprintf(out, "v%u = ", (unsigned)s->as.assign.var_idx);
            hir_dump_expr(s->as.assign.value, out, indent);
            fputc('\n', out);
            break;

        case HIR_STMT_RETURN:
            fprintf(out, "return ");
            hir_dump_expr(s->as.ret.value, out, indent);
            fputc('\n', out);
            break;

        case HIR_STMT_IF:
            fprintf(out, "if ");
            hir_dump_expr(s->as.if_stmt.cond, out, indent);
            fprintf(out, ":\n");
            hir_dump_block(&s->as.if_stmt.then_block, out, indent + 2);
            if (s->as.if_stmt.else_block.count > 0) {
                hir_print_indent(out, indent);
                fprintf(out, "else:\n");
                hir_dump_block(&s->as.if_stmt.else_block, out, indent + 2);
            }
            break;

        case HIR_STMT_WHILE:
            fprintf(out, "while ");
            hir_dump_expr(s->as.while_stmt.cond, out, indent);
            fprintf(out, ":\n");
            hir_dump_block(&s->as.while_stmt.body, out, indent + 2);
            break;

        case HIR_STMT_PRINT:
            fprintf(out, "(print ");
            hir_dump_expr(s->as.print.value, out, indent);
            fprintf(out, ")\n");
            break;
    }
}

void hir_dump_block(const HirBlock* b, FILE* out, int indent) {
    for (uint32_t i = 0; i < b->count; i++) {
        hir_dump_stmt(&b->stmts[i], out, indent);
    }
}

void hir_dump(const HirModule* m, FILE* out) {
    /* Dump each function */
    for (uint32_t i = 0; i < m->func_count; i++) {
        const HirFunction* f = &m->functions[i];
        fprintf(out, "func %s(arity=%u, locals=%u):\n",
                f->name, (unsigned)f->arity, (unsigned)f->local_count);
        hir_dump_block(&f->body, out, 2);
        fputc('\n', out);
    }

    /* Dump main body */
    if (m->main_body.count > 0) {
        fprintf(out, "main:\n");
        hir_dump_block(&m->main_body, out, 2);
        hir_print_indent(out, 2);
        fprintf(out, "halt\n");
    }
}

/* ------------------------------------------------------------------ */
/*  Validation                                                          */
/* ------------------------------------------------------------------ */

static void hir_val_push(HirValidation* v, uint32_t func_idx,
                          const char* fmt, ...) {
    if (v->count >= v->cap) {
        v->cap = v->cap < 8 ? 8 : v->cap * 2;
        v->errors = ARC_REALLOC(v->errors, HirError, v->cap);
        assert(v->errors && "hir_validate: allocation failed");
    }
    HirError* err = &v->errors[v->count++];
    err->func_idx = func_idx;
    v->valid = false;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, ap);
    va_end(ap);
}

static void hir_validate_expr(const HirExpr* e, const HirModule* m,
                               uint16_t local_count, uint32_t func_idx,
                               HirValidation* v) {
    if (!e) {
        hir_val_push(v, func_idx, "NULL expression pointer");
        return;
    }

    if (hir_expr_is_binary(e->kind)) {
        if (!e->as.binary.lhs) {
            hir_val_push(v, func_idx,
                         "binary %s: lhs is NULL", hir_expr_kind_name(e->kind));
        } else {
            hir_validate_expr(e->as.binary.lhs, m, local_count, func_idx, v);
        }
        if (!e->as.binary.rhs) {
            hir_val_push(v, func_idx,
                         "binary %s: rhs is NULL", hir_expr_kind_name(e->kind));
        } else {
            hir_validate_expr(e->as.binary.rhs, m, local_count, func_idx, v);
        }
    } else if (hir_expr_is_unary(e->kind)) {
        if (!e->as.unary.operand) {
            hir_val_push(v, func_idx,
                         "unary %s: operand is NULL", hir_expr_kind_name(e->kind));
        } else {
            hir_validate_expr(e->as.unary.operand, m, local_count, func_idx, v);
        }
    } else if (e->kind == HIR_VAR_LOAD) {
        if (e->as.var_idx >= local_count) {
            hir_val_push(v, func_idx,
                         "var_load v%u out of range (local_count=%u)",
                         (unsigned)e->as.var_idx, (unsigned)local_count);
        }
    } else if (e->kind == HIR_CALL) {
        if (e->as.call.func_idx >= m->func_count) {
            hir_val_push(v, func_idx,
                         "call to f%u out of range (func_count=%u)",
                         (unsigned)e->as.call.func_idx, (unsigned)m->func_count);
        } else {
            const HirFunction* target = &m->functions[e->as.call.func_idx];
            if (e->as.call.argc != target->arity) {
                hir_val_push(v, func_idx,
                             "call to %s: argc=%u but arity=%u",
                             target->name, (unsigned)e->as.call.argc,
                             (unsigned)target->arity);
            }
        }
        for (uint8_t i = 0; i < e->as.call.argc; i++) {
            hir_validate_expr(e->as.call.args[i], m, local_count, func_idx, v);
        }
    } else if (e->kind == HIR_INTRINSIC) {
        for (uint8_t i = 0; i < e->as.intrinsic.argc; i++) {
            hir_validate_expr(e->as.intrinsic.args[i], m, local_count,
                              func_idx, v);
        }
    }
    /* CONST_INT, CONST_FLOAT, CONST_BOOL, CONST_NULL, POISON: always valid */
}

static void hir_validate_block(const HirBlock* b, const HirModule* m,
                                uint16_t local_count, uint32_t func_idx,
                                HirValidation* v);

static void hir_validate_stmt(const HirStmt* s, const HirModule* m,
                               uint16_t local_count, uint32_t func_idx,
                               HirValidation* v) {
    switch (s->kind) {
        case HIR_STMT_EXPR:
            hir_validate_expr(s->as.expr, m, local_count, func_idx, v);
            break;
        case HIR_STMT_LET:
            if (s->as.let.var_idx >= local_count) {
                hir_val_push(v, func_idx,
                             "let v%u out of range (local_count=%u)",
                             (unsigned)s->as.let.var_idx,
                             (unsigned)local_count);
            }
            hir_validate_expr(s->as.let.value, m, local_count, func_idx, v);
            break;
        case HIR_STMT_ASSIGN:
            if (s->as.assign.var_idx >= local_count) {
                hir_val_push(v, func_idx,
                             "assign v%u out of range (local_count=%u)",
                             (unsigned)s->as.assign.var_idx,
                             (unsigned)local_count);
            }
            hir_validate_expr(s->as.assign.value, m, local_count, func_idx, v);
            break;
        case HIR_STMT_RETURN:
            hir_validate_expr(s->as.ret.value, m, local_count, func_idx, v);
            break;
        case HIR_STMT_IF:
            hir_validate_expr(s->as.if_stmt.cond, m, local_count, func_idx, v);
            hir_validate_block(&s->as.if_stmt.then_block, m, local_count,
                               func_idx, v);
            hir_validate_block(&s->as.if_stmt.else_block, m, local_count,
                               func_idx, v);
            break;
        case HIR_STMT_WHILE:
            hir_validate_expr(s->as.while_stmt.cond, m, local_count,
                              func_idx, v);
            hir_validate_block(&s->as.while_stmt.body, m, local_count,
                               func_idx, v);
            break;
        case HIR_STMT_PRINT:
            hir_validate_expr(s->as.print.value, m, local_count, func_idx, v);
            break;
    }
}

static void hir_validate_block(const HirBlock* b, const HirModule* m,
                                uint16_t local_count, uint32_t func_idx,
                                HirValidation* v) {
    for (uint32_t i = 0; i < b->count; i++) {
        hir_validate_stmt(&b->stmts[i], m, local_count, func_idx, v);
    }
}

HirValidation hir_validate(const HirModule* m) {
    HirValidation v;
    v.errors = NULL;
    v.count  = 0;
    v.cap    = 0;
    v.valid  = true;

    /* Validate each function body */
    for (uint32_t i = 0; i < m->func_count; i++) {
        const HirFunction* f = &m->functions[i];
        hir_validate_block(&f->body, m, f->local_count, i, &v);
    }

    /*
     * Main body uses UINT16_MAX as the local_count upper bound: main-body
     * variable indices are validated by whichever pass populates them.
     * We use func_idx = UINT32_MAX as a sentinel for "main body".
     */
    hir_validate_block(&m->main_body, m, UINT16_MAX, UINT32_MAX, &v);

    return v;
}

void hir_validation_free(HirValidation* v) {
    ARC_FREE(v->errors);
    v->errors = NULL;
    v->count  = 0;
    v->cap    = 0;
    v->valid  = true;
}
