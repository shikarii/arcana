#include "compiler_internal.h"

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

/* --- Region compilation --- */
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

/* --- Compile a "value" or "in" input port --- */
static void compile_value_port(Compiler* c, ArcNodeId nid) {
    ArcPortId vp = find_port_by_role(c->graph, nid, "value", ARC_PORT_INPUT);
    if (vp == ARC_INVALID_ID) vp = find_port_by_role(c->graph, nid, "in", ARC_PORT_INPUT);
    ArcNodeId src = find_source_node(c->graph, vp);
    if (src != ARC_INVALID_ID) compile_node(c, src);
}

/* --- Compile 3-input node (INDEX_SET, STR_SLICE) --- */
static void compile_3input(Compiler* c, ArcNodeId nid,
                           const char* a, const char* b, const char* d, uint8_t op) {
    const char* roles[] = {a, b, d};
    for (int i = 0; i < 3; i++) {
        ArcPortId p = find_port_by_role(c->graph, nid, roles[i], ARC_PORT_INPUT);
        ArcNodeId s = find_source_node(c->graph, p);
        if (s != ARC_INVALID_ID) compile_node(c, s);
    }
    emit_op(c, op);
}

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
        compile_3input(c, node_id, "container", "key", "value", OP_INDEX_SET);
    } else {
        compile_3input(c, node_id, "value", "start", "end", OP_STR_SLICE);
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

/* --- Closure compilation --- */
static void compile_closure(Compiler* c, ArcNodeId node_id) {
    ArcPortId fp = find_port_by_role(c->graph, node_id, "func", ARC_PORT_INPUT);
    if (fp == ARC_INVALID_ID) { cerr(c, "closure: no func input"); return; }
    ArcNodeId fs = find_source_node(c->graph, fp);
    if (fs == ARC_INVALID_ID) { cerr(c, "closure: disconnected func"); return; }
    /* Save outer compilation state */
    uint16_t save_lc = c->local_count, save_sd = (uint16_t)c->stack_depth;
    int save_ms = c->max_stack; uint16_t save_dfi = c->debug_func_idx;
    uint16_t save_ec = c->enclosing_count; uint8_t save_uvc = c->upval_count;
    memcpy(c->enclosing + c->enclosing_count, c->locals, save_lc * sizeof(LocalVar));
    c->enclosing_count += save_lc;
    c->upval_count = 0;
    /* Compile the inner function (may reference enclosing variables) */
    compile_function(c, fs);
    uint16_t fi = (uint16_t)(c->image.functions.count - 1);
    /* Attach upvalue descriptors to the function record */
    if (c->upval_count > 0) {
        ArcFuncRecord* fr = &c->image.functions.funcs[fi];
        fr->upvalue_count = c->upval_count;
        fr->upvalues = ARC_ALLOC(ArcUpvalueDesc, c->upval_count);
        for (uint8_t i = 0; i < c->upval_count; i++) {
            fr->upvalues[i].is_local = c->upvals[i].is_local;
            fr->upvalues[i].index = c->upvals[i].index;
        }
    }
    /* Restore outer state */
    c->enclosing_count = save_ec; c->upval_count = save_uvc;
    c->local_count = save_lc; c->stack_depth = save_sd;
    c->max_stack = save_ms; c->debug_func_idx = save_dfi;
    memcpy(c->locals, c->enclosing + save_ec, save_lc * sizeof(LocalVar));
    emit_byte(c, OP_CLOSURE); emit_u16(c, fi); track_stack(c, 0, 1);
}

static void compile_intrinsic_call(Compiler* c, const ArcNode* n) {
    uint8_t argc = compile_input_ports(c, n);
    emit_byte(c, OP_INTRINSIC); emit_u16(c, n->attr.intrinsic.id);
    emit_byte(c, argc); emit_byte(c, 0);
    track_stack(c, argc, 1);
}

/* --- Statement compilation --- */
static void compile_stmts(Compiler* c, ArcNodeId node_id, const ArcNode* n) {
    switch (n->kind) {
    case ARC_NODE_LET: {
        compile_value_port(c, node_id);
        uint16_t slot = add_local(c, n->attr.name);
        emit_op(c, OP_STORE_LOCAL); emit_u16(c, slot);
        break;
    }
    case ARC_NODE_ASSIGN: {
        compile_value_port(c, node_id);
        int slot = find_local(c, n->attr.name);
        if (slot >= 0) { emit_op(c, OP_STORE_LOCAL); emit_u16(c, (uint16_t)slot); }
        else if (c->enclosing_count > 0) {
            int uv = find_upvalue(c, n->attr.name);
            if (uv < 0) { cerr(c, "undefined '%s' in assign", n->attr.name); return; }
            emit_byte(c, OP_SET_UPVAL); emit_u16(c, (uint16_t)uv); track_stack(c, 1, 0);
        } else { cerr(c, "undefined '%s' in assign", n->attr.name); return; }
        break;
    }
    case ARC_NODE_PRINT:
        compile_value_port(c, node_id);
        emit_byte(c, OP_INTRINSIC); emit_u16(c, ARC_INTRINSIC_PRINT);
        emit_byte(c, 1); emit_byte(c, 0); c->stack_depth -= 1;
        break;
    case ARC_NODE_ROOT_OUTPUT: compile_value_port(c, node_id); break;
    case ARC_NODE_RETURN: {
        ArcPortId vp = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        if (vp != ARC_INVALID_ID) compile_value_port(c, node_id);
        else emit_const(c, arc_const_pool_add_null(&c->image.constants));
        emit_op(c, OP_RETURN); break;
    }
    default: break;
    }
}

/* --- Control flow compilation --- */
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
        const ArcRegion* body = &c->graph->regions[n->attr.cycle.body_region];
        uint32_t top = (uint32_t)c->code.len, exit_jump = 0;
        bool has_break = false;
        for (uint32_t bi = 0; bi < body->member_count && !has_break; bi++) {
            if (c->graph->nodes[body->members[bi]].kind != ARC_NODE_BREAK_IF) continue;
            ArcPortId cp = find_port_by_role(c->graph, body->members[bi], "cond", ARC_PORT_INPUT);
            ArcNodeId cs = find_source_node(c->graph, cp);
            if (cs != ARC_INVALID_ID) compile_node(c, cs);
            exit_jump = emit_jump(c, OP_JUMP_IF_TRUE);
            has_break = true;
        }
        compile_region_from(c, n->attr.cycle.body_region, 0);
        emit_op(c, OP_JUMP);
        emit_i32(c, (int32_t)top - (int32_t)(c->code.len + 4));
        if (has_break) patch_jump(c, exit_jump);
    }
}

