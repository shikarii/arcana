#include "typecheck.h"
#include <stdarg.h>

/* ===================================================================
 * Type name lookup
 * =================================================================== */

const char* arc_type_name(ArcType t) {
    switch (t) {
    case ARC_TYPE_UNKNOWN: return "unknown";
    case ARC_TYPE_NULL:    return "null";
    case ARC_TYPE_BOOL:    return "bool";
    case ARC_TYPE_I64:     return "i64";
    case ARC_TYPE_F64:     return "f64";
    case ARC_TYPE_STRING:  return "string";
    case ARC_TYPE_ARRAY:   return "array";
    case ARC_TYPE_MAP:     return "map";
    case ARC_TYPE_CLOSURE: return "closure";
    case ARC_TYPE_ANY:     return "any";
    }
    return "unknown";
}

/* ===================================================================
 * Type checker context
 * =================================================================== */

typedef struct {
    const ArcGraph* graph;
    ArcDiagList*    diags;
    ArcType*        types;   /* per-node inferred type */
    bool            had_error;
} TcCtx;

static void tc_error(TcCtx* ctx, ArcNodeId node_id, const char* fmt, ...) {
    ArcDiagnostic* d = arc_diag_add(ctx->diags);
    d->severity = ARC_DIAG_ERROR;
    d->code = 3001; /* ARC-TYPE-0001 */
    d->primary.node_id = node_id;
    d->primary.element_id = node_id < ctx->graph->node_count
        ? ctx->graph->nodes[node_id].source_id : 0;
    d->primary.port_role = NULL;
    d->related_count = 0;
    d->note[0] = '\0';

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(d->message, sizeof(d->message), fmt, ap);
    va_end(ap);

    ctx->had_error = true;
}

/* ===================================================================
 * Graph navigation helpers
 * =================================================================== */

static ArcNodeId tc_find_source(const ArcGraph* g, ArcPortId input_port) {
    if (input_port == ARC_INVALID_ID) return ARC_INVALID_ID;
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].to == input_port)
            return g->ports[g->edges[i].from].owner;
    }
    return ARC_INVALID_ID;
}

static ArcPortId tc_find_port(const ArcGraph* g, ArcNodeId node_id,
                              const char* role, ArcPortDir dir) {
    const ArcNode* n = &g->nodes[node_id];
    for (uint32_t i = 0; i < n->port_count; i++) {
        const ArcPort* p = &g->ports[n->ports[i]];
        if (p->dir == dir && p->role && strcmp(p->role, role) == 0)
            return p->id;
    }
    return ARC_INVALID_ID;
}

/* ===================================================================
 * Type inference — recursive, memoized via ctx->types[]
 * =================================================================== */

static ArcType infer_type(TcCtx* ctx, ArcNodeId node_id);

static ArcType infer_input(TcCtx* ctx, ArcNodeId node_id, const char* role) {
    ArcPortId port = tc_find_port(ctx->graph, node_id, role, ARC_PORT_INPUT);
    if (port == ARC_INVALID_ID)
        port = tc_find_port(ctx->graph, node_id, "in", ARC_PORT_INPUT);
    if (port == ARC_INVALID_ID) return ARC_TYPE_UNKNOWN;
    ArcNodeId src = tc_find_source(ctx->graph, port);
    if (src == ARC_INVALID_ID) return ARC_TYPE_UNKNOWN;
    return infer_type(ctx, src);
}

static bool is_numeric(ArcType t) {
    return t == ARC_TYPE_I64 || t == ARC_TYPE_F64;
}

/* --- Infer type for ADD (polymorphic: numeric + string concatenation) --- */
static ArcType infer_add(TcCtx* ctx, ArcNodeId node_id) {
    ArcType lhs = infer_input(ctx, node_id, "lhs");
    ArcType rhs = infer_input(ctx, node_id, "rhs");
    if (lhs == ARC_TYPE_STRING && rhs == ARC_TYPE_STRING)
        return ARC_TYPE_STRING;
    if (is_numeric(lhs) && is_numeric(rhs))
        return (lhs == ARC_TYPE_F64 || rhs == ARC_TYPE_F64) ? ARC_TYPE_F64 : ARC_TYPE_I64;
    if (lhs == ARC_TYPE_ANY || rhs == ARC_TYPE_ANY)
        return ARC_TYPE_ANY;
    tc_error(ctx, node_id, "ADD requires numeric or string operands, got %s and %s",
             arc_type_name(lhs), arc_type_name(rhs));
    return ARC_TYPE_ANY;
}

