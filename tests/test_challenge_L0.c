/*
 * Arcana Challenge Corpus -- Level 0: Machine Sanity
 *
 * 10 fixture-based tests that exercise the full pipeline:
 * fixture parse -> compile -> verify -> VM run -> result check.
 *
 * Plus inline comparison tests and an invalid-program rejection test.
 */
#include "test_harness.h"

/* ================================================================
 * Shared helper: load fixture, compile, run, return result
 * ================================================================ */

typedef struct {
    ArcValue      result;
    ArcVm         vm;
    ArcCompileResult cr;
    ArcFixtureResult fr;
    bool          ok;
} L0Run;

static L0Run l0_run_fixture(const char* filename) {
    L0Run r; memset(&r, 0, sizeof(r));
    char path[512]; fixture_path(path, sizeof(path), filename);
    r.fr = arc_fixture_parse_file(path);
    if (!r.fr.success) { printf("  parse: %s\n", r.fr.error); return r; }
    r.cr = arc_compile(&r.fr.graph);
    if (!r.cr.success) { arc_graph_free(&r.fr.graph); return r; }
    ArcVerifyResult vr = arc_verify(&r.cr.image);
    if (!vr.valid) { arc_compile_result_free(&r.cr); arc_graph_free(&r.fr.graph); return r; }
    arc_vm_init(&r.vm, &r.cr.image);
    r.vm.output = fopen(DEV_NULL, "w");
    if (arc_vm_run(&r.vm) != ARC_OK) { fclose(r.vm.output); return r; }
    fclose(r.vm.output);
    r.result = arc_vm_result(&r.vm);
    r.ok = true;
    return r;
}

static void l0_cleanup(L0Run* r) {
    if (r->ok) arc_vm_destroy(&r->vm);
    arc_compile_result_free(&r->cr);
    arc_graph_free(&r->fr.graph);
}

/* Helper for inline graph E2E: parse string, compile, run, check i64 */
static L0Run l0_run_inline(const char* graph_text) {
    L0Run r; memset(&r, 0, sizeof(r));
    r.fr = arc_fixture_parse(graph_text);
    if (!r.fr.success) { printf("  parse: %s\n", r.fr.error); return r; }
    r.cr = arc_compile(&r.fr.graph);
    if (!r.cr.success) { arc_graph_free(&r.fr.graph); return r; }
    arc_vm_init(&r.vm, &r.cr.image);
    r.vm.output = fopen(DEV_NULL, "w");
    if (arc_vm_run(&r.vm) != ARC_OK) { fclose(r.vm.output); return r; }
    fclose(r.vm.output);
    r.result = arc_vm_result(&r.vm);
    r.ok = true;
    return r;
}

/* ================================================================
 * L0.01: Constant 42
 * ================================================================ */
TEST(test_L0_01_constant) {
    L0Run r = l0_run_fixture("L0_01_constant.graph");
    ASSERT(r.ok);
    ASSERT(r.result.tag == VAL_I64);
    ASSERT_EQ_I64(r.result.as.i64, 42);
    l0_cleanup(&r);
}

/* ================================================================
 * L0.02: Simple addition (5 + 10 = 15)
 * ================================================================ */
TEST(test_L0_02_addition) {
    L0Run r = l0_run_fixture("L0_02_addition.graph");
    ASSERT(r.ok);
    ASSERT(r.result.tag == VAL_I64);
    ASSERT_EQ_I64(r.result.as.i64, 15);
    l0_cleanup(&r);
}

/* ================================================================
 * L0.03: All arithmetic ops chained → 2
 * ================================================================ */
TEST(test_L0_03_arithmetic) {
    L0Run r = l0_run_fixture("L0_03_arithmetic.graph");
    ASSERT(r.ok);
    ASSERT(r.result.tag == VAL_I64);
    ASSERT_EQ_I64(r.result.as.i64, 2);
    l0_cleanup(&r);
}

/* ================================================================
 * L0.04: Branch (if 3 < 5 → 42 else 0)
 * ================================================================ */
TEST(test_L0_04_branch) {
    L0Run r = l0_run_fixture("L0_04_branch.graph");
    ASSERT(r.ok);
    ASSERT(r.result.tag == VAL_I64);
    ASSERT_EQ_I64(r.result.as.i64, 42);
    l0_cleanup(&r);
}

