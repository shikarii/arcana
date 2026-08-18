/*
 * Architecture verifier tests — Experiment 1.
 *
 * Tests Visible Dependency Completeness:
 *   - Effect completeness: Effects(R) <= Capabilities(R)
 *   - Boundary completeness: cross-region edges need declared channels
 *
 * 8 fixture-based tests: 4 PASS, 4 FAIL.
 * 3 unit tests: effect lookup, effect parse, spec API.
 */
#include "test_harness.h"
#include "../src/verifier/architecture.h"
#include <string.h>

/* --- PASS: valid sealed architecture with declared capabilities --- */
TEST(test_L3_01_arch_valid) {
    char path[512]; fixture_path(path, sizeof(path), "L3_01_arch_valid.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);
    ArcArchResult ar = arc_arch_verify(&fr.graph, &fr.arch);
    ASSERT(ar.valid);
    ASSERT(ar.error_count == 0);
    arc_graph_free(&fr.graph);
}

/* --- FAIL: hidden effect (clock without Clock capability) --- */
TEST(test_L3_02_arch_hidden_effect) {
    char path[512]; fixture_path(path, sizeof(path), "L3_02_arch_hidden_effect.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);
    ArcArchResult ar = arc_arch_verify(&fr.graph, &fr.arch);
    ASSERT(!ar.valid);
    ASSERT(ar.error_count > 0);
    ASSERT(strstr(ar.errors[0].message, "Clock") != NULL);
    arc_graph_free(&fr.graph);
}

/* --- FAIL: hidden cross-boundary edge (no channel declared) --- */
TEST(test_L3_03_arch_hidden_edge) {
    char path[512]; fixture_path(path, sizeof(path), "L3_03_arch_hidden_edge.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);
    ArcArchResult ar = arc_arch_verify(&fr.graph, &fr.arch);
    ASSERT(!ar.valid);
    ASSERT(ar.error_count > 0);
    ASSERT(strstr(ar.errors[0].message, "undeclared dependency") != NULL);
    arc_graph_free(&fr.graph);
}

/* --- FAIL: transitive hidden effect (func_call -> print) --- */
TEST(test_L3_04_arch_transitive_effect) {
    char path[512]; fixture_path(path, sizeof(path), "L3_04_arch_transitive_effect.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);
    ArcArchResult ar = arc_arch_verify(&fr.graph, &fr.arch);
    ASSERT(!ar.valid);
    ASSERT(ar.error_count > 0);
    ASSERT(strstr(ar.errors[0].message, "Logger") != NULL);
    arc_graph_free(&fr.graph);
}

/* --- PASS: declared channel allows cross-boundary edge --- */
TEST(test_L3_05_arch_declared_channel) {
    char path[512]; fixture_path(path, sizeof(path), "L3_05_arch_declared_channel.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);
    ArcArchResult ar = arc_arch_verify(&fr.graph, &fr.arch);
    ASSERT(ar.valid);
    ASSERT(ar.error_count == 0);
    arc_graph_free(&fr.graph);
}

/* --- PASS: pure internal computation, no effects needed --- */
TEST(test_L3_06_arch_pure_internal) {
    char path[512]; fixture_path(path, sizeof(path), "L3_06_arch_pure_internal.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);
    ArcArchResult ar = arc_arch_verify(&fr.graph, &fr.arch);
    ASSERT(ar.valid);
    ASSERT(ar.error_count == 0);
    arc_graph_free(&fr.graph);
}

/* --- PASS: multiple effects all declared --- */
TEST(test_L3_07_arch_multi_effect) {
    char path[512]; fixture_path(path, sizeof(path), "L3_07_arch_multi_effect.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);
    ArcArchResult ar = arc_arch_verify(&fr.graph, &fr.arch);
    ASSERT(ar.valid);
    ASSERT(ar.error_count == 0);
    arc_graph_free(&fr.graph);
}

/* --- FAIL: partial capability (Clock ok, Logger missing) --- */
TEST(test_L3_08_arch_partial_capability) {
    char path[512]; fixture_path(path, sizeof(path), "L3_08_arch_partial_capability.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);
    ArcArchResult ar = arc_arch_verify(&fr.graph, &fr.arch);
    ASSERT(!ar.valid);
    ASSERT(ar.error_count > 0);
    ASSERT(strstr(ar.errors[0].message, "Logger") != NULL);
    arc_graph_free(&fr.graph);
}

/* --- Unit test: effect lookup --- */
TEST(test_arch_effect_lookup) {
    ASSERT(arc_node_effect(ARC_NODE_ADD) == ARC_EFFECT_PURE);
    ASSERT(arc_node_effect(ARC_NODE_PRINT) == ARC_EFFECT_LOGGER);
    ASSERT(arc_intrinsic_effect(ARC_INTRINSIC_CLOCK) == ARC_EFFECT_CLOCK);
    ASSERT(arc_intrinsic_effect(ARC_INTRINSIC_PRINT) == ARC_EFFECT_LOGGER);
    ASSERT(arc_intrinsic_effect(ARC_INTRINSIC_INPUT) == ARC_EFFECT_INPUT);
    ASSERT(arc_intrinsic_effect(ARC_INTRINSIC_ASSERT) == ARC_EFFECT_PURE);
}

/* --- Unit test: effect parse --- */
TEST(test_arch_effect_parse) {
    ASSERT(arc_effect_parse("Clock") == ARC_EFFECT_CLOCK);
    ASSERT(arc_effect_parse("Logger") == ARC_EFFECT_LOGGER);
    ASSERT(arc_effect_parse("Network") == ARC_EFFECT_NETWORK);
    ASSERT(arc_effect_parse("Filesystem") == ARC_EFFECT_FILESYSTEM);
    ASSERT(arc_effect_parse("Hardware") == ARC_EFFECT_HARDWARE);
    ASSERT(arc_effect_parse("Input") == ARC_EFFECT_INPUT);
    ASSERT(arc_effect_parse("Bogus") == 0);
}

/* --- Unit test: programmatic spec building --- */
TEST(test_arch_spec_api) {
    ArcArchSpec spec;
    arc_arch_spec_init(&spec);
    arc_arch_spec_add_sealed(&spec, 1, ARC_EFFECT_CLOCK | ARC_EFFECT_LOGGER);
    arc_arch_spec_add_channel(&spec, 1, 2);
    ASSERT(spec.region_count == 1);
    ASSERT(spec.regions[0].sealed);
    ASSERT(spec.regions[0].capabilities == (ARC_EFFECT_CLOCK | ARC_EFFECT_LOGGER));
    ASSERT(spec.channel_count == 1);
    ASSERT(spec.channels[0].from_region == 1);
    ASSERT(spec.channels[0].to_region == 2);
}
