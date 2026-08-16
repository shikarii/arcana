#include "compiler.h"
#include "../bytecode/opcodes.h"
#include <stdarg.h>

/* --- Compiler state --- */
typedef struct {
    const ArcGraph*   graph;
    ArcBytecodeImage  image;
    ArcBuf            code;       /* code being emitted */

    /* Error accumulation */
    ArcCompileError*  errors;
    int               error_count;
    int               error_cap;
    bool              had_error;

    /* Local variable table for current function */
    struct { const char* name; uint16_t slot; } locals[256];
    uint16_t local_count;

    /* Function compilation tracking */
    uint16_t current_func;
    int      stack_depth;
    int      max_stack;

    /* Debug metadata: track which function is being compiled */
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
    int pops = arc_op_pops(op);
    int pushes = arc_op_pushes(op);
    if (pops > 0) c->stack_depth -= pops;
    if (pushes > 0) c->stack_depth += pushes;
    if (c->stack_depth > c->max_stack) c->max_stack = c->stack_depth;
}

static void emit_const(Compiler* c, uint16_t idx) {
    emit_op(c, OP_CONST);
    emit_u16(c, idx);
}

static uint32_t emit_jump(Compiler* c, uint8_t op) {
    emit_op(c, op);
    uint32_t patch_pos = (uint32_t)c->code.len;
    emit_i32(c, 0); /* placeholder */
    return patch_pos;
}

static void patch_jump(Compiler* c, uint32_t patch_pos) {
    uint32_t target = (uint32_t)c->code.len;
    int32_t offset = (int32_t)target - (int32_t)(patch_pos + 4);
    c->code.data[patch_pos]     = (uint8_t)(((uint32_t)offset) & 0xFF);
    c->code.data[patch_pos + 1] = (uint8_t)((((uint32_t)offset) >> 8) & 0xFF);
    c->code.data[patch_pos + 2] = (uint8_t)((((uint32_t)offset) >> 16) & 0xFF);
    c->code.data[patch_pos + 3] = (uint8_t)((((uint32_t)offset) >> 24) & 0xFF);
}

/* --- Local variable resolution --- */
static int find_local(Compiler* c, const char* name) {
    for (int i = c->local_count - 1; i >= 0; i--) {
        if (strcmp(c->locals[i].name, name) == 0) return c->locals[i].slot;
    }
    return -1;
}

static uint16_t add_local(Compiler* c, const char* name) {
    uint16_t slot = c->local_count;
    c->locals[c->local_count].name = name;
    c->locals[c->local_count].slot = slot;
    c->local_count++;
    return slot;
}

/* --- Find the edge that feeds a given input port --- */
static ArcNodeId find_source_node(const ArcGraph* g, ArcPortId input_port) {
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].to == input_port) {
            ArcPortId from = g->edges[i].from;
            return g->ports[from].owner;
        }
    }
    return ARC_INVALID_ID;
}

/* --- Find input port by role on a node --- */
static ArcPortId find_port_by_role(const ArcGraph* g, ArcNodeId node_id, const char* role, ArcPortDir dir) {
    const ArcNode* n = &g->nodes[node_id];
    for (uint32_t i = 0; i < n->port_count; i++) {
        const ArcPort* p = &g->ports[n->ports[i]];
        if (p->dir == dir && p->role && strcmp(p->role, role) == 0)
            return p->id;
    }
    return ARC_INVALID_ID;
}

