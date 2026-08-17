/*
 * Arcana test suite -- constant folding tests.
 */
#include "test_harness.h"

/* Compile, run, write result to *out. Caller cleans up via cleanup_fold(). */
static void fold_compile_run(ArcGraph* g, ArcCompileResult* cr,
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

static void cleanup_fold(ArcVm* vm, ArcCompileResult* cr, ArcGraph* g) {
    arc_vm_destroy(vm);
    arc_compile_result_free(cr);
    arc_graph_free(g);
}

/* Build binary op graph with int operands */
static void build_binop_int(ArcGraph* g, ArcNodeKind op, int64_t lhs, int64_t rhs) {
    arc_graph_init(g);
    ArcRegionId r0 = arc_graph_add_region(g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g->root_region = r0;
    ArcNodeId n0 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, 1);
    g->nodes[n0].attr.int_value = lhs;
    ArcPortId p0 = arc_graph_add_port(g, n0, ARC_PORT_OUTPUT, "out");
    ArcNodeId n1 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, 2);
    g->nodes[n1].attr.int_value = rhs;
    ArcPortId p1 = arc_graph_add_port(g, n1, ARC_PORT_OUTPUT, "out");
    ArcNodeId n2 = arc_graph_add_node(g, op, r0, 3);
    ArcPortId pl = arc_graph_add_port(g, n2, ARC_PORT_INPUT, "lhs");
    ArcPortId pr = arc_graph_add_port(g, n2, ARC_PORT_INPUT, "rhs");
    ArcPortId po = arc_graph_add_port(g, n2, ARC_PORT_OUTPUT, "out");
    ArcPortId order[] = { pl, pr, po };
    arc_node_set_cyclic_order(g, n2, order, 3);
    ArcNodeId n3 = arc_graph_add_node(g, ARC_NODE_ROOT_OUTPUT, r0, 4);
    ArcPortId pi = arc_graph_add_port(g, n3, ARC_PORT_INPUT, "value");
    g->output_node = n3;
    arc_graph_add_edge(g, p0, pl); arc_graph_add_edge(g, p1, pr);
    arc_graph_add_edge(g, po, pi);
}

TEST(test_fold_int_arithmetic) {
    ArcGraph g; ArcCompileResult cr; ArcVm vm; ArcValue v;
    /* 7 * 6 = 42, folded to single const */
    build_binop_int(&g, ARC_NODE_MUL, 7, 6);
    fold_compile_run(&g, &cr, &vm, &v);
    ASSERT(v.tag == VAL_I64); ASSERT_EQ_I64(v.as.i64, 42);
    cleanup_fold(&vm, &cr, &g);
}

TEST(test_fold_int_comparison) {
    ArcGraph g; ArcCompileResult cr; ArcVm vm; ArcValue v;
    build_binop_int(&g, ARC_NODE_LT, 3, 10);
    fold_compile_run(&g, &cr, &vm, &v);
    ASSERT(v.tag == VAL_BOOL); ASSERT(v.as.b == true);
    cleanup_fold(&vm, &cr, &g);
}

TEST(test_fold_unary_neg) {
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;
    ArcNodeId n0 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 1);
    g.nodes[n0].attr.int_value = 42;
    ArcPortId p0 = arc_graph_add_port(&g, n0, ARC_PORT_OUTPUT, "out");
    ArcNodeId n1 = arc_graph_add_node(&g, ARC_NODE_NEG, r0, 2);
    ArcPortId pi = arc_graph_add_port(&g, n1, ARC_PORT_INPUT, "value");
    ArcPortId po = arc_graph_add_port(&g, n1, ARC_PORT_OUTPUT, "out");
    ArcNodeId n2 = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 3);
    ArcPortId pv = arc_graph_add_port(&g, n2, ARC_PORT_INPUT, "value");
    g.output_node = n2;
    arc_graph_add_edge(&g, p0, pi); arc_graph_add_edge(&g, po, pv);
    ArcCompileResult cr; ArcVm vm; ArcValue v;
    fold_compile_run(&g, &cr, &vm, &v);
    ASSERT(v.tag == VAL_I64); ASSERT_EQ_I64(v.as.i64, -42);
    cleanup_fold(&vm, &cr, &g);
}

TEST(test_fold_nested_expr) {
    /* (3 + 4) * 2 = 14: both additions and multiply should fold recursively */
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;
    ArcNodeId c3 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 1);
    g.nodes[c3].attr.int_value = 3;
    ArcPortId p3 = arc_graph_add_port(&g, c3, ARC_PORT_OUTPUT, "out");
    ArcNodeId c4 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 2);
    g.nodes[c4].attr.int_value = 4;
    ArcPortId p4 = arc_graph_add_port(&g, c4, ARC_PORT_OUTPUT, "out");
    ArcNodeId add = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 3);
    ArcPortId al = arc_graph_add_port(&g, add, ARC_PORT_INPUT, "lhs");
    ArcPortId ar = arc_graph_add_port(&g, add, ARC_PORT_INPUT, "rhs");
    ArcPortId ao = arc_graph_add_port(&g, add, ARC_PORT_OUTPUT, "out");
    ArcPortId aord[] = { al, ar, ao };
    arc_node_set_cyclic_order(&g, add, aord, 3);
    ArcNodeId c2 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 4);
    g.nodes[c2].attr.int_value = 2;
    ArcPortId p2 = arc_graph_add_port(&g, c2, ARC_PORT_OUTPUT, "out");
    ArcNodeId mul = arc_graph_add_node(&g, ARC_NODE_MUL, r0, 5);
    ArcPortId ml = arc_graph_add_port(&g, mul, ARC_PORT_INPUT, "lhs");
    ArcPortId mr = arc_graph_add_port(&g, mul, ARC_PORT_INPUT, "rhs");
    ArcPortId mo = arc_graph_add_port(&g, mul, ARC_PORT_OUTPUT, "out");
    ArcPortId mord[] = { ml, mr, mo };
    arc_node_set_cyclic_order(&g, mul, mord, 3);
    ArcNodeId out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 6);
    ArcPortId ov = arc_graph_add_port(&g, out, ARC_PORT_INPUT, "value");
    g.output_node = out;
    arc_graph_add_edge(&g, p3, al); arc_graph_add_edge(&g, p4, ar);
    arc_graph_add_edge(&g, ao, ml); arc_graph_add_edge(&g, p2, mr);
    arc_graph_add_edge(&g, mo, ov);
    ArcCompileResult cr; ArcVm vm; ArcValue v;
    fold_compile_run(&g, &cr, &vm, &v);
    ASSERT(v.tag == VAL_I64); ASSERT_EQ_I64(v.as.i64, 14);
    cleanup_fold(&vm, &cr, &g);
}
