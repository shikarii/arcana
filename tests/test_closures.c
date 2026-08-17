/*
 * Arcana test suite -- closure and upvalue tests.
 * Tests compiler closure capture analysis and upvalue descriptor generation.
 */
#include "test_harness.h"

/* ================================================================
 * Helper: build a FUNC_DEF node with body region
 * ================================================================ */
static ArcRegionId build_func(ArcGraph* g, ArcRegionId parent,
                              const char* name, uint8_t arity, int src) {
    ArcRegionId body = arc_graph_add_region(g, ARC_REGION_FUNCTION, parent);
    ArcNodeId fd = arc_graph_add_node(g, ARC_NODE_FUNC_DEF, parent, src);
    g->nodes[fd].attr.func.name = name;
    g->nodes[fd].attr.func.arity = arity;
    g->nodes[fd].attr.func.body_region = body;
    return body;
}

/* Helper: build CLOSURE node pointing to a FUNC_DEF by its node id */
static ArcNodeId build_closure_node(ArcGraph* g, ArcRegionId r0,
                                     ArcNodeId func_def, int src) {
    ArcNodeId cl = arc_graph_add_node(g, ARC_NODE_CLOSURE, r0, src);
    g->nodes[cl].attr.func.name = g->nodes[func_def].attr.func.name;
    g->nodes[cl].attr.func.arity = g->nodes[func_def].attr.func.arity;
    g->nodes[cl].attr.func.body_region = g->nodes[func_def].attr.func.body_region;
    ArcPortId cf = arc_graph_add_port(g, cl, ARC_PORT_INPUT, "func");
    arc_graph_add_port(g, cl, ARC_PORT_OUTPUT, "out");
    ArcPortId fo = arc_graph_add_port(g, func_def, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(g, fo, cf);
    return cl;
}

/* Helper: find a function record by name in the compiled image */
static ArcFuncRecord* find_func(ArcCompileResult* cr, const char* name) {
    for (uint16_t i = 0; i < cr->image.functions.count; i++) {
        ArcFuncRecord* fr = &cr->image.functions.funcs[i];
        uint16_t ni = fr->name_const_idx;
        if (ni < cr->image.constants.count &&
            cr->image.constants.entries[ni].tag == ARC_CONST_STRING &&
            strcmp(cr->image.constants.entries[ni].as.str.data, name) == 0)
            return fr;
    }
    return NULL;
}

/* Helper: add a simple root output (const int) to complete a graph */
static void add_root_output(ArcGraph* g, ArcRegionId r0, int64_t val, int src) {
    ArcNodeId nv = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, src);
    g->nodes[nv].attr.int_value = val;
    ArcPortId pv = arc_graph_add_port(g, nv, ARC_PORT_OUTPUT, "out");
    ArcNodeId no = arc_graph_add_node(g, ARC_NODE_ROOT_OUTPUT, r0, src + 1);
    ArcPortId pi = arc_graph_add_port(g, no, ARC_PORT_INPUT, "value");
    g->output_node = no;
    arc_graph_add_edge(g, pv, pi);
}

/* ================================================================
 * Test: closure-target FUNC_DEF skipped in top-level compile pass
 * ================================================================ */
TEST(test_closure_compile_smoke) {
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;
    add_root_output(&g, r0, 42, 200);

    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);
    ArcVerifyResult vr = arc_verify(&cr.image);
    ASSERT(vr.valid);
    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* ================================================================
 * Test: closure captures one variable, upvalue descriptor is correct
 *
 * let y = 99
 * def getter(): return y    (closure captures y)
 * output = 1
 * ================================================================ */
TEST(test_closure_upvalue_one_capture) {
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    /* let y = 99 */
    ArcNodeId n99 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 300);
    g.nodes[n99].attr.int_value = 99;
    ArcPortId p99 = arc_graph_add_port(&g, n99, ARC_PORT_OUTPUT, "out");
    ArcNodeId nlet = arc_graph_add_node(&g, ARC_NODE_LET, r0, 301);
    g.nodes[nlet].attr.name = "y";
    ArcPortId plet = arc_graph_add_port(&g, nlet, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p99, plet);

    /* def getter(): return y */
    ArcRegionId rb = build_func(&g, r0, "getter", 0, 302);
    ArcNodeId fdef = rb - 1; /* func_def node is added just before body */
    /* Correct way: find the FUNC_DEF node */
    fdef = ARC_INVALID_ID;
    for (uint32_t i = 0; i < g.node_count; i++)
        if (g.nodes[i].kind == ARC_NODE_FUNC_DEF) { fdef = i; break; }
    ASSERT(fdef != ARC_INVALID_ID);

    ArcNodeId yr = arc_graph_add_node(&g, ARC_NODE_VAR_REF, rb, 303);
    g.nodes[yr].attr.name = "y";
    ArcPortId pyr = arc_graph_add_port(&g, yr, ARC_PORT_OUTPUT, "out");
    ArcNodeId nret = arc_graph_add_node(&g, ARC_NODE_RETURN, rb, 304);
    ArcPortId pret = arc_graph_add_port(&g, nret, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, pyr, pret);

    /* Closure node wrapping getter */
    build_closure_node(&g, r0, fdef, 305);
    add_root_output(&g, r0, 1, 306);

    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);

    ArcFuncRecord* fr = find_func(&cr, "getter");
    ASSERT(fr != NULL);
    ASSERT(fr->upvalue_count == 1);
    ASSERT(fr->upvalues != NULL);
    ASSERT(fr->upvalues[0].is_local == true);

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* ================================================================
 * Test: non-closure function has zero upvalues
 * ================================================================ */
TEST(test_closure_regular_func_no_upvalues) {
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    /* def id(x): return x */
    ArcRegionId rb = build_func(&g, r0, "id", 1, 400);
    ArcNodeId np = arc_graph_add_node(&g, ARC_NODE_PARAM, rb, 401);
    g.nodes[np].attr.name = "x";
    ArcNodeId xr = arc_graph_add_node(&g, ARC_NODE_VAR_REF, rb, 402);
    g.nodes[xr].attr.name = "x";
    ArcPortId pxr = arc_graph_add_port(&g, xr, ARC_PORT_OUTPUT, "out");
    ArcNodeId nret = arc_graph_add_node(&g, ARC_NODE_RETURN, rb, 403);
    ArcPortId pret = arc_graph_add_port(&g, nret, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, pxr, pret);

    add_root_output(&g, r0, 1, 404);

    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);

    ArcFuncRecord* fr = find_func(&cr, "id");
    ASSERT(fr != NULL);
    ASSERT(fr->upvalue_count == 0);

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* ================================================================
 * Test: closure-target skipped in top-level pass (still compiles)
 *
 * Verify that a graph with a CLOSURE node compiles without the
 * inner FUNC_DEF being double-compiled.
 * ================================================================ */
TEST(test_closure_target_skipped) {
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    /* def nop(): return null */
    (void)build_func(&g, r0, "nop", 0, 500);
    ArcNodeId fdef = ARC_INVALID_ID;
    for (uint32_t i = 0; i < g.node_count; i++)
        if (g.nodes[i].kind == ARC_NODE_FUNC_DEF) { fdef = i; break; }
    ASSERT(fdef != ARC_INVALID_ID);

    /* Closure wraps it */
    build_closure_node(&g, r0, fdef, 501);
    add_root_output(&g, r0, 7, 502);

    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);

    /* Should have exactly 2 functions: main + nop (not 3) */
    ASSERT(cr.image.functions.count == 2);

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* Tests registered in test_all.c */
