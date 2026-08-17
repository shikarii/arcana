#include "semantic_internal.h"

/* ===================================================================
 * Scope stack implementation
 * =================================================================== */

void scope_init(SemScope* s) {
    s->local_count = 0;
    s->scope_depth = 0;
}

void scope_push(SemScope* s) {
    assert(s->scope_depth < SEM_MAX_SCOPES);
    s->scope_starts[s->scope_depth++] = s->local_count;
}

void scope_pop(SemScope* s) {
    assert(s->scope_depth > 0);
    s->local_count = s->scope_starts[--s->scope_depth];
}

int scope_find(const SemScope* s, const char* name) {
    for (int i = (int)s->local_count - 1; i >= 0; i--) {
        if (strcmp(s->locals[i].name, name) == 0)
            return (int)s->locals[i].slot;
    }
    return -1;
}

uint16_t scope_add(SemScope* s, const char* name) {
    assert(s->local_count < SEM_MAX_LOCALS);
    uint16_t slot = s->local_count;
    s->locals[s->local_count].name = name;
    s->locals[s->local_count].slot = slot;
    s->local_count++;
    return slot;
}

/* ===================================================================
 * Diagnostics helper
 * =================================================================== */

void sem_error(SemCtx* ctx, ArcDiagCode code,
               ArcNodeId node_id, ArcElementId elem,
               const char* fmt, ...) {
    ArcDiagnostic* d = arc_diag_add(ctx->diags);
    d->severity = ARC_DIAG_ERROR;
    d->code = code;
    d->primary.element_id = elem;
    d->primary.node_id = node_id;
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
 * Function table helpers
 * =================================================================== */

void register_func(SemCtx* ctx, const char* name, uint16_t idx) {
    assert(ctx->func_table_count < 256);
    ctx->func_table[ctx->func_table_count].name = name;
    ctx->func_table[ctx->func_table_count].idx = idx;
    ctx->func_table_count++;
}

int find_func(const SemCtx* ctx, const char* name) {
    for (int i = 0; i < (int)ctx->func_table_count; i++) {
        if (strcmp(ctx->func_table[i].name, name) == 0)
            return (int)ctx->func_table[i].idx;
    }
    return -1;
}

/* ===================================================================
 * Graph navigation helpers
 * =================================================================== */

ArcNodeId find_source_node(const ArcGraph* g, ArcPortId input_port) {
    if (input_port == ARC_INVALID_ID) return ARC_INVALID_ID;
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].to == input_port) {
            ArcPortId from = g->edges[i].from;
            return g->ports[from].owner;
        }
    }
    return ARC_INVALID_ID;
}

ArcPortId find_port_by_role(const ArcGraph* g, ArcNodeId node_id,
                             const char* role, ArcPortDir dir) {
    const ArcNode* n = &g->nodes[node_id];
    for (uint32_t i = 0; i < n->port_count; i++) {
        const ArcPort* p = &g->ports[n->ports[i]];
        if (p->dir == dir && p->role && strcmp(p->role, role) == 0)
            return p->id;
    }
    return ARC_INVALID_ID;
}

bool is_expr_node(ArcNodeKind k) {
    switch (k) {
    case ARC_NODE_CONST_INT: case ARC_NODE_CONST_FLOAT:
    case ARC_NODE_CONST_BOOL: case ARC_NODE_CONST_NULL:
    case ARC_NODE_ADD: case ARC_NODE_SUB: case ARC_NODE_MUL:
    case ARC_NODE_DIV: case ARC_NODE_MOD: case ARC_NODE_NEG:
    case ARC_NODE_EQ: case ARC_NODE_NEQ: case ARC_NODE_LT:
    case ARC_NODE_LE: case ARC_NODE_GT: case ARC_NODE_GE:
    case ARC_NODE_VAR_REF: case ARC_NODE_FUNC_CALL:
    case ARC_NODE_NOT:
        return true;
    default:
        return false;
    }
}

HirExprKind node_kind_to_binary_expr(ArcNodeKind k) {
    switch (k) {
    case ARC_NODE_ADD: return HIR_ADD;
    case ARC_NODE_SUB: return HIR_SUB;
    case ARC_NODE_MUL: return HIR_MUL;
    case ARC_NODE_DIV: return HIR_DIV;
    case ARC_NODE_MOD: return HIR_MOD;
    case ARC_NODE_EQ:  return HIR_EQ;
    case ARC_NODE_NEQ: return HIR_NEQ;
    case ARC_NODE_LT:  return HIR_LT;
    case ARC_NODE_LE:  return HIR_LE;
    case ARC_NODE_GT:  return HIR_GT;
    case ARC_NODE_GE:  return HIR_GE;
    default:           return HIR_ADD; /* unreachable */
    }
}

