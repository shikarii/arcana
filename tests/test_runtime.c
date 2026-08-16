/*
 * Arcana test suite — arena allocator, type checker, records, diagnostics.
 * Split from test_all.c for modularity.
 */
#include "test_harness.h"

/* ================================================================
 * Arena Allocator Tests
 * ================================================================ */

TEST(test_arena_basic) {
    ArcArena a; arc_arena_init(&a);
    int* p1 = (int*)arc_arena_alloc(&a, sizeof(int));
    ASSERT(p1 != NULL);
    *p1 = 42;
    ASSERT(*p1 == 42);

    int* p2 = (int*)arc_arena_alloc(&a, sizeof(int));
    ASSERT(p2 != NULL);
    ASSERT(p2 != p1);
    *p2 = 99;
    ASSERT(*p1 == 42); /* p1 still valid */

    arc_arena_free(&a);
    ASSERT(a.head == NULL);
    ASSERT(a.total == 0);
}

TEST(test_arena_many_allocs) {
    ArcArena a; arc_arena_init(&a);
    /* Allocate enough to span multiple blocks */
    for (int i = 0; i < 10000; i++) {
        int* p = (int*)arc_arena_alloc(&a, sizeof(int));
        ASSERT(p != NULL);
        *p = i;
    }
    ASSERT(a.total > 0);
    arc_arena_free(&a);
}

TEST(test_arena_large_alloc) {
    ArcArena a; arc_arena_init(&a);
    /* Allocate larger than default block size */
    void* p = arc_arena_alloc(&a, 128 * 1024);
    ASSERT(p != NULL);
    arc_arena_free(&a);
}

/* ================================================================
 * Type Checker Tests
 * ================================================================ */

TEST(test_typecheck_arithmetic_valid) {
    /* 5 + 10 should pass type checking with i64 result */
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId root = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = root;

    ArcNodeId c5 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, root, 1);
    g.nodes[c5].attr.int_value = 5;
    ArcNodeId c10 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, root, 2);
    g.nodes[c10].attr.int_value = 10;
    ArcNodeId add = arc_graph_add_node(&g, ARC_NODE_ADD, root, 3);

    ArcPortId p_lhs = arc_graph_add_port(&g, add, ARC_PORT_INPUT, "lhs");
    ArcPortId p_rhs = arc_graph_add_port(&g, add, ARC_PORT_INPUT, "rhs");
    ArcPortId p_5out = arc_graph_add_port(&g, c5, ARC_PORT_OUTPUT, "out");
    ArcPortId p_10out = arc_graph_add_port(&g, c10, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_5out, p_lhs);
    arc_graph_add_edge(&g, p_10out, p_rhs);

    ArcTypeCheckResult r = arc_typecheck(&g);
    ASSERT(r.success);
    ASSERT(r.node_types[c5] == ARC_TYPE_I64);
    ASSERT(r.node_types[c10] == ARC_TYPE_I64);
    ASSERT(r.node_types[add] == ARC_TYPE_I64);

    arc_typecheck_result_free(&r);
    arc_graph_free(&g);
}

TEST(test_typecheck_bitwise_error) {
    /* BIT_AND on a float should produce an error */
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId root = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = root;

    ArcNodeId cf = arc_graph_add_node(&g, ARC_NODE_CONST_FLOAT, root, 1);
    g.nodes[cf].attr.float_value = 3.14;
    ArcNodeId ci = arc_graph_add_node(&g, ARC_NODE_CONST_INT, root, 2);
    g.nodes[ci].attr.int_value = 7;
    ArcNodeId ba = arc_graph_add_node(&g, ARC_NODE_BIT_AND, root, 3);

    ArcPortId p_lhs = arc_graph_add_port(&g, ba, ARC_PORT_INPUT, "lhs");
    ArcPortId p_rhs = arc_graph_add_port(&g, ba, ARC_PORT_INPUT, "rhs");
    ArcPortId p_fout = arc_graph_add_port(&g, cf, ARC_PORT_OUTPUT, "out");
    ArcPortId p_iout = arc_graph_add_port(&g, ci, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_fout, p_lhs);
    arc_graph_add_edge(&g, p_iout, p_rhs);

    ArcTypeCheckResult r = arc_typecheck(&g);
    ASSERT(!r.success);
    ASSERT(r.diagnostics.count > 0);

    arc_typecheck_result_free(&r);
    arc_graph_free(&g);
}

TEST(test_typecheck_string_concat) {
    /* string + string should type as string */
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId root = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = root;

    ArcNodeId s1 = arc_graph_add_node(&g, ARC_NODE_CONST_STRING, root, 1);
    (void)s1;
    ArcNodeId s2 = arc_graph_add_node(&g, ARC_NODE_CONST_STRING, root, 2);
    (void)s2;
    ArcNodeId add = arc_graph_add_node(&g, ARC_NODE_ADD, root, 3);

    ArcPortId p_lhs = arc_graph_add_port(&g, add, ARC_PORT_INPUT, "lhs");
    ArcPortId p_rhs = arc_graph_add_port(&g, add, ARC_PORT_INPUT, "rhs");
    ArcPortId p_s1out = arc_graph_add_port(&g, s1, ARC_PORT_OUTPUT, "out");
    ArcPortId p_s2out = arc_graph_add_port(&g, s2, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_s1out, p_lhs);
    arc_graph_add_edge(&g, p_s2out, p_rhs);

    ArcTypeCheckResult r = arc_typecheck(&g);
    ASSERT(r.success);
    ASSERT(r.node_types[add] == ARC_TYPE_STRING);

    arc_typecheck_result_free(&r);
    arc_graph_free(&g);
}