/* --- Expression node check: nodes that should only be compiled on-demand via edges --- */
static bool is_expr_node(ArcNodeKind k) {
    switch (k) {
    case ARC_NODE_CONST_INT: case ARC_NODE_CONST_FLOAT:
    case ARC_NODE_CONST_BOOL: case ARC_NODE_CONST_NULL:
    case ARC_NODE_CONST_STRING:
    case ARC_NODE_ADD: case ARC_NODE_SUB: case ARC_NODE_MUL:
    case ARC_NODE_DIV: case ARC_NODE_MOD: case ARC_NODE_NEG:
    case ARC_NODE_EQ: case ARC_NODE_NEQ: case ARC_NODE_LT:
    case ARC_NODE_LE: case ARC_NODE_GT: case ARC_NODE_GE:
    case ARC_NODE_NOT: case ARC_NODE_AND: case ARC_NODE_OR:
    case ARC_NODE_BIT_AND: case ARC_NODE_BIT_OR: case ARC_NODE_BIT_XOR:
    case ARC_NODE_BIT_NOT: case ARC_NODE_SHL: case ARC_NODE_SHR:
    case ARC_NODE_CAST_I64: case ARC_NODE_CAST_F64: case ARC_NODE_CAST_STR:
    case ARC_NODE_VAR_REF: case ARC_NODE_FUNC_CALL:
    case ARC_NODE_ARRAY_LITERAL: case ARC_NODE_MAP_LITERAL:
    case ARC_NODE_INDEX_GET: case ARC_NODE_STR_LEN: case ARC_NODE_STR_SLICE:
    case ARC_NODE_STR_INDEX: case ARC_NODE_LENGTH:
    case ARC_NODE_CLOSURE: case ARC_NODE_INTRINSIC_CALL:
        return true;
    default: return false;
    }
}

/* Compile only statement nodes from a region (skip expression-only nodes) */
static void compile_node(Compiler* c, ArcNodeId node_id);

static void compile_region_stmts(Compiler* c, ArcRegionId rid) {
    if (rid == ARC_INVALID_ID) return;
    const ArcRegion* r = &c->graph->regions[rid];
    for (uint32_t i = 0; i < r->member_count; i++) {
        const ArcNode* m = &c->graph->nodes[r->members[i]];
        if (m->kind == ARC_NODE_PARAM || is_expr_node(m->kind)) continue;
        compile_node(c, r->members[i]);
    }
}

static void compile_binary(Compiler* c, ArcNodeId node_id, uint8_t op) {
    const ArcNode* n = &c->graph->nodes[node_id];

    /* Determine operand order from cyclic port order */
    ArcPortId lhs_port = ARC_INVALID_ID, rhs_port = ARC_INVALID_ID;

    if (n->cyclic_count >= 2) {
        /* First two input ports in cyclic order are lhs, rhs */
        int input_idx = 0;
        for (uint32_t i = 0; i < n->cyclic_count && input_idx < 2; i++) {
            ArcPort* p = arc_graph_port((ArcGraph*)c->graph, n->cyclic_order[i]);
            if (p && p->dir == ARC_PORT_INPUT) {
                if (input_idx == 0) lhs_port = p->id;
                else rhs_port = p->id;
                input_idx++;
            }
        }
    } else {
        /* Fallback: find ports by role */
        lhs_port = find_port_by_role(c->graph, node_id, "lhs", ARC_PORT_INPUT);
        rhs_port = find_port_by_role(c->graph, node_id, "rhs", ARC_PORT_INPUT);
    }

    if (lhs_port == ARC_INVALID_ID || rhs_port == ARC_INVALID_ID) {
        cerr(c, "binary op node %u missing lhs/rhs input ports", node_id);
        return;
    }

    ArcNodeId lhs_src = find_source_node(c->graph, lhs_port);
    ArcNodeId rhs_src = find_source_node(c->graph, rhs_port);

    if (lhs_src == ARC_INVALID_ID || rhs_src == ARC_INVALID_ID) {
        cerr(c, "binary op node %u has disconnected inputs", node_id);
        return;
    }

    compile_node(c, lhs_src);
    compile_node(c, rhs_src);
    emit_op(c, op);
}

/* --- Compile a unary op node (single input port) --- */
static void compile_unary(Compiler* c, ArcNodeId node_id, uint8_t op) {
    ArcPortId in = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
    if (in == ARC_INVALID_ID) in = find_port_by_role(c->graph, node_id, "in", ARC_PORT_INPUT);
    ArcNodeId src = find_source_node(c->graph, in);
    if (src == ARC_INVALID_ID) { cerr(c, "unary node %u has no input", node_id); return; }
    compile_node(c, src);
    emit_op(c, op);
}

