/*
 * Arcana test suite -- platform, diagnostics, and reference interpreter tests.
 * Split from test_all.c for modularity.
 */
#include "test_harness.h"

/* ================================================================
 * Platform Abstraction
 * ================================================================ */

TEST(test_platform_strdup) {
    char* s = arc_platform_strdup("hello");
    ASSERT(s != NULL);
    ASSERT(strcmp(s, "hello") == 0);
    free(s);

    /* NULL input */
    char* n = arc_platform_strdup(NULL);
    ASSERT(n == NULL);
}

TEST(test_platform_clock) {
    double t1 = arc_platform_clock();
    double t2 = arc_platform_clock();
    ASSERT(t2 >= t1);
    ASSERT(t1 >= 0.0);
}

TEST(test_platform_file_io) {
    const char* path = "test_platform_tmp.bin";
    const uint8_t data[] = { 0xDE, 0xAD, 0xBE, 0xEF };

    ArcStatus ws = arc_platform_write_file(path, data, 4);
    ASSERT(ws == ARC_OK);

    size_t len = 0;
    uint8_t* read_data = arc_platform_read_file(path, &len);
    ASSERT(read_data != NULL);
    ASSERT(len == 4);
    ASSERT(memcmp(read_data, data, 4) == 0);
    free(read_data);

    /* Read nonexistent file */
    uint8_t* bad = arc_platform_read_file("nonexistent_file_abc123.bin", &len);
    ASSERT(bad == NULL);

    remove(path);
}

/* ================================================================
 * Diagnostic Code Formatting
 * ================================================================ */

TEST(test_diag_code_formatting) {
    ASSERT(strcmp(arc_diag_code_str(ARC_DIAG_NONE), "ARC-0000") == 0);
    ASSERT(strcmp(arc_diag_code_str(ARC_DIAG_GRAPH_0001), "ARC-GRAPH-0001") == 0);
    ASSERT(strcmp(arc_diag_code_str(ARC_DIAG_GRAPH_0008), "ARC-GRAPH-0008") == 0);
    ASSERT(strcmp(arc_diag_code_str(ARC_DIAG_SEM_0001), "ARC-SEM-0001") == 0);
    ASSERT(strcmp(arc_diag_code_str(ARC_DIAG_SEM_0004), "ARC-SEM-0004") == 0);
    ASSERT(strcmp(arc_diag_code_str(ARC_DIAG_CODE_0001), "ARC-CODE-0001") == 0);
}

TEST(test_diag_backward_compat) {
    /* Backward-compatible aliases should map to the same numeric values */
    ASSERT(ARC_ERR_UNDEFINED_VARIABLE == ARC_DIAG_SEM_0001);
    ASSERT(ARC_ERR_UNDEFINED_FUNCTION == ARC_DIAG_SEM_0002);
    ASSERT(ARC_ERR_ARITY_MISMATCH == ARC_DIAG_SEM_0003);
    ASSERT(ARC_ERR_MISSING_PORT == ARC_DIAG_GRAPH_0003);
    ASSERT(ARC_ERR_GRAPH_VALIDATION == ARC_DIAG_GRAPH_0005);
    ASSERT(ARC_ERR_INVALID_NODE_ID == ARC_DIAG_GRAPH_0001);
}

/* ================================================================
 * String Runtime Values
 * ================================================================ */

TEST(test_string_values) {
    /* Test GC-managed string creation and equality */
    ArcGC gc;
    arc_gc_init(&gc);

    ArcObjString* s1 = arc_obj_string_new(&gc, "hello", 5);
    ASSERT(s1->len == 5);
    ASSERT(strcmp(s1->data, "hello") == 0);

    ArcObjString* s2 = arc_obj_string_new(&gc, "hello", 5);
    ArcValue v1 = arc_val_obj((ArcObject*)s1);
    ArcValue v2 = arc_val_obj((ArcObject*)s2);
    ASSERT(arc_val_equal(v1, v2));
    ASSERT(arc_val_is_truthy(v1));

    ArcObjString* empty = arc_obj_string_new(&gc, "", 0);
    ArcValue ve = arc_val_obj((ArcObject*)empty);
    ASSERT(arc_val_is_truthy(ve));  /* non-null object is truthy */

    arc_gc_free_all(&gc);
}

/* ================================================================
 * Structured Diagnostics
 * ================================================================ */

TEST(test_structured_diagnostics) {
    ArcDiagList diags;
    arc_diag_init(&diags);

    ArcDiagnostic* d = arc_diag_add(&diags);
    d->severity = ARC_DIAG_ERROR;
    d->code = ARC_ERR_MISSING_PORT;
    snprintf(d->message, sizeof(d->message), "missing port 'lhs' on node 5");
    d->primary.element_id = 42;
    d->primary.node_id = 5;
    d->primary.port_role = "lhs";

    ASSERT(diags.count == 1);
    ASSERT(diags.items[0].severity == ARC_DIAG_ERROR);
    ASSERT(diags.items[0].code == ARC_ERR_MISSING_PORT);
    ASSERT(diags.items[0].primary.element_id == 42);

    /* Add a related ref */
    d->related[0].ref.element_id = 99;
    snprintf(d->related[0].label, sizeof(d->related[0].label), "defined here");
    d->related_count = 1;

    /* Print to NUL to exercise the printer */
    FILE* f = fopen(DEV_NULL, "w");
    arc_diag_print(&diags, f);
    fclose(f);

    arc_diag_free(&diags);
    ASSERT(diags.count == 0);
}

/* ================================================================
 * Differential Reference Interpreter
 * ================================================================ */

