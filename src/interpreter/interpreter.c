#include "interpreter.h"
#include <stdarg.h>

/*
 * Reference interpreter — walks the semantic graph directly.
 * Used for differential testing against the compiler -> VM pipeline.
 */

typedef struct {
    const char* name;
    ArcValue    value;
} InterpVar;

typedef struct {
    uint32_t base_var;  /* variable stack base for this frame */
} InterpFrame;

typedef struct {
    const ArcGraph* graph;
    InterpVar       vars[ARC_INTERP_VARS_MAX];
    uint32_t        var_count;
    InterpFrame     frames[ARC_INTERP_FRAMES_MAX];
    uint32_t        fp;
    ArcValue        result;
    char            error[256];
    bool            had_error;
    bool            returned;
    FILE*           output;
} Interp;

static void ierr(Interp* ip, const char* fmt, ...) {
    if (ip->had_error) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(ip->error, sizeof(ip->error), fmt, ap);
    va_end(ap);
    ip->had_error = true;
}

static int find_var(Interp* ip, const char* name) {
    uint32_t base = ip->fp > 0 ? ip->frames[ip->fp - 1].base_var : 0;
    for (int i = (int)ip->var_count - 1; i >= (int)base; i--)
        if (strcmp(ip->vars[i].name, name) == 0) return i;
    return -1;
}

static void set_var(Interp* ip, const char* name, ArcValue val) {
    int idx = find_var(ip, name);
    if (idx >= 0) { ip->vars[idx].value = val; return; }
    if (ip->var_count >= ARC_INTERP_VARS_MAX) { ierr(ip, "too many variables"); return; }
    ip->vars[ip->var_count].name = name;
    ip->vars[ip->var_count].value = val;
    ip->var_count++;
}

static ArcValue get_var(Interp* ip, const char* name) {
    int idx = find_var(ip, name);
    if (idx >= 0) return ip->vars[idx].value;
    ierr(ip, "undefined variable '%s'", name);
    return arc_val_null();
}

/* Forward declaration */
static ArcValue eval_node(Interp* ip, ArcNodeId node_id);

/* Find the source node connected to a given input port */
static ArcNodeId find_source(const ArcGraph* g, ArcPortId input_port) {
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].to == input_port) {
            return g->ports[g->edges[i].from].owner;
        }
    }
    return ARC_INVALID_ID;
}

static ArcPortId find_port_by_role_dir(const ArcGraph* g, ArcNodeId nid,
                                        const char* role, ArcPortDir dir) {
    const ArcNode* n = &g->nodes[nid];
    for (uint32_t i = 0; i < n->port_count; i++) {
        const ArcPort* p = &g->ports[n->ports[i]];
        if (p->dir == dir && p->role && strcmp(p->role, role) == 0) return p->id;
    }
    return ARC_INVALID_ID;
}

static ArcValue eval_input(Interp* ip, ArcNodeId nid, const char* role) {
    ArcPortId port = find_port_by_role_dir(ip->graph, nid, role, ARC_PORT_INPUT);
    if (port == ARC_INVALID_ID) port = find_port_by_role_dir(ip->graph, nid, "in", ARC_PORT_INPUT);
    if (port == ARC_INVALID_ID) return arc_val_null();
    ArcNodeId src = find_source(ip->graph, port);
    if (src == ARC_INVALID_ID) return arc_val_null();
    return eval_node(ip, src);
}

static ArcValue eval_binary_lhs_rhs(Interp* ip, ArcNodeId nid, ArcValue* lhs, ArcValue* rhs) {
    const ArcNode* n = &ip->graph->nodes[nid];

    /* Use cyclic order if available */
    if (n->cyclic_count >= 2) {
        int input_idx = 0;
        for (uint32_t i = 0; i < n->cyclic_count && input_idx < 2; i++) {
            const ArcPort* p = &ip->graph->ports[n->cyclic_order[i]];
            if (p->dir == ARC_PORT_INPUT) {
                ArcNodeId src = find_source(ip->graph, p->id);
                ArcValue v = (src != ARC_INVALID_ID) ? eval_node(ip, src) : arc_val_null();
                if (input_idx == 0) *lhs = v; else *rhs = v;
                input_idx++;
            }
        }
    } else {
        *lhs = eval_input(ip, nid, "lhs");
        *rhs = eval_input(ip, nid, "rhs");
    }
    return arc_val_null();
}

