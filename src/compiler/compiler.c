#include "compiler.h"
#include "../bytecode/opcodes.h"
#include <stdarg.h>

/* --- Compiler state --- */
typedef struct {
    const ArcGraph*   graph;
    ArcBytecodeImage  image;
    ArcBuf            code;
    ArcCompileError*  errors;
    int               error_count;
    int               error_cap;
    bool              had_error;
    struct { const char* name; uint16_t slot; } locals[256];
    uint16_t local_count;
    uint16_t current_func;
    int      stack_depth;
    int      max_stack;
    uint16_t debug_func_idx;
} Compiler;

static void cerr(Compiler* c, const char* fmt, ...) {
    if (c->error_count >= c->error_cap) {
        c->error_cap = c->error_cap < 16 ? 16 : c->error_cap * 2;
        c->errors = ARC_REALLOC(c->errors, ArcCompileError, c->error_cap);
    }
    va_list ap; va_start(ap, fmt);
    vsnprintf(c->errors[c->error_count].message,
              sizeof(c->errors[c->error_count].message), fmt, ap);
    va_end(ap);
    c->error_count++;
    c->had_error = true;
}

/* --- Code emission helpers --- */
static void emit_byte(Compiler* c, uint8_t b) { arc_buf_push(&c->code, b); }
static void emit_u16(Compiler* c, uint16_t v) { arc_buf_push_u16(&c->code, v); }
static void emit_i32(Compiler* c, int32_t v)  { arc_buf_push_i32(&c->code, v); }

static void emit_op(Compiler* c, uint8_t op) {
    emit_byte(c, op);
    int pops = arc_op_pops(op), pushes = arc_op_pushes(op);
    if (pops > 0) c->stack_depth -= pops;
    if (pushes > 0) c->stack_depth += pushes;
    if (c->stack_depth > c->max_stack) c->max_stack = c->stack_depth;
}

static void emit_const(Compiler* c, uint16_t idx) { emit_op(c, OP_CONST); emit_u16(c, idx); }

static uint32_t emit_jump(Compiler* c, uint8_t op) {
    emit_op(c, op);
    uint32_t pp = (uint32_t)c->code.len;
    emit_i32(c, 0);
    return pp;
}

static void patch_jump(Compiler* c, uint32_t pp) {
    uint32_t t = (uint32_t)c->code.len;
    int32_t off = (int32_t)t - (int32_t)(pp + 4);
    c->code.data[pp]     = (uint8_t)(((uint32_t)off) & 0xFF);
    c->code.data[pp + 1] = (uint8_t)((((uint32_t)off) >> 8) & 0xFF);
    c->code.data[pp + 2] = (uint8_t)((((uint32_t)off) >> 16) & 0xFF);
    c->code.data[pp + 3] = (uint8_t)((((uint32_t)off) >> 24) & 0xFF);
}

static void track_stack(Compiler* c, int pops, int pushes) {
    c->stack_depth -= pops;
    c->stack_depth += pushes;
    if (c->stack_depth > c->max_stack) c->max_stack = c->stack_depth;
}

/* --- Local variable resolution --- */
static int find_local(Compiler* c, const char* name) {
    for (int i = c->local_count - 1; i >= 0; i--)
        if (strcmp(c->locals[i].name, name) == 0) return c->locals[i].slot;
    return -1;
}

static uint16_t add_local(Compiler* c, const char* name) {
    uint16_t slot = c->local_count;
    c->locals[c->local_count].name = name;
    c->locals[c->local_count].slot = slot;
    c->local_count++;
    return slot;
}

/* --- Graph traversal helpers --- */
static ArcNodeId find_source_node(const ArcGraph* g, ArcPortId input_port) {
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].to == input_port)
            return g->ports[g->edges[i].from].owner;
    }
    return ARC_INVALID_ID;
}

