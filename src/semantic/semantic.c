#include "semantic.h"
#include <stdarg.h>

/* ===================================================================
 * Scope stack — maps variable names to local slot indices.
 * Each scope level tracks its own locals; lookup walks outward.
 * =================================================================== */

#define SEM_MAX_LOCALS 256
#define SEM_MAX_SCOPES 64

typedef struct {
    const char* name;
    uint16_t    slot;
} SemLocal;

typedef struct {
    SemLocal locals[SEM_MAX_LOCALS];
    uint16_t local_count;
    uint16_t scope_starts[SEM_MAX_SCOPES];  /* stack of local_count at scope entry */
    uint16_t scope_depth;
} SemScope;

static void scope_init(SemScope* s) {
    s->local_count = 0;
    s->scope_depth = 0;
}

static void scope_push(SemScope* s) {
    assert(s->scope_depth < SEM_MAX_SCOPES);
    s->scope_starts[s->scope_depth++] = s->local_count;
}

static void scope_pop(SemScope* s) {
    assert(s->scope_depth > 0);
    s->local_count = s->scope_starts[--s->scope_depth];
}

static int scope_find(const SemScope* s, const char* name) {
    for (int i = (int)s->local_count - 1; i >= 0; i--) {
        if (strcmp(s->locals[i].name, name) == 0)
            return (int)s->locals[i].slot;
    }
    return -1;
}

static uint16_t scope_add(SemScope* s, const char* name) {
    assert(s->local_count < SEM_MAX_LOCALS);
    uint16_t slot = s->local_count;
    s->locals[s->local_count].name = name;
    s->locals[s->local_count].slot = slot;
    s->local_count++;
    return slot;
}

/* ===================================================================
 * Lowering context — carries mutable state through the walk.
 * =================================================================== */

typedef struct {
    const ArcGraph* graph;
    HirModule*      module;
    ArcDiagList*    diags;
    SemScope        scope;
    bool            had_error;

    /* Function name -> func_idx mapping for call resolution */
    struct { const char* name; uint16_t idx; } func_table[256];
    uint16_t func_table_count;
} SemCtx;

/* --- Diagnostics helper --- */
static void sem_error(SemCtx* ctx, ArcDiagCode code,
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

/* --- Register a function name for call resolution --- */
static void register_func(SemCtx* ctx, const char* name, uint16_t idx) {
    assert(ctx->func_table_count < 256);
    ctx->func_table[ctx->func_table_count].name = name;
    ctx->func_table[ctx->func_table_count].idx = idx;
    ctx->func_table_count++;
}

static int find_func(const SemCtx* ctx, const char* name) {
    for (int i = 0; i < (int)ctx->func_table_count; i++) {
        if (strcmp(ctx->func_table[i].name, name) == 0)
            return (int)ctx->func_table[i].idx;
    }
    return -1;
}

/* ===================================================================
 * Graph navigation helpers — mirror compiler.c's topology traversal.
 * =================================================================== */

/* Find the node that produces a value into an input port (trace edge backward) */
static ArcNodeId find_source_node(const ArcGraph* g, ArcPortId input_port) {
    if (input_port == ARC_INVALID_ID) return ARC_INVALID_ID;
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].to == input_port) {
            ArcPortId from = g->edges[i].from;
            return g->ports[from].owner;
        }
    }
    return ARC_INVALID_ID;
}

/* Find a port on a node by role and direction */
static ArcPortId find_port_by_role(const ArcGraph* g, ArcNodeId node_id,
                                   const char* role, ArcPortDir dir) {
    const ArcNode* n = &g->nodes[node_id];
    for (uint32_t i = 0; i < n->port_count; i++) {
        const ArcPort* p = &g->ports[n->ports[i]];
        if (p->dir == dir && p->role && strcmp(p->role, role) == 0)
            return p->id;
    }
    return ARC_INVALID_ID;
}