/* --- Function call compilation --- */
static void compile_func_call(Compiler* c, ArcNodeId node_id, const ArcNode* n) {
    (void)node_id;
    uint8_t argc = 0;
    uint32_t cnt = n->cyclic_count > 0 ? n->cyclic_count : n->port_count;
    for (uint32_t i = 0; i < cnt; i++) {
        ArcPortId pid = n->cyclic_count > 0 ? n->cyclic_order[i] : n->ports[i];
        ArcPort* p = arc_graph_port((ArcGraph*)c->graph, pid);
        if (p && p->dir == ARC_PORT_INPUT) {
            ArcNodeId src = find_source_node(c->graph, p->id);
            if (src != ARC_INVALID_ID) { compile_node(c, src); argc++; }
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
void compile_node(Compiler* c, ArcNodeId node_id) {
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
        if (slot >= 0) { emit_op(c, OP_LOAD_LOCAL); emit_u16(c, (uint16_t)slot); }
        else if (c->enclosing_count > 0) {
            int uv = find_upvalue(c, n->attr.name);
            if (uv < 0) { cerr(c, "undefined '%s'", n->attr.name); return; }
            emit_byte(c, OP_GET_UPVAL); emit_u16(c, (uint16_t)uv); track_stack(c, 0, 1);
        } else { cerr(c, "undefined '%s'", n->attr.name); return; }
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
void compile_function(Compiler* c, ArcNodeId func_node_id) {
    const ArcNode* fn = &c->graph->nodes[func_node_id];
    c->local_count = 0; c->stack_depth = 0; c->max_stack = 0;
    uint16_t name_ci = arc_const_pool_add_string(&c->image.constants,
        fn->attr.func.name, (uint32_t)strlen(fn->attr.func.name));
    /* Check if already pre-registered (mutual recursion support) */
    uint16_t fi = UINT16_MAX;
    for (uint16_t k = 0; k < c->image.functions.count; k++) {
        if (c->image.functions.funcs[k].name_const_idx == name_ci) { fi = k; break; }
    }
    if (fi == UINT16_MAX)
        fi = arc_func_table_add(&c->image.functions,
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