static ArcPortId find_port_by_role(const ArcGraph* g, ArcNodeId nid, const char* role, ArcPortDir dir) {
    const ArcNode* n = &g->nodes[nid];
    for (uint32_t i = 0; i < n->port_count; i++) {
        const ArcPort* p = &g->ports[n->ports[i]];
        if (p->dir == dir && p->role && strcmp(p->role, role) == 0) return p->id;
    }
    return ARC_INVALID_ID;
}

/* --- Node-kind to opcode mapping --- */
static int binary_op_for_kind(ArcNodeKind k) {
    switch (k) {
    case ARC_NODE_ADD: return OP_ADD; case ARC_NODE_SUB: return OP_SUB;
    case ARC_NODE_MUL: return OP_MUL; case ARC_NODE_DIV: return OP_DIV;
    case ARC_NODE_MOD: return OP_MOD; case ARC_NODE_EQ:  return OP_EQ;
    case ARC_NODE_NEQ: return OP_NEQ; case ARC_NODE_LT:  return OP_LT;
    case ARC_NODE_LE:  return OP_LE;  case ARC_NODE_GT:  return OP_GT;
    case ARC_NODE_GE:  return OP_GE;
    case ARC_NODE_BIT_AND: return OP_BIT_AND; case ARC_NODE_BIT_OR: return OP_BIT_OR;
    case ARC_NODE_BIT_XOR: return OP_BIT_XOR; case ARC_NODE_SHL: return OP_SHL;
    case ARC_NODE_SHR: return OP_SHR; case ARC_NODE_STR_INDEX: return OP_STR_INDEX;
    case ARC_NODE_INDEX_GET: return OP_INDEX_GET;
    default: return -1;
    }
}

static int unary_op_for_kind(ArcNodeKind k) {
    switch (k) {
    case ARC_NODE_NEG: return OP_NEG; case ARC_NODE_NOT: return OP_NOT;
    case ARC_NODE_BIT_NOT: return OP_BIT_NOT;
    case ARC_NODE_CAST_I64: return OP_CAST_I64; case ARC_NODE_CAST_F64: return OP_CAST_F64;
    case ARC_NODE_CAST_STR: return OP_CAST_STR; case ARC_NODE_STR_LEN: return OP_STR_LEN;
    case ARC_NODE_LENGTH: return OP_LENGTH;
    default: return -1;
    }
}

/* --- Expression node check --- */
static bool is_expr_node(ArcNodeKind k) {
    if (binary_op_for_kind(k) >= 0 || unary_op_for_kind(k) >= 0) return true;
    switch (k) {
    case ARC_NODE_CONST_INT: case ARC_NODE_CONST_FLOAT:
    case ARC_NODE_CONST_BOOL: case ARC_NODE_CONST_NULL:
    case ARC_NODE_CONST_STRING: case ARC_NODE_AND: case ARC_NODE_OR:
    case ARC_NODE_VAR_REF: case ARC_NODE_FUNC_CALL:
    case ARC_NODE_ARRAY_LITERAL: case ARC_NODE_MAP_LITERAL:
    case ARC_NODE_STR_SLICE: case ARC_NODE_CLOSURE: case ARC_NODE_INTRINSIC_CALL:
        return true;
    default: return false;
    }
}

/* Forward declaration */
static void compile_node(Compiler* c, ArcNodeId node_id);

static void compile_region_from(Compiler* c, ArcRegionId rid, uint32_t start) {
    if (rid == ARC_INVALID_ID) return;
    const ArcRegion* r = &c->graph->regions[rid];
    for (uint32_t i = start; i < r->member_count; i++) {
        const ArcNode* m = &c->graph->nodes[r->members[i]];
        if (m->kind == ARC_NODE_PARAM || is_expr_node(m->kind)) continue;
        compile_node(c, r->members[i]);
    }
}
#define compile_region_stmts(c, rid) compile_region_from(c, rid, 0)

