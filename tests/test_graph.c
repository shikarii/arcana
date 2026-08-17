/*
 * Arcana test suite — graph construction, HIR, MIR, and semantic analysis tests.
 * Split from test_all.c for modularity.
 */
#include "test_harness.h"

/* ================================================================
 * Semantic Graph Validation
 * ================================================================ */

TEST(test_graph_valid) {
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n0 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 100);
    g.nodes[n0].attr.int_value = 5;
    ArcPortId p0out = arc_graph_add_port(&g, n0, ARC_PORT_OUTPUT, "out");

    ArcNodeId n1 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 101);
    g.nodes[n1].attr.int_value = 10;
    ArcPortId p1out = arc_graph_add_port(&g, n1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n2 = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 102);
    ArcPortId p2lhs = arc_graph_add_port(&g, n2, ARC_PORT_INPUT, "lhs");
    ArcPortId p2rhs = arc_graph_add_port(&g, n2, ARC_PORT_INPUT, "rhs");
    ArcPortId p2out = arc_graph_add_port(&g, n2, ARC_PORT_OUTPUT, "out");
    (void)p2out;

    ArcPortId order[] = { p2lhs, p2rhs, p2out };
    arc_node_set_cyclic_order(&g, n2, order, 3);

    arc_graph_add_edge(&g, p0out, p2lhs);
    arc_graph_add_edge(&g, p1out, p2rhs);

    ArcGraphValidation v = arc_graph_validate(&g);
    ASSERT(v.valid);
    arc_graph_validation_free(&v);
    arc_graph_free(&g);
}

TEST(test_graph_invalid_edge) {
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n0 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 100);
    ArcPortId p0in = arc_graph_add_port(&g, n0, ARC_PORT_INPUT, "in"); /* wrong direction! */

    ArcNodeId n1 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 101);
    ArcPortId p1in = arc_graph_add_port(&g, n1, ARC_PORT_INPUT, "in");

    /* Edge from input to input — should fail validation */
    arc_graph_add_edge(&g, p0in, p1in);

    ArcGraphValidation v = arc_graph_validate(&g);
    ASSERT(!v.valid);
    arc_graph_validation_free(&v);
    arc_graph_free(&g);
}

TEST(test_graph_validation_bidirectional_port) {
    /* Bidirectional port direction should be accepted */
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n0 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 100);
    g.nodes[n0].attr.int_value = 1;
    ArcPortId p_bidir = arc_graph_add_port(&g, n0, ARC_PORT_BIDIRECTIONAL, "io");

    ASSERT(g.ports[p_bidir].dir == ARC_PORT_BIDIRECTIONAL);

    ArcGraphValidation v = arc_graph_validate(&g);
    ASSERT(v.valid);
    arc_graph_validation_free(&v);
    arc_graph_free(&g);
}

/* ================================================================
 * HIR: Construction, dump, validation
 * ================================================================ */

TEST(test_hir_construct_and_dump) {
    /* Build a simple HIR module: main_body = print(5 + 10) */
    HirModule m;
    hir_module_init(&m);

    HirExpr* five = hir_expr_new(HIR_CONST_INT, 100);
    five->as.int_val = 5;

    HirExpr* ten = hir_expr_new(HIR_CONST_INT, 101);
    ten->as.int_val = 10;

    HirExpr* add = hir_expr_new(HIR_ADD, 102);
    add->as.binary.lhs = five;
    add->as.binary.rhs = ten;

    HirStmt* stmt = hir_block_add(&m.main_body, HIR_STMT_PRINT, 103);
    stmt->as.print.value = add;

    ASSERT(m.main_body.count == 1);
    ASSERT(m.main_body.stmts[0].kind == HIR_STMT_PRINT);
    ASSERT(m.main_body.stmts[0].as.print.value->kind == HIR_ADD);
    ASSERT(m.main_body.stmts[0].as.print.value->as.binary.lhs->as.int_val == 5);
    ASSERT(m.main_body.stmts[0].as.print.value->as.binary.rhs->as.int_val == 10);

    /* Dump to NUL — exercises the pretty-printer without crashing */
    FILE* f = fopen(DEV_NULL, "w");
    hir_dump(&m, f);
    fclose(f);

    hir_module_free(&m);
}