static void compile_node(Compiler* c, ArcNodeId node_id) {
    if (node_id >= c->graph->node_count) {
        cerr(c, "invalid node id %u", node_id); return;
    }
    const ArcNode* n = &c->graph->nodes[node_id];
    uint32_t debug_start = (uint32_t)c->code.len;

    switch (n->kind) {
    case ARC_NODE_CONST_INT: {
        uint16_t ci = arc_const_pool_add_i64(&c->image.constants, n->attr.int_value);
        emit_const(c, ci);
        break;
    }
    case ARC_NODE_CONST_FLOAT: {
        uint16_t ci = arc_const_pool_add_f64(&c->image.constants, n->attr.float_value);
        emit_const(c, ci);
        break;
    }
    case ARC_NODE_CONST_BOOL: {
        uint16_t ci = arc_const_pool_add_bool(&c->image.constants, n->attr.bool_value);
        emit_const(c, ci);
        break;
    }
    case ARC_NODE_CONST_NULL: {
        uint16_t ci = arc_const_pool_add_null(&c->image.constants);
        emit_const(c, ci);
        break;
    }

    case ARC_NODE_ADD: compile_binary(c, node_id, OP_ADD); break;
    case ARC_NODE_SUB: compile_binary(c, node_id, OP_SUB); break;
    case ARC_NODE_MUL: compile_binary(c, node_id, OP_MUL); break;
    case ARC_NODE_DIV: compile_binary(c, node_id, OP_DIV); break;
    case ARC_NODE_MOD: compile_binary(c, node_id, OP_MOD); break;
    case ARC_NODE_EQ:  compile_binary(c, node_id, OP_EQ);  break;
    case ARC_NODE_NEQ: compile_binary(c, node_id, OP_NEQ); break;
    case ARC_NODE_LT:  compile_binary(c, node_id, OP_LT);  break;
    case ARC_NODE_LE:  compile_binary(c, node_id, OP_LE);  break;
    case ARC_NODE_GT:  compile_binary(c, node_id, OP_GT);  break;
    case ARC_NODE_GE:  compile_binary(c, node_id, OP_GE);  break;

    case ARC_NODE_NEG: compile_unary(c, node_id, OP_NEG); break;
    case ARC_NODE_NOT: compile_unary(c, node_id, OP_NOT); break;
    case ARC_NODE_BIT_NOT: compile_unary(c, node_id, OP_BIT_NOT); break;
    case ARC_NODE_CAST_I64: compile_unary(c, node_id, OP_CAST_I64); break;
    case ARC_NODE_CAST_F64: compile_unary(c, node_id, OP_CAST_F64); break;
    case ARC_NODE_CAST_STR: compile_unary(c, node_id, OP_CAST_STR); break;
    case ARC_NODE_STR_LEN: compile_unary(c, node_id, OP_STR_LEN); break;
    case ARC_NODE_LENGTH: compile_unary(c, node_id, OP_LENGTH); break;

    case ARC_NODE_BIT_AND: compile_binary(c, node_id, OP_BIT_AND); break;
    case ARC_NODE_BIT_OR:  compile_binary(c, node_id, OP_BIT_OR); break;
    case ARC_NODE_BIT_XOR: compile_binary(c, node_id, OP_BIT_XOR); break;
    case ARC_NODE_SHL:     compile_binary(c, node_id, OP_SHL); break;
    case ARC_NODE_SHR:     compile_binary(c, node_id, OP_SHR); break;
    case ARC_NODE_STR_INDEX: compile_binary(c, node_id, OP_STR_INDEX); break;

    case ARC_NODE_CONST_STRING: {
        uint16_t ci = arc_const_pool_add_string(&c->image.constants,
            n->attr.string_value.data, n->attr.string_value.len);
        emit_const(c, ci);
        break;
    }

    /* Short-circuit AND: lhs && rhs → compile lhs, dup, jump_if_false skip, pop, compile rhs */
    case ARC_NODE_AND: {
        ArcPortId lhs_port = find_port_by_role(c->graph, node_id, "lhs", ARC_PORT_INPUT);
        ArcPortId rhs_port = find_port_by_role(c->graph, node_id, "rhs", ARC_PORT_INPUT);
        ArcNodeId lhs_src = find_source_node(c->graph, lhs_port);
        ArcNodeId rhs_src = find_source_node(c->graph, rhs_port);
        if (lhs_src == ARC_INVALID_ID || rhs_src == ARC_INVALID_ID) {
            cerr(c, "AND node %u has disconnected inputs", node_id); return;
        }
        compile_node(c, lhs_src);
        emit_op(c, OP_DUP);
        uint32_t skip = emit_jump(c, OP_JUMP_IF_FALSE);
        emit_op(c, OP_POP);
        compile_node(c, rhs_src);
        patch_jump(c, skip);
        break;
    }

    /* Short-circuit OR: lhs || rhs → compile lhs, dup, jump_if_true skip, pop, compile rhs */
    case ARC_NODE_OR: {
        ArcPortId lhs_port = find_port_by_role(c->graph, node_id, "lhs", ARC_PORT_INPUT);
        ArcPortId rhs_port = find_port_by_role(c->graph, node_id, "rhs", ARC_PORT_INPUT);
        ArcNodeId lhs_src = find_source_node(c->graph, lhs_port);
        ArcNodeId rhs_src = find_source_node(c->graph, rhs_port);
        if (lhs_src == ARC_INVALID_ID || rhs_src == ARC_INVALID_ID) {
            cerr(c, "OR node %u has disconnected inputs", node_id); return;
        }
        compile_node(c, lhs_src);
        emit_op(c, OP_DUP);
        uint32_t skip = emit_jump(c, OP_JUMP_IF_TRUE);
        emit_op(c, OP_POP);
        compile_node(c, rhs_src);
        patch_jump(c, skip);
        break;
    }

    case ARC_NODE_ARRAY_LITERAL: {
        /* Compile each element in order via input ports */
        uint16_t count = 0;
        for (uint32_t i = 0; i < n->port_count; i++) {
            ArcPort* p = arc_graph_port((ArcGraph*)c->graph, n->ports[i]);
            if (p && p->dir == ARC_PORT_INPUT) {
                ArcNodeId src = find_source_node(c->graph, p->id);
                if (src != ARC_INVALID_ID) { compile_node(c, src); count++; }
            }
        }
        emit_byte(c, OP_ARRAY_NEW);
        emit_u16(c, count);
        c->stack_depth -= count;
        c->stack_depth += 1;
        if (c->stack_depth > c->max_stack) c->max_stack = c->stack_depth;
        break;
    }

    case ARC_NODE_MAP_LITERAL: {
        /* Compile key-value pairs via input ports (key0, val0, key1, val1, ...) */
        uint16_t pair_count = 0;
        for (uint32_t i = 0; i < n->port_count; i++) {
            ArcPort* p = arc_graph_port((ArcGraph*)c->graph, n->ports[i]);
            if (p && p->dir == ARC_PORT_INPUT) {
                ArcNodeId src = find_source_node(c->graph, p->id);
                if (src != ARC_INVALID_ID) compile_node(c, src);
            }
        }
        pair_count = n->attr.collection.count;
        emit_byte(c, OP_MAP_NEW);
        emit_u16(c, pair_count);
        c->stack_depth -= (pair_count * 2);
        c->stack_depth += 1;
        if (c->stack_depth > c->max_stack) c->max_stack = c->stack_depth;
        break;
    }

    case ARC_NODE_INDEX_GET: {
        /* container[key] → compile container, compile key, emit INDEX_GET */
        compile_binary(c, node_id, OP_INDEX_GET);
        break;
    }

    case ARC_NODE_INDEX_SET: {
        /* container[key] = value → compile container, key, value, emit INDEX_SET */
        ArcPortId cont_port = find_port_by_role(c->graph, node_id, "container", ARC_PORT_INPUT);
        ArcPortId key_port = find_port_by_role(c->graph, node_id, "key", ARC_PORT_INPUT);
        ArcPortId val_port = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        ArcNodeId cont_src = find_source_node(c->graph, cont_port);
        ArcNodeId key_src = find_source_node(c->graph, key_port);
        ArcNodeId val_src = find_source_node(c->graph, val_port);
        if (cont_src != ARC_INVALID_ID) compile_node(c, cont_src);
        if (key_src != ARC_INVALID_ID) compile_node(c, key_src);
        if (val_src != ARC_INVALID_ID) compile_node(c, val_src);
        emit_op(c, OP_INDEX_SET);
        break;
    }

    case ARC_NODE_STR_SLICE: {
        /* str_slice(str, start, end) → compile str, start, end, emit STR_SLICE */
        ArcPortId str_port = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        ArcPortId start_port = find_port_by_role(c->graph, node_id, "start", ARC_PORT_INPUT);
        ArcPortId end_port = find_port_by_role(c->graph, node_id, "end", ARC_PORT_INPUT);
        ArcNodeId str_src = find_source_node(c->graph, str_port);
        ArcNodeId start_src = find_source_node(c->graph, start_port);
        ArcNodeId end_src = find_source_node(c->graph, end_port);
        if (str_src != ARC_INVALID_ID) compile_node(c, str_src);
        if (start_src != ARC_INVALID_ID) compile_node(c, start_src);
        if (end_src != ARC_INVALID_ID) compile_node(c, end_src);
        emit_op(c, OP_STR_SLICE);
        break;
    }

    case ARC_NODE_TRY: {
        /* try { ... } catch { ... } */
        uint32_t try_begin = emit_jump(c, OP_TRY_BEGIN);
        compile_region_stmts(c, n->attr.try_catch.try_region);
        emit_op(c, OP_TRY_END);
        uint32_t end_jump = emit_jump(c, OP_JUMP);
        patch_jump(c, try_begin);
        /* Catch block: thrown value is on stack */
        compile_region_stmts(c, n->attr.try_catch.catch_region);
        patch_jump(c, end_jump);
        break;
    }

    case ARC_NODE_THROW: {
        compile_unary(c, node_id, OP_THROW);
        break;
    }

    case ARC_NODE_CLOSURE: {
        /* Find the associated function and emit OP_CLOSURE */
        ArcPortId func_port = find_port_by_role(c->graph, node_id, "func", ARC_PORT_INPUT);
        if (func_port == ARC_INVALID_ID) {
            cerr(c, "closure node %u has no func input", node_id); return;
        }
        ArcNodeId func_src = find_source_node(c->graph, func_port);
        if (func_src == ARC_INVALID_ID) {
            cerr(c, "closure node %u has disconnected func input", node_id); return;
        }
        /* Find function index by name */
        const ArcNode* fn = &c->graph->nodes[func_src];
        uint16_t func_idx = UINT16_MAX;
        for (uint16_t fi = 0; fi < c->image.functions.count; fi++) {
            uint16_t ni = c->image.functions.funcs[fi].name_const_idx;
            if (ni < c->image.constants.count &&
                c->image.constants.entries[ni].tag == ARC_CONST_STRING &&
                strcmp(c->image.constants.entries[ni].as.str.data, fn->attr.func.name) == 0) {
                func_idx = fi;
                break;
            }
        }
        if (func_idx == UINT16_MAX) {
            cerr(c, "closure: undefined function '%s'", fn->attr.func.name); return;
        }
        emit_byte(c, OP_CLOSURE);
        emit_u16(c, func_idx);
        c->stack_depth += 1;
        if (c->stack_depth > c->max_stack) c->max_stack = c->stack_depth;
        break;
    }

    case ARC_NODE_INTRINSIC_CALL: {
        /* Compile arguments, then emit OP_INTRINSIC */
        uint8_t argc = 0;
        for (uint32_t i = 0; i < n->port_count; i++) {
            ArcPort* p = arc_graph_port((ArcGraph*)c->graph, n->ports[i]);
            if (p && p->dir == ARC_PORT_INPUT) {
                ArcNodeId src = find_source_node(c->graph, p->id);
                if (src != ARC_INVALID_ID) { compile_node(c, src); argc++; }
            }
        }
        emit_byte(c, OP_INTRINSIC);
        emit_u16(c, n->attr.intrinsic.id);
        emit_byte(c, argc);
        emit_byte(c, 0); /* padding */
        c->stack_depth -= argc;
        c->stack_depth += 1; /* intrinsics push a result */
        if (c->stack_depth > c->max_stack) c->max_stack = c->stack_depth;
        break;
    }

    case ARC_NODE_VAR_REF: {
        int slot = find_local(c, n->attr.name);
        if (slot < 0) { cerr(c, "undefined variable '%s'", n->attr.name); return; }
        emit_op(c, OP_LOAD_LOCAL);
        emit_u16(c, (uint16_t)slot);
        break;
    }

    case ARC_NODE_LET: {
        /* Compile the value expression */
        ArcPortId val_port = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        if (val_port != ARC_INVALID_ID) {
            ArcNodeId src = find_source_node(c->graph, val_port);
            if (src != ARC_INVALID_ID) compile_node(c, src);
        }
        uint16_t slot = add_local(c, n->attr.name);
        emit_op(c, OP_STORE_LOCAL);
        emit_u16(c, slot);
        break;
    }

    case ARC_NODE_ASSIGN: {
        ArcPortId val_port = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        ArcNodeId src = find_source_node(c->graph, val_port);
        if (src != ARC_INVALID_ID) compile_node(c, src);
        int slot = find_local(c, n->attr.name);
        if (slot < 0) { cerr(c, "undefined variable '%s' in assign", n->attr.name); return; }
        emit_op(c, OP_STORE_LOCAL);
        emit_u16(c, (uint16_t)slot);
        break;
    }

    case ARC_NODE_PRINT: {
        ArcPortId val_port = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        if (val_port == ARC_INVALID_ID) val_port = find_port_by_role(c->graph, node_id, "in", ARC_PORT_INPUT);
        ArcNodeId src = find_source_node(c->graph, val_port);
        if (src != ARC_INVALID_ID) compile_node(c, src);
        emit_byte(c, OP_INTRINSIC);
        emit_u16(c, ARC_INTRINSIC_PRINT);
        emit_byte(c, 1); /* argc */
        emit_byte(c, 0); /* padding */
        c->stack_depth -= 1; /* intrinsic consumes 1 */
        break;
    }

    case ARC_NODE_ROOT_OUTPUT: {
        ArcPortId val_port = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        if (val_port == ARC_INVALID_ID) val_port = find_port_by_role(c->graph, node_id, "in", ARC_PORT_INPUT);
        ArcNodeId src = find_source_node(c->graph, val_port);
        if (src != ARC_INVALID_ID) compile_node(c, src);
        /* Value is on stack — will be used as program result */
        break;
    }

    case ARC_NODE_IF: {
        /* Compile condition */
        ArcPortId cond_port = find_port_by_role(c->graph, node_id, "cond", ARC_PORT_INPUT);
        ArcNodeId cond_src = find_source_node(c->graph, cond_port);
        if (cond_src != ARC_INVALID_ID) compile_node(c, cond_src);

        uint32_t else_jump = emit_jump(c, OP_JUMP_IF_FALSE);

        /* Compile then region (statement nodes only) */
        compile_region_stmts(c, n->attr.branch.then_region);
        uint32_t end_jump = emit_jump(c, OP_JUMP);
        patch_jump(c, else_jump);

        /* Compile else region (statement nodes only) */
        compile_region_stmts(c, n->attr.branch.else_region);
        patch_jump(c, end_jump);
        break;
    }

    case ARC_NODE_WHILE: {
        /* Loop top: compile condition */
        uint32_t loop_top = (uint32_t)c->code.len;

        ArcPortId cond_port = find_port_by_role(c->graph, node_id, "cond", ARC_PORT_INPUT);
        ArcNodeId cond_src = find_source_node(c->graph, cond_port);
        if (cond_src != ARC_INVALID_ID) compile_node(c, cond_src);

        uint32_t exit_jump = emit_jump(c, OP_JUMP_IF_FALSE);

        /* Compile body region */
        compile_region_stmts(c, n->attr.loop.body_region);

        /* Jump back to loop top */
        emit_op(c, OP_JUMP);
        int32_t back_offset = (int32_t)loop_top - (int32_t)(c->code.len + 4);
        emit_i32(c, back_offset);

        patch_jump(c, exit_jump);
        break;
    }

    case ARC_NODE_SEQUENCE: {
        /* Sequence node: compile children in order via edges or region */
        /* For now, sequence is handled by region iteration */
        break;
    }

    case ARC_NODE_RETURN: {
        ArcPortId val_port = find_port_by_role(c->graph, node_id, "value", ARC_PORT_INPUT);
        if (val_port != ARC_INVALID_ID) {
            ArcNodeId src = find_source_node(c->graph, val_port);
            if (src != ARC_INVALID_ID) compile_node(c, src);
        } else {
            uint16_t ci = arc_const_pool_add_null(&c->image.constants);
            emit_const(c, ci);
        }
        emit_op(c, OP_RETURN);
        break;
    }

    case ARC_NODE_FUNC_CALL: {
        /* For now, find the function by name in the function table */
        /* Compile arguments first */
        /* Arguments are connected to input ports in cyclic order */
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

        /* Find function index by name — search the function table */
        /* The function name is stored in n->attr.name */
        uint16_t func_idx = UINT16_MAX;
        for (uint16_t fi = 0; fi < c->image.functions.count; fi++) {
            uint16_t ni = c->image.functions.funcs[fi].name_const_idx;
            if (ni < c->image.constants.count &&
                c->image.constants.entries[ni].tag == ARC_CONST_STRING &&
                strcmp(c->image.constants.entries[ni].as.str.data, n->attr.name) == 0) {
                func_idx = fi;
                break;
            }
        }
        if (func_idx == UINT16_MAX) {
            cerr(c, "undefined function '%s'", n->attr.name);
            return;
        }

        emit_byte(c, OP_CALL);
        emit_u16(c, func_idx);
        emit_byte(c, argc);
        emit_byte(c, 0); /* padding */
        c->stack_depth -= argc;
        c->stack_depth += 1; /* call pushes return value */
        if (c->stack_depth > c->max_stack) c->max_stack = c->stack_depth;
        break;
    }

    case ARC_NODE_PARAM: {
        /* Parameters are pre-loaded as locals by the calling convention */
        break;
    }

    default:
        cerr(c, "unsupported node kind %d at node %u", n->kind, node_id);
        break;
    }

    /* Emit debug source map entry if we generated any code */
    uint32_t debug_end = (uint32_t)c->code.len;
    if (debug_end > debug_start && n->source_id != 0) {
        ArcDebugEntry de = {
            .func_idx = c->debug_func_idx,
            .bc_start = debug_start,
            .bc_end = debug_end,
            .element_id = n->source_id
        };
        arc_debug_table_add(&c->image.debug, de);
    }
}