/* --- Binary/unary compilation --- */
static void compile_binary(Compiler* c, ArcNodeId node_id, uint8_t op) {
    const ArcNode* n = &c->graph->nodes[node_id];
    ArcPortId lhs_port = ARC_INVALID_ID, rhs_port = ARC_INVALID_ID;
    if (n->cyclic_count >= 2) {
        int idx = 0;
        for (uint32_t i = 0; i < n->cyclic_count && idx < 2; i++) {
            ArcPort* p = arc_graph_port((ArcGraph*)c->graph, n->cyclic_order[i]);
            if (p && p->dir == ARC_PORT_INPUT) {
                if (idx == 0) lhs_port = p->id; else rhs_port = p->id;
                idx++;
            }
        }
    } else {
        lhs_port = find_port_by_role(c->graph, node_id, "lhs", ARC_PORT_INPUT);
        rhs_port = find_port_by_role(c->graph, node_id, "rhs", ARC_PORT_INPUT);
    }
    if (lhs_port == ARC_INVALID_ID || rhs_port == ARC_INVALID_ID) {
        cerr(c, "binary op node %u missing lhs/rhs input ports", node_id); return;
    }
    ArcNodeId lhs_src = find_source_node(c->graph, lhs_port);
    ArcNodeId rhs_src = find_source_node(c->graph, rhs_port);
    if (lhs_src == ARC_INVALID_ID || rhs_src == ARC_INVALID_ID) {
        cerr(c, "binary op node %u has disconnected inputs", node_id); return;
    }
    compile_node(c, lhs_src);
    compile_node(c, rhs_src);
    emit_op(c, op);
}

static void compile_unary(Compiler* c, ArcNodeId node_id, uint8_t op) {
    ArcPortId in = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
    if (in == ARC_INVALID_ID) in = find_port_by_role(c->graph, node_id, "in", ARC_PORT_INPUT);
    ArcNodeId src = find_source_node(c->graph, in);
    if (src == ARC_INVALID_ID) { cerr(c, "unary node %u has no input", node_id); return; }
    compile_node(c, src);
    emit_op(c, op);
}

/* --- Compile input ports and count them --- */
static uint8_t compile_input_ports(Compiler* c, const ArcNode* n) {
    uint8_t count = 0;
    for (uint32_t i = 0; i < n->port_count; i++) {
        ArcPort* p = arc_graph_port((ArcGraph*)c->graph, n->ports[i]);
        if (p && p->dir == ARC_PORT_INPUT) {
            ArcNodeId src = find_source_node(c->graph, p->id);
            if (src != ARC_INVALID_ID) { compile_node(c, src); count++; }
        }
    }
    return count;
}

/* --- Node-specific helpers --- */
static void compile_const(Compiler* c, const ArcNode* n) {
    uint16_t ci;
    switch (n->kind) {
    case ARC_NODE_CONST_INT:   ci = arc_const_pool_add_i64(&c->image.constants, n->attr.int_value); break;
    case ARC_NODE_CONST_FLOAT: ci = arc_const_pool_add_f64(&c->image.constants, n->attr.float_value); break;
    case ARC_NODE_CONST_BOOL:  ci = arc_const_pool_add_bool(&c->image.constants, n->attr.bool_value); break;
    case ARC_NODE_CONST_NULL:  ci = arc_const_pool_add_null(&c->image.constants); break;
    case ARC_NODE_CONST_STRING:
        ci = arc_const_pool_add_string(&c->image.constants, n->attr.string_value.data, n->attr.string_value.len);
        break;
    default: return;
    }
    emit_const(c, ci);
}

static void compile_shortcircuit(Compiler* c, ArcNodeId node_id, bool is_or) {
    ArcPortId lp = find_port_by_role(c->graph, node_id, "lhs", ARC_PORT_INPUT);
    ArcPortId rp = find_port_by_role(c->graph, node_id, "rhs", ARC_PORT_INPUT);
    ArcNodeId ls = find_source_node(c->graph, lp), rs = find_source_node(c->graph, rp);
    if (ls == ARC_INVALID_ID || rs == ARC_INVALID_ID) {
        cerr(c, "%s node %u has disconnected inputs", is_or ? "OR" : "AND", node_id); return;
    }
    compile_node(c, ls);
    emit_op(c, OP_DUP);
    uint32_t skip = emit_jump(c, is_or ? OP_JUMP_IF_TRUE : OP_JUMP_IF_FALSE);
    emit_op(c, OP_POP);
    compile_node(c, rs);
    patch_jump(c, skip);
}