TEST(test_hir_function_with_params) {
    /* Build HIR: fn add1(x) { return x + 1 } */
    HirModule m;
    hir_module_init(&m);

    m.func_cap = 4;
    m.functions = ARC_ALLOC(HirFunction, 4);
    memset(m.functions, 0, sizeof(HirFunction) * 4);
    m.func_count = 1;

    HirFunction* fn = &m.functions[0];
    snprintf(fn->name, sizeof(fn->name), "%s", "add1");
    fn->arity = 1;
    fn->local_count = 1;
    fn->source_id = 200;

    HirExpr* var_x = hir_expr_new(HIR_VAR_LOAD, 201);
    var_x->as.var_idx = 0;

    HirExpr* one = hir_expr_new(HIR_CONST_INT, 202);
    one->as.int_val = 1;

    HirExpr* add_expr = hir_expr_new(HIR_ADD, 203);
    add_expr->as.binary.lhs = var_x;
    add_expr->as.binary.rhs = one;

    HirStmt* ret = hir_block_add(&fn->body, HIR_STMT_RETURN, 204);
    ret->as.ret.value = add_expr;

    ASSERT(fn->body.count == 1);
    ASSERT(fn->body.stmts[0].kind == HIR_STMT_RETURN);

    FILE* f = fopen(DEV_NULL, "w");
    hir_dump(&m, f);
    fclose(f);

    hir_module_free(&m);
}

TEST(test_hir_validation_valid) {
    /* A valid module should pass validation */
    HirModule m;
    hir_module_init(&m);

    HirExpr* val = hir_expr_new(HIR_CONST_INT, 50);
    val->as.int_val = 42;
    HirStmt* stmt = hir_block_add(&m.main_body, HIR_STMT_PRINT, 51);
    stmt->as.print.value = val;

    HirValidation v = hir_validate(&m);
    ASSERT(v.valid);
    ASSERT(v.count == 0);
    hir_validation_free(&v);

    hir_module_free(&m);
}

TEST(test_hir_validation_null_expr) {
    /* Print with NULL value — validator should catch this */
    HirModule m;
    hir_module_init(&m);

    HirStmt* stmt = hir_block_add(&m.main_body, HIR_STMT_PRINT, 60);
    stmt->as.print.value = NULL;

    HirValidation v = hir_validate(&m);
    ASSERT(!v.valid);
    ASSERT(v.count > 0);
    hir_validation_free(&v);

    hir_module_free(&m);
}

/* ================================================================
 * MIR: Construction, dump, validation
 * ================================================================ */

TEST(test_mir_construct_and_dump) {
    /* Build: block_0: t0 = const_int 42; halt */
    MirModule m;
    mir_module_init(&m);

    MirFunction* fn = mir_module_add_func(&m);
    snprintf(fn->name, sizeof(fn->name), "%s", "main");
    fn->arity = 0;
    fn->local_count = 0;

    MirBlockId b0 = mir_func_add_block(fn);
    ASSERT(b0 == 0);

    MirBlock* blk = &fn->blocks[b0];
    MirTemp t0 = mir_func_new_temp(fn);
    ASSERT(t0 == 0);

    MirInstr* instr = mir_block_add_instr(blk);
    instr->kind = MIR_OP_CONST_INT;
    instr->dest = t0;
    instr->source_id = 10;
    instr->as.int_val = 42;

    blk->term.kind = MIR_TERM_HALT;
    blk->term.source_id = 11;
    blk->has_term = true;

    ASSERT(fn->block_count == 1);
    ASSERT(blk->instr_count == 1);
    ASSERT(blk->instrs[0].as.int_val == 42);

    FILE* f = fopen(DEV_NULL, "w");
    mir_dump(&m, f);
    fclose(f);

    mir_module_free(&m);
}