/* --- Compile a function definition --- */
static void compile_function(Compiler* c, ArcNodeId func_node_id) {
    const ArcNode* fn = &c->graph->nodes[func_node_id];

    /* Reset locals */
    c->local_count = 0;
    c->stack_depth = 0;
    c->max_stack = 0;

    /* Add function name to constant pool */
    uint16_t name_ci = arc_const_pool_add_string(&c->image.constants,
        fn->attr.func.name, (uint32_t)strlen(fn->attr.func.name));

    /* Register placeholder so recursive calls can resolve this function */
    uint16_t func_idx = arc_func_table_add(&c->image.functions, (ArcFuncRecord){
        .name_const_idx = name_ci, .arity = fn->attr.func.arity
    });
    c->debug_func_idx = func_idx;

    /* Add parameters as locals */
    ArcRegionId body_rid = fn->attr.func.body_region;
    const ArcRegion* body = &c->graph->regions[body_rid];

    for (uint32_t i = 0; i < body->member_count; i++) {
        const ArcNode* member = &c->graph->nodes[body->members[i]];
        if (member->kind == ARC_NODE_PARAM)
            add_local(c, member->attr.name);
    }

    uint32_t code_start = (uint32_t)c->code.len;

    /* Compile body (statement nodes only, expressions pulled in via edges) */
    compile_region_stmts(c, body_rid);

    /* Ensure function ends with a return */
    if (c->code.len == code_start ||
        c->code.data[c->code.len - 1] != OP_RETURN) {
        uint16_t null_ci = arc_const_pool_add_null(&c->image.constants);
        emit_const(c, null_ci);
        emit_op(c, OP_RETURN);
    }

    uint32_t code_end = (uint32_t)c->code.len;

    /* Update placeholder with actual code offsets/metadata */
    c->image.functions.funcs[func_idx] = (ArcFuncRecord){
        .name_const_idx = name_ci,
        .arity = fn->attr.func.arity,
        .local_count = c->local_count,
        .max_stack = (uint16_t)c->max_stack,
        .code_offset = code_start,
        .code_length = code_end - code_start
    };
}

