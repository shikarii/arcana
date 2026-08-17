/*
 * test_cycle.c -- Tests for topology-derived iteration (CYCLE regions).
 *
 * CYCLE regions + BREAK_IF nodes represent loops that emerge from graph
 * topology rather than explicit "while" keywords. This is the first
 * Stage 2 (topology-derived semantics) feature per the guardrails doc.
 */
#include "test_harness.h"

/* ================================================================
 * Shared helpers (same pattern as test_pipeline_e2e.c)
 * ================================================================ */

static void compile_run(ArcGraph* g, ArcCompileResult* cr,
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

static void cleanup(ArcVm* vm, ArcCompileResult* cr, ArcGraph* g) {
    arc_vm_destroy(vm);
    arc_compile_result_free(cr);
    arc_graph_free(g);
}

/* ================================================================
 * Graph builder: CYCLE body for sum(0..n-1)
 *
 * Expects vars "i" and "acc" already defined.
 * Body: BREAK_IF(i >= limit), acc = acc + i, i = i + 1
 * ================================================================ */

static void build_sum_cycle(ArcGraph* g, ArcRegionId rc, int limit) {
    /* BREAK_IF: i >= limit */
    ArcNodeId ri = arc_graph_add_node(g, ARC_NODE_VAR_REF, rc, 7001);
    g->nodes[ri].attr.name = "i";
    ArcPortId pri = arc_graph_add_port(g, ri, ARC_PORT_OUTPUT, "out");
    ArcNodeId nl = arc_graph_add_node(g, ARC_NODE_CONST_INT, rc, 7002);
    g->nodes[nl].attr.int_value = limit;
    ArcPortId pnl = arc_graph_add_port(g, nl, ARC_PORT_OUTPUT, "out");
    ArcNodeId ge = arc_graph_add_node(g, ARC_NODE_GE, rc, 7003);
    ArcPortId gl = arc_graph_add_port(g, ge, ARC_PORT_INPUT, "lhs");
    ArcPortId gr = arc_graph_add_port(g, ge, ARC_PORT_INPUT, "rhs");
    ArcPortId go = arc_graph_add_port(g, ge, ARC_PORT_OUTPUT, "out");
    ArcPortId gord[] = { gl, gr, go };
    arc_node_set_cyclic_order(g, ge, gord, 3);
    arc_graph_add_edge(g, pri, gl);
    arc_graph_add_edge(g, pnl, gr);
    ArcNodeId brk = arc_graph_add_node(g, ARC_NODE_BREAK_IF, rc, 7004);
    ArcPortId pb = arc_graph_add_port(g, brk, ARC_PORT_INPUT, "cond");
    arc_graph_add_edge(g, go, pb);

    /* acc = acc + i */
    ArcNodeId ra = arc_graph_add_node(g, ARC_NODE_VAR_REF, rc, 7010);
    g->nodes[ra].attr.name = "acc";
    ArcPortId pra = arc_graph_add_port(g, ra, ARC_PORT_OUTPUT, "out");
    ArcNodeId ri2 = arc_graph_add_node(g, ARC_NODE_VAR_REF, rc, 7011);
    g->nodes[ri2].attr.name = "i";
    ArcPortId pri2 = arc_graph_add_port(g, ri2, ARC_PORT_OUTPUT, "out");
    ArcNodeId add = arc_graph_add_node(g, ARC_NODE_ADD, rc, 7012);
    ArcPortId al = arc_graph_add_port(g, add, ARC_PORT_INPUT, "lhs");
    ArcPortId ar = arc_graph_add_port(g, add, ARC_PORT_INPUT, "rhs");
    ArcPortId ao = arc_graph_add_port(g, add, ARC_PORT_OUTPUT, "out");
    ArcPortId aord[] = { al, ar, ao };
    arc_node_set_cyclic_order(g, add, aord, 3);
    arc_graph_add_edge(g, pra, al);
    arc_graph_add_edge(g, pri2, ar);
    ArcNodeId asn = arc_graph_add_node(g, ARC_NODE_ASSIGN, rc, 7013);
    g->nodes[asn].attr.name = "acc";
    ArcPortId pav = arc_graph_add_port(g, asn, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, ao, pav);

    /* i = i + 1 */
    ArcNodeId ri3 = arc_graph_add_node(g, ARC_NODE_VAR_REF, rc, 7020);
    g->nodes[ri3].attr.name = "i";
    ArcPortId pri3 = arc_graph_add_port(g, ri3, ARC_PORT_OUTPUT, "out");
    ArcNodeId n1 = arc_graph_add_node(g, ARC_NODE_CONST_INT, rc, 7021);
    g->nodes[n1].attr.int_value = 1;
    ArcPortId p1 = arc_graph_add_port(g, n1, ARC_PORT_OUTPUT, "out");
    ArcNodeId add2 = arc_graph_add_node(g, ARC_NODE_ADD, rc, 7022);
    ArcPortId a2l = arc_graph_add_port(g, add2, ARC_PORT_INPUT, "lhs");
    ArcPortId a2r = arc_graph_add_port(g, add2, ARC_PORT_INPUT, "rhs");
    ArcPortId a2o = arc_graph_add_port(g, add2, ARC_PORT_OUTPUT, "out");
    ArcPortId a2ord[] = { a2l, a2r, a2o };
    arc_node_set_cyclic_order(g, add2, a2ord, 3);
    arc_graph_add_edge(g, pri3, a2l);
    arc_graph_add_edge(g, p1, a2r);
    ArcNodeId asi = arc_graph_add_node(g, ARC_NODE_ASSIGN, rc, 7023);
    g->nodes[asi].attr.name = "i";
    ArcPortId piv = arc_graph_add_port(g, asi, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, a2o, piv);
}

/* Helper: build let i=0, let acc=0, cycle{...}, output acc */
static void build_sum_graph(ArcGraph* g, int limit) {
    arc_graph_init(g);
    ArcRegionId r0 = arc_graph_add_region(g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g->root_region = r0;

    ArcNodeId c0 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, 7100);
    g->nodes[c0].attr.int_value = 0;
    ArcPortId p0 = arc_graph_add_port(g, c0, ARC_PORT_OUTPUT, "out");
    ArcNodeId li = arc_graph_add_node(g, ARC_NODE_LET, r0, 7101);
    g->nodes[li].attr.name = "i";
    ArcPortId pli = arc_graph_add_port(g, li, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p0, pli);

    ArcNodeId c02 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, 7102);
    g->nodes[c02].attr.int_value = 0;
    ArcPortId p02 = arc_graph_add_port(g, c02, ARC_PORT_OUTPUT, "out");
    ArcNodeId la = arc_graph_add_node(g, ARC_NODE_LET, r0, 7103);
    g->nodes[la].attr.name = "acc";
    ArcPortId pla = arc_graph_add_port(g, la, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, p02, pla);

    ArcRegionId rc = arc_graph_add_region(g, ARC_REGION_CYCLE, r0);
    ArcNodeId cyc = arc_graph_add_node(g, ARC_NODE_CYCLE, r0, 7104);
    g->nodes[cyc].attr.cycle.body_region = rc;
    build_sum_cycle(g, rc, limit);

    ArcNodeId ra = arc_graph_add_node(g, ARC_NODE_VAR_REF, r0, 7200);
    g->nodes[ra].attr.name = "acc";
    ArcPortId pra = arc_graph_add_port(g, ra, ARC_PORT_OUTPUT, "out");
    ArcNodeId out = arc_graph_add_node(g, ARC_NODE_ROOT_OUTPUT, r0, 7201);
    ArcPortId pov = arc_graph_add_port(g, out, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, pra, pov);
    g->output_node = out;
}

