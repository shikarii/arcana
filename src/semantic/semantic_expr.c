/*
 * semantic_expr.c -- expression lowering helpers for the semantic pass.
 *
 * Extracts lower_expr() and its sub-helpers from semantic.c to comply
 * with file and function LOC limits.
 */

#include "semantic_internal.h"

/* --- lower a literal constant node --- */
static HirExpr* lower_literal(SemCtx* ctx, const ArcNode* n) {
    (void)ctx;
    switch (n->kind) {
    case ARC_NODE_CONST_INT: {
        HirExpr* e = hir_expr_new(HIR_CONST_INT, n->source_id);
        e->as.int_val = n->attr.int_value;
        return e;
    }
    case ARC_NODE_CONST_FLOAT: {
        HirExpr* e = hir_expr_new(HIR_CONST_FLOAT, n->source_id);
        e->as.float_val = n->attr.float_value;
        return e;
    }
    case ARC_NODE_CONST_BOOL: {
        HirExpr* e = hir_expr_new(HIR_CONST_BOOL, n->source_id);
        e->as.bool_val = n->attr.bool_value;
        return e;
    }
    default:
        return hir_expr_new(HIR_CONST_NULL, n->source_id);
    }
}

/* --- resolve binary operand ports (cyclic order or roles) --- */
static bool resolve_binary_ports(SemCtx* ctx, ArcNodeId node_id,
                                  const ArcNode* n,
                                  ArcPortId* lhs_out, ArcPortId* rhs_out) {
    *lhs_out = ARC_INVALID_ID;
    *rhs_out = ARC_INVALID_ID;

    if (n->cyclic_count >= 2) {
        int input_idx = 0;
        for (uint32_t i = 0; i < n->cyclic_count && input_idx < 2; i++) {
            const ArcPort* p = &ctx->graph->ports[n->cyclic_order[i]];
            if (p->dir == ARC_PORT_INPUT) {
                if (input_idx == 0) *lhs_out = p->id;
                else                *rhs_out = p->id;
                input_idx++;
            }
        }
    } else {
        *lhs_out = find_port_by_role(ctx->graph, node_id, "lhs", ARC_PORT_INPUT);
        *rhs_out = find_port_by_role(ctx->graph, node_id, "rhs", ARC_PORT_INPUT);
    }
    return (*lhs_out != ARC_INVALID_ID && *rhs_out != ARC_INVALID_ID);
}

/* --- lower a binary operator node --- */
static HirExpr* lower_binary_op(SemCtx* ctx, ArcNodeId node_id, const ArcNode* n) {
    ArcPortId lhs_port, rhs_port;
    if (!resolve_binary_ports(ctx, node_id, n, &lhs_port, &rhs_port)) {
        sem_error(ctx, ARC_ERR_MISSING_PORT, node_id, n->source_id,
                  "binary op node %u missing lhs/rhs input ports", node_id);
        return hir_expr_new(HIR_POISON, n->source_id);
    }

    ArcNodeId lhs_src = find_source_node(ctx->graph, lhs_port);
    ArcNodeId rhs_src = find_source_node(ctx->graph, rhs_port);
    if (lhs_src == ARC_INVALID_ID || rhs_src == ARC_INVALID_ID) {
        sem_error(ctx, ARC_ERR_DISCONNECTED_INPUT, node_id, n->source_id,
                  "binary op node %u has disconnected inputs", node_id);
        return hir_expr_new(HIR_POISON, n->source_id);
    }

    HirExpr* lhs = lower_expr(ctx, lhs_src);
    HirExpr* rhs = lower_expr(ctx, rhs_src);
    if (lhs->kind == HIR_POISON || rhs->kind == HIR_POISON) {
        hir_expr_free(lhs);
        hir_expr_free(rhs);
        return hir_expr_new(HIR_POISON, n->source_id);
    }

    HirExpr* e = hir_expr_new(node_kind_to_binary_expr(n->kind), n->source_id);
    e->as.binary.lhs = lhs;
    e->as.binary.rhs = rhs;
    return e;
}

