/*
 * Arcana test suite -- large end-to-end compilation + execution tests.
 * Split from test_pipeline.c to stay under 600-line file limit.
 */
#include "test_harness.h"

/* ================================================================
 * Shared Helpers
 * ================================================================ */

/* Compile a graph, verify, run VM, write result to *out.
 * Caller must call cleanup_e2e() with the out params when done. */
static void compile_and_run(ArcGraph* g, ArcCompileResult* cr,
                            ArcVm* vm, ArcValue* out) {
    *cr = arc_compile(g);
    if (!cr->success) {
        for (int i = 0; i < cr->error_count; i++)
            printf("    compile: %s\n", cr->errors[i].message);
    }
    ASSERT(cr->success);
    ArcVerifyResult vr = arc_verify(&cr->image);
    ASSERT(vr.valid);

    arc_vm_init(vm, &cr->image);
    vm->output = fopen(DEV_NULL, "w");
    ArcStatus s = arc_vm_run(vm);
    fclose(vm->output); vm->output = NULL;
    ASSERT(s == ARC_OK);
    *out = arc_vm_result(vm);
}

static void cleanup_e2e(ArcVm* vm, ArcCompileResult* cr, ArcGraph* g) {
    arc_vm_destroy(vm);
    arc_compile_result_free(cr);
    arc_graph_free(g);
}

/* ================================================================
 * Build helpers for the "double" function graph pattern
 * ================================================================ */

/* Build the "double(x) = x + x" function definition within a graph.
 * Adds FUNC_DEF in r0, PARAM/VAR_REF/ADD/RETURN in r_body.
 * Returns the function's body region. */
