/*
 * Arcana test suite -- compilation pipeline tests (basics, compiler features).
 * Large E2E tests live in test_pipeline_e2e.c.
 */
#include "test_harness.h"

/* ================================================================
 * Shared Helpers
 * ================================================================ */

/* Build a graph: CONST_INT(lhs) OP CONST_INT(rhs) -> ROOT_OUTPUT. */
static void build_add_graph(ArcGraph* g, int64_t lhs, int64_t rhs) {
    arc_graph_init(g);
    ArcRegionId r0 = arc_graph_add_region(g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g->root_region = r0;

    ArcNodeId n0 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, 1001);
    g->nodes[n0].attr.int_value = lhs;
    ArcPortId p0 = arc_graph_add_port(g, n0, ARC_PORT_OUTPUT, "out");

    ArcNodeId n1 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, 1002);
    g->nodes[n1].attr.int_value = rhs;
    ArcPortId p1 = arc_graph_add_port(g, n1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n2 = arc_graph_add_node(g, ARC_NODE_ADD, r0, 1003);
    ArcPortId p2lhs = arc_graph_add_port(g, n2, ARC_PORT_INPUT, "lhs");
    ArcPortId p2rhs = arc_graph_add_port(g, n2, ARC_PORT_INPUT, "rhs");
    ArcPortId p2out = arc_graph_add_port(g, n2, ARC_PORT_OUTPUT, "out");
    ArcPortId order[] = { p2lhs, p2rhs, p2out };
    arc_node_set_cyclic_order(g, n2, order, 3);

    ArcNodeId n3 = arc_graph_add_node(g, ARC_NODE_ROOT_OUTPUT, r0, 1004);
    ArcPortId p3in = arc_graph_add_port(g, n3, ARC_PORT_INPUT, "value");
    g->output_node = n3;

    arc_graph_add_edge(g, p0, p2lhs);
    arc_graph_add_edge(g, p1, p2rhs);
    arc_graph_add_edge(g, p2out, p3in);
}

/* Compile, run, write result to *out. Caller cleans up via cleanup_pipeline(). */
static void pipeline_compile_run(ArcGraph* g, ArcCompileResult* cr,
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

static void cleanup_pipeline(ArcVm* vm, ArcCompileResult* cr, ArcGraph* g) {
    arc_vm_destroy(vm);
    arc_compile_result_free(cr);
    arc_graph_free(g);
}

/* Build a unary op graph: NODE_KIND(input_node) -> ROOT_OUTPUT */
static void build_unary_graph(ArcGraph* g, ArcNodeKind op,
                              ArcNodeKind input_kind, int base_id) {
    arc_graph_init(g);
    ArcRegionId r0 = arc_graph_add_region(g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g->root_region = r0;

    ArcNodeId n_in = arc_graph_add_node(g, input_kind, r0, base_id);
    ArcPortId p_in_out = arc_graph_add_port(g, n_in, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_op = arc_graph_add_node(g, op, r0, base_id + 1);
    ArcPortId p_op_in = arc_graph_add_port(g, n_op, ARC_PORT_INPUT, "value");
    ArcPortId p_op_out = arc_graph_add_port(g, n_op, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(g, p_in_out, p_op_in);

    ArcNodeId n_out = arc_graph_add_node(g, ARC_NODE_ROOT_OUTPUT, r0, base_id + 2);
    ArcPortId p_out_in = arc_graph_add_port(g, n_out, ARC_PORT_INPUT, "value");
    g->output_node = n_out;
    arc_graph_add_edge(g, p_op_out, p_out_in);
}

/* ================================================================
 * E2E Basics
 * ================================================================ */

TEST(test_e2e_5_plus_10) {
    ArcGraph g;
    build_add_graph(&g, 5, 10);

    ArcCompileResult cr; ArcVm vm;
    ArcValue result; pipeline_compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 15);
    cleanup_pipeline(&vm, &cr, &g);
}

TEST(test_e2e_5_plus_10_vm_result) {
    /* Same graph but without output_node — just compile and verify it works */
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n0 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 2001);
    g.nodes[n0].attr.int_value = 5;
    ArcPortId p0 = arc_graph_add_port(&g, n0, ARC_PORT_OUTPUT, "out");
    ArcNodeId n1 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 2002);
    g.nodes[n1].attr.int_value = 10;
    ArcPortId p1 = arc_graph_add_port(&g, n1, ARC_PORT_OUTPUT, "out");
    ArcNodeId n2 = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 2003);
    ArcPortId p2lhs = arc_graph_add_port(&g, n2, ARC_PORT_INPUT, "lhs");
    ArcPortId p2rhs = arc_graph_add_port(&g, n2, ARC_PORT_INPUT, "rhs");
    ArcPortId p2out = arc_graph_add_port(&g, n2, ARC_PORT_OUTPUT, "out");
    ArcPortId order[] = { p2lhs, p2rhs, p2out };
    arc_node_set_cyclic_order(&g, n2, order, 3);
    arc_graph_add_edge(&g, p0, p2lhs);
    arc_graph_add_edge(&g, p1, p2rhs);
    (void)p2out;

    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);
    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* ================================================================
 * Compile-Fail Diagnostics
 * ================================================================ */