static void exec_region_stmts(Interp* ip, ArcRegionId rid);

static ArcValue eval_node(Interp* ip, ArcNodeId node_id) {
    if (ip->had_error || ip->returned) return arc_val_null();
    if (node_id >= ip->graph->node_count) {
        ierr(ip, "invalid node %u", node_id); return arc_val_null();
    }
    const ArcNode* n = &ip->graph->nodes[node_id];

    switch (n->kind) {
    case ARC_NODE_CONST_INT:   return arc_val_i64(n->attr.int_value);
    case ARC_NODE_CONST_FLOAT: return arc_val_f64(n->attr.float_value);
    case ARC_NODE_CONST_BOOL:  return arc_val_bool(n->attr.bool_value);
    case ARC_NODE_CONST_NULL:  return arc_val_null();

    case ARC_NODE_VAR_REF: return get_var(ip, n->attr.name);

    case ARC_NODE_ADD: case ARC_NODE_SUB: case ARC_NODE_MUL:
    case ARC_NODE_DIV: case ARC_NODE_MOD: {
        ArcValue lhs, rhs;
        eval_binary_lhs_rhs(ip, node_id, &lhs, &rhs);
        if (lhs.tag == VAL_I64 && rhs.tag == VAL_I64) {
            switch (n->kind) {
            case ARC_NODE_ADD: return arc_val_i64(lhs.as.i64 + rhs.as.i64);
            case ARC_NODE_SUB: return arc_val_i64(lhs.as.i64 - rhs.as.i64);
            case ARC_NODE_MUL: return arc_val_i64(lhs.as.i64 * rhs.as.i64);
            case ARC_NODE_DIV:
                if (rhs.as.i64 == 0) { ierr(ip, "division by zero"); return arc_val_null(); }
                return arc_val_i64(lhs.as.i64 / rhs.as.i64);
            case ARC_NODE_MOD:
                if (rhs.as.i64 == 0) { ierr(ip, "modulo by zero"); return arc_val_null(); }
                return arc_val_i64(lhs.as.i64 % rhs.as.i64);
            default: break;
            }
        } else if (lhs.tag == VAL_F64 && rhs.tag == VAL_F64) {
            switch (n->kind) {
            case ARC_NODE_ADD: return arc_val_f64(lhs.as.f64 + rhs.as.f64);
            case ARC_NODE_SUB: return arc_val_f64(lhs.as.f64 - rhs.as.f64);
            case ARC_NODE_MUL: return arc_val_f64(lhs.as.f64 * rhs.as.f64);
            case ARC_NODE_DIV: return arc_val_f64(lhs.as.f64 / rhs.as.f64);
            default: break;
            }
        }
        ierr(ip, "type mismatch in arithmetic"); return arc_val_null();
    }

    case ARC_NODE_EQ: case ARC_NODE_NEQ:
    case ARC_NODE_LT: case ARC_NODE_LE:
    case ARC_NODE_GT: case ARC_NODE_GE: {
        ArcValue lhs, rhs;
        eval_binary_lhs_rhs(ip, node_id, &lhs, &rhs);
        if (n->kind == ARC_NODE_EQ)  return arc_val_bool(arc_val_equal(lhs, rhs));
        if (n->kind == ARC_NODE_NEQ) return arc_val_bool(!arc_val_equal(lhs, rhs));
        if (lhs.tag == VAL_I64 && rhs.tag == VAL_I64) {
            switch (n->kind) {
            case ARC_NODE_LT: return arc_val_bool(lhs.as.i64 < rhs.as.i64);
            case ARC_NODE_LE: return arc_val_bool(lhs.as.i64 <= rhs.as.i64);
            case ARC_NODE_GT: return arc_val_bool(lhs.as.i64 > rhs.as.i64);
            case ARC_NODE_GE: return arc_val_bool(lhs.as.i64 >= rhs.as.i64);
            default: break;
            }
        } else if (lhs.tag == VAL_F64 && rhs.tag == VAL_F64) {
            switch (n->kind) {
            case ARC_NODE_LT: return arc_val_bool(lhs.as.f64 < rhs.as.f64);
            case ARC_NODE_LE: return arc_val_bool(lhs.as.f64 <= rhs.as.f64);
            case ARC_NODE_GT: return arc_val_bool(lhs.as.f64 > rhs.as.f64);
            case ARC_NODE_GE: return arc_val_bool(lhs.as.f64 >= rhs.as.f64);
            default: break;
            }
        }
        ierr(ip, "type mismatch in comparison"); return arc_val_null();
    }

    case ARC_NODE_NEG: {
        ArcValue v = eval_input(ip, node_id, "value");
        if (v.tag == VAL_I64) return arc_val_i64(-v.as.i64);
        if (v.tag == VAL_F64) return arc_val_f64(-v.as.f64);
        ierr(ip, "neg: type mismatch"); return arc_val_null();
    }

    case ARC_NODE_NOT: {
        ArcValue v = eval_input(ip, node_id, "value");
        return arc_val_bool(!arc_val_is_truthy(v));
    }

    case ARC_NODE_FUNC_CALL: {
        /* Find the function definition */
        const char* fname = n->attr.name;
        ArcNodeId func_def = ARC_INVALID_ID;
        for (uint32_t i = 0; i < ip->graph->node_count; i++) {
            if (ip->graph->nodes[i].kind == ARC_NODE_FUNC_DEF &&
                strcmp(ip->graph->nodes[i].attr.func.name, fname) == 0) {
                func_def = i; break;
            }
        }
        if (func_def == ARC_INVALID_ID) {
            ierr(ip, "undefined function '%s'", fname); return arc_val_null();
        }

        const ArcNode* fdef = &ip->graph->nodes[func_def];

        /* Evaluate arguments */
        ArcValue args[16]; uint8_t argc = 0;
        if (n->cyclic_count > 0) {
            for (uint32_t i = 0; i < n->cyclic_count && argc < 16; i++) {
                const ArcPort* p = &ip->graph->ports[n->cyclic_order[i]];
                if (p->dir == ARC_PORT_INPUT) {
                    ArcNodeId src = find_source(ip->graph, p->id);
                    args[argc++] = (src != ARC_INVALID_ID) ? eval_node(ip, src) : arc_val_null();
                }
            }
        } else {
            for (uint32_t i = 0; i < n->port_count && argc < 16; i++) {
                const ArcPort* p = &ip->graph->ports[n->ports[i]];
                if (p->dir == ARC_PORT_INPUT) {
                    ArcNodeId src = find_source(ip->graph, p->id);
                    args[argc++] = (src != ARC_INVALID_ID) ? eval_node(ip, src) : arc_val_null();
                }
            }
        }

        /* Push call frame */
        if (ip->fp >= ARC_INTERP_FRAMES_MAX) { ierr(ip, "call stack overflow"); return arc_val_null(); }
        ip->frames[ip->fp++] = (InterpFrame){ .base_var = ip->var_count };

        /* Bind parameters */
        ArcRegionId body_rid = fdef->attr.func.body_region;
        const ArcRegion* body = &ip->graph->regions[body_rid];
        uint8_t pi = 0;
        for (uint32_t i = 0; i < body->member_count && pi < argc; i++) {
            if (ip->graph->nodes[body->members[i]].kind == ARC_NODE_PARAM)
                set_var(ip, ip->graph->nodes[body->members[i]].attr.name, args[pi++]);
        }

        /* Execute body */
        ip->returned = false;
        exec_region_stmts(ip, body_rid);

        ArcValue ret = ip->result;
        ip->returned = false;

        /* Pop frame */
        ip->fp--;
        ip->var_count = ip->frames[ip->fp].base_var;
        return ret;
    }

    case ARC_NODE_ROOT_OUTPUT:
        return eval_input(ip, node_id, "value");

    default:
        ierr(ip, "eval: unsupported node kind %d", n->kind);
        return arc_val_null();
    }
}