/* --- lower a unary operator node (NEG or NOT) --- */
static HirExpr* lower_unary_op(SemCtx* ctx, ArcNodeId node_id,
                                const ArcNode* n, HirExprKind hir_kind) {
    ArcPortId in = find_port_by_role(ctx->graph, node_id, "value", ARC_PORT_INPUT);
    if (in == ARC_INVALID_ID)
        in = find_port_by_role(ctx->graph, node_id, "in", ARC_PORT_INPUT);

    ArcNodeId src = find_source_node(ctx->graph, in);
    if (src == ARC_INVALID_ID) {
        sem_error(ctx, ARC_ERR_DISCONNECTED_INPUT, node_id, n->source_id,
                  "%s node %u has no input",
                  hir_kind == HIR_NEG ? "neg" : "not", node_id);
        return hir_expr_new(HIR_POISON, n->source_id);
    }

    HirExpr* operand = lower_expr(ctx, src);
    if (operand->kind == HIR_POISON) return operand;

    HirExpr* e = hir_expr_new(hir_kind, n->source_id);
    e->as.unary.operand = operand;
    return e;
}

/* --- lower a variable reference --- */
static HirExpr* lower_var_ref(SemCtx* ctx, ArcNodeId node_id, const ArcNode* n) {
    int slot = arc_scope_find(&ctx->scope, n->attr.name);
    if (slot < 0) {
        sem_error(ctx, ARC_ERR_UNDEFINED_VARIABLE, node_id, n->source_id,
                  "undefined variable '%s'", n->attr.name);
        return hir_expr_new(HIR_POISON, n->source_id);
    }
    HirExpr* e = hir_expr_new(HIR_VAR_LOAD, n->source_id);
    e->as.var_idx = (uint16_t)slot;
    return e;
}

/* --- collect call arguments from input ports --- */
static uint8_t collect_call_args(SemCtx* ctx, const ArcNode* n,
                                  HirExpr** args, bool* has_poison) {
    uint8_t argc = 0;
    *has_poison = false;

    if (n->cyclic_count > 0) {
        for (uint32_t i = 0; i < n->cyclic_count; i++) {
            const ArcPort* p = &ctx->graph->ports[n->cyclic_order[i]];
            if (p->dir == ARC_PORT_INPUT) {
                ArcNodeId src = find_source_node(ctx->graph, p->id);
                if (src != ARC_INVALID_ID) {
                    HirExpr* arg = lower_expr(ctx, src);
                    if (arg->kind == HIR_POISON) *has_poison = true;
                    args[argc++] = arg;
                }
            }
        }
    } else {
        for (uint32_t i = 0; i < n->port_count; i++) {
            const ArcPort* p = &ctx->graph->ports[n->ports[i]];
            if (p->dir == ARC_PORT_INPUT) {
                ArcNodeId src = find_source_node(ctx->graph, p->id);
                if (src != ARC_INVALID_ID) {
                    HirExpr* arg = lower_expr(ctx, src);
                    if (arg->kind == HIR_POISON) *has_poison = true;
                    args[argc++] = arg;
                }
            }
        }
    }
    return argc;
}