TEST(test_compile_fail_diagnostic) {
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_add = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 9001);
    ArcPortId p_lhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId p_rhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId p_out = arc_graph_add_port(&g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId order[] = { p_lhs, p_rhs, p_out };
    arc_node_set_cyclic_order(&g, n_add, order, 3);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 9002);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_out, p_out_in);

    ArcCompileResult cr = arc_compile(&g);
    ASSERT(!cr.success);
    ASSERT(cr.error_count > 0);
    ASSERT(strstr(cr.errors[0].message, "disconnected") != NULL);
    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* ================================================================
 * Golden Disassembly
 * ================================================================ */

TEST(test_golden_disassembly) {
    ArcGraph g;
    build_add_graph(&g, 5, 10);

    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);

    /* Disassemble to tmpfile */
    FILE* tmp = tmpfile();
    ASSERT(tmp != NULL);
    arc_disassemble(&cr.image, tmp);
    char buf[4096] = {0};
    fseek(tmp, 0, SEEK_SET);
    size_t rd = fread(buf, 1, sizeof(buf) - 1, tmp);
    buf[rd] = '\0';
    fclose(tmp);

    ASSERT(strstr(buf, "main") != NULL);
    ASSERT(strstr(buf, "const") != NULL);
    ASSERT(strstr(buf, "add") != NULL);
    ASSERT(strstr(buf, "halt") != NULL);

    /* Determinism: recompile, binary output must match */
    ArcCompileResult cr2 = arc_compile(&g);
    ASSERT(cr2.success);
    uint8_t *b1, *b2; size_t l1, l2;
    arc_image_write(&cr.image, &b1, &l1);
    arc_image_write(&cr2.image, &b2, &l2);
    ASSERT(l1 == l2);
    ASSERT(memcmp(b1, b2, l1) == 0);

    ARC_FREE(b1); ARC_FREE(b2);
    arc_compile_result_free(&cr);
    arc_compile_result_free(&cr2);
    arc_graph_free(&g);
}

/* ================================================================
 * Short-Circuit Booleans
 * ================================================================ */