/* Is this node kind an expression (pulled in via edges, not compiled as a statement)? */
static bool is_expr_node(ArcNodeKind k) {
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

/* ===================================================================
 * Map ArcNodeKind to HirExprKind for binary/unary/constant nodes.
 * =================================================================== */

static HirExprKind node_kind_to_binary_expr(ArcNodeKind k) {
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

static bool is_binary_node(ArcNodeKind k) {
    return k >= ARC_NODE_ADD && k <= ARC_NODE_GE;
}

/* ===================================================================
 * Forward declarations for mutually recursive lowering functions.
 * =================================================================== */

static HirExpr* lower_expr(SemCtx* ctx, ArcNodeId node_id);
static void     lower_stmt(SemCtx* ctx, ArcNodeId node_id, HirBlock* block);
static void     lower_region_stmts(SemCtx* ctx, ArcRegionId region_id, HirBlock* block);

/* ===================================================================
 * lower_expr — recursively lower an expression-producing graph node
 *              into a heap-allocated HirExpr tree.
 * =================================================================== */

static HirExpr* lower_expr(SemCtx* ctx, ArcNodeId node_id) {
    if (node_id >= ctx->graph->node_count) {
        sem_error(ctx, ARC_ERR_INVALID_NODE_ID, node_id, 0,
                  "invalid node id %u in expression", node_id);
        return NULL;
    }

    const ArcNode* n = &ctx->graph->nodes[node_id];

    switch (n->kind) {

    /* --- Literals --- */
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
    case ARC_NODE_CONST_NULL: {
        return hir_expr_new(HIR_CONST_NULL, n->source_id);
    }

    /* --- Binary operators --- */
    case ARC_NODE_ADD: case ARC_NODE_SUB: case ARC_NODE_MUL:
    case ARC_NODE_DIV: case ARC_NODE_MOD:
    case ARC_NODE_EQ: case ARC_NODE_NEQ:
    case ARC_NODE_LT: case ARC_NODE_LE:
    case ARC_NODE_GT: case ARC_NODE_GE: {
        /* Resolve operand order from cyclic port order, falling back to roles */
        ArcPortId lhs_port = ARC_INVALID_ID, rhs_port = ARC_INVALID_ID;

        if (n->cyclic_count >= 2) {
            int input_idx = 0;
            for (uint32_t i = 0; i < n->cyclic_count && input_idx < 2; i++) {
                const ArcPort* p = &ctx->graph->ports[n->cyclic_order[i]];
                if (p->dir == ARC_PORT_INPUT) {
                    if (input_idx == 0) lhs_port = p->id;
                    else                rhs_port = p->id;
                    input_idx++;
                }
            }
        } else {
            lhs_port = find_port_by_role(ctx->graph, node_id, "lhs", ARC_PORT_INPUT);
            rhs_port = find_port_by_role(ctx->graph, node_id, "rhs", ARC_PORT_INPUT);
        }

        if (lhs_port == ARC_INVALID_ID || rhs_port == ARC_INVALID_ID) {
            sem_error(ctx, ARC_ERR_MISSING_PORT, node_id, n->source_id,
                      "binary op node %u missing lhs/rhs input ports", node_id);
            return NULL;
        }

        ArcNodeId lhs_src = find_source_node(ctx->graph, lhs_port);
        ArcNodeId rhs_src = find_source_node(ctx->graph, rhs_port);

        if (lhs_src == ARC_INVALID_ID || rhs_src == ARC_INVALID_ID) {
            sem_error(ctx, ARC_ERR_DISCONNECTED_INPUT, node_id, n->source_id,
                      "binary op node %u has disconnected inputs", node_id);
            return NULL;
        }

        HirExpr* lhs = lower_expr(ctx, lhs_src);
        HirExpr* rhs = lower_expr(ctx, rhs_src);
        if (!lhs || !rhs) {
            ARC_FREE(lhs);
            ARC_FREE(rhs);
            return NULL;
        }

        HirExpr* e = hir_expr_new(node_kind_to_binary_expr(n->kind), n->source_id);
        e->as.binary.lhs = lhs;
        e->as.binary.rhs = rhs;
        return e;
    }

    /* --- Unary negate --- */
    case ARC_NODE_NEG: {
        ArcPortId in = find_port_by_role(ctx->graph, node_id, "value", ARC_PORT_INPUT);
        if (in == ARC_INVALID_ID)
            in = find_port_by_role(ctx->graph, node_id, "in", ARC_PORT_INPUT);

        ArcNodeId src = find_source_node(ctx->graph, in);
        if (src == ARC_INVALID_ID) {
            sem_error(ctx, ARC_ERR_DISCONNECTED_INPUT, node_id, n->source_id,
                      "neg node %u has no input", node_id);
            return NULL;
        }

        HirExpr* operand = lower_expr(ctx, src);
        if (!operand) return NULL;

        HirExpr* e = hir_expr_new(HIR_NEG, n->source_id);
        e->as.unary.operand = operand;
        return e;
    }

    /* --- Logical not --- */
    case ARC_NODE_NOT: {
        ArcPortId in = find_port_by_role(ctx->graph, node_id, "value", ARC_PORT_INPUT);
        if (in == ARC_INVALID_ID)
            in = find_port_by_role(ctx->graph, node_id, "in", ARC_PORT_INPUT);

        ArcNodeId src = find_source_node(ctx->graph, in);
        if (src == ARC_INVALID_ID) {
            sem_error(ctx, ARC_ERR_DISCONNECTED_INPUT, node_id, n->source_id,
                      "not node %u has no input", node_id);
            return NULL;
        }

        HirExpr* operand = lower_expr(ctx, src);
        if (!operand) return NULL;

        HirExpr* e = hir_expr_new(HIR_NOT, n->source_id);
        e->as.unary.operand = operand;
        return e;
    }

    /* --- Variable reference --- */
    case ARC_NODE_VAR_REF: {
        int slot = scope_find(&ctx->scope, n->attr.name);
        if (slot < 0) {
            sem_error(ctx, ARC_ERR_UNDEFINED_VARIABLE, node_id, n->source_id,
                      "undefined variable '%s'", n->attr.name);
            return NULL;
        }

        HirExpr* e = hir_expr_new(HIR_VAR_LOAD, n->source_id);
        e->as.var_idx = (uint16_t)slot;
        return e;
    }

    /* --- Function call (expression position) --- */
    case ARC_NODE_FUNC_CALL: {
        int func_idx = find_func(ctx, n->attr.name);
        if (func_idx < 0) {
            sem_error(ctx, ARC_ERR_UNDEFINED_FUNCTION, node_id, n->source_id,
                      "undefined function '%s'", n->attr.name);
            return NULL;
        }

        /* Collect arguments from input ports in cyclic order (or port order) */
        uint8_t argc = 0;
        HirExpr* args[256];

        if (n->cyclic_count > 0) {
            for (uint32_t i = 0; i < n->cyclic_count; i++) {
                const ArcPort* p = &ctx->graph->ports[n->cyclic_order[i]];
                if (p->dir == ARC_PORT_INPUT) {
                    ArcNodeId src = find_source_node(ctx->graph, p->id);
                    if (src != ARC_INVALID_ID) {
                        HirExpr* arg = lower_expr(ctx, src);
                        if (arg) args[argc++] = arg;
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
                        if (arg) args[argc++] = arg;
                    }
                }
            }
        }

        /* Check arity against registered function */
        if (func_idx < (int)ctx->module->func_count) {
            uint8_t expected = ctx->module->functions[func_idx].arity;
            if (argc != expected) {
                sem_error(ctx, ARC_ERR_ARITY_MISMATCH, node_id, n->source_id,
                          "function '%s' expects %u args, got %u",
                          n->attr.name, expected, argc);
                for (uint8_t a = 0; a < argc; a++) ARC_FREE(args[a]);
                return NULL;
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

    default:
        sem_error(ctx, ARC_ERR_UNSUPPORTED_NODE, node_id, n->source_id,
                  "unsupported expression node kind %d at node %u", n->kind, node_id);
        return NULL;
    }
}

/* ===================================================================
 * lower_value_from_port — helper to trace an input port edge and
 *                         lower the source node as an expression.
 * =================================================================== */

static HirExpr* lower_value_from_port(SemCtx* ctx, ArcNodeId node_id,
                                      const char* role, const char* alt_role) {
    ArcPortId port = find_port_by_role(ctx->graph, node_id, role, ARC_PORT_INPUT);
    if (port == ARC_INVALID_ID && alt_role)
        port = find_port_by_role(ctx->graph, node_id, alt_role, ARC_PORT_INPUT);

    if (port == ARC_INVALID_ID) return NULL;

    ArcNodeId src = find_source_node(ctx->graph, port);
    if (src == ARC_INVALID_ID) return NULL;

    return lower_expr(ctx, src);
}

/* ===================================================================
 * lower_stmt — lower a statement-producing graph node and append
 *              the resulting HirStmt to the given block.
 * =================================================================== */

static void lower_stmt(SemCtx* ctx, ArcNodeId node_id, HirBlock* block) {
    if (node_id >= ctx->graph->node_count) {
        sem_error(ctx, ARC_ERR_INVALID_NODE_ID, node_id, 0,
                  "invalid node id %u in statement", node_id);
        return;
    }

    const ArcNode* n = &ctx->graph->nodes[node_id];

    switch (n->kind) {

    /* --- Let binding --- */
    case ARC_NODE_LET: {
        HirExpr* value = lower_value_from_port(ctx, node_id, "value", "in");
        uint16_t slot = scope_add(&ctx->scope, n->attr.name);

        HirStmt* stmt = hir_block_add(block, HIR_STMT_LET, n->source_id);
        stmt->as.let.var_idx = slot;
        stmt->as.let.value = value; /* may be NULL if no initializer */
        break;
    }

    /* --- Assignment --- */
    case ARC_NODE_ASSIGN: {
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
        break;
    }

    /* --- Print --- */
    case ARC_NODE_PRINT: {
        HirExpr* value = lower_value_from_port(ctx, node_id, "value", "in");
        HirStmt* stmt = hir_block_add(block, HIR_STMT_PRINT, n->source_id);
        stmt->as.print.value = value;
        break;
    }

    /* --- Return --- */
    case ARC_NODE_RETURN: {
        HirExpr* value = lower_value_from_port(ctx, node_id, "value", NULL);
        HirStmt* stmt = hir_block_add(block, HIR_STMT_RETURN, n->source_id);
        stmt->as.ret.value = value; /* NULL means return null */
        break;
    }

    /* --- Root output (top-level expression result) --- */
    case ARC_NODE_ROOT_OUTPUT: {
        HirExpr* value = lower_value_from_port(ctx, node_id, "value", "in");
        if (value) {
            /* Lower as a print + expression-statement so HIR->bytecode
               can decide how to handle program output */
            HirStmt* stmt = hir_block_add(block, HIR_STMT_PRINT, n->source_id);
            stmt->as.print.value = value;
        }
        break;
    }

    /* --- If/else --- */
    case ARC_NODE_IF: {
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

        /* Lower then branch */
        scope_push(&ctx->scope);
        lower_region_stmts(ctx, n->attr.branch.then_region, &stmt->as.if_stmt.then_block);
        scope_pop(&ctx->scope);

        /* Lower else branch */
        if (n->attr.branch.else_region != ARC_INVALID_ID) {
            scope_push(&ctx->scope);
            lower_region_stmts(ctx, n->attr.branch.else_region, &stmt->as.if_stmt.else_block);
            scope_pop(&ctx->scope);
        }
        break;
    }

    /* --- While loop --- */
    case ARC_NODE_WHILE: {
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
        break;
    }

    /* --- Sequence node: region iteration handles child ordering --- */
    case ARC_NODE_SEQUENCE:
        break;

    /* --- Function call as statement (discard return value) --- */
    case ARC_NODE_FUNC_CALL: {
        HirExpr* call_expr = lower_expr(ctx, node_id);
        if (call_expr) {
            HirStmt* stmt = hir_block_add(block, HIR_STMT_EXPR, n->source_id);
            stmt->as.expr = call_expr;
        }
        break;
    }

    /* Skip parameter nodes — handled by lower_function */
    case ARC_NODE_PARAM:
        break;

    default:
        /* If it's a pure expression node appearing at statement level,
           wrap it as an expression-statement */
        if (is_expr_node(n->kind)) {
            HirExpr* expr = lower_expr(ctx, node_id);
            if (expr) {
                HirStmt* stmt = hir_block_add(block, HIR_STMT_EXPR, n->source_id);
                stmt->as.expr = expr;
            }
        } else {
            sem_error(ctx, ARC_ERR_UNSUPPORTED_NODE, node_id, n->source_id,
                      "unsupported statement node kind %d at node %u",
                      n->kind, node_id);
        }
        break;
    }
}

/* ===================================================================
 * lower_region_stmts — iterate a region's member nodes and lower
 *                      each statement node into the target HirBlock.
 *                      Expression-only nodes are skipped (they get
 *                      pulled in on-demand when lowering edges).
 * =================================================================== */

static void lower_region_stmts(SemCtx* ctx, ArcRegionId region_id, HirBlock* block) {
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

        /* Skip pure expression nodes and params — they're pulled in by edges */
        if (m->kind == ARC_NODE_PARAM) continue;
        if (is_expr_node(m->kind)) continue;
        /* Skip function definitions in non-root regions */
        if (m->kind == ARC_NODE_FUNC_DEF) continue;

        lower_stmt(ctx, mid, block);
    }
}

/* ===================================================================
 * lower_function — lower a FUNC_DEF node into a HirFunction.
 *                  Parameters become the first locals; body becomes
 *                  a HirBlock.
 * =================================================================== */

static void lower_function(SemCtx* ctx, ArcNodeId func_node_id) {
    const ArcNode* fn = &ctx->graph->nodes[func_node_id];
    ArcRegionId body_rid = fn->attr.func.body_region;

    /* Grow function array in module */
    if (ctx->module->func_count >= ctx->module->func_cap) {
        ctx->module->func_cap = ctx->module->func_cap < 8 ? 8 : ctx->module->func_cap * 2;
        ctx->module->functions = ARC_REALLOC(ctx->module->functions,
                                             HirFunction, ctx->module->func_cap);
    }
    uint16_t func_idx = (uint16_t)ctx->module->func_count;
    HirFunction* hf = &ctx->module->functions[func_idx];
    memset(hf, 0, sizeof(*hf));
    ctx->module->func_count++;

    /* Copy function name */
    if (fn->attr.func.name) {
        strncpy(hf->name, fn->attr.func.name, sizeof(hf->name) - 1);
        hf->name[sizeof(hf->name) - 1] = '\0';
    }
    hf->arity = fn->attr.func.arity;
    hf->source_id = fn->source_id;

    /* Register function name for call resolution */
    register_func(ctx, hf->name, func_idx);

    /* Save and reset scope for this function's body */
    SemScope saved_scope = ctx->scope;
    scope_init(&ctx->scope);
    scope_push(&ctx->scope);

    /* Add parameters as the first locals */
    if (body_rid != ARC_INVALID_ID && body_rid < ctx->graph->region_count) {
        const ArcRegion* body = &ctx->graph->regions[body_rid];
        for (uint32_t i = 0; i < body->member_count; i++) {
            const ArcNode* member = &ctx->graph->nodes[body->members[i]];
            if (member->kind == ARC_NODE_PARAM)
                scope_add(&ctx->scope, member->attr.name);
        }
    }

    /* Lower function body */
    lower_region_stmts(ctx, body_rid, &hf->body);

    hf->local_count = ctx->scope.local_count;

    /* Restore outer scope */
    scope_pop(&ctx->scope);
    ctx->scope = saved_scope;
}

/* ===================================================================
 * arc_semantic_lower — main entry point.
 *
 * Phase 1: Pre-scan for function definitions so calls can be resolved.
 * Phase 2: Lower each function body to HIR.
 * Phase 3: Lower root region non-function nodes to main_body.
 * Phase 4: Handle output node (if any).
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

    /* Phase 1: Pre-register all function definitions so forward calls resolve.
       We scan the entire graph for FUNC_DEF nodes and record their names
       with the indices they will occupy in the HirModule function array. */
    {
        uint16_t next_idx = 0;
        for (uint32_t i = 0; i < graph->node_count; i++) {
            if (graph->nodes[i].kind == ARC_NODE_FUNC_DEF) {
                const char* fname = graph->nodes[i].attr.func.name;
                if (fname)
                    register_func(&ctx, fname, next_idx);
                next_idx++;
            }
        }
        /* Clear the func table — lower_function will re-register with real indices,
           but we need the pre-scan so that the table is populated before any
           function body is lowered (enabling forward references). We keep the
           pre-scan registrations; lower_function will overwrite with same indices. */
    }

    /* Phase 2: Lower each function definition */
    for (uint32_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].kind == ARC_NODE_FUNC_DEF)
            lower_function(&ctx, i);
    }

    /* Phase 3: Lower root region statement nodes (excluding functions and root_output) */
    scope_push(&ctx.scope);

    if (graph->root_region != ARC_INVALID_ID && graph->root_region < graph->region_count) {
        const ArcRegion* root = &graph->regions[graph->root_region];
        for (uint32_t i = 0; i < root->member_count; i++) {
            ArcNodeId nid = root->members[i];
            const ArcNode* n = &graph->nodes[nid];

            if (n->kind == ARC_NODE_FUNC_DEF) continue;
            if (n->kind == ARC_NODE_ROOT_OUTPUT) continue;
            if (n->kind == ARC_NODE_PARAM) continue;
            if (is_expr_node(n->kind)) continue;

            lower_stmt(&ctx, nid, &result.module.main_body);
        }
    }

    /* Phase 4: Handle the output node last — its edges pull in all needed expressions */
    if (graph->output_node != ARC_INVALID_ID &&
        graph->output_node < graph->node_count &&
        graph->nodes[graph->output_node].kind == ARC_NODE_ROOT_OUTPUT) {

        result.module.output_node = graph->output_node;
        lower_stmt(&ctx, graph->output_node, &result.module.main_body);
    }

    scope_pop(&ctx.scope);

    result.success = !ctx.had_error;
    return result;
}

/* ===================================================================
 * arc_semantic_result_free — release all memory owned by the result.
 * =================================================================== */

void arc_semantic_result_free(ArcSemanticResult* r) {
    hir_module_free(&r->module);
    arc_diag_free(&r->diagnostics);
    r->success = false;
}
