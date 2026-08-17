/*
 * Arcana test suite -- Tooling Service API tests.
 */
#include "test_harness.h"
#include "../src/service/arcana_service.h"

static const char* GRAPH_42 =
    "region r0 module\n"
    "node n0 const_int(42) in r0\n"
    "root n0.out\n";

static const char* GRAPH_ADD =
    "region r0 module\n"
    "node a const_int(5) in r0\n"
    "node b const_int(10) in r0\n"
    "node c add in r0\n"
    "edge e0 a.out -> c.lhs\n"
    "edge e1 b.out -> c.rhs\n"
    "root c.out\n";

static const char* GRAPH_BAD =
    "node n0 const_int(1) in r_nonexistent\n";

/* ================================================================
 * Test: arc_service_run succeeds on valid program
 * ================================================================ */
TEST(test_service_run_ok) {
    ArcServiceResult* r = arc_service_run(GRAPH_42);
    ASSERT(arc_service_ok(r));
    ASSERT(arc_service_stage(r) == ARC_STAGE_RUN);
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 42);
    arc_service_free(r);
}

/* ================================================================
 * Test: arc_service_run with addition
 * ================================================================ */
TEST(test_service_run_add) {
    ArcServiceResult* r = arc_service_run(GRAPH_ADD);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 15);
    arc_service_free(r);
}

/* ================================================================
 * Test: arc_service_run fails on bad graph
 * ================================================================ */
TEST(test_service_run_parse_fail) {
    ArcServiceResult* r = arc_service_run(GRAPH_BAD);
    ASSERT(!arc_service_ok(r));
    ASSERT(arc_service_stage(r) == ARC_STAGE_PARSE);
    ASSERT(strlen(arc_service_error(r)) > 0);
    arc_service_free(r);
}

/* ================================================================
 * Test: arc_service_check returns diagnostics without running
 * ================================================================ */
TEST(test_service_check_ok) {
    ArcServiceResult* r = arc_service_check(GRAPH_42);
    ASSERT(arc_service_ok(r));
    ASSERT(arc_service_stage(r) == ARC_STAGE_TYPECHECK);
    const ArcTypeCheckResult* tc = arc_service_types(r);
    ASSERT(tc != NULL);
    arc_service_free(r);
}

/* ================================================================
 * Test: arc_service_compile produces verified bytecode
 * ================================================================ */
TEST(test_service_compile_ok) {
    ArcServiceResult* r = arc_service_compile(GRAPH_ADD);
    ASSERT(arc_service_ok(r));
    ASSERT(arc_service_stage(r) == ARC_STAGE_VERIFY);
    arc_service_free(r);
}

/* ================================================================
 * Test: arc_service_check on bad graph fails at parse
 * ================================================================ */
TEST(test_service_check_parse_fail) {
    ArcServiceResult* r = arc_service_check(GRAPH_BAD);
    ASSERT(!arc_service_ok(r));
    ASSERT(arc_service_stage(r) == ARC_STAGE_PARSE);
    arc_service_free(r);
}

/* ================================================================
 * Test: diagnostics list is accessible
 * ================================================================ */
TEST(test_service_diagnostics) {
    ArcServiceResult* r = arc_service_run(GRAPH_42);
    const ArcDiagList* d = arc_service_diagnostics(r);
    ASSERT(d != NULL);
    arc_service_free(r);
}

/* ================================================================
 * Test: error string is empty on success
 * ================================================================ */
TEST(test_service_error_empty_on_success) {
    ArcServiceResult* r = arc_service_run(GRAPH_42);
    ASSERT(arc_service_ok(r));
    ASSERT(strlen(arc_service_error(r)) == 0);
    arc_service_free(r);
}

/* Tests registered in test_all.c */