/* Check if a node kind is expression-only */
static bool is_interp_expr(ArcNodeKind k) {
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
    default: return false;
    }
}

static void exec_region_stmts(Interp* ip, ArcRegionId rid) {
    if (rid == ARC_INVALID_ID || ip->had_error) return;
    const ArcRegion* r = &ip->graph->regions[rid];

    for (uint32_t i = 0; i < r->member_count && !ip->had_error && !ip->returned; i++) {
        ArcNodeId mid = r->members[i];
        const ArcNode* m = &ip->graph->nodes[mid];

        if (m->kind == ARC_NODE_PARAM || is_interp_expr(m->kind)) continue;

        switch (m->kind) {
        case ARC_NODE_LET: {
            ArcValue val = eval_input(ip, mid, "value");
            set_var(ip, m->attr.name, val);
            break;
        }
        case ARC_NODE_ASSIGN: {
            ArcValue val = eval_input(ip, mid, "value");
            int idx = find_var(ip, m->attr.name);
            if (idx < 0) { ierr(ip, "undefined variable '%s'", m->attr.name); return; }
            ip->vars[idx].value = val;
            break;
        }
        case ARC_NODE_PRINT: {
            ArcValue val = eval_input(ip, mid, "value");
            FILE* out = ip->output ? ip->output : stdout;
            arc_val_print(val, out);
            fprintf(out, "\n");
            break;
        }
        case ARC_NODE_RETURN: {
            ip->result = eval_input(ip, mid, "value");
            ip->returned = true;
            return;
        }
        case ARC_NODE_IF: {
            ArcValue cond = eval_input(ip, mid, "cond");
            if (arc_val_is_truthy(cond))
                exec_region_stmts(ip, m->attr.branch.then_region);
            else
                exec_region_stmts(ip, m->attr.branch.else_region);
            break;
        }
        case ARC_NODE_WHILE: {
            int max_iter = 100000;
            while (max_iter-- > 0 && !ip->had_error && !ip->returned) {
                /* Re-evaluate condition — follow cond port edge */
                ArcValue cond = eval_input(ip, mid, "cond");
                if (!arc_val_is_truthy(cond)) break;
                exec_region_stmts(ip, m->attr.loop.body_region);
            }
            if (max_iter <= 0) ierr(ip, "loop iteration limit exceeded");
            break;
        }
        case ARC_NODE_ROOT_OUTPUT: {
            ip->result = eval_input(ip, mid, "value");
            break;
        }
        default:
            break;
        }
    }
}