static void compile_collections(Compiler* c, ArcNodeId node_id, const ArcNode* n) {
    if (n->kind == ARC_NODE_ARRAY_LITERAL) {
        uint16_t cnt = compile_input_ports(c, n);
        emit_byte(c, OP_ARRAY_NEW); emit_u16(c, cnt); track_stack(c, cnt, 1);
    } else if (n->kind == ARC_NODE_MAP_LITERAL) {
        compile_input_ports(c, n);
        uint16_t pc = n->attr.collection.count;
        emit_byte(c, OP_MAP_NEW); emit_u16(c, pc); track_stack(c, pc * 2, 1);
    } else if (n->kind == ARC_NODE_INDEX_SET) {
        ArcPortId cp = find_port_by_role(c->graph, node_id, "container", ARC_PORT_INPUT);
        ArcPortId kp = find_port_by_role(c->graph, node_id, "key", ARC_PORT_INPUT);
        ArcPortId vp = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        ArcNodeId cs = find_source_node(c->graph, cp);
        ArcNodeId ks = find_source_node(c->graph, kp);
        ArcNodeId vs = find_source_node(c->graph, vp);
        if (cs != ARC_INVALID_ID) compile_node(c, cs);
        if (ks != ARC_INVALID_ID) compile_node(c, ks);
        if (vs != ARC_INVALID_ID) compile_node(c, vs);
        emit_op(c, OP_INDEX_SET);
    } else { /* STR_SLICE */
        ArcPortId sp = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        ArcPortId bp = find_port_by_role(c->graph, node_id, "start", ARC_PORT_INPUT);
        ArcPortId ep = find_port_by_role(c->graph, node_id, "end", ARC_PORT_INPUT);
        ArcNodeId ss = find_source_node(c->graph, sp);
        ArcNodeId bs = find_source_node(c->graph, bp);
        ArcNodeId es = find_source_node(c->graph, ep);
        if (ss != ARC_INVALID_ID) compile_node(c, ss);
        if (bs != ARC_INVALID_ID) compile_node(c, bs);
        if (es != ARC_INVALID_ID) compile_node(c, es);
        emit_op(c, OP_STR_SLICE);
    }
}

static void compile_try(Compiler* c, const ArcNode* n) {
    uint32_t tb = emit_jump(c, OP_TRY_BEGIN);
    compile_region_stmts(c, n->attr.try_catch.try_region);
    emit_op(c, OP_TRY_END);
    uint32_t ej = emit_jump(c, OP_JUMP);
    patch_jump(c, tb);
    compile_region_stmts(c, n->attr.try_catch.catch_region);
    patch_jump(c, ej);
}

static void compile_closure(Compiler* c, ArcNodeId node_id) {
    ArcPortId fp = find_port_by_role(c->graph, node_id, "func", ARC_PORT_INPUT);
    if (fp == ARC_INVALID_ID) { cerr(c, "closure node %u has no func input", node_id); return; }
    ArcNodeId fs = find_source_node(c->graph, fp);
    if (fs == ARC_INVALID_ID) { cerr(c, "closure node %u has disconnected func input", node_id); return; }
    const ArcNode* fn = &c->graph->nodes[fs];
    uint16_t fi = UINT16_MAX;
    for (uint16_t j = 0; j < c->image.functions.count; j++) {
        uint16_t ni = c->image.functions.funcs[j].name_const_idx;
        if (ni < c->image.constants.count &&
            c->image.constants.entries[ni].tag == ARC_CONST_STRING &&
            strcmp(c->image.constants.entries[ni].as.str.data, fn->attr.func.name) == 0) {
            fi = j; break;
        }
    }
    if (fi == UINT16_MAX) { cerr(c, "closure: undefined function '%s'", fn->attr.func.name); return; }
    emit_byte(c, OP_CLOSURE); emit_u16(c, fi); track_stack(c, 0, 1);
}