/* ===================================================================
 * lower_stmt helpers — each handles a subset of statement node kinds
 * =================================================================== */

static void lower_stmt_let(SemCtx* ctx, ArcNodeId node_id,
                            const ArcNode* n, HirBlock* block) {
    HirExpr* value = lower_value_from_port(ctx, node_id, "value", "in");
    uint16_t slot = scope_add(&ctx->scope, n->attr.name);
    HirStmt* stmt = hir_block_add(block, HIR_STMT_LET, n->source_id);
    stmt->as.let.var_idx = slot;
    stmt->as.let.value = value;
}

static void lower_stmt_assign(SemCtx* ctx, ArcNodeId node_id,
                                const ArcNode* n, HirBlock* block) {
    HirExpr* value = lower_value_from_port(ctx, node_id, "value", "in");
    int slot = scope_find(&ctx->scope, n->attr.name);
    if (slot < 0) {
        sem_error(ctx, ARC_ERR_UNDEFINED_VARIABLE, node_id, n->source_id,
                  "undefined variable '%s' in assign", n->attr.name);
        ARC_FREE(value);
        return;
    }
    HirStmt* stmt = hir_block_add(block, HIR_STMT_ASSIGN, n->source_id);
    stmt->as.assign.var_idx = (uint16_t)slot;
    stmt->as.assign.value = value;
}

static void lower_stmt_simple_value(SemCtx* ctx, ArcNodeId node_id,
                                     const ArcNode* n, HirBlock* block,
                                     HirStmtKind kind) {
    HirExpr* value = lower_value_from_port(ctx, node_id, "value",
                                            kind == HIR_STMT_RETURN ? NULL : "in");
    if (kind == HIR_STMT_PRINT && n->kind == ARC_NODE_ROOT_OUTPUT && !value)
        return;
    HirStmt* stmt = hir_block_add(block, kind, n->source_id);
    if (kind == HIR_STMT_PRINT)
        stmt->as.print.value = value;
    else
        stmt->as.ret.value = value;
}

static void lower_stmt_as_expr(SemCtx* ctx, ArcNodeId node_id,
                                const ArcNode* n, HirBlock* block) {
    HirExpr* expr = lower_expr(ctx, node_id);
    if (expr) {
        HirStmt* stmt = hir_block_add(block, HIR_STMT_EXPR, n->source_id);
        stmt->as.expr = expr;
    }
}

static void lower_stmt_if(SemCtx* ctx, ArcNodeId node_id,
                           const ArcNode* n, HirBlock* block) {
    HirExpr* cond = lower_value_from_port(ctx, node_id, "cond", NULL);
    if (!cond) {
        sem_error(ctx, ARC_ERR_DISCONNECTED_INPUT, node_id, n->source_id,
                  "if node %u has no condition input", node_id);
        return;
    }
    HirStmt* stmt = hir_block_add(block, HIR_STMT_IF, n->source_id);
    stmt->as.if_stmt.cond = cond;
    memset(&stmt->as.if_stmt.then_block, 0, sizeof(HirBlock));
    memset(&stmt->as.if_stmt.else_block, 0, sizeof(HirBlock));

    scope_push(&ctx->scope);
    lower_region_stmts(ctx, n->attr.branch.then_region, &stmt->as.if_stmt.then_block);
    scope_pop(&ctx->scope);

    if (n->attr.branch.else_region != ARC_INVALID_ID) {
        scope_push(&ctx->scope);
        lower_region_stmts(ctx, n->attr.branch.else_region, &stmt->as.if_stmt.else_block);
        scope_pop(&ctx->scope);
    }
}