/* Helper: build bool_val OP int_val -> ROOT_OUTPUT */
static void build_bool_int_graph(ArcGraph* g, ArcNodeKind op, bool bval,
                                 int64_t ival, int base_id) {
    arc_graph_init(g);
    ArcRegionId r0 = arc_graph_add_region(g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g->root_region = r0;

    ArcNodeId n_b = arc_graph_add_node(g, ARC_NODE_CONST_BOOL, r0, base_id);
    g->nodes[n_b].attr.bool_value = bval;
    ArcPortId p_b = arc_graph_add_port(g, n_b, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_i = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, base_id + 1);
    g->nodes[n_i].attr.int_value = ival;
    ArcPortId p_i = arc_graph_add_port(g, n_i, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_op = arc_graph_add_node(g, op, r0, base_id + 2);
    ArcPortId p_lhs = arc_graph_add_port(g, n_op, ARC_PORT_INPUT, "lhs");
    ArcPortId p_rhs = arc_graph_add_port(g, n_op, ARC_PORT_INPUT, "rhs");
    ArcPortId p_out = arc_graph_add_port(g, n_op, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(g, p_b, p_lhs);
    arc_graph_add_edge(g, p_i, p_rhs);

    ArcNodeId n_out = arc_graph_add_node(g, ARC_NODE_ROOT_OUTPUT, r0, base_id + 3);
    ArcPortId p_out_in = arc_graph_add_port(g, n_out, ARC_PORT_INPUT, "value");
    g->output_node = n_out;
    arc_graph_add_edge(g, p_out, p_out_in);
}

TEST(test_compiler_short_circuit_and) {
    ArcGraph g;
    build_bool_int_graph(&g, ARC_NODE_AND, true, 42, 9001);
    ArcCompileResult cr; ArcVm vm;
    ArcValue result; pipeline_compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 42);
    cleanup_pipeline(&vm, &cr, &g);
}

TEST(test_compiler_short_circuit_and_false) {
    ArcGraph g;
    build_bool_int_graph(&g, ARC_NODE_AND, false, 42, 9101);
    ArcCompileResult cr; ArcVm vm;
    ArcValue result; pipeline_compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_BOOL);
    ASSERT(result.as.b == false);
    cleanup_pipeline(&vm, &cr, &g);
}

TEST(test_compiler_short_circuit_or) {
    ArcGraph g;
    build_bool_int_graph(&g, ARC_NODE_OR, false, 99, 9201);
    ArcCompileResult cr; ArcVm vm;
    ArcValue result; pipeline_compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 99);
    cleanup_pipeline(&vm, &cr, &g);
}

/* ================================================================
 * Compiler: Strings, Bitwise, Casts, Str_Len
 * ================================================================ */

TEST(test_compiler_const_string) {
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_str = arc_graph_add_node(&g, ARC_NODE_CONST_STRING, r0, 9301);
    g.nodes[n_str].attr.string_value.data = "hello";
    g.nodes[n_str].attr.string_value.len = 5;
    ArcPortId p_str = arc_graph_add_port(&g, n_str, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 9302);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_str, p_out_in);

    ArcCompileResult cr; ArcVm vm;
    ArcValue result; pipeline_compile_run(&g, &cr, &vm, &result);
    ASSERT(ARC_IS_STRING(result));
    ASSERT(strcmp(ARC_AS_STRING(result)->data, "hello") == 0);
    cleanup_pipeline(&vm, &cr, &g);
}

TEST(test_compiler_bitwise_and) {
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_a = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 9401);
    g.nodes[n_a].attr.int_value = 0xFF;
    ArcPortId p_a = arc_graph_add_port(&g, n_a, ARC_PORT_OUTPUT, "out");
    ArcNodeId n_b = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 9402);
    g.nodes[n_b].attr.int_value = 0x0F;
    ArcPortId p_b = arc_graph_add_port(&g, n_b, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_band = arc_graph_add_node(&g, ARC_NODE_BIT_AND, r0, 9403);
    ArcPortId p_lhs = arc_graph_add_port(&g, n_band, ARC_PORT_INPUT, "lhs");
    ArcPortId p_rhs = arc_graph_add_port(&g, n_band, ARC_PORT_INPUT, "rhs");
    ArcPortId p_out = arc_graph_add_port(&g, n_band, ARC_PORT_OUTPUT, "out");
    ArcPortId order[] = { p_lhs, p_rhs, p_out };
    arc_node_set_cyclic_order(&g, n_band, order, 3);
    arc_graph_add_edge(&g, p_a, p_lhs);
    arc_graph_add_edge(&g, p_b, p_rhs);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 9404);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_out, p_out_in);

    ArcCompileResult cr; ArcVm vm;
    ArcValue result; pipeline_compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 15);
    cleanup_pipeline(&vm, &cr, &g);
}