TEST(test_mir_branching) {
    /* Build: block_0 branches to block_1/block_2; both return */
    MirModule m;
    mir_module_init(&m);

    MirFunction* fn = mir_module_add_func(&m);
    snprintf(fn->name, sizeof(fn->name), "%s", "test");

    MirBlockId b0 = mir_func_add_block(fn);
    MirBlockId b1 = mir_func_add_block(fn);
    MirBlockId b2 = mir_func_add_block(fn);
    ASSERT(b0 == 0 && b1 == 1 && b2 == 2);

    /* b0: branch on t0 -> b1/b2 */
    MirTemp cond = mir_func_new_temp(fn);
    MirInstr* i0 = mir_block_add_instr(&fn->blocks[b0]);
    i0->kind = MIR_OP_CONST_BOOL;
    i0->dest = cond;
    i0->as.bool_val = true;

    fn->blocks[b0].term.kind = MIR_TERM_BRANCH;
    fn->blocks[b0].term.as.branch.cond = cond;
    fn->blocks[b0].term.as.branch.then_block = b1;
    fn->blocks[b0].term.as.branch.else_block = b2;
    fn->blocks[b0].has_term = true;

    /* b1: return t1 */
    MirTemp t1 = mir_func_new_temp(fn);
    MirInstr* i1 = mir_block_add_instr(&fn->blocks[b1]);
    i1->kind = MIR_OP_CONST_INT;
    i1->dest = t1;
    i1->as.int_val = 1;
    fn->blocks[b1].term.kind = MIR_TERM_RETURN;
    fn->blocks[b1].term.as.return_val = t1;
    fn->blocks[b1].has_term = true;

    /* b2: return t2 */
    MirTemp t2 = mir_func_new_temp(fn);
    MirInstr* i2 = mir_block_add_instr(&fn->blocks[b2]);
    i2->kind = MIR_OP_CONST_INT;
    i2->dest = t2;
    i2->as.int_val = 2;
    fn->blocks[b2].term.kind = MIR_TERM_RETURN;
    fn->blocks[b2].term.as.return_val = t2;
    fn->blocks[b2].has_term = true;

    MirValidation v = mir_validate(&m);
    ASSERT(v.valid);
    ASSERT(v.count == 0);
    mir_validation_free(&v);

    mir_module_free(&m);
}

TEST(test_mir_validation_missing_terminator) {
    /* Block without a terminator should fail validation */
    MirModule m;
    mir_module_init(&m);

    MirFunction* fn = mir_module_add_func(&m);
    snprintf(fn->name, sizeof(fn->name), "%s", "bad");
    mir_func_add_block(fn);  /* no terminator */

    MirValidation v = mir_validate(&m);
    ASSERT(!v.valid);
    ASSERT(v.count > 0);
    mir_validation_free(&v);

    mir_module_free(&m);
}

/* ================================================================
 * Semantic Analysis: Graph -> HIR lowering
 * ================================================================ */

TEST(test_semantic_5_plus_10) {
    /* Lower the canonical 5+10 graph to HIR and verify structure */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_5 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 1);
    g.nodes[n_5].attr.int_value = 5;
    ArcPortId p5 = arc_graph_add_port(&g, n_5, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_10 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 2);
    g.nodes[n_10].attr.int_value = 10;
    ArcPortId p10 = arc_graph_add_port(&g, n_10, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 3);
    ArcPortId pa_lhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId pa_rhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId pa_out = arc_graph_add_port(&g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId order[] = { pa_lhs, pa_rhs, pa_out };
    arc_node_set_cyclic_order(&g, n_add, order, 3);
    arc_graph_add_edge(&g, p5, pa_lhs);
    arc_graph_add_edge(&g, p10, pa_rhs);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 4);
    ArcPortId po_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, pa_out, po_in);

    /* Lower to HIR */
    ArcSemanticResult sr = arc_semantic_lower(&g);
    ASSERT(sr.success);
    ASSERT(sr.module.func_count == 0);       /* no functions, just main body */
    ASSERT(sr.module.main_body.count >= 1);  /* at least the output stmt */

    /* The main body should end with a print(5 + 10) statement */
    HirStmt* last = &sr.module.main_body.stmts[sr.module.main_body.count - 1];
    ASSERT(last->kind == HIR_STMT_PRINT);
    ASSERT(last->as.print.value != NULL);
    ASSERT(last->as.print.value->kind == HIR_ADD);
    ASSERT(last->as.print.value->as.binary.lhs->kind == HIR_CONST_INT);
    ASSERT(last->as.print.value->as.binary.lhs->as.int_val == 5);
    ASSERT(last->as.print.value->as.binary.rhs->kind == HIR_CONST_INT);
    ASSERT(last->as.print.value->as.binary.rhs->as.int_val == 10);

    /* Dump should not crash */
    FILE* f = fopen(DEV_NULL, "w");
    hir_dump(&sr.module, f);
    fclose(f);

    arc_semantic_result_free(&sr);
    arc_graph_free(&g);
}