static void compile_intrinsic_call(Compiler* c, const ArcNode* n) {
    uint8_t argc = compile_input_ports(c, n);
    emit_byte(c, OP_INTRINSIC); emit_u16(c, n->attr.intrinsic.id);
    emit_byte(c, argc); emit_byte(c, 0);
    track_stack(c, argc, 1);
}

static void compile_stmts(Compiler* c, ArcNodeId node_id, const ArcNode* n) {
    switch (n->kind) {
    case ARC_NODE_LET: {
        ArcPortId vp = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        if (vp != ARC_INVALID_ID) {
            ArcNodeId src = find_source_node(c->graph, vp);
            if (src != ARC_INVALID_ID) compile_node(c, src);
        }
        uint16_t slot = add_local(c, n->attr.name);
        emit_op(c, OP_STORE_LOCAL); emit_u16(c, slot);
        break;
    }
    case ARC_NODE_ASSIGN: {
        ArcPortId vp = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        ArcNodeId src = find_source_node(c->graph, vp);
        if (src != ARC_INVALID_ID) compile_node(c, src);
        int slot = find_local(c, n->attr.name);
        if (slot < 0) { cerr(c, "undefined variable '%s' in assign", n->attr.name); return; }
        emit_op(c, OP_STORE_LOCAL); emit_u16(c, (uint16_t)slot);
        break;
    }
    case ARC_NODE_PRINT: {
        ArcPortId vp = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        if (vp == ARC_INVALID_ID) vp = find_port_by_role(c->graph, node_id, "in", ARC_PORT_INPUT);
        ArcNodeId src = find_source_node(c->graph, vp);
        if (src != ARC_INVALID_ID) compile_node(c, src);
        emit_byte(c, OP_INTRINSIC); emit_u16(c, ARC_INTRINSIC_PRINT);
        emit_byte(c, 1); emit_byte(c, 0); c->stack_depth -= 1;
        break;
    }
    case ARC_NODE_ROOT_OUTPUT: {
        ArcPortId vp = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        if (vp == ARC_INVALID_ID) vp = find_port_by_role(c->graph, node_id, "in", ARC_PORT_INPUT);
        ArcNodeId src = find_source_node(c->graph, vp);
        if (src != ARC_INVALID_ID) compile_node(c, src);
        break;
    }
    case ARC_NODE_RETURN: {
        ArcPortId vp = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        if (vp != ARC_INVALID_ID) {
            ArcNodeId src = find_source_node(c->graph, vp);
            if (src != ARC_INVALID_ID) compile_node(c, src);
        } else {
            emit_const(c, arc_const_pool_add_null(&c->image.constants));
        }
        emit_op(c, OP_RETURN);
        break;
    }
    default: break;
    }
}