TEST(test_compiler_cast_i64) {
    ArcGraph g;
    build_unary_graph(&g, ARC_NODE_CAST_I64, ARC_NODE_CONST_FLOAT, 9501);
    g.nodes[0].attr.float_value = 3.7;  /* node 0 is the CONST_FLOAT */

    ArcCompileResult cr; ArcVm vm;
    ArcValue result; pipeline_compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 3);
    cleanup_pipeline(&vm, &cr, &g);
}

TEST(test_compiler_str_len) {
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_str = arc_graph_add_node(&g, ARC_NODE_CONST_STRING, r0, 9701);
    g.nodes[n_str].attr.string_value.data = "abc";
    g.nodes[n_str].attr.string_value.len = 3;
    ArcPortId p_str = arc_graph_add_port(&g, n_str, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_len = arc_graph_add_node(&g, ARC_NODE_STR_LEN, r0, 9702);
    ArcPortId p_in = arc_graph_add_port(&g, n_len, ARC_PORT_INPUT, "value");
    ArcPortId p_out = arc_graph_add_port(&g, n_len, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_str, p_in);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 9703);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_out, p_out_in);

    ArcCompileResult cr; ArcVm vm;
    ArcValue result; pipeline_compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 3);
    cleanup_pipeline(&vm, &cr, &g);
}

/* ================================================================
 * Compiler: Try/Throw
 * ================================================================ */

/* Build: let result=0; try{throw 42}catch{result=99}; output=result */
static void build_try_throw_graph(ArcGraph* g) {
    arc_graph_init(g);
    ArcRegionId r0 = arc_graph_add_region(g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g->root_region = r0;
    ArcRegionId r_try = arc_graph_add_region(g, ARC_REGION_TRY_BODY, r0);
    ArcRegionId r_catch = arc_graph_add_region(g, ARC_REGION_CATCH_BODY, r0);

    /* let result = 0 */
    ArcNodeId n_c0 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, 9601);
    g->nodes[n_c0].attr.int_value = 0;
    ArcPortId p_c0 = arc_graph_add_port(g, n_c0, ARC_PORT_OUTPUT, "out");
    ArcNodeId n_let = arc_graph_add_node(g, ARC_NODE_LET, r0, 9602);
    g->nodes[n_let].attr.name = "result";
    ArcPortId p_let = arc_graph_add_port(g, n_let, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_c0, p_let);

    /* try { throw 42 } */
    ArcNodeId n_try = arc_graph_add_node(g, ARC_NODE_TRY, r0, 9603);
    g->nodes[n_try].attr.try_catch.try_region = r_try;
    g->nodes[n_try].attr.try_catch.catch_region = r_catch;

    ArcNodeId n_42 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r_try, 9604);
    g->nodes[n_42].attr.int_value = 42;
    ArcPortId p_42 = arc_graph_add_port(g, n_42, ARC_PORT_OUTPUT, "out");
    ArcNodeId n_throw = arc_graph_add_node(g, ARC_NODE_THROW, r_try, 9605);
    ArcPortId p_throw = arc_graph_add_port(g, n_throw, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_42, p_throw);

    /* catch { result = 99 } */
    ArcNodeId n_99 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r_catch, 9606);
    g->nodes[n_99].attr.int_value = 99;
    ArcPortId p_99 = arc_graph_add_port(g, n_99, ARC_PORT_OUTPUT, "out");
    ArcNodeId n_assign = arc_graph_add_node(g, ARC_NODE_ASSIGN, r_catch, 9607);
    g->nodes[n_assign].attr.name = "result";
    ArcPortId p_asgn = arc_graph_add_port(g, n_assign, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p_99, p_asgn);

    /* output = result */
    ArcNodeId n_ref = arc_graph_add_node(g, ARC_NODE_VAR_REF, r0, 9608);
    g->nodes[n_ref].attr.name = "result";
    ArcPortId p_ref = arc_graph_add_port(g, n_ref, ARC_PORT_OUTPUT, "out");
    ArcNodeId n_out = arc_graph_add_node(g, ARC_NODE_ROOT_OUTPUT, r0, 9609);
    ArcPortId p_out_in = arc_graph_add_port(g, n_out, ARC_PORT_INPUT, "value");
    g->output_node = n_out;
    arc_graph_add_edge(g, p_ref, p_out_in);
}