/* --- Infer type for SUB/MUL/DIV/MOD (numeric only) --- */
static ArcType infer_arith(TcCtx* ctx, ArcNodeId node_id, const ArcNode* n) {
    ArcType lhs = infer_input(ctx, node_id, "lhs");
    ArcType rhs = infer_input(ctx, node_id, "rhs");
    if (is_numeric(lhs) && is_numeric(rhs))
        return (lhs == ARC_TYPE_F64 || rhs == ARC_TYPE_F64) ? ARC_TYPE_F64 : ARC_TYPE_I64;
    if (lhs == ARC_TYPE_ANY || rhs == ARC_TYPE_ANY)
        return ARC_TYPE_ANY;
    tc_error(ctx, node_id, "%s requires numeric operands, got %s and %s",
             n->kind == ARC_NODE_SUB ? "SUB" :
             n->kind == ARC_NODE_MUL ? "MUL" :
             n->kind == ARC_NODE_DIV ? "DIV" : "MOD",
             arc_type_name(lhs), arc_type_name(rhs));
    return ARC_TYPE_ANY;
}

/* --- Infer type for comparison ops (LT/LE/GT/GE) --- */
static ArcType infer_comparison(TcCtx* ctx, ArcNodeId node_id) {
    ArcType lhs = infer_input(ctx, node_id, "lhs");
    ArcType rhs = infer_input(ctx, node_id, "rhs");
    if (!is_numeric(lhs) && lhs != ARC_TYPE_ANY)
        tc_error(ctx, node_id, "comparison requires numeric operands, got %s", arc_type_name(lhs));
    if (!is_numeric(rhs) && rhs != ARC_TYPE_ANY)
        tc_error(ctx, node_id, "comparison requires numeric operands, got %s", arc_type_name(rhs));
    return ARC_TYPE_BOOL;
}

/* --- Infer type for bitwise ops (i64 only) --- */
static ArcType infer_bitwise_binary(TcCtx* ctx, ArcNodeId node_id) {
    ArcType lhs = infer_input(ctx, node_id, "lhs");
    ArcType rhs = infer_input(ctx, node_id, "rhs");
    if (lhs != ARC_TYPE_I64 && lhs != ARC_TYPE_ANY)
        tc_error(ctx, node_id, "bitwise op requires i64 operands, got %s", arc_type_name(lhs));
    if (rhs != ARC_TYPE_I64 && rhs != ARC_TYPE_ANY)
        tc_error(ctx, node_id, "bitwise op requires i64 operands, got %s", arc_type_name(rhs));
    return ARC_TYPE_I64;
}

/* --- Infer type for unary numeric (NEG) --- */
static ArcType infer_neg(TcCtx* ctx, ArcNodeId node_id) {
    ArcType val = infer_input(ctx, node_id, "value");
    if (is_numeric(val)) return val;
    if (val == ARC_TYPE_ANY) return ARC_TYPE_ANY;
    tc_error(ctx, node_id, "NEG requires numeric operand, got %s", arc_type_name(val));
    return ARC_TYPE_ANY;
}

/* --- Infer type for bitwise NOT (i64 only) --- */
static ArcType infer_bit_not(TcCtx* ctx, ArcNodeId node_id) {
    ArcType val = infer_input(ctx, node_id, "value");
    if (val != ARC_TYPE_I64 && val != ARC_TYPE_ANY)
        tc_error(ctx, node_id, "BIT_NOT requires i64 operand, got %s", arc_type_name(val));
    return ARC_TYPE_I64;
}

/* --- Infer type for STR_LEN --- */
static ArcType infer_str_len(TcCtx* ctx, ArcNodeId node_id) {
    ArcType val = infer_input(ctx, node_id, "value");
    if (val != ARC_TYPE_STRING && val != ARC_TYPE_ANY)
        tc_error(ctx, node_id, "STR_LEN requires string, got %s", arc_type_name(val));
    return ARC_TYPE_I64;
}