static ArcRegionId build_double_func(ArcGraph* g, ArcRegionId r0, int base_id) {
    ArcRegionId r_body = arc_graph_add_region(g, ARC_REGION_FUNCTION, r0);

    ArcNodeId n_fdef = arc_graph_add_node(g, ARC_NODE_FUNC_DEF, r0, base_id);
    g->nodes[n_fdef].attr.func.name = "double";
    g->nodes[n_fdef].attr.func.arity = 1;
    g->nodes[n_fdef].attr.func.body_region = r_body;

    ArcNodeId n_param = arc_graph_add_node(g, ARC_NODE_PARAM, r_body, base_id + 1);
    g->nodes[n_param].attr.name = "x";

    ArcNodeId n_ref1 = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_body, base_id + 2);
    g->nodes[n_ref1].attr.name = "x";
    ArcPortId p_ref1 = arc_graph_add_port(g, n_ref1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ref2 = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_body, base_id + 3);
    g->nodes[n_ref2].attr.name = "x";
    ArcPortId p_ref2 = arc_graph_add_port(g, n_ref2, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add = arc_graph_add_node(g, ARC_NODE_ADD, r_body, base_id + 4);
    ArcPortId p_lhs = arc_graph_add_port(g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId p_rhs = arc_graph_add_port(g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId p_out = arc_graph_add_port(g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId order[] = { p_lhs, p_rhs, p_out };
    arc_node_set_cyclic_order(g, n_add, order, 3);
    arc_graph_add_edge(g, p_ref1, p_lhs);
    arc_graph_add_edge(g, p_ref2, p_rhs);

    ArcNodeId n_ret = arc_graph_add_node(g, ARC_NODE_RETURN, r_body, base_id + 5);
    ArcPortId p_ret = arc_graph_add_port(g, n_ret, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_out, p_ret);

    return r_body;
}

/* Add a call: CONST_INT(arg_val) -> FUNC_CALL(func_name) -> ROOT_OUTPUT */
static void build_call_with_output(ArcGraph* g, ArcRegionId r0,
                                   const char* func_name, int64_t arg_val,
                                   int base_id) {
    ArcNodeId n_arg = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, base_id);
    g->nodes[n_arg].attr.int_value = arg_val;
    ArcPortId p_arg_out = arc_graph_add_port(g, n_arg, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_call = arc_graph_add_node(g, ARC_NODE_FUNC_CALL, r0, base_id + 1);
    g->nodes[n_call].attr.name = func_name;
    ArcPortId p_call_arg = arc_graph_add_port(g, n_call, ARC_PORT_INPUT, "arg0");
    ArcPortId p_call_out = arc_graph_add_port(g, n_call, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(g, p_arg_out, p_call_arg);

    ArcNodeId n_out = arc_graph_add_node(g, ARC_NODE_ROOT_OUTPUT, r0, base_id + 2);
    ArcPortId p_out_in = arc_graph_add_port(g, n_out, ARC_PORT_INPUT, "value");
    g->output_node = n_out;
    arc_graph_add_edge(g, p_call_out, p_out_in);
}

/* ================================================================
 * E2E Tests: Function Call
 * ================================================================ */

TEST(test_e2e_function_call) {
    /* def double(x): return x + x; output = double(7) => 14 */
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    build_double_func(&g, r0, 3001);
    build_call_with_output(&g, r0, "double", 7, 3020);

    ArcCompileResult cr; ArcVm vm;
    ArcValue result; compile_and_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 14);
    cleanup_e2e(&vm, &cr, &g);
}

/* ================================================================
 * E2E Tests: If/Else
 * ================================================================ */

/* Build "pick(flag): if flag > 0: return 42, else: return 99" */
static void build_pick_func(ArcGraph* g, ArcRegionId r0) {
    ArcRegionId r_body = arc_graph_add_region(g, ARC_REGION_FUNCTION, r0);
    ArcRegionId r_then = arc_graph_add_region(g, ARC_REGION_THEN, r_body);
    ArcRegionId r_else = arc_graph_add_region(g, ARC_REGION_ELSE, r_body);

    ArcNodeId n_fdef = arc_graph_add_node(g, ARC_NODE_FUNC_DEF, r0, 4001);
    g->nodes[n_fdef].attr.func.name = "pick";
    g->nodes[n_fdef].attr.func.arity = 1;
    g->nodes[n_fdef].attr.func.body_region = r_body;

    ArcNodeId n_param = arc_graph_add_node(g, ARC_NODE_PARAM, r_body, 4010);
    g->nodes[n_param].attr.name = "flag";

    ArcNodeId n_ref = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_body, 4011);
    g->nodes[n_ref].attr.name = "flag";
    ArcPortId p_ref_out = arc_graph_add_port(g, n_ref, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_zero = arc_graph_add_node(g, ARC_NODE_CONST_INT, r_body, 4012);
    g->nodes[n_zero].attr.int_value = 0;
    ArcPortId p_zero_out = arc_graph_add_port(g, n_zero, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_gt = arc_graph_add_node(g, ARC_NODE_GT, r_body, 4013);
    ArcPortId p_gt_lhs = arc_graph_add_port(g, n_gt, ARC_PORT_INPUT, "lhs");
    ArcPortId p_gt_rhs = arc_graph_add_port(g, n_gt, ARC_PORT_INPUT, "rhs");
    ArcPortId p_gt_out = arc_graph_add_port(g, n_gt, ARC_PORT_OUTPUT, "out");
    ArcPortId gt_order[] = { p_gt_lhs, p_gt_rhs, p_gt_out };
    arc_node_set_cyclic_order(g, n_gt, gt_order, 3);
    arc_graph_add_edge(g, p_ref_out, p_gt_lhs);
    arc_graph_add_edge(g, p_zero_out, p_gt_rhs);

    ArcNodeId n_if = arc_graph_add_node(g, ARC_NODE_IF, r_body, 4014);
    ArcPortId p_if_cond = arc_graph_add_port(g, n_if, ARC_PORT_INPUT, "cond");
    g->nodes[n_if].attr.branch.then_region = r_then;
    g->nodes[n_if].attr.branch.else_region = r_else;
    arc_graph_add_edge(g, p_gt_out, p_if_cond);

    /* Then: return 42 */
    ArcNodeId n_42 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r_then, 4020);
    g->nodes[n_42].attr.int_value = 42;
    ArcPortId p_42 = arc_graph_add_port(g, n_42, ARC_PORT_OUTPUT, "out");
    ArcNodeId n_ret_t = arc_graph_add_node(g, ARC_NODE_RETURN, r_then, 4021);
    ArcPortId p_ret_t = arc_graph_add_port(g, n_ret_t, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_42, p_ret_t);

    /* Else: return 99 */
    ArcNodeId n_99 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r_else, 4030);
    g->nodes[n_99].attr.int_value = 99;
    ArcPortId p_99 = arc_graph_add_port(g, n_99, ARC_PORT_OUTPUT, "out");
    ArcNodeId n_ret_e = arc_graph_add_node(g, ARC_NODE_RETURN, r_else, 4031);
    ArcPortId p_ret_e = arc_graph_add_port(g, n_ret_e, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_99, p_ret_e);
}

TEST(test_e2e_if_else) {
    /* pick(1) => 42 */
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    build_pick_func(&g, r0);
    build_call_with_output(&g, r0, "pick", 1, 4040);

    ArcCompileResult cr; ArcVm vm;
    ArcValue result; compile_and_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 42);
    cleanup_e2e(&vm, &cr, &g);
}

/* ================================================================
 * E2E Tests: Fibonacci
 * ================================================================ */

/* Build fib condition: n <= 1 */
static void build_fib_condition(ArcGraph* g, ArcRegionId r_body,
                                ArcPortId* p_cond_out) {
    ArcNodeId n_ref = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_body, 5011);
    g->nodes[n_ref].attr.name = "n";
    ArcPortId p_ref = arc_graph_add_port(g, n_ref, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_1 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r_body, 5012);
    g->nodes[n_1].attr.int_value = 1;
    ArcPortId p_1 = arc_graph_add_port(g, n_1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_le = arc_graph_add_node(g, ARC_NODE_LE, r_body, 5013);
    ArcPortId p_lhs = arc_graph_add_port(g, n_le, ARC_PORT_INPUT, "lhs");
    ArcPortId p_rhs = arc_graph_add_port(g, n_le, ARC_PORT_INPUT, "rhs");
    *p_cond_out = arc_graph_add_port(g, n_le, ARC_PORT_OUTPUT, "out");
    ArcPortId order[] = { p_lhs, p_rhs, *p_cond_out };
    arc_node_set_cyclic_order(g, n_le, order, 3);
    arc_graph_add_edge(g, p_ref, p_lhs);
    arc_graph_add_edge(g, p_1, p_rhs);
}

/* Build fib(n-offset) recursive call */
static ArcPortId build_fib_recursive_call(ArcGraph* g, ArcRegionId r_else,
                                          int64_t offset, int base_id) {
    ArcNodeId n_ref = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_else, base_id);
    g->nodes[n_ref].attr.name = "n";
    ArcPortId p_ref = arc_graph_add_port(g, n_ref, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_off = arc_graph_add_node(g, ARC_NODE_CONST_INT, r_else, base_id + 1);
    g->nodes[n_off].attr.int_value = offset;
    ArcPortId p_off = arc_graph_add_port(g, n_off, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_sub = arc_graph_add_node(g, ARC_NODE_SUB, r_else, base_id + 2);
    ArcPortId p_sub_lhs = arc_graph_add_port(g, n_sub, ARC_PORT_INPUT, "lhs");
    ArcPortId p_sub_rhs = arc_graph_add_port(g, n_sub, ARC_PORT_INPUT, "rhs");
    ArcPortId p_sub_out = arc_graph_add_port(g, n_sub, ARC_PORT_OUTPUT, "out");
    ArcPortId sub_order[] = { p_sub_lhs, p_sub_rhs, p_sub_out };
    arc_node_set_cyclic_order(g, n_sub, sub_order, 3);
    arc_graph_add_edge(g, p_ref, p_sub_lhs);
    arc_graph_add_edge(g, p_off, p_sub_rhs);

    ArcNodeId n_call = arc_graph_add_node(g, ARC_NODE_FUNC_CALL, r_else, base_id + 3);
    g->nodes[n_call].attr.name = "fib";
    ArcPortId p_call_arg = arc_graph_add_port(g, n_call, ARC_PORT_INPUT, "arg0");
    ArcPortId p_call_out = arc_graph_add_port(g, n_call, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(g, p_sub_out, p_call_arg);
    return p_call_out;
}

/* Build the else branch: return fib(n-1) + fib(n-2) */
static void build_fib_else(ArcGraph* g, ArcRegionId r_else) {
    ArcPortId p_call1 = build_fib_recursive_call(g, r_else, 1, 5030);
    ArcPortId p_call2 = build_fib_recursive_call(g, r_else, 2, 5034);

    ArcNodeId n_add = arc_graph_add_node(g, ARC_NODE_ADD, r_else, 5038);
    ArcPortId p_add_lhs = arc_graph_add_port(g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId p_add_rhs = arc_graph_add_port(g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId p_add_out = arc_graph_add_port(g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId add_order[] = { p_add_lhs, p_add_rhs, p_add_out };
    arc_node_set_cyclic_order(g, n_add, add_order, 3);
    arc_graph_add_edge(g, p_call1, p_add_lhs);
    arc_graph_add_edge(g, p_call2, p_add_rhs);

    ArcNodeId n_ret = arc_graph_add_node(g, ARC_NODE_RETURN, r_else, 5039);
    ArcPortId p_ret = arc_graph_add_port(g, n_ret, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_add_out, p_ret);
}

/* Build the full fib function graph */
static void build_fib_func(ArcGraph* g, ArcRegionId r0) {
    ArcRegionId r_body = arc_graph_add_region(g, ARC_REGION_FUNCTION, r0);
    ArcRegionId r_then = arc_graph_add_region(g, ARC_REGION_THEN, r_body);
    ArcRegionId r_else = arc_graph_add_region(g, ARC_REGION_ELSE, r_body);

    ArcNodeId n_fdef = arc_graph_add_node(g, ARC_NODE_FUNC_DEF, r0, 5001);
    g->nodes[n_fdef].attr.func.name = "fib";
    g->nodes[n_fdef].attr.func.arity = 1;
    g->nodes[n_fdef].attr.func.body_region = r_body;

    ArcNodeId n_param = arc_graph_add_node(g, ARC_NODE_PARAM, r_body, 5010);
    g->nodes[n_param].attr.name = "n";

    ArcPortId p_cond_out;
    build_fib_condition(g, r_body, &p_cond_out);

    ArcNodeId n_if = arc_graph_add_node(g, ARC_NODE_IF, r_body, 5014);
    ArcPortId p_if_cond = arc_graph_add_port(g, n_if, ARC_PORT_INPUT, "cond");
    g->nodes[n_if].attr.branch.then_region = r_then;
    g->nodes[n_if].attr.branch.else_region = r_else;
    arc_graph_add_edge(g, p_cond_out, p_if_cond);

    /* Then: return n */
    ArcNodeId n_ref = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_then, 5020);
    g->nodes[n_ref].attr.name = "n";
    ArcPortId p_ref = arc_graph_add_port(g, n_ref, ARC_PORT_OUTPUT, "out");
    ArcNodeId n_ret = arc_graph_add_node(g, ARC_NODE_RETURN, r_then, 5021);
    ArcPortId p_ret = arc_graph_add_port(g, n_ret, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_ref, p_ret);

    build_fib_else(g, r_else);
}

TEST(test_e2e_fibonacci) {
    /* fib(10) => 55 */
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    build_fib_func(&g, r0);
    build_call_with_output(&g, r0, "fib", 10, 5050);

    ArcCompileResult cr; ArcVm vm;
    ArcValue result; compile_and_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 55);
    cleanup_e2e(&vm, &cr, &g);
}

/* ================================================================
 * E2E Tests: While Loop
 * ================================================================ */

/* Build the while-loop body: acc = acc + i; i = i + 1 */
static void build_sum_loop_body(ArcGraph* g, ArcRegionId r_loop) {
    /* acc = acc + i */
    ArcNodeId n_ref_acc = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_loop, 6020);
    g->nodes[n_ref_acc].attr.name = "acc";
    ArcPortId p_acc = arc_graph_add_port(g, n_ref_acc, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ref_i = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_loop, 6021);
    g->nodes[n_ref_i].attr.name = "i";
    ArcPortId p_i = arc_graph_add_port(g, n_ref_i, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add = arc_graph_add_node(g, ARC_NODE_ADD, r_loop, 6022);
    ArcPortId p_add_lhs = arc_graph_add_port(g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId p_add_rhs = arc_graph_add_port(g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId p_add_out = arc_graph_add_port(g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId add_order[] = { p_add_lhs, p_add_rhs, p_add_out };
    arc_node_set_cyclic_order(g, n_add, add_order, 3);
    arc_graph_add_edge(g, p_acc, p_add_lhs);
    arc_graph_add_edge(g, p_i, p_add_rhs);

    ArcNodeId n_assign_acc = arc_graph_add_node(g, ARC_NODE_ASSIGN, r_loop, 6023);
    g->nodes[n_assign_acc].attr.name = "acc";
    ArcPortId p_asgn_acc = arc_graph_add_port(g, n_assign_acc, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_add_out, p_asgn_acc);

    /* i = i + 1 */
    ArcNodeId n_ref_i2 = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_loop, 6024);
    g->nodes[n_ref_i2].attr.name = "i";
    ArcPortId p_i2 = arc_graph_add_port(g, n_ref_i2, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_one = arc_graph_add_node(g, ARC_NODE_CONST_INT, r_loop, 6025);
    g->nodes[n_one].attr.int_value = 1;
    ArcPortId p_one = arc_graph_add_port(g, n_one, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add2 = arc_graph_add_node(g, ARC_NODE_ADD, r_loop, 6026);
    ArcPortId p_add2_lhs = arc_graph_add_port(g, n_add2, ARC_PORT_INPUT, "lhs");
    ArcPortId p_add2_rhs = arc_graph_add_port(g, n_add2, ARC_PORT_INPUT, "rhs");
    ArcPortId p_add2_out = arc_graph_add_port(g, n_add2, ARC_PORT_OUTPUT, "out");
    ArcPortId add2_order[] = { p_add2_lhs, p_add2_rhs, p_add2_out };
    arc_node_set_cyclic_order(g, n_add2, add2_order, 3);
    arc_graph_add_edge(g, p_i2, p_add2_lhs);
    arc_graph_add_edge(g, p_one, p_add2_rhs);

    ArcNodeId n_assign_i = arc_graph_add_node(g, ARC_NODE_ASSIGN, r_loop, 6027);
    g->nodes[n_assign_i].attr.name = "i";
    ArcPortId p_asgn_i = arc_graph_add_port(g, n_assign_i, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_add2_out, p_asgn_i);
}

/* Build while-loop condition and node: while i <= n { body } */
static void build_while_i_le_n(ArcGraph* g, ArcRegionId r_body, ArcRegionId r_loop) {
    ArcNodeId n_ref_i = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_body, 6015);
    g->nodes[n_ref_i].attr.name = "i";
    ArcPortId p_ref_i = arc_graph_add_port(g, n_ref_i, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ref_n = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_body, 6016);
    g->nodes[n_ref_n].attr.name = "n";
    ArcPortId p_ref_n = arc_graph_add_port(g, n_ref_n, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_le = arc_graph_add_node(g, ARC_NODE_LE, r_body, 6017);
    ArcPortId p_le_lhs = arc_graph_add_port(g, n_le, ARC_PORT_INPUT, "lhs");
    ArcPortId p_le_rhs = arc_graph_add_port(g, n_le, ARC_PORT_INPUT, "rhs");
    ArcPortId p_le_out = arc_graph_add_port(g, n_le, ARC_PORT_OUTPUT, "out");
    ArcPortId le_order[] = { p_le_lhs, p_le_rhs, p_le_out };
    arc_node_set_cyclic_order(g, n_le, le_order, 3);
    arc_graph_add_edge(g, p_ref_i, p_le_lhs);
    arc_graph_add_edge(g, p_ref_n, p_le_rhs);

    ArcNodeId n_while = arc_graph_add_node(g, ARC_NODE_WHILE, r_body, 6018);
    ArcPortId p_cond = arc_graph_add_port(g, n_while, ARC_PORT_INPUT, "cond");
    g->nodes[n_while].attr.loop.body_region = r_loop;
    arc_graph_add_edge(g, p_le_out, p_cond);
}

/* Build the sum_to function: let acc=0; let i=1; while i<=n: body; return acc */
static void build_sum_to_func(ArcGraph* g, ArcRegionId r0) {
    ArcRegionId r_body = arc_graph_add_region(g, ARC_REGION_FUNCTION, r0);
    ArcRegionId r_loop = arc_graph_add_region(g, ARC_REGION_LOOP_BODY, r_body);

    ArcNodeId n_fdef = arc_graph_add_node(g, ARC_NODE_FUNC_DEF, r0, 6001);
    g->nodes[n_fdef].attr.func.name = "sum_to";
    g->nodes[n_fdef].attr.func.arity = 1;
    g->nodes[n_fdef].attr.func.body_region = r_body;

    ArcNodeId n_param = arc_graph_add_node(g, ARC_NODE_PARAM, r_body, 6010);
    g->nodes[n_param].attr.name = "n";

    /* let acc = 0 */
    ArcNodeId n_c0 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r_body, 6011);
    g->nodes[n_c0].attr.int_value = 0;
    ArcPortId p_c0 = arc_graph_add_port(g, n_c0, ARC_PORT_OUTPUT, "out");
    ArcNodeId n_let_acc = arc_graph_add_node(g, ARC_NODE_LET, r_body, 6012);
    g->nodes[n_let_acc].attr.name = "acc";
    ArcPortId p_let_acc = arc_graph_add_port(g, n_let_acc, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_c0, p_let_acc);

    /* let i = 1 */
    ArcNodeId n_c1 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r_body, 6013);
    g->nodes[n_c1].attr.int_value = 1;
    ArcPortId p_c1 = arc_graph_add_port(g, n_c1, ARC_PORT_OUTPUT, "out");
    ArcNodeId n_let_i = arc_graph_add_node(g, ARC_NODE_LET, r_body, 6014);
    g->nodes[n_let_i].attr.name = "i";
    ArcPortId p_let_i = arc_graph_add_port(g, n_let_i, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_c1, p_let_i);

    build_while_i_le_n(g, r_body, r_loop);
    build_sum_loop_body(g, r_loop);

    /* return acc */
    ArcNodeId n_ref_acc = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_body, 6030);
    g->nodes[n_ref_acc].attr.name = "acc";
    ArcPortId p_ref_acc = arc_graph_add_port(g, n_ref_acc, ARC_PORT_OUTPUT, "out");
    ArcNodeId n_ret = arc_graph_add_node(g, ARC_NODE_RETURN, r_body, 6031);
    ArcPortId p_ret = arc_graph_add_port(g, n_ret, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_ref_acc, p_ret);
}

TEST(test_e2e_while_loop) {
    /* sum_to(10) => 55 */
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    build_sum_to_func(&g, r0);
    build_call_with_output(&g, r0, "sum_to", 10, 6040);

    ArcCompileResult cr; ArcVm vm;
    ArcValue result; compile_and_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 55);
    ASSERT(cr.image.debug.count > 0);
    cleanup_e2e(&vm, &cr, &g);
}

/* ================================================================
 * E2E Tests: NOT Operator
 * ================================================================ */

TEST(test_e2e_not_operator) {
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_true = arc_graph_add_node(&g, ARC_NODE_CONST_BOOL, r0, 8001);
    g.nodes[n_true].attr.bool_value = true;
    ArcPortId p_true = arc_graph_add_port(&g, n_true, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_not = arc_graph_add_node(&g, ARC_NODE_NOT, r0, 8002);
    ArcPortId p_not_in = arc_graph_add_port(&g, n_not, ARC_PORT_INPUT, "value");
    ArcPortId p_not_out = arc_graph_add_port(&g, n_not, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_true, p_not_in);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 8003);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_not_out, p_out_in);

    ArcCompileResult cr; ArcVm vm;
    ArcValue result; compile_and_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_BOOL);
    ASSERT(result.as.b == false);
    cleanup_e2e(&vm, &cr, &g);
}

TEST(test_e2e_not_via_fixture) {
    char path[512]; fixture_path(path, sizeof(path), "not_true.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);

    ArcCompileResult cr = arc_compile(&fr.graph);
    ASSERT(cr.success);

    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen(DEV_NULL, "w");
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    fclose(vm.output);

    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_BOOL);
    ASSERT(result.as.b == false);

    arc_vm_destroy(&vm);
    arc_compile_result_free(&cr);
    arc_graph_free(&fr.graph);
}