TEST(test_compiler_try_throw) {
    ArcGraph g;
    build_try_throw_graph(&g);
    ArcCompileResult cr; ArcVm vm;
    ArcValue result; pipeline_compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 99);
    cleanup_pipeline(&vm, &cr, &g);
}

/* ================================================================
 * New Node Kinds
 * ================================================================ */

TEST(test_compiler_new_node_kinds_exist) {
    ASSERT(ARC_NODE_CONST_STRING > ARC_NODE_SEQUENCE);
    ASSERT(ARC_NODE_AND > ARC_NODE_SEQUENCE);
    ASSERT(ARC_NODE_OR > ARC_NODE_SEQUENCE);
    ASSERT(ARC_NODE_BIT_AND > ARC_NODE_SEQUENCE);
    ASSERT(ARC_NODE_CAST_I64 > ARC_NODE_SEQUENCE);
    ASSERT(ARC_NODE_ARRAY_LITERAL > ARC_NODE_SEQUENCE);
    ASSERT(ARC_NODE_MAP_LITERAL > ARC_NODE_SEQUENCE);
    ASSERT(ARC_NODE_INDEX_GET > ARC_NODE_SEQUENCE);
    ASSERT(ARC_NODE_TRY > ARC_NODE_SEQUENCE);
    ASSERT(ARC_NODE_THROW > ARC_NODE_SEQUENCE);
    ASSERT(ARC_NODE_CLOSURE > ARC_NODE_SEQUENCE);
    ASSERT(ARC_NODE_INTRINSIC_CALL > ARC_NODE_SEQUENCE);
    ASSERT(ARC_REGION_TRY_BODY > ARC_REGION_LOOP_BODY);
    ASSERT(ARC_REGION_CATCH_BODY > ARC_REGION_LOOP_BODY);
}

/* ================================================================
 * Fixture Parser
 * ================================================================ */

TEST(test_fixture_parse_add) {
    const char* fixture =
        "region r0 module\n"
        "node n0 const_int(5) in r0\n"
        "node n1 const_int(10) in r0\n"
        "node n2 add in r0\n"
        "edge e0 n0.out -> n2.lhs\n"
        "edge e1 n1.out -> n2.rhs\n"
        "root n2.out\n";

    ArcFixtureResult fr = arc_fixture_parse(fixture);
    if (!fr.success) printf("    fixture error: %s\n", fr.error);
    ASSERT(fr.success);
    ASSERT(fr.graph.region_count == 1);
    ASSERT(fr.graph.node_count == 4);
    ASSERT(fr.graph.output_node != ARC_INVALID_ID);
    ASSERT(fr.graph.edge_count == 3);

    ArcCompileResult cr = arc_compile(&fr.graph);
    ASSERT(cr.success);
    ArcVerifyResult vr = arc_verify(&cr.image);
    ASSERT(vr.valid);

    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen(DEV_NULL, "w");
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    fclose(vm.output);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 15);

    arc_vm_destroy(&vm);
    arc_compile_result_free(&cr);
    arc_graph_free(&fr.graph);
}

TEST(test_fixture_parse_file) {
    char path[512]; fixture_path(path, sizeof(path), "add_5_10.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    if (!fr.success) printf("    fixture error: %s\n", fr.error);
    ASSERT(fr.success);
    ASSERT(fr.graph.node_count >= 3);

    ArcCompileResult cr = arc_compile(&fr.graph);
    ASSERT(cr.success);
    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen(DEV_NULL, "w");
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    fclose(vm.output);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 15);

    arc_vm_destroy(&vm);
    arc_compile_result_free(&cr);
    arc_graph_free(&fr.graph);
}

TEST(test_fixture_parse_malformed) {
    char mpath[512]; fixture_path(mpath, sizeof(mpath), "malformed.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(mpath);
    ASSERT(!fr.success);
    ASSERT(strlen(fr.error) > 0);
    if (fr.success) arc_graph_free(&fr.graph);
}