ArcInterpResult arc_interpret(const ArcGraph* graph) {
    ArcInterpResult result;
    memset(&result, 0, sizeof(result));
    result.success = true;

    Interp ip = {0};
    ip.graph = graph;
    ip.output = result.output;

    /* Execute root region statements */
    const ArcRegion* root = &graph->regions[graph->root_region];

    /* Process function definitions first (they're registered by name for calls) */
    /* Process non-function, non-expression, non-output nodes */
    for (uint32_t i = 0; i < root->member_count && !ip.had_error; i++) {
        ArcNodeId mid = root->members[i];
        const ArcNode* m = &graph->nodes[mid];
        if (m->kind == ARC_NODE_FUNC_DEF) continue;
        if (m->kind == ARC_NODE_ROOT_OUTPUT) continue;
        if (is_interp_expr(m->kind)) continue;

        switch (m->kind) {
        case ARC_NODE_LET: {
            ArcValue val = eval_input(&ip, mid, "value");
            set_var(&ip, m->attr.name, val);
            break;
        }
        case ARC_NODE_ASSIGN: {
            ArcValue val = eval_input(&ip, mid, "value");
            int idx = find_var(&ip, m->attr.name);
            if (idx < 0) { ierr(&ip, "undefined variable '%s'", m->attr.name); break; }
            ip.vars[idx].value = val;
            break;
        }
        case ARC_NODE_PRINT: {
            ArcValue val = eval_input(&ip, mid, "value");
            FILE* out = ip.output ? ip.output : stdout;
            arc_val_print(val, out);
            fprintf(out, "\n");
            break;
        }
        default: break;
        }
    }

    /* Evaluate output node */
    if (graph->output_node != ARC_INVALID_ID && !ip.had_error) {
        ip.result = eval_node(&ip, graph->output_node);
    }

    result.result = ip.result;
    if (ip.had_error) {
        result.success = false;
        snprintf(result.error, sizeof(result.error), "%s", ip.error);
    }
    return result;
}