TEST(test_typecheck_cast_result_type) {
    /* cast_i64 should always produce i64 */
    ArcGraph g; arc_graph_init(&g);
    ArcRegionId root = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = root;

    ArcNodeId cf = arc_graph_add_node(&g, ARC_NODE_CONST_FLOAT, root, 1);
    g.nodes[cf].attr.float_value = 3.14;
    ArcNodeId cast = arc_graph_add_node(&g, ARC_NODE_CAST_I64, root, 2);

    ArcPortId p_in = arc_graph_add_port(&g, cast, ARC_PORT_INPUT, "value");
    ArcPortId p_fout = arc_graph_add_port(&g, cf, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_fout, p_in);

    ArcTypeCheckResult r = arc_typecheck(&g);
    ASSERT(r.success);
    ASSERT(r.node_types[cast] == ARC_TYPE_I64);

    arc_typecheck_result_free(&r);
    arc_graph_free(&g);
}

TEST(test_typecheck_empty_graph) {
    /* Empty graph should succeed */
    ArcGraph g; arc_graph_init(&g);
    ArcTypeCheckResult r = arc_typecheck(&g);
    ASSERT(r.success);
    arc_typecheck_result_free(&r);
    arc_graph_free(&g);
}

TEST(test_type_name) {
    ASSERT(strcmp(arc_type_name(ARC_TYPE_I64), "i64") == 0);
    ASSERT(strcmp(arc_type_name(ARC_TYPE_F64), "f64") == 0);
    ASSERT(strcmp(arc_type_name(ARC_TYPE_STRING), "string") == 0);
    ASSERT(strcmp(arc_type_name(ARC_TYPE_BOOL), "bool") == 0);
    ASSERT(strcmp(arc_type_name(ARC_TYPE_NULL), "null") == 0);
    ASSERT(strcmp(arc_type_name(ARC_TYPE_ARRAY), "array") == 0);
    ASSERT(strcmp(arc_type_name(ARC_TYPE_MAP), "map") == 0);
    ASSERT(strcmp(arc_type_name(ARC_TYPE_CLOSURE), "closure") == 0);
    ASSERT(strcmp(arc_type_name(ARC_TYPE_ANY), "any") == 0);
}

/* ================================================================
 * Record Tests
 * ================================================================ */

TEST(test_record_create_and_fields) {
    ArcGC gc; arc_gc_init(&gc);
    ArcObjRecord* rec = arc_obj_record_new(&gc, "Point", 2);
    ASSERT(rec != NULL);
    ASSERT(strcmp(rec->type_name, "Point") == 0);
    ASSERT(rec->field_count == 0);

    /* Set fields */
    arc_obj_record_set(rec, "x", arc_val_i64(10));
    arc_obj_record_set(rec, "y", arc_val_i64(20));
    ASSERT(rec->field_count == 2);

    /* Get fields */
    ArcValue v;
    ASSERT(arc_obj_record_get(rec, "x", &v));
    ASSERT(v.tag == VAL_I64 && v.as.i64 == 10);
    ASSERT(arc_obj_record_get(rec, "y", &v));
    ASSERT(v.tag == VAL_I64 && v.as.i64 == 20);

    /* Missing field */
    ASSERT(!arc_obj_record_get(rec, "z", &v));

    /* Update field */
    arc_obj_record_set(rec, "x", arc_val_i64(99));
    ASSERT(arc_obj_record_get(rec, "x", &v));
    ASSERT(v.as.i64 == 99);

    arc_gc_free_all(&gc);
}

TEST(test_record_gc_tracing) {
    ArcGC gc; arc_gc_init(&gc);

    /* Create a record with string fields */
    ArcObjRecord* rec = arc_obj_record_new(&gc, "Named", 2);
    ArcObjString* name = arc_obj_string_new(&gc, "hello", 5);
    arc_obj_record_set(rec, "name", arc_val_obj((ArcObject*)name));
    arc_obj_record_set(rec, "val", arc_val_i64(42));

    /* Put record on stack and collect — record + string should survive */
    ArcValue stack[4];
    stack[0] = arc_val_obj((ArcObject*)rec);

    arc_gc_collect(&gc, stack, 1, NULL, 0, NULL);

    /* Record still alive */
    ASSERT(ARC_IS_RECORD(stack[0]));
    ArcObjRecord* r = ARC_AS_RECORD(stack[0]);
    ASSERT(r->field_count == 2);
    ArcValue v;
    ASSERT(arc_obj_record_get(r, "name", &v));
    ASSERT(ARC_IS_STRING(v));

    arc_gc_free_all(&gc);
}

TEST(test_record_print) {
    ArcGC gc; arc_gc_init(&gc);
    ArcObjRecord* rec = arc_obj_record_new(&gc, "Pair", 2);
    arc_obj_record_set(rec, "a", arc_val_i64(1));
    arc_obj_record_set(rec, "b", arc_val_i64(2));

    /* Just verify print doesn't crash */
    FILE* devnull = fopen(DEV_NULL, "w");
    arc_obj_print((ArcObject*)rec, devnull);
    fclose(devnull);

    ASSERT(strcmp(arc_obj_type_name(OBJ_RECORD), "record") == 0);
    arc_gc_free_all(&gc);
}

TEST(test_diag_type_code_format) {
    /* Verify TYPE diagnostic codes format correctly */
    const char* s = arc_diag_code_str(3001);
    ASSERT(strcmp(s, "ARC-TYPE-0001") == 0);
    s = arc_diag_code_str(3002);
    ASSERT(strcmp(s, "ARC-TYPE-0002") == 0);
}
