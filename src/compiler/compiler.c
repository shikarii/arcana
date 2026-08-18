#include "compiler_internal.h"

/* --- Check if a FUNC_DEF is a closure target (skip in top-level compile loop) --- */
static bool is_closure_target(const ArcGraph* g, ArcNodeId func_id) {
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].kind != ARC_NODE_CLOSURE) continue;
        ArcPortId fp = find_port_by_role(g, i, "func", ARC_PORT_INPUT);
        if (fp != ARC_INVALID_ID && find_source_node(g, fp) == func_id) return true;
    }
    return false;
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

/* --- Patch CALL/CLOSURE instructions and debug table after inserting main at index 0 --- */
static void patch_calls_for_main(Compiler* c, uint32_t main_start) {
    for (size_t ip = 0; ip < c->code.len; ) {
        uint8_t op = c->code.data[ip];
        int ob = arc_op_operand_bytes(op);
        if (ob < 0) break;
        if (op == OP_CALL || op == OP_CLOSURE || op == OP_THREAD_SPAWN
            || op == OP_CORO_NEW) {
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
    /* Pre-register all top-level functions so mutual recursion resolves */
    for (uint32_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].kind == ARC_NODE_FUNC_DEF && !is_closure_target(graph, i)) {
            const ArcNode* fn = &graph->nodes[i];
            uint16_t nci = arc_const_pool_add_string(&c.image.constants,
                fn->attr.func.name, (uint32_t)strlen(fn->attr.func.name));
            arc_func_table_add(&c.image.functions,
                (ArcFuncRecord){ .name_const_idx = nci, .arity = fn->attr.func.arity });
        }
    }
    /* Compile all function bodies (calls can now resolve any function) */
    for (uint32_t i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].kind == ARC_NODE_FUNC_DEF && !is_closure_target(graph, i))
            compile_function(&c, i);
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
