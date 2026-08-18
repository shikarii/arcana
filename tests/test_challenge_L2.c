/*
 * Arcana Challenge Corpus -- Level 2: Concurrency
 *
 * 11 fixture-based tests proving the concurrency model through
 * the full pipeline: fixture parse -> compile -> verify -> VM run.
 * Building blocks: thread return/args, parallel join, channels, mutex.
 * Classic problems: producer-consumer, dining philosophers, map-reduce.
 */
#include "test_harness.h"
#include "../src/service/arcana_service.h"

/* Shared helper: run fixture via service API */
static ArcServiceResult* run_fixture_L2(const char* filename) {
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

/* L2.01: Thread return — spawn worker, join gets 42 */
TEST(test_L2_01_thread_return) {
    ArcServiceResult* r = run_fixture_L2("L2_01_thread_return.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 42);
    arc_service_free(r);
}

/* L2.02: Thread args — thread receives (17,25), returns 42 */
TEST(test_L2_02_thread_args) {
    ArcServiceResult* r = run_fixture_L2("L2_02_thread_args.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 42);
    arc_service_free(r);
}

/* L2.03: Parallel join — two threads, sum results = 42 */
TEST(test_L2_03_parallel_join) {
    ArcServiceResult* r = run_fixture_L2("L2_03_parallel_join.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 42);
    arc_service_free(r);
}

/* L2.04: Channel ping — thread sends 42 via channel */
TEST(test_L2_04_channel_ping) {
    ArcServiceResult* r = run_fixture_L2("L2_04_channel_ping.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 42);
    arc_service_free(r);
}

/* L2.05: Channel sum — thread sends 10+20+30 via channel = 60 */
TEST(test_L2_05_channel_sum) {
    ArcServiceResult* r = run_fixture_L2("L2_05_channel_sum.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 60);
    arc_service_free(r);
}

/* L2.06: Channel pipeline — 10 * 2 + 3 = 23 through two stages */
TEST(test_L2_06_channel_pipeline) {
    ArcServiceResult* r = run_fixture_L2("L2_06_channel_pipeline.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 23);
    arc_service_free(r);
}

/* L2.07: Mutex handoff — mutex-guarded channel write = 99 */
TEST(test_L2_07_mutex_handoff) {
    ArcServiceResult* r = run_fixture_L2("L2_07_mutex_handoff.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 99);
    arc_service_free(r);
}

/* L2.08: Thread factorial — fact(10) offloaded to thread = 3628800 */
TEST(test_L2_08_thread_factorial) {
    ArcServiceResult* r = run_fixture_L2("L2_08_thread_factorial.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 3628800);
    arc_service_free(r);
}

/* L2.09: Producer-Consumer — bounded buffer, sum 1..5 = 15 */
TEST(test_L2_09_producer_consumer) {
    ArcServiceResult* r = run_fixture_L2("L2_09_producer_consumer.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 15);
    arc_service_free(r);
}

/* L2.10: Dining Philosophers — 3 philosophers, ordered fork pickup = 3 */
TEST(test_L2_10_dining_philosophers) {
    ArcServiceResult* r = run_fixture_L2("L2_10_dining_philosophers.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 3);
    arc_service_free(r);
}

/* L2.11: Map-Reduce — 3 workers sum ranges, total = 55 */
TEST(test_L2_11_map_reduce) {
    ArcServiceResult* r = run_fixture_L2("L2_11_map_reduce.graph");
    ASSERT(r != NULL);
    ASSERT(arc_service_ok(r));
    ArcValue v = arc_service_value(r);
    ASSERT(v.tag == VAL_I64);
    ASSERT_EQ_I64(v.as.i64, 55);
    arc_service_free(r);
}

/* Tests registered in test_all.c */