static void lower_stmt_while(SemCtx* ctx, ArcNodeId node_id,
                              const ArcNode* n, HirBlock* block) {
    HirExpr* cond = lower_value_from_port(ctx, node_id, "cond", NULL);
    if (!cond) {
        sem_error(ctx, ARC_ERR_DISCONNECTED_INPUT, node_id, n->source_id,
                  "while node %u has no condition input", node_id);
        return;
    }
    HirStmt* stmt = hir_block_add(block, HIR_STMT_WHILE, n->source_id);
    stmt->as.while_stmt.cond = cond;
    memset(&stmt->as.while_stmt.body, 0, sizeof(HirBlock));

    scope_push(&ctx->scope);
    lower_region_stmts(ctx, n->attr.loop.body_region, &stmt->as.while_stmt.body);
    scope_pop(&ctx->scope);
}

void lower_stmt(SemCtx* ctx, ArcNodeId node_id, HirBlock* block) {
    if (node_id >= ctx->graph->node_count) {
        sem_error(ctx, ARC_ERR_INVALID_NODE_ID, node_id, 0,
                  "invalid node id %u in statement", node_id);
        return;
    }

    const ArcNode* n = &ctx->graph->nodes[node_id];

    switch (n->kind) {
    case ARC_NODE_LET:         lower_stmt_let(ctx, node_id, n, block); break;
    case ARC_NODE_ASSIGN:      lower_stmt_assign(ctx, node_id, n, block); break;
    case ARC_NODE_PRINT:       lower_stmt_simple_value(ctx, node_id, n, block, HIR_STMT_PRINT); break;
    case ARC_NODE_RETURN:      lower_stmt_simple_value(ctx, node_id, n, block, HIR_STMT_RETURN); break;
    case ARC_NODE_ROOT_OUTPUT: lower_stmt_simple_value(ctx, node_id, n, block, HIR_STMT_PRINT); break;
    case ARC_NODE_IF:          lower_stmt_if(ctx, node_id, n, block); break;
    case ARC_NODE_WHILE:       lower_stmt_while(ctx, node_id, n, block); break;
    case ARC_NODE_SEQUENCE: case ARC_NODE_PARAM: break;
    case ARC_NODE_FUNC_CALL:   lower_stmt_as_expr(ctx, node_id, n, block); break;
    default:
        if (is_expr_node(n->kind))
            lower_stmt_as_expr(ctx, node_id, n, block);
        else
            sem_error(ctx, ARC_ERR_UNSUPPORTED_NODE, node_id, n->source_id,
                      "unsupported statement node kind %d at node %u",
                      n->kind, node_id);
        break;
    }
}

/* ===================================================================
 * lower_region_stmts
 * =================================================================== */

void lower_region_stmts(SemCtx* ctx, ArcRegionId region_id, HirBlock* block) {
    if (region_id == ARC_INVALID_ID) return;
    if (region_id >= ctx->graph->region_count) {
        sem_error(ctx, ARC_ERR_INVALID_REGION, ARC_INVALID_ID, 0,
                  "invalid region id %u", region_id);
        return;
    }

    const ArcRegion* r = &ctx->graph->regions[region_id];
    for (uint32_t i = 0; i < r->member_count; i++) {
        ArcNodeId mid = r->members[i];
        const ArcNode* m = &ctx->graph->nodes[mid];
        if (m->kind == ARC_NODE_PARAM) continue;
        if (is_expr_node(m->kind)) continue;
        if (m->kind == ARC_NODE_FUNC_DEF) continue;
        lower_stmt(ctx, mid, block);
    }
}

/* ===================================================================
 * lower_function
 * =================================================================== */

static void lower_function(SemCtx* ctx, ArcNodeId func_node_id) {
    const ArcNode* fn = &ctx->graph->nodes[func_node_id];
    ArcRegionId body_rid = fn->attr.func.body_region;

    if (ctx->module->func_count >= ctx->module->func_cap) {
        ctx->module->func_cap = ctx->module->func_cap < 8 ? 8 : ctx->module->func_cap * 2;
        ctx->module->functions = ARC_REALLOC(ctx->module->functions,
                                             HirFunction, ctx->module->func_cap);
    }
    uint16_t func_idx = (uint16_t)ctx->module->func_count;
    HirFunction* hf = &ctx->module->functions[func_idx];
    memset(hf, 0, sizeof(*hf));
    ctx->module->func_count++;

    if (fn->attr.func.name)
        snprintf(hf->name, sizeof(hf->name), "%s", fn->attr.func.name);
    hf->arity = fn->attr.func.arity;
    hf->source_id = fn->source_id;

    register_func(ctx, hf->name, func_idx);

    SemScope saved_scope = ctx->scope;
    scope_init(&ctx->scope);
    scope_push(&ctx->scope);

    if (body_rid != ARC_INVALID_ID && body_rid < ctx->graph->region_count) {
        const ArcRegion* body = &ctx->graph->regions[body_rid];
        for (uint32_t i = 0; i < body->member_count; i++) {
            const ArcNode* member = &ctx->graph->nodes[body->members[i]];
            if (member->kind == ARC_NODE_PARAM)
                scope_add(&ctx->scope, member->attr.name);
        }
    }

    lower_region_stmts(ctx, body_rid, &hf->body);
    hf->local_count = ctx->scope.local_count;

    scope_pop(&ctx->scope);
    ctx->scope = saved_scope;
}