static void compile_control(Compiler* c, ArcNodeId node_id, const ArcNode* n) {
    if (n->kind == ARC_NODE_IF) {
        ArcPortId cp = find_port_by_role(c->graph, node_id, "cond", ARC_PORT_INPUT);
        ArcNodeId cs = find_source_node(c->graph, cp);
        if (cs != ARC_INVALID_ID) compile_node(c, cs);
        uint32_t ej = emit_jump(c, OP_JUMP_IF_FALSE);
        compile_region_stmts(c, n->attr.branch.then_region);
        uint32_t endj = emit_jump(c, OP_JUMP);
        patch_jump(c, ej);
        compile_region_stmts(c, n->attr.branch.else_region);
        patch_jump(c, endj);
    } else if (n->kind == ARC_NODE_WHILE) {
        uint32_t top = (uint32_t)c->code.len;
        ArcPortId cp = find_port_by_role(c->graph, node_id, "cond", ARC_PORT_INPUT);
        ArcNodeId cs = find_source_node(c->graph, cp);
        if (cs != ARC_INVALID_ID) compile_node(c, cs);
        uint32_t ex = emit_jump(c, OP_JUMP_IF_FALSE);
        compile_region_stmts(c, n->attr.loop.body_region);
        emit_op(c, OP_JUMP);
        emit_i32(c, (int32_t)top - (int32_t)(c->code.len + 4));
        patch_jump(c, ex);
    } else { /* CYCLE — topology-derived loop */
        ArcRegionId body_rid = n->attr.cycle.body_region;
        const ArcRegion* body = &c->graph->regions[body_rid];
        uint32_t top = (uint32_t)c->code.len;
        uint32_t exit_jump = 0;
        bool has_break = false;
        /* Find BREAK_IF among members, emit condition + exit jump */
        for (uint32_t bi = 0; bi < body->member_count; bi++) {
            if (c->graph->nodes[body->members[bi]].kind == ARC_NODE_BREAK_IF) {
                ArcNodeId brk = body->members[bi];
                ArcPortId cp = find_port_by_role(c->graph, brk, "cond", ARC_PORT_INPUT);
                ArcNodeId cs = find_source_node(c->graph, cp);
                if (cs != ARC_INVALID_ID) compile_node(c, cs);
                exit_jump = emit_jump(c, OP_JUMP_IF_TRUE);
                has_break = true;
                break;
            }
        }
        compile_region_from(c, body_rid, 0);
        emit_op(c, OP_JUMP);
        emit_i32(c, (int32_t)top - (int32_t)(c->code.len + 4));
        if (has_break) patch_jump(c, exit_jump);
    }
}

static void compile_func_call(Compiler* c, ArcNodeId node_id, const ArcNode* n) {
    (void)node_id;
    uint8_t argc = 0;
    if (n->cyclic_count > 0) {
        for (uint32_t i = 0; i < n->cyclic_count; i++) {
            ArcPort* p = arc_graph_port((ArcGraph*)c->graph, n->cyclic_order[i]);
            if (p && p->dir == ARC_PORT_INPUT) {
                ArcNodeId src = find_source_node(c->graph, p->id);
                if (src != ARC_INVALID_ID) { compile_node(c, src); argc++; }
            }
        }
    } else {
        for (uint32_t i = 0; i < n->port_count; i++) {
            ArcPort* p = arc_graph_port((ArcGraph*)c->graph, n->ports[i]);
            if (p && p->dir == ARC_PORT_INPUT) {
                ArcNodeId src = find_source_node(c->graph, p->id);
                if (src != ARC_INVALID_ID) { compile_node(c, src); argc++; }
            }
        }
    }
    uint16_t func_idx = UINT16_MAX;
    for (uint16_t fi = 0; fi < c->image.functions.count; fi++) {
        uint16_t ni = c->image.functions.funcs[fi].name_const_idx;
        if (ni < c->image.constants.count &&
            c->image.constants.entries[ni].tag == ARC_CONST_STRING &&
            strcmp(c->image.constants.entries[ni].as.str.data, n->attr.name) == 0) {
            func_idx = fi; break;
        }
    }
    if (func_idx == UINT16_MAX) { cerr(c, "undefined function '%s'", n->attr.name); return; }
    emit_byte(c, OP_CALL); emit_u16(c, func_idx);
    emit_byte(c, argc); emit_byte(c, 0);
    track_stack(c, argc, 1);
}