TEST(test_semantic_undefined_variable) {
    /* A graph referencing an undefined variable should produce a diagnostic */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_ref = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r0, 10);
    g.nodes[n_ref].attr.name = "noexist";
    ArcPortId p_ref = arc_graph_add_port(&g, n_ref, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 11);
    ArcPortId p_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_ref, p_in);

    ArcSemanticResult sr = arc_semantic_lower(&g);
    ASSERT(!sr.success);
    ASSERT(sr.diagnostics.count > 0);
    ASSERT(sr.diagnostics.items[0].code == ARC_DIAG_SEM_0001);

    arc_semantic_result_free(&sr);
    arc_graph_free(&g);
}

TEST(test_semantic_null_graph) {
    /* Passing NULL should produce a clean error, not a crash */
    ArcSemanticResult sr = arc_semantic_lower(NULL);
    ASSERT(!sr.success);
    ASSERT(sr.diagnostics.count > 0);
    arc_semantic_result_free(&sr);
}

/* Build a double(x)=x+x function graph with call and output */
static void build_double_call_graph(ArcGraph* g) {
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
    ArcPortId pa_lhs = arc_graph_add_port(g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId pa_rhs = arc_graph_add_port(g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId pa_out = arc_graph_add_port(g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId add_order[] = { pa_lhs, pa_rhs, pa_out };
    arc_node_set_cyclic_order(g, n_add, add_order, 3);
    arc_graph_add_edge(g, p_ref1, pa_lhs);
    arc_graph_add_edge(g, p_ref2, pa_rhs);

    ArcNodeId n_ret = arc_graph_add_node(g, ARC_NODE_RETURN, r_body, 0);
    ArcPortId p_ret = arc_graph_add_port(g, n_ret, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(g, pa_out, p_ret);

    ArcNodeId n_7 = arc_graph_add_node(g, ARC_NODE_CONST_INT, r0, 0);
    g->nodes[n_7].attr.int_value = 7;
    ArcPortId p_7 = arc_graph_add_port(g, n_7, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_call = arc_graph_add_node(g, ARC_NODE_FUNC_CALL, r0, 0);
    g->nodes[n_call].attr.name = "double";
    ArcPortId p_call_arg = arc_graph_add_port(g, n_call, ARC_PORT_INPUT, "arg0");
    ArcPortId p_call_out = arc_graph_add_port(g, n_call, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(g, p_7, p_call_arg);

    ArcNodeId n_out = arc_graph_add_node(g, ARC_NODE_ROOT_OUTPUT, r0, 0);
    ArcPortId p_out_in = arc_graph_add_port(g, n_out, ARC_PORT_INPUT, "value");
    g->output_node = n_out;
    arc_graph_add_edge(g, p_call_out, p_out_in);
}

TEST(test_semantic_function_lowering) {
    ArcGraph g;
    build_double_call_graph(&g);

    ArcSemanticResult sr = arc_semantic_lower(&g);
    ASSERT(sr.success);
    ASSERT(sr.module.func_count == 1);
    ASSERT(strcmp(sr.module.functions[0].name, "double") == 0);
    ASSERT(sr.module.functions[0].arity == 1);
    ASSERT(sr.module.functions[0].body.count >= 1);

    arc_semantic_result_free(&sr);
    arc_graph_free(&g);
}
