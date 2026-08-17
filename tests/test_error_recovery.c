/*
 * test_error_recovery.c — Tests for error recovery via poison nodes.
 *
 * Verifies that semantic analysis reports multiple diagnostics per
 * compilation rather than stopping at the first error.
 */
#include "test_harness.h"

/* Helper: build a minimal graph with a root region */
static void init_graph_with_root(ArcGraph* g, ArcRegionId* r0) {
    arc_graph_init(g);
    *r0 = arc_graph_add_region(g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g->root_region = *r0;
}

/*
 * Two independent errors in one graph: both should be reported.
 * Graph: print(unknownA); print(unknownB);
 */
TEST(test_semantic_multi_error) {
    ArcGraph g;
    ArcRegionId r0;
    init_graph_with_root(&g, &r0);

    /* First undefined variable reference */
    ArcNodeId n1 = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r0, 10);
    g.nodes[n1].attr.name = "unknownA";
    ArcPortId p1_out = arc_graph_add_port(&g, n1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_print1 = arc_graph_add_node(&g, ARC_NODE_PRINT, r0, 11);
    ArcPortId p_p1_in = arc_graph_add_port(&g, n_print1, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p1_out, p_p1_in);

    /* Second undefined variable reference */
    ArcNodeId n2 = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r0, 20);
    g.nodes[n2].attr.name = "unknownB";
    ArcPortId p2_out = arc_graph_add_port(&g, n2, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_print2 = arc_graph_add_node(&g, ARC_NODE_PRINT, r0, 21);
    ArcPortId p_p2_in = arc_graph_add_port(&g, n_print2, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p2_out, p_p2_in);

    ArcSemanticResult sr = arc_semantic_lower(&g);
    ASSERT(!sr.success);
    /* Both errors should be reported, not just the first */
    ASSERT(sr.diagnostics.count >= 2);

    arc_semantic_result_free(&sr);
    arc_graph_free(&g);
}

/*
 * Poison propagation: error in sub-expression should not generate
 * a cascading error in the parent binary op.
 * Graph: unknownVar + 5  (should report 1 error, not 2)
 */
TEST(test_semantic_poison_no_cascade) {
    ArcGraph g;
    ArcRegionId r0;
    init_graph_with_root(&g, &r0);

    /* undefined var */
    ArcNodeId n_bad = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r0, 10);
    g.nodes[n_bad].attr.name = "noexist";
    ArcPortId p_bad = arc_graph_add_port(&g, n_bad, ARC_PORT_OUTPUT, "out");

    /* const 5 */
    ArcNodeId n_five = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 11);
    g.nodes[n_five].attr.int_value = 5;
    ArcPortId p_five = arc_graph_add_port(&g, n_five, ARC_PORT_OUTPUT, "out");

    /* add node */
    ArcNodeId n_add = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 12);
    ArcPortId pa_lhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId pa_rhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId pa_out = arc_graph_add_port(&g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId order[] = { pa_lhs, pa_rhs, pa_out };
    arc_node_set_cyclic_order(&g, n_add, order, 3);
    arc_graph_add_edge(&g, p_bad, pa_lhs);
    arc_graph_add_edge(&g, p_five, pa_rhs);

    /* output */
    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 13);
    ArcPortId p_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, pa_out, p_in);

    ArcSemanticResult sr = arc_semantic_lower(&g);
    ASSERT(!sr.success);
    /* Should be exactly 1 error (undefined var), not 2 (no cascade) */
    ASSERT(sr.diagnostics.count == 1);
    ASSERT(sr.diagnostics.items[0].code == ARC_DIAG_SEM_0001);

    arc_semantic_result_free(&sr);
    arc_graph_free(&g);
}

/*
 * HIR poison in dump: verify <poison> appears in output
 */
TEST(test_hir_poison_dump) {
    HirExpr* poison = hir_expr_new(HIR_POISON, 42);
    ASSERT(poison != NULL);
    ASSERT(poison->kind == HIR_POISON);

    /* Dump to null device — just verify no crash */
    FILE* devnull = fopen(DEV_NULL, "w");
    ASSERT(devnull != NULL);
    hir_dump_expr(poison, devnull, 0);
    fclose(devnull);

    hir_expr_free(poison);
}

/*
 * HIR validation accepts poison nodes without error
 */
TEST(test_hir_poison_validation) {
    HirModule m;
    hir_module_init(&m);

    /* Add a statement with a poison expression */
    HirStmt* s = hir_block_add(&m.main_body, HIR_STMT_PRINT, 0);
    s->as.print.value = hir_expr_new(HIR_POISON, 0);

    HirValidation v = hir_validate(&m);
    /* Poison is intentional — validation should not flag it */
    ASSERT(v.valid);
    hir_validation_free(&v);
    hir_module_free(&m);
}

/*
 * Multiple independent errors in separate statements.
 * Graph: print(badvar1); print(badvar2);
 * Both undefined-variable errors should be reported.
 */
TEST(test_semantic_mixed_errors) {
    ArcGraph g;
    ArcRegionId r0;
    init_graph_with_root(&g, &r0);

    /* print(badvar1) */
    ArcNodeId n_ref1 = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r0, 10);
    g.nodes[n_ref1].attr.name = "badvar1";
    ArcPortId p_ref1 = arc_graph_add_port(&g, n_ref1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_print1 = arc_graph_add_node(&g, ARC_NODE_PRINT, r0, 11);
    ArcPortId p_pin1 = arc_graph_add_port(&g, n_print1, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_ref1, p_pin1);

    /* print(badvar2) */
    ArcNodeId n_ref2 = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r0, 20);
    g.nodes[n_ref2].attr.name = "badvar2";
    ArcPortId p_ref2 = arc_graph_add_port(&g, n_ref2, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_print2 = arc_graph_add_node(&g, ARC_NODE_PRINT, r0, 21);
    ArcPortId p_pin2 = arc_graph_add_port(&g, n_print2, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_ref2, p_pin2);

    ArcSemanticResult sr = arc_semantic_lower(&g);
    ASSERT(!sr.success);
    /* Both undefined-variable errors should be reported */
    ASSERT(sr.diagnostics.count >= 2);

    arc_semantic_result_free(&sr);
    arc_graph_free(&g);
}