/* --- Main compile entry point --- */
ArcCompileResult arc_compile(const ArcGraph* graph) {
    Compiler c = {0};
    c.graph = graph;
    arc_image_init(&c.image);
    arc_buf_init(&c.code);

    /* Phase 1: Compile function definitions first */
    for (uint32_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].kind == ARC_NODE_FUNC_DEF)
            compile_function(&c, i);
    }

    /* Phase 2: Compile main (root region, non-function nodes) */
    c.local_count = 0;
    c.stack_depth = 0;
    c.max_stack = 0;

    uint16_t main_name = arc_const_pool_add_string(&c.image.constants, "main", 4);
    c.debug_func_idx = 0; /* will be adjusted when we insert main at index 0 */
    uint32_t main_start = (uint32_t)c.code.len;

    const ArcRegion* root = &graph->regions[graph->root_region];
    for (uint32_t i = 0; i < root->member_count; i++) {
        const ArcNode* n = &graph->nodes[root->members[i]];
        if (n->kind == ARC_NODE_FUNC_DEF) continue;
        if (n->kind == ARC_NODE_ROOT_OUTPUT) continue; /* handled below */
        if (is_expr_node(n->kind)) continue;
        compile_node(&c, root->members[i]);
    }

    /* Compile output node last — its edge-following pulls in all needed exprs */
    if (graph->output_node != ARC_INVALID_ID &&
        graph->nodes[graph->output_node].kind == ARC_NODE_ROOT_OUTPUT) {
        compile_node(&c, graph->output_node);
        /* DUP so value remains on stack after print (for arc_vm_result) */
        emit_op(&c, OP_DUP);
        /* Print result */
        emit_byte(&c, OP_INTRINSIC);
        emit_u16(&c, ARC_INTRINSIC_PRINT);
        emit_byte(&c, 1);
        emit_byte(&c, 0);
        c.stack_depth -= 1;
    }

    emit_op(&c, OP_HALT);

    uint32_t main_end = (uint32_t)c.code.len;

    /* Insert main as function 0 by shifting everything */
    /* Actually, we need main to be at index 0. Let's prepend it. */
    /* For simplicity, reorder: main goes first in the function table */
    ArcFuncRecord main_rec = {
        .name_const_idx = main_name,
        .arity = 0,
        .local_count = c.local_count,
        .max_stack = (uint16_t)c.max_stack,
        .code_offset = main_start,
        .code_length = main_end - main_start
    };

    /* Shift existing functions up and insert main at 0 */
    if (c.image.functions.count > 0) {
        /* Need to update CALL instructions to point to shifted indices */
        arc_func_table_add(&c.image.functions, (ArcFuncRecord){0}); /* grow */
        /* Shift */
        for (int i = (int)c.image.functions.count - 1; i > 0; i--)
            c.image.functions.funcs[i] = c.image.functions.funcs[i - 1];
        c.image.functions.funcs[0] = main_rec;

        /* Patch all CALL instructions: increment func_idx by 1 */
        for (size_t ip = 0; ip < c.code.len; ) {
            uint8_t op = c.code.data[ip];
            int ob = arc_op_operand_bytes(op);
            if (ob < 0) break;
            if (op == OP_CALL) {
                uint16_t fi = arc_read_u16(c.code.data + ip + 1);
                fi += 1; /* shift for main at 0 */
                c.code.data[ip + 1] = (uint8_t)(fi & 0xFF);
                c.code.data[ip + 2] = (uint8_t)(fi >> 8);
            }
            ip += 1 + (uint32_t)ob;
        }

        /* Patch debug table func_idx: increment by 1 for shifted functions */
        for (uint32_t di = 0; di < c.image.debug.count; di++) {
            if (c.image.debug.entries[di].bc_start < main_start)
                c.image.debug.entries[di].func_idx += 1;
        }
    } else {
        arc_func_table_add(&c.image.functions, main_rec);
    }

    /* Transfer code buffer to image */
    c.image.code = c.code.data;
    c.image.code_len = (uint32_t)c.code.len;

    ArcCompileResult result;
    result.image = c.image;
    result.errors = c.errors;
    result.error_count = c.error_count;
    result.error_cap = c.error_cap;
    result.success = !c.had_error;
    /* Don't free c.code — it's owned by image now */
    return result;
}

void arc_compile_result_free(ArcCompileResult* r) {
    arc_image_free(&r->image);
    ARC_FREE(r->errors);
    r->errors = NULL; r->error_count = r->error_cap = 0;
}