/* ================================================================
 * Tests
 * ================================================================ */

TEST(test_cycle_sum_5) {
    /* sum(0..4) = 0+1+2+3+4 = 10 */
    ArcGraph g; build_sum_graph(&g, 5);
    ArcCompileResult cr; ArcVm vm; ArcValue result;
    compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 10);
    cleanup(&vm, &cr, &g);
}

TEST(test_cycle_sum_0) {
    /* limit=0 → BREAK_IF fires immediately, acc stays 0 */
    ArcGraph g; build_sum_graph(&g, 0);
    ArcCompileResult cr; ArcVm vm; ArcValue result;
    compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 0);
    cleanup(&vm, &cr, &g);
}

TEST(test_cycle_sum_1) {
    /* limit=1 → one iteration, acc = 0 */
    ArcGraph g; build_sum_graph(&g, 1);
    ArcCompileResult cr; ArcVm vm; ArcValue result;
    compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 0);
    cleanup(&vm, &cr, &g);
}

TEST(test_cycle_sum_100) {
    /* sum(0..99) = 4950 */
    ArcGraph g; build_sum_graph(&g, 100);
    ArcCompileResult cr; ArcVm vm; ArcValue result;
    compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 4950);
    cleanup(&vm, &cr, &g);
}

TEST(test_cycle_interp_agreement) {
    /* Differential: interpreter and compiler+VM must agree */
    ArcGraph g; build_sum_graph(&g, 5);

    ArcInterpResult ir = arc_interpret(&g);
    ASSERT(ir.success);
    ASSERT(ir.result.tag == VAL_I64);
    ASSERT_EQ_I64(ir.result.as.i64, 10);

    ArcCompileResult cr; ArcVm vm; ArcValue result;
    compile_run(&g, &cr, &vm, &result);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 10);
    cleanup(&vm, &cr, &g);
}

TEST(test_cycle_semantic_lowering) {
    /* Verify that the semantic pass lowers CYCLE to HIR correctly */
    ArcGraph g; build_sum_graph(&g, 5);
    ArcSemanticResult sr = arc_semantic_lower(&g);
    ASSERT(sr.success);
    /* main_body should contain: LET i, LET acc, WHILE(...) */
    ASSERT(sr.module.main_body.count >= 3);
    /* The third statement should be a WHILE (lowered from CYCLE) */
    ASSERT(sr.module.main_body.stmts[2].kind == HIR_STMT_WHILE);
    arc_semantic_result_free(&sr);
    arc_graph_free(&g);
}

TEST(test_cycle_scope_resolution) {
    /* Variables defined before CYCLE are visible inside */
    ArcGraph g; build_sum_graph(&g, 3);
    ArcSemanticResult sr = arc_semantic_lower(&g);
    ASSERT(sr.success);
    ASSERT(sr.diagnostics.count == 0);
    arc_semantic_result_free(&sr);
    arc_graph_free(&g);
}