/* --- Infer type for nodes needing no deep analysis --- */
static ArcType infer_simple_kind(TcCtx* ctx, ArcNodeId node_id, const ArcNode* n) {
    switch (n->kind) {
    case ARC_NODE_EQ: case ARC_NODE_NEQ:
        infer_input(ctx, node_id, "lhs"); infer_input(ctx, node_id, "rhs");
        return ARC_TYPE_BOOL;
    case ARC_NODE_NOT: infer_input(ctx, node_id, "value"); return ARC_TYPE_BOOL;
    case ARC_NODE_AND: case ARC_NODE_OR:
        infer_input(ctx, node_id, "lhs"); infer_input(ctx, node_id, "rhs");
        return ARC_TYPE_ANY;
    case ARC_NODE_CAST_I64:  infer_input(ctx, node_id, "value"); return ARC_TYPE_I64;
    case ARC_NODE_CAST_F64:  infer_input(ctx, node_id, "value"); return ARC_TYPE_F64;
    case ARC_NODE_CAST_STR:  infer_input(ctx, node_id, "value"); return ARC_TYPE_STRING;
    case ARC_NODE_STR_SLICE: case ARC_NODE_STR_INDEX: return ARC_TYPE_STRING;
    case ARC_NODE_LENGTH: infer_input(ctx, node_id, "value"); return ARC_TYPE_I64;
    case ARC_NODE_ARRAY_LITERAL: return ARC_TYPE_ARRAY;
    case ARC_NODE_MAP_LITERAL:   return ARC_TYPE_MAP;
    case ARC_NODE_INDEX_GET:     return ARC_TYPE_ANY;
    case ARC_NODE_INDEX_SET:     return ARC_TYPE_NULL;
    case ARC_NODE_IF: case ARC_NODE_WHILE: case ARC_NODE_LET:
    case ARC_NODE_ASSIGN: case ARC_NODE_SEQUENCE:
    case ARC_NODE_ROOT_OUTPUT: case ARC_NODE_PRINT: return ARC_TYPE_NULL;
    case ARC_NODE_VAR_REF: case ARC_NODE_FUNC_CALL:
    case ARC_NODE_INTRINSIC_CALL: case ARC_NODE_PARAM: return ARC_TYPE_ANY;
    case ARC_NODE_FUNC_DEF: case ARC_NODE_CLOSURE: return ARC_TYPE_CLOSURE;
    case ARC_NODE_RETURN: case ARC_NODE_THROW:
        infer_input(ctx, node_id, "value"); return ARC_TYPE_NULL;
    case ARC_NODE_TRY: return ARC_TYPE_NULL;
    default: return ARC_TYPE_ANY;
    }
}

/* --- Main infer_type function --- */
static ArcType infer_type(TcCtx* ctx, ArcNodeId node_id) {
    if (node_id >= ctx->graph->node_count) return ARC_TYPE_UNKNOWN;
    if (ctx->types[node_id] != ARC_TYPE_UNKNOWN) return ctx->types[node_id];

    const ArcNode* n = &ctx->graph->nodes[node_id];
    ArcType result;

    switch (n->kind) {
    case ARC_NODE_CONST_INT:    result = ARC_TYPE_I64; break;
    case ARC_NODE_CONST_FLOAT:  result = ARC_TYPE_F64; break;
    case ARC_NODE_CONST_BOOL:   result = ARC_TYPE_BOOL; break;
    case ARC_NODE_CONST_NULL:   result = ARC_TYPE_NULL; break;
    case ARC_NODE_CONST_STRING: result = ARC_TYPE_STRING; break;
    case ARC_NODE_ADD:  result = infer_add(ctx, node_id); break;
    case ARC_NODE_SUB: case ARC_NODE_MUL: case ARC_NODE_DIV: case ARC_NODE_MOD:
        result = infer_arith(ctx, node_id, n); break;
    case ARC_NODE_NEG:  result = infer_neg(ctx, node_id); break;
    case ARC_NODE_LT: case ARC_NODE_LE: case ARC_NODE_GT: case ARC_NODE_GE:
        result = infer_comparison(ctx, node_id); break;
    case ARC_NODE_BIT_AND: case ARC_NODE_BIT_OR: case ARC_NODE_BIT_XOR:
    case ARC_NODE_SHL: case ARC_NODE_SHR:
        result = infer_bitwise_binary(ctx, node_id); break;
    case ARC_NODE_BIT_NOT: result = infer_bit_not(ctx, node_id); break;
    case ARC_NODE_STR_LEN: result = infer_str_len(ctx, node_id); break;
    default: result = infer_simple_kind(ctx, node_id, n); break;
    }

    ctx->types[node_id] = result;
    return result;
}

/* ===================================================================
 * Public API
 * =================================================================== */

ArcTypeCheckResult arc_typecheck(const ArcGraph* graph) {
    ArcTypeCheckResult result;
    memset(&result, 0, sizeof(result));
    arc_diag_init(&result.diagnostics);

    if (!graph || graph->node_count == 0) {
        result.success = true;
        return result;
    }

    result.node_types = ARC_ALLOC(ArcType, graph->node_count);
    result.node_count = graph->node_count;

    TcCtx ctx;
    ctx.graph = graph;
    ctx.diags = &result.diagnostics;
    ctx.types = result.node_types;
    ctx.had_error = false;

    for (uint32_t i = 0; i < graph->node_count; i++)
        infer_type(&ctx, i);

    result.success = !ctx.had_error;
    return result;
}

void arc_typecheck_result_free(ArcTypeCheckResult* r) {
    arc_diag_free(&r->diagnostics);
    ARC_FREE(r->node_types);
    r->node_types = NULL;
    r->node_count = 0;
    r->success = false;
}