TEST(test_ref_interp_5_plus_10) {
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_5 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);
    g.nodes[n_5].attr.int_value = 5;
    ArcPortId p5 = arc_graph_add_port(&g, n_5, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_10 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);
    g.nodes[n_10].attr.int_value = 10;
    ArcPortId p10 = arc_graph_add_port(&g, n_10, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 0);
    ArcPortId pa_lhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId pa_rhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId pa_out = arc_graph_add_port(&g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId pa_order[] = { pa_lhs, pa_rhs, pa_out };
    arc_node_set_cyclic_order(&g, n_add, pa_order, 3);
    arc_graph_add_edge(&g, p5, pa_lhs);
    arc_graph_add_edge(&g, p10, pa_rhs);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 0);
    ArcPortId po_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, pa_out, po_in);

    /* Reference interpreter */
    ArcInterpResult ir = arc_interpret(&g);
    ASSERT(ir.success);
    ASSERT(ir.result.tag == VAL_I64);
    ASSERT_EQ_I64(ir.result.as.i64, 15);

    /* Compiler -> VM */
    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);
    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen(DEV_NULL, "w");
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    fclose(vm.output);
    ArcValue vm_result = arc_vm_result(&vm);

    /* Differential comparison */
    ASSERT(arc_val_equal(ir.result, vm_result));

    arc_vm_destroy(&vm);
    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* Build the double(x)=x+x, double(7) graph */
static void build_double_graph(ArcGraph* g) {
    arc_graph_init(g);
    ArcRegionId r0 = arc_graph_add_region(g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g->root_region = r0;
    ArcRegionId r_body = arc_graph_add_region(g, ARC_REGION_FUNCTION, r0);

    ArcNodeId n_fdef = arc_graph_add_node(g, ARC_NODE_FUNC_DEF, r0, 0);
    g->nodes[n_fdef].attr.func.name = "double";
    g->nodes[n_fdef].attr.func.arity = 1;
    g->nodes[n_fdef].attr.func.body_region = r_body;

    ArcNodeId n_param = arc_graph_add_node(g, ARC_NODE_PARAM, r_body, 0);
    g->nodes[n_param].attr.name = "x";

    ArcNodeId n_ref1 = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_body, 0);
    g->nodes[n_ref1].attr.name = "x";
    ArcPortId p_ref1 = arc_graph_add_port(g, n_ref1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ref2 = arc_graph_add_node(g, ARC_NODE_VAR_REF, r_body, 0);
    g->nodes[n_ref2].attr.name = "x";
    ArcPortId p_ref2 = arc_graph_add_port(g, n_ref2, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add = arc_graph_add_node(g, ARC_NODE_ADD, r_body, 0);
    ArcPortId p_lhs = arc_graph_add_port(g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId p_rhs = arc_graph_add_port(g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId p_out = arc_graph_add_port(g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId order[] = { p_lhs, p_rhs, p_out };
    arc_node_set_cyclic_order(g, n_add, order, 3);
    arc_graph_add_edge(g, p_ref1, p_lhs);
    arc_graph_add_edge(g, p_ref2, p_rhs);

    ArcNodeId n_ret = arc_graph_add_node(g, ARC_NODE_RETURN, r_body, 0);
    ArcPortId p_ret = arc_graph_add_port(g, n_ret, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_out, p_ret);

    ArcNodeId n_7 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, 0);
    g->nodes[n_7].attr.int_value = 7;
    ArcPortId p_7 = arc_graph_add_port(g, n_7, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_call = arc_graph_add_node(g, ARC_NODE_FUNC_CALL, r0, 0);
    g->nodes[n_call].attr.name = "double";
    ArcPortId p_call_arg = arc_graph_add_port(g, n_call, ARC_PORT_INPUT, "arg0");
    ArcPortId p_call_out = arc_graph_add_port(g, n_call, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(g, p_7, p_call_arg);

    ArcNodeId n_root = arc_graph_add_node(g, ARC_NODE_ROOT_OUTPUT, r0, 0);
    ArcPortId p_root_in = arc_graph_add_port(g, n_root, ARC_PORT_INPUT, "value");
    g->output_node = n_root;
    arc_graph_add_edge(g, p_call_out, p_root_in);
}

TEST(test_ref_interp_function) {
    ArcGraph g;
    build_double_graph(&g);

    ArcInterpResult ir = arc_interpret(&g);
    ASSERT(ir.success);
    ASSERT_EQ_I64(ir.result.as.i64, 14);

    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);
    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen(DEV_NULL, "w");
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    fclose(vm.output);

    ASSERT(arc_val_equal(ir.result, arc_vm_result(&vm)));

    arc_vm_destroy(&vm);
    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

TEST(test_ref_interp_not) {
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_true = arc_graph_add_node(&g, ARC_NODE_CONST_BOOL, r0, 0);
    g.nodes[n_true].attr.bool_value = true;
    ArcPortId p_true = arc_graph_add_port(&g, n_true, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_not = arc_graph_add_node(&g, ARC_NODE_NOT, r0, 0);
    ArcPortId p_not_in = arc_graph_add_port(&g, n_not, ARC_PORT_INPUT, "value");
    ArcPortId p_not_out = arc_graph_add_port(&g, n_not, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_true, p_not_in);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 0);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_not_out, p_out_in);

    ArcInterpResult ir = arc_interpret(&g);
    ASSERT(ir.success);
    ASSERT(ir.result.tag == VAL_BOOL);
    ASSERT(ir.result.as.b == false);

    arc_graph_free(&g);
}