/* ===================================================================
 * Phase helpers for arc_semantic_lower
 * =================================================================== */

static void phase_prescan_funcs(SemCtx* ctx) {
    uint16_t next_idx = 0;
    for (uint32_t i = 0; i < ctx->graph->node_count; i++) {
        if (ctx->graph->nodes[i].kind == ARC_NODE_FUNC_DEF) {
            const char* fname = ctx->graph->nodes[i].attr.func.name;
            if (fname)
                register_func(ctx, fname, next_idx);
            next_idx++;
        }
    }
}

static void phase_lower_funcs(SemCtx* ctx) {
    for (uint32_t i = 0; i < ctx->graph->node_count; i++) {
        if (ctx->graph->nodes[i].kind == ARC_NODE_FUNC_DEF)
            lower_function(ctx, i);
    }
}

static void phase_lower_root(SemCtx* ctx, HirBlock* main_body) {
    const ArcGraph* graph = ctx->graph;
    if (graph->root_region == ARC_INVALID_ID ||
        graph->root_region >= graph->region_count) return;

    const ArcRegion* root = &graph->regions[graph->root_region];
    for (uint32_t i = 0; i < root->member_count; i++) {
        ArcNodeId nid = root->members[i];
        const ArcNode* n = &graph->nodes[nid];
        if (n->kind == ARC_NODE_FUNC_DEF) continue;
        if (n->kind == ARC_NODE_ROOT_OUTPUT) continue;
        if (n->kind == ARC_NODE_PARAM) continue;
        if (is_expr_node(n->kind)) continue;
        lower_stmt(ctx, nid, main_body);
    }
}

static void phase_lower_output(SemCtx* ctx, HirBlock* main_body) {
    const ArcGraph* graph = ctx->graph;
    if (graph->output_node != ARC_INVALID_ID &&
        graph->output_node < graph->node_count &&
        graph->nodes[graph->output_node].kind == ARC_NODE_ROOT_OUTPUT) {
        ctx->module->output_node = graph->output_node;
        lower_stmt(ctx, graph->output_node, main_body);
    }
}

/* ===================================================================
 * arc_semantic_lower — main entry point.
 * =================================================================== */

ArcSemanticResult arc_semantic_lower(const ArcGraph* graph) {
    ArcSemanticResult result;
    memset(&result, 0, sizeof(result));

    hir_module_init(&result.module);
    arc_diag_init(&result.diagnostics);

    if (!graph) {
        ArcDiagnostic* d = arc_diag_add(&result.diagnostics);
        d->severity = ARC_DIAG_ERROR;
        d->code = ARC_ERR_GRAPH_VALIDATION;
        snprintf(d->message, sizeof(d->message), "null graph passed to semantic lowering");
        d->primary.element_id = 0;
        d->primary.node_id = ARC_INVALID_ID;
        d->primary.port_role = NULL;
        d->related_count = 0;
        d->note[0] = '\0';
        result.success = false;
        return result;
    }

    SemCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.graph = graph;
    ctx.module = &result.module;
    ctx.diags = &result.diagnostics;
    ctx.had_error = false;
    ctx.func_table_count = 0;
    scope_init(&ctx.scope);

    phase_prescan_funcs(&ctx);
    phase_lower_funcs(&ctx);

    scope_push(&ctx.scope);
    phase_lower_root(&ctx, &result.module.main_body);
    phase_lower_output(&ctx, &result.module.main_body);
    scope_pop(&ctx.scope);

    result.success = !ctx.had_error;
    return result;
}

/* ===================================================================
 * arc_semantic_result_free
 * =================================================================== */

void arc_semantic_result_free(ArcSemanticResult* r) {
    hir_module_free(&r->module);
    arc_diag_free(&r->diagnostics);
    r->success = false;
}