/* --- Main compile_node: dispatch switch --- */
static void compile_node(Compiler* c, ArcNodeId node_id) {
    if (node_id >= c->graph->node_count) { cerr(c, "invalid node id %u", node_id); return; }
    const ArcNode* n = &c->graph->nodes[node_id];
    uint32_t ds = (uint32_t)c->code.len;
    int bop = binary_op_for_kind(n->kind), uop = unary_op_for_kind(n->kind);

    if (bop >= 0)      compile_binary(c, node_id, (uint8_t)bop);
    else if (uop >= 0) compile_unary(c, node_id, (uint8_t)uop);
    else switch (n->kind) {
    case ARC_NODE_CONST_INT: case ARC_NODE_CONST_FLOAT:
    case ARC_NODE_CONST_BOOL: case ARC_NODE_CONST_NULL:
    case ARC_NODE_CONST_STRING: compile_const(c, n); break;
    case ARC_NODE_AND: compile_shortcircuit(c, node_id, false); break;
    case ARC_NODE_OR:  compile_shortcircuit(c, node_id, true); break;
    case ARC_NODE_ARRAY_LITERAL: case ARC_NODE_MAP_LITERAL:
    case ARC_NODE_INDEX_SET: case ARC_NODE_STR_SLICE:
        compile_collections(c, node_id, n); break;
    case ARC_NODE_TRY: compile_try(c, n); break;
    case ARC_NODE_THROW: compile_unary(c, node_id, OP_THROW); break;
    case ARC_NODE_CLOSURE: compile_closure(c, node_id); break;
    case ARC_NODE_INTRINSIC_CALL: compile_intrinsic_call(c, n); break;
    case ARC_NODE_VAR_REF: {
        int slot = find_local(c, n->attr.name);
        if (slot < 0) { cerr(c, "undefined variable '%s'", n->attr.name); return; }
        emit_op(c, OP_LOAD_LOCAL); emit_u16(c, (uint16_t)slot);
        break;
    }
    case ARC_NODE_LET: case ARC_NODE_ASSIGN: case ARC_NODE_PRINT:
    case ARC_NODE_ROOT_OUTPUT: case ARC_NODE_RETURN:
        compile_stmts(c, node_id, n); break;
    case ARC_NODE_IF: case ARC_NODE_WHILE: case ARC_NODE_CYCLE:
        compile_control(c, node_id, n); break;
    case ARC_NODE_BREAK_IF: break; /* handled inside CYCLE compilation */
    case ARC_NODE_FUNC_CALL: compile_func_call(c, node_id, n); break;
    case ARC_NODE_SEQUENCE: case ARC_NODE_PARAM: break;
    default: cerr(c, "unsupported node kind %d at node %u", n->kind, node_id); break;
    }

    uint32_t de = (uint32_t)c->code.len;
    if (de > ds && n->source_id != 0) {
        ArcDebugEntry entry = { .func_idx = c->debug_func_idx, .bc_start = ds,
                                .bc_end = de, .element_id = n->source_id };
        arc_debug_table_add(&c->image.debug, entry);
    }
}

/* --- Compile a function definition --- */
static void compile_function(Compiler* c, ArcNodeId func_node_id) {
    const ArcNode* fn = &c->graph->nodes[func_node_id];
    c->local_count = 0; c->stack_depth = 0; c->max_stack = 0;
    uint16_t name_ci = arc_const_pool_add_string(&c->image.constants,
        fn->attr.func.name, (uint32_t)strlen(fn->attr.func.name));
    uint16_t fi = arc_func_table_add(&c->image.functions,
        (ArcFuncRecord){ .name_const_idx = name_ci, .arity = fn->attr.func.arity });
    c->debug_func_idx = fi;
    ArcRegionId body_rid = fn->attr.func.body_region;
    const ArcRegion* body = &c->graph->regions[body_rid];
    for (uint32_t i = 0; i < body->member_count; i++) {
        if (c->graph->nodes[body->members[i]].kind == ARC_NODE_PARAM)
            add_local(c, c->graph->nodes[body->members[i]].attr.name);
    }
    uint32_t cs = (uint32_t)c->code.len;
    compile_region_stmts(c, body_rid);
    if (c->code.len == cs || c->code.data[c->code.len - 1] != OP_RETURN) {
        emit_const(c, arc_const_pool_add_null(&c->image.constants));
        emit_op(c, OP_RETURN);
    }
    uint32_t ce = (uint32_t)c->code.len;
    c->image.functions.funcs[fi] = (ArcFuncRecord){
        .name_const_idx = name_ci, .arity = fn->attr.func.arity,
        .local_count = c->local_count, .max_stack = (uint16_t)c->max_stack,
        .code_offset = cs, .code_length = ce - cs };
}