/* ================================================================
 * L0.05: Loop (count to 10)
 * ================================================================ */
TEST(test_L0_05_loop) {
    L0Run r = l0_run_fixture("L0_05_loop.graph");
    ASSERT(r.ok);
    ASSERT(r.result.tag == VAL_I64);
    ASSERT_EQ_I64(r.result.as.i64, 10);
    l0_cleanup(&r);
}

/* ================================================================
 * L0.06: Function call (double(21) = 42)
 * ================================================================ */
TEST(test_L0_06_function) {
    L0Run r = l0_run_fixture("L0_06_function.graph");
    ASSERT(r.ok);
    ASSERT(r.result.tag == VAL_I64);
    ASSERT_EQ_I64(r.result.as.i64, 42);
    l0_cleanup(&r);
}

/* ================================================================
 * L0.07: Nested function calls (add_one(double(10)) = 21)
 * ================================================================ */
TEST(test_L0_07_nested_calls) {
    L0Run r = l0_run_fixture("L0_07_nested_calls.graph");
    ASSERT(r.ok);
    ASSERT(r.result.tag == VAL_I64);
    ASSERT_EQ_I64(r.result.as.i64, 21);
    l0_cleanup(&r);
}

/* ================================================================
 * L0.08: Recursive Fibonacci (fib(10) = 55)
 * ================================================================ */
TEST(test_L0_08_fib_recursive) {
    L0Run r = l0_run_fixture("L0_08_fib_recursive.graph");
    ASSERT(r.ok);
    ASSERT(r.result.tag == VAL_I64);
    ASSERT_EQ_I64(r.result.as.i64, 55);
    l0_cleanup(&r);
}

/* ================================================================
 * L0.09: Iterative Fibonacci (fib(10) = 55)
 * ================================================================ */
TEST(test_L0_09_fib_iterative) {
    L0Run r = l0_run_fixture("L0_09_fib_iterative.graph");
    ASSERT(r.ok);
    ASSERT(r.result.tag == VAL_I64);
    ASSERT_EQ_I64(r.result.as.i64, 55);
    l0_cleanup(&r);
}

/* ================================================================
 * L0.10: Stack-depth stress (chain of 9 additions → 10)
 * ================================================================ */
TEST(test_L0_10_stress) {
    L0Run r = l0_run_fixture("L0_10_stress.graph");
    ASSERT(r.ok);
    ASSERT(r.result.tag == VAL_I64);
    ASSERT_EQ_I64(r.result.as.i64, 10);
    l0_cleanup(&r);
}

/* ================================================================
 * L0.11: Every comparison operator (inline)
 * ================================================================ */
static void check_cmp(const char* op, int64_t lhs, int64_t rhs, bool expect) {
    char graph[256];
    snprintf(graph, sizeof(graph),
        "region r0 module\n"
        "node a const_int(%lld) in r0\n"
        "node b const_int(%lld) in r0\n"
        "node c %s in r0\n"
        "edge e0 a.out -> c.lhs\n"
        "edge e1 b.out -> c.rhs\n"
        "root c.out\n",
        (long long)lhs, (long long)rhs, op);
    L0Run r = l0_run_inline(graph);
    ASSERT(r.ok);
    ASSERT(r.result.tag == VAL_BOOL);
    ASSERT(r.result.as.b == expect);
    l0_cleanup(&r);
}

TEST(test_L0_11_comparisons) {
    check_cmp("eq", 5, 5, true);
    check_cmp("eq", 5, 3, false);
    check_cmp("neq", 5, 3, true);
    check_cmp("neq", 5, 5, false);
    check_cmp("lt", 3, 5, true);
    check_cmp("lt", 5, 3, false);
    check_cmp("le", 3, 3, true);
    check_cmp("le", 4, 3, false);
    check_cmp("gt", 5, 3, true);
    check_cmp("gt", 3, 5, false);
    check_cmp("ge", 3, 3, true);
    check_cmp("ge", 2, 3, false);
}

/* ================================================================
 * L0.12: Invalid program rejection
 * ================================================================ */
TEST(test_L0_12_invalid_rejection) {
    char path[512]; fixture_path(path, sizeof(path), "malformed.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(!fr.success);
    ASSERT(strlen(fr.error) > 0);
}

/* Tests registered in test_all.c */
