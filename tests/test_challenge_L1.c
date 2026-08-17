/*
 * Arcana Challenge Corpus -- Level 1: Classic Algorithms
 *
 * 8 fixture-based tests that exercise classic algorithms through
 * the full pipeline: fixture parse -> compile -> verify -> VM run.
 */
#include "test_harness.h"
#include "../src/service/arcana_service.h"

/* ================================================================
 * Shared helper: run fixture via service API, check i64 result
 * ================================================================ */

static ArcServiceResult* run_fixture_L1(const char* filename) {
    char path[512]; fixture_path(path, sizeof(path), filename);
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    ArcServiceResult* r = arc_service_run(buf);
    free(buf);
    return r;
}

/* ================================================================
 * L1.01: Recursive factorial — fact(10) = 3628800
 * ================================================================ */
TEST(test_L1_01_factorial) {
    ArcServiceResult* r = run_fixture_L1("L1_01_factorial.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 3628800);
    arc_service_free(r);
}

/* ================================================================
 * L1.02: Euclidean GCD — gcd(48, 18) = 6
 * ================================================================ */
TEST(test_L1_02_gcd) {
    ArcServiceResult* r = run_fixture_L1("L1_02_gcd.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 6);
    arc_service_free(r);
}

/* ================================================================
 * L1.03: Iterative power — pow(2,10) = 1024
 * ================================================================ */
TEST(test_L1_03_power) {
    ArcServiceResult* r = run_fixture_L1("L1_03_power.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 1024);
    arc_service_free(r);
}

/* ================================================================
 * L1.04: Count multiples of 3 in [1..15] = 5
 * ================================================================ */
TEST(test_L1_04_fizzbuzz) {
    ArcServiceResult* r = run_fixture_L1("L1_04_fizzbuzz_count.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 5);
    arc_service_free(r);
}

/* ================================================================
 * L1.05: Sum of squares 1^2+...+10^2 = 385
 * ================================================================ */
TEST(test_L1_05_sum_squares) {
    ArcServiceResult* r = run_fixture_L1("L1_05_sum_squares.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 385);
    arc_service_free(r);
}

/* ================================================================
 * L1.06: Collatz sequence length from 27 = 111 steps
 * ================================================================ */
TEST(test_L1_06_collatz) {
    ArcServiceResult* r = run_fixture_L1("L1_06_collatz.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 111);
    arc_service_free(r);
}

/* ================================================================
 * L1.07: Primality test — is_prime(97) = 1
 * ================================================================ */
TEST(test_L1_07_is_prime) {
    ArcServiceResult* r = run_fixture_L1("L1_07_is_prime.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 1);
    arc_service_free(r);
}

/* ================================================================
 * L1.08: Mutual recursion — is_even(10) = 1
 * ================================================================ */
TEST(test_L1_08_mutual_recursion) {
    ArcServiceResult* r = run_fixture_L1("L1_08_mutual_recursion.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 1);
    arc_service_free(r);
}

/* ================================================================
 * L1.09: Bubble sort — sort [5,3,8,1,2], return arr[0] = 1
 * ================================================================ */
TEST(test_L1_09_bubble_sort) {
    ArcServiceResult* r = run_fixture_L1("L1_09_bubble_sort.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 1);
    arc_service_free(r);
}

/* ================================================================
 * L1.10: Binary search — find 7 in sorted array, index = 3
 * ================================================================ */
TEST(test_L1_10_binary_search) {
    ArcServiceResult* r = run_fixture_L1("L1_10_binary_search.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 3);
    arc_service_free(r);
}

/* ================================================================
 * L1.11: Selection sort — sort [4,2,7,1,3], return max = 7
 * ================================================================ */
TEST(test_L1_11_selection_sort) {
    ArcServiceResult* r = run_fixture_L1("L1_11_selection_sort.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 7);
    arc_service_free(r);
}

/* ================================================================
 * L1.12: Max subarray (Kadane's) — max sum = 6
 * ================================================================ */
TEST(test_L1_12_max_subarray) {
    ArcServiceResult* r = run_fixture_L1("L1_12_max_subarray.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 6);
    arc_service_free(r);
}

/* ================================================================
 * L1.13: Insertion sort — sort [9,5,1,4,3], return arr[2] = 4
 * ================================================================ */
TEST(test_L1_13_insertion_sort) {
    ArcServiceResult* r = run_fixture_L1("L1_13_insertion_sort.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 4);
    arc_service_free(r);
}

/* ================================================================
 * L1.14: Two Sum — find pair summing to 9, return i+j = 1
 * ================================================================ */
TEST(test_L1_14_two_sum) {
    ArcServiceResult* r = run_fixture_L1("L1_14_two_sum.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 1);
    arc_service_free(r);
}

/* ================================================================
 * L1.15: Record/struct — Point{x=3,y=4}, distance_sq = 25
 * ================================================================ */
TEST(test_L1_15_record_point) {
    ArcServiceResult* r = run_fixture_L1("L1_15_record_point.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 25);
    arc_service_free(r);
}

/* ================================================================
 * L1.16: Linked list sum using records — 10+20+30 = 60
 * ================================================================ */
TEST(test_L1_16_linked_list) {
    ArcServiceResult* r = run_fixture_L1("L1_16_linked_list.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 60);
    arc_service_free(r);
}

/* Tests registered in test_all.c */