/* --- Compile root region statements --- */
static void compile_root_stmts(Compiler* c) {
    const ArcRegion* root = &c->graph->regions[c->graph->root_region];
    for (uint32_t i = 0; i < root->member_count; i++) {
        const ArcNode* n = &c->graph->nodes[root->members[i]];
        if (n->kind == ARC_NODE_FUNC_DEF || n->kind == ARC_NODE_ROOT_OUTPUT) continue;
        if (is_expr_node(n->kind)) continue;
        compile_node(c, root->members[i]);
    }
    if (c->graph->output_node != ARC_INVALID_ID &&
        c->graph->nodes[c->graph->output_node].kind == ARC_NODE_ROOT_OUTPUT) {
        compile_node(c, c->graph->output_node);
        emit_op(c, OP_DUP);
        emit_byte(c, OP_INTRINSIC); emit_u16(c, ARC_INTRINSIC_PRINT);
        emit_byte(c, 1); emit_byte(c, 0); c->stack_depth -= 1;
    }
    emit_op(c, OP_HALT);
}

/* --- Patch CALL instructions and debug table after inserting main at index 0 --- */
static void patch_calls_for_main(Compiler* c, uint32_t main_start) {
    for (size_t ip = 0; ip < c->code.len; ) {
        uint8_t op = c->code.data[ip];
        int ob = arc_op_operand_bytes(op);
        if (ob < 0) break;
        if (op == OP_CALL) {
            uint16_t fi = arc_read_u16(c->code.data + ip + 1) + 1;
            c->code.data[ip + 1] = (uint8_t)(fi & 0xFF);
            c->code.data[ip + 2] = (uint8_t)(fi >> 8);
        }
        ip += 1 + (uint32_t)ob;
    }
    for (uint32_t di = 0; di < c->image.debug.count; di++) {
        if (c->image.debug.entries[di].bc_start < main_start)
            c->image.debug.entries[di].func_idx += 1;
    }
}

/* --- Main compile entry point --- */
ArcCompileResult arc_compile(const ArcGraph* graph) {
    Compiler c = {0};
    c.graph = graph;
    arc_image_init(&c.image);
    arc_buf_init(&c.code);
    for (uint32_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].kind == ARC_NODE_FUNC_DEF) compile_function(&c, i);
    }
    c.local_count = 0; c.stack_depth = 0; c.max_stack = 0;
    uint16_t mn = arc_const_pool_add_string(&c.image.constants, "main", 4);
    c.debug_func_idx = 0;
    uint32_t ms = (uint32_t)c.code.len;
    compile_root_stmts(&c);
    uint32_t me = (uint32_t)c.code.len;
    ArcFuncRecord mr = { .name_const_idx = mn, .arity = 0,
        .local_count = c.local_count, .max_stack = (uint16_t)c.max_stack,
        .code_offset = ms, .code_length = me - ms };
    if (c.image.functions.count > 0) {
        arc_func_table_add(&c.image.functions, (ArcFuncRecord){0});
        for (int i = (int)c.image.functions.count - 1; i > 0; i--)
            c.image.functions.funcs[i] = c.image.functions.funcs[i - 1];
        c.image.functions.funcs[0] = mr;
        patch_calls_for_main(&c, ms);
    } else {
        arc_func_table_add(&c.image.functions, mr);
    }
    c.image.code = c.code.data;
    c.image.code_len = (uint32_t)c.code.len;
    ArcCompileResult r;
    r.image = c.image; r.errors = c.errors;
    r.error_count = c.error_count; r.error_cap = c.error_cap;
    r.success = !c.had_error;
    return r;
}

void arc_compile_result_free(ArcCompileResult* r) {
    arc_image_free(&r->image);
    ARC_FREE(r->errors);
    r->errors = NULL; r->error_count = r->error_cap = 0;
}