/* --- lower a function call expression --- */
static HirExpr* lower_func_call_expr(SemCtx* ctx, ArcNodeId node_id,
                                      const ArcNode* n) {
    int func_idx = find_func(ctx, n->attr.name);
    if (func_idx < 0) {
        sem_error(ctx, ARC_ERR_UNDEFINED_FUNCTION, node_id, n->source_id,
                  "undefined function '%s'", n->attr.name);
        return hir_expr_new(HIR_POISON, n->source_id);
    }

    HirExpr* args[256];
    bool has_poison;
    uint8_t argc = collect_call_args(ctx, n, args, &has_poison);

    if (has_poison) {
        for (uint8_t a = 0; a < argc; a++) hir_expr_free(args[a]);
        return hir_expr_new(HIR_POISON, n->source_id);
    }

    if (func_idx < (int)ctx->module->func_count) {
        uint8_t expected = ctx->module->functions[func_idx].arity;
        if (argc != expected) {
            sem_error(ctx, ARC_ERR_ARITY_MISMATCH, node_id, n->source_id,
                      "function '%s' expects %u args, got %u",
                      n->attr.name, expected, argc);
            for (uint8_t a = 0; a < argc; a++) hir_expr_free(args[a]);
            return hir_expr_new(HIR_POISON, n->source_id);
        }
    }

    HirExpr* e = hir_expr_new(HIR_CALL, n->source_id);
    e->as.call.func_idx = (uint16_t)func_idx;
    e->as.call.argc = argc;
    if (argc > 0) {
        e->as.call.args = ARC_ALLOC(HirExpr*, argc);
        memcpy(e->as.call.args, args, sizeof(HirExpr*) * argc);
    } else {
        e->as.call.args = NULL;
    }
    return e;
}

/* ===================================================================
 * lower_expr — recursively lower an expression-producing graph node
 *              into a heap-allocated HirExpr tree.
 * =================================================================== */

HirExpr* lower_expr(SemCtx* ctx, ArcNodeId node_id) {
    if (node_id >= ctx->graph->node_count) {
        sem_error(ctx, ARC_ERR_INVALID_NODE_ID, node_id, 0,
                  "invalid node id %u in expression", node_id);
        return hir_expr_new(HIR_POISON, 0);
    }

    const ArcNode* n = &ctx->graph->nodes[node_id];

    switch (n->kind) {
    case ARC_NODE_CONST_INT:
    case ARC_NODE_CONST_FLOAT:
    case ARC_NODE_CONST_BOOL:
    case ARC_NODE_CONST_NULL:
        return lower_literal(ctx, n);

    case ARC_NODE_ADD: case ARC_NODE_SUB: case ARC_NODE_MUL:
    case ARC_NODE_DIV: case ARC_NODE_MOD:
    case ARC_NODE_EQ: case ARC_NODE_NEQ:
    case ARC_NODE_LT: case ARC_NODE_LE:
    case ARC_NODE_GT: case ARC_NODE_GE:
        return lower_binary_op(ctx, node_id, n);

    case ARC_NODE_NEG:
        return lower_unary_op(ctx, node_id, n, HIR_NEG);
    case ARC_NODE_NOT:
        return lower_unary_op(ctx, node_id, n, HIR_NOT);

    case ARC_NODE_VAR_REF:
        return lower_var_ref(ctx, node_id, n);

    case ARC_NODE_FUNC_CALL:
        return lower_func_call_expr(ctx, node_id, n);

    default:
        sem_error(ctx, ARC_ERR_UNSUPPORTED_NODE, node_id, n->source_id,
                  "unsupported expression node kind %d at node %u", n->kind, node_id);
        return hir_expr_new(HIR_POISON, n->source_id);
    }
}

/* ===================================================================
 * lower_value_from_port — helper to trace an input port edge and
 *                         lower the source node as an expression.
 * =================================================================== */

HirExpr* lower_value_from_port(SemCtx* ctx, ArcNodeId node_id,
                                const char* role, const char* alt_role) {
    ArcPortId port = find_port_by_role(ctx->graph, node_id, role, ARC_PORT_INPUT);
    if (port == ARC_INVALID_ID && alt_role)
        port = find_port_by_role(ctx->graph, node_id, alt_role, ARC_PORT_INPUT);

    if (port == ARC_INVALID_ID) return NULL;

    ArcNodeId src = find_source_node(ctx->graph, port);
    if (src == ARC_INVALID_ID) return NULL;

    return lower_expr(ctx, src);
}
