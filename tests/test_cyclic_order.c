#include "../src/geometry/cyclic_order.h"
#include "../src/semantic_graph/semantic_graph.h"
#include <math.h>
#include <stdio.h>

extern int tests_run, tests_passed, tests_failed;
#define ASSERT(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); tests_failed++; return; } \
} while(0)
#define PASS() do { tests_passed++; } while(0)

/*
 * Experiment 2B Test Suite — Cyclic Order
 *
 * Phase 2B.0: Manual ρ (scaffolding — proves plumbing, not geometry).
 * Phase 2B.1: Geometry-derived ρ (the real experiment).
 *
 * Evidence hierarchy (from reporting lock):
 *   E0: plumbing works (manual ρ → queries)
 *   E2: formal G3 (same graph + different ρ → different semantics)
 *   E3: geometry pipeline (atan2 → ρ → queries)
 *   E4: naturalness (realistic fixture feels non-contrived)
 */

/* ================================================================
 * Helper: build a 4-member region with edges (sensor ring pattern)
 * Nodes: S0, S1, S2, S3 — all FUNC_CALL in region r_ring
 * Each Si has output port "out", feeding a fusion node
 * ================================================================ */
static void build_sensor_graph(ArcGraph* g, ArcNodeId nodes[4],
                                ArcRegionId* r_ring) {
    arc_graph_init(g);
    ArcRegionId r0 = arc_graph_add_region(g, ARC_REGION_MODULE, ARC_INVALID_ID);
    *r_ring = arc_graph_add_region(g, ARC_REGION_BLOCK, r0);
    g->root_region = r0;

    for (int i = 0; i < 4; i++) {
        nodes[i] = arc_graph_add_node(g, ARC_NODE_FUNC_CALL, *r_ring, (ArcElementId)(i+1));
        ArcPortId out = arc_graph_add_port(g, nodes[i], ARC_PORT_OUTPUT, "out");
        (void)out;
    }

    /* Fusion node in parent region (add_node auto-adds to region) */
    ArcNodeId fusion = arc_graph_add_node(g, ARC_NODE_FUNC_CALL, r0, 10);
    for (int i = 0; i < 4; i++) {
        char role[8];
        snprintf(role, sizeof(role), "arg%d", i);
        ArcPortId inp = arc_graph_add_port(g, fusion, ARC_PORT_INPUT, role);
        /* Edge from Si.out → fusion.argI */
        ArcPortId si_out = g->nodes[nodes[i]].ports[0];
        arc_graph_add_edge(g, si_out, inp);
    }
}

/* ================================================================
 * Phase 2B.0: Manual ρ — scaffolding tests (E0 evidence)
 * ================================================================ */

void test_cyclic_next_prev(void) {
    ArcCyclicOrder order;
    ArcNodeId members[] = {0, 1, 2, 3};
    arc_cyclic_set(&order, 0, members, 4);

    ASSERT(arc_cyclic_next(&order, 0) == 1, "next(0) = 1");
    ASSERT(arc_cyclic_next(&order, 1) == 2, "next(1) = 2");
    ASSERT(arc_cyclic_next(&order, 3) == 0, "next(3) wraps to 0");
    ASSERT(arc_cyclic_prev(&order, 0) == 3, "prev(0) wraps to 3");
    ASSERT(arc_cyclic_prev(&order, 2) == 1, "prev(2) = 1");
    ASSERT(arc_cyclic_next(&order, 99) == ARC_INVALID_ID, "next(missing) = INVALID");
    PASS();
}

void test_cyclic_adjacent(void) {
    ArcCyclicOrder order;
    ArcNodeId members[] = {0, 1, 2, 3};
    arc_cyclic_set(&order, 0, members, 4);

    ASSERT(arc_cyclic_adjacent(&order, 0, 1), "0,1 adjacent");
    ASSERT(arc_cyclic_adjacent(&order, 3, 0), "3,0 adjacent (wrap)");
    ASSERT(!arc_cyclic_adjacent(&order, 0, 2), "0,2 not adjacent");
    ASSERT(!arc_cyclic_adjacent(&order, 1, 3), "1,3 not adjacent");
    PASS();
}

/* ================================================================
 * Phase 2B.0: Minimal pair — same graph, different ρ (E2 evidence)
 *
 * Graph A: ρ_A = (S0, S1, S2, S3) — next(S0)=S1, next(S1)=S2
 * Graph B: ρ_B = (S0, S2, S1, S3) — next(S0)=S2, next(S2)=S1
 *
 * Same nodes, same edges, same regions. Different ρ → different
 * next_clockwise → different semantics. This is formal G3.
 *
 * CRITICAL: This is 2B.0 (manual ρ), NOT evidence for the geometry
 * claim. It proves only that "different ordering metadata → different
 * semantics," which is trivially true. See reporting lock §3.
 * ================================================================ */

void test_cyclic_minimal_pair(void) {
    ArcGraph gA, gB;
    ArcNodeId nodesA[4], nodesB[4];
    ArcRegionId rA, rB;

    build_sensor_graph(&gA, nodesA, &rA);
    build_sensor_graph(&gB, nodesB, &rB);

    /* Verify identical abstract topology via fingerprint */
    uint64_t fpA = arc_graph_fingerprint(&gA);
    uint64_t fpB = arc_graph_fingerprint(&gB);
    ASSERT(fpA == fpB, "minimal pair: fingerprints must match (U(A)=U(B))");

    /* Set different cyclic orders */
    ArcCyclicOrder orderA, orderB;
    ArcNodeId rhoA[] = {nodesA[0], nodesA[1], nodesA[2], nodesA[3]};
    ArcNodeId rhoB[] = {nodesB[0], nodesB[2], nodesB[1], nodesB[3]};
    arc_cyclic_set(&orderA, rA, rhoA, 4);
    arc_cyclic_set(&orderB, rB, rhoB, 4);

    /* Different ρ → different next_clockwise */
    ASSERT(arc_cyclic_next(&orderA, nodesA[0]) == nodesA[1],
           "A: next(S0)=S1");
    ASSERT(arc_cyclic_next(&orderB, nodesB[0]) == nodesB[2],
           "B: next(S0)=S2 (not S1)");

    /* Different adjacency: S0-S1 adjacent in A, not in B */
    ASSERT(arc_cyclic_adjacent(&orderA, nodesA[0], nodesA[1]),
           "A: S0,S1 adjacent");
    ASSERT(!arc_cyclic_adjacent(&orderB, nodesB[0], nodesB[1]),
           "B: S0,S1 NOT adjacent (S2 between them)");

    arc_graph_free(&gA);
    arc_graph_free(&gB);
    PASS();
}

/* ================================================================
 * Fingerprint tests — forgetful map infrastructure
 * ================================================================ */

void test_fingerprint_same_topology(void) {
    ArcGraph gA, gB;
    ArcNodeId nodesA[4], nodesB[4];
    ArcRegionId rA, rB;

    build_sensor_graph(&gA, nodesA, &rA);
    build_sensor_graph(&gB, nodesB, &rB);

    uint64_t fpA = arc_graph_fingerprint(&gA);
    uint64_t fpB = arc_graph_fingerprint(&gB);
    ASSERT(fpA == fpB, "same topology → same fingerprint");

    arc_graph_free(&gA);
    arc_graph_free(&gB);
    PASS();
}

void test_fingerprint_different_topology(void) {
    ArcGraph gA, gB;
    ArcNodeId nodesA[4], nodesB[4];
    ArcRegionId rA, rB;

    build_sensor_graph(&gA, nodesA, &rA);
    build_sensor_graph(&gB, nodesB, &rB);

    /* Add an extra edge to B, changing topology */
    ArcPortId extra_out = arc_graph_add_port(&gB, nodesB[0], ARC_PORT_OUTPUT, "extra");
    ArcPortId extra_in = arc_graph_add_port(&gB, nodesB[1], ARC_PORT_INPUT, "extra");
    arc_graph_add_edge(&gB, extra_out, extra_in);

    uint64_t fpA = arc_graph_fingerprint(&gA);
    uint64_t fpB = arc_graph_fingerprint(&gB);
    ASSERT(fpA != fpB, "different topology → different fingerprint");

    arc_graph_free(&gA);
    arc_graph_free(&gB);
    PASS();
}

/* Hidden topology control: catches the case where a "minimal pair"
 * accidentally has different edge sets (section 6 of test harness lock). */
void test_fingerprint_hidden_topology_control(void) {
    ArcGraph gA, gB;
    ArcNodeId nodesA[4], nodesB[4];
    ArcRegionId rA, rB;

    build_sensor_graph(&gA, nodesA, &rA);
    build_sensor_graph(&gB, nodesB, &rB);

    /* Before setting cyclic orders, verify fingerprints match.
     * If they don't match, the "minimal pair" is invalid. */
    uint64_t fpA = arc_graph_fingerprint(&gA);
    uint64_t fpB = arc_graph_fingerprint(&gB);
    ASSERT(fpA == fpB,
        "hidden topology control: graphs must be identical before applying rho");

    arc_graph_free(&gA);
    arc_graph_free(&gB);
    PASS();
}

/* ================================================================
 * Phase 2B.1: Geometry-derived ρ via atan2 (E3 evidence)
 * ================================================================ */

void test_cyclic_geo_derive_basic(void) {
    ArcGraph g;
    ArcNodeId nodes[4];
    ArcRegionId r_ring;
    build_sensor_graph(&g, nodes, &r_ring);

    /* Position 4 sensors around center (100, 100) */
    ArcGeoLayout layout;
    arc_geo_init(&layout);
    arc_geo_add_position(&layout, nodes[0], 150.0, 100.0);  /* east: angle ~0 */
    arc_geo_add_position(&layout, nodes[1], 100.0, 150.0);  /* north: angle ~π/2 */
    arc_geo_add_position(&layout, nodes[2],  50.0, 100.0);  /* west: angle ~π */
    arc_geo_add_position(&layout, nodes[3], 100.0,  50.0);  /* south: angle ~-π/2 */

    ArcCyclicResult result = arc_geo_derive_cyclic_order(
        &g, &layout, r_ring, 100.0, 100.0);
    ASSERT(result.valid, "derivation succeeds");
    ASSERT(result.count == 1, "one cyclic order derived");
    ASSERT(result.orders[0].count == 4, "4 members in order");

    /* atan2 order: S3 (~-π/2), S0 (~0), S1 (~π/2), S2 (~π) */
    const ArcCyclicOrder* order = &result.orders[0];
    ASSERT(order->members[0] == nodes[3], "first by angle: S3 (south)");
    ASSERT(order->members[1] == nodes[0], "second: S0 (east)");
    ASSERT(order->members[2] == nodes[1], "third: S1 (north)");
    ASSERT(order->members[3] == nodes[2], "fourth: S2 (west)");

    /* Query: next(S0) = S1 (north is next clockwise after east in atan2 order) */
    ASSERT(arc_cyclic_next(order, nodes[0]) == nodes[1],
           "next(east) = north");
    ASSERT(arc_cyclic_next(order, nodes[2]) == nodes[3],
           "next(west) wraps to south");

    arc_graph_free(&g);
    PASS();
}

/* G3 test: same graph, different positions → different ρ → different semantics */
void test_cyclic_geo_g3(void) {
    ArcGraph gA, gB;
    ArcNodeId nodesA[4], nodesB[4];
    ArcRegionId rA, rB;
    build_sensor_graph(&gA, nodesA, &rA);
    build_sensor_graph(&gB, nodesB, &rB);

    /* Precondition: identical abstract topology */
    ASSERT(arc_graph_fingerprint(&gA) == arc_graph_fingerprint(&gB),
           "G3 precondition: U(A)=U(B)");

    ArcGeoLayout layoutA, layoutB;
    arc_geo_init(&layoutA);
    arc_geo_init(&layoutB);

    /* Layout A: S0=east, S1=north, S2=west, S3=south */
    arc_geo_add_position(&layoutA, nodesA[0], 150.0, 100.0);
    arc_geo_add_position(&layoutA, nodesA[1], 100.0, 150.0);
    arc_geo_add_position(&layoutA, nodesA[2],  50.0, 100.0);
    arc_geo_add_position(&layoutA, nodesA[3], 100.0,  50.0);

    /* Layout B: SWAP S1 and S2 positions — S2 at north, S1 at west */
    arc_geo_add_position(&layoutB, nodesB[0], 150.0, 100.0);
    arc_geo_add_position(&layoutB, nodesB[2], 100.0, 150.0);  /* S2 at north */
    arc_geo_add_position(&layoutB, nodesB[1],  50.0, 100.0);  /* S1 at west */
    arc_geo_add_position(&layoutB, nodesB[3], 100.0,  50.0);

    ArcCyclicResult resA = arc_geo_derive_cyclic_order(&gA, &layoutA, rA, 100.0, 100.0);
    ArcCyclicResult resB = arc_geo_derive_cyclic_order(&gB, &layoutB, rB, 100.0, 100.0);
    ASSERT(resA.valid && resB.valid, "both derivations succeed");

    /* A: next(S0)=S1.  B: next(S0)=S2.  Different ρ → different semantics. */
    ASSERT(arc_cyclic_next(&resA.orders[0], nodesA[0]) == nodesA[1],
           "A: next(S0)=S1");
    ASSERT(arc_cyclic_next(&resB.orders[0], nodesB[0]) == nodesB[2],
           "B: next(S0)=S2 (geometry swapped S1/S2)");

    /* Adjacency differs */
    ASSERT(arc_cyclic_adjacent(&resA.orders[0], nodesA[0], nodesA[1]),
           "A: S0,S1 adjacent");
    ASSERT(!arc_cyclic_adjacent(&resB.orders[0], nodesB[0], nodesB[1]),
           "B: S0,S1 NOT adjacent");

    arc_graph_free(&gA);
    arc_graph_free(&gB);
    PASS();
}

/* G4 control: same ρ with different positions → same semantics */
void test_cyclic_geo_g4_control(void) {
    ArcGraph gA, gB;
    ArcNodeId nodesA[4], nodesB[4];
    ArcRegionId rA, rB;
    build_sensor_graph(&gA, nodesA, &rA);
    build_sensor_graph(&gB, nodesB, &rB);

    ArcGeoLayout layoutA, layoutB;
    arc_geo_init(&layoutA);
    arc_geo_init(&layoutB);

    /* Layout A: sensors at distance 50 from center */
    arc_geo_add_position(&layoutA, nodesA[0], 150.0, 100.0);
    arc_geo_add_position(&layoutA, nodesA[1], 100.0, 150.0);
    arc_geo_add_position(&layoutA, nodesA[2],  50.0, 100.0);
    arc_geo_add_position(&layoutA, nodesA[3], 100.0,  50.0);

    /* Layout B: sensors at distance 80 from center — different coords, same angles */
    arc_geo_add_position(&layoutB, nodesB[0], 180.0, 100.0);
    arc_geo_add_position(&layoutB, nodesB[1], 100.0, 180.0);
    arc_geo_add_position(&layoutB, nodesB[2],  20.0, 100.0);
    arc_geo_add_position(&layoutB, nodesB[3], 100.0,  20.0);

    ArcCyclicResult resA = arc_geo_derive_cyclic_order(&gA, &layoutA, rA, 100.0, 100.0);
    ArcCyclicResult resB = arc_geo_derive_cyclic_order(&gB, &layoutB, rB, 100.0, 100.0);
    ASSERT(resA.valid && resB.valid, "both derivations succeed");

    /* Same angles → same cyclic order → same next() */
    ASSERT(arc_cyclic_next(&resA.orders[0], nodesA[0]) ==
           arc_cyclic_next(&resB.orders[0], nodesB[0]),
           "G4: same angular positions → same next (S0)");
    ASSERT(arc_cyclic_next(&resA.orders[0], nodesA[2]) ==
           arc_cyclic_next(&resB.orders[0], nodesB[2]),
           "G4: same angular positions → same next (S2)");

    arc_graph_free(&gA);
    arc_graph_free(&gB);
    PASS();
}

/* Angular ambiguity: two sensors at nearly the same angle → rejection */
void test_cyclic_geo_angular_ambiguity(void) {
    ArcGraph g;
    ArcNodeId nodes[4];
    ArcRegionId r_ring;
    build_sensor_graph(&g, nodes, &r_ring);

    ArcGeoLayout layout;
    arc_geo_init(&layout);
    /* S0 and S1 at nearly the same angle (both roughly east) */
    arc_geo_add_position(&layout, nodes[0], 150.0, 100.0);
    arc_geo_add_position(&layout, nodes[1], 150.0, 100.5);  /* tiny y offset */
    arc_geo_add_position(&layout, nodes[2],  50.0, 100.0);
    arc_geo_add_position(&layout, nodes[3], 100.0,  50.0);

    ArcCyclicResult result = arc_geo_derive_cyclic_order(
        &g, &layout, r_ring, 100.0, 100.0);
    ASSERT(!result.valid, "angular ambiguity → rejection");

    arc_graph_free(&g);
    PASS();
}

/* Center proximity: node too close to ring center → rejection */
void test_cyclic_geo_center_proximity(void) {
    ArcGraph g;
    ArcNodeId nodes[4];
    ArcRegionId r_ring;
    build_sensor_graph(&g, nodes, &r_ring);

    ArcGeoLayout layout;
    arc_geo_init(&layout);
    arc_geo_add_position(&layout, nodes[0], 100.5, 100.0);  /* too close to center */
    arc_geo_add_position(&layout, nodes[1], 100.0, 150.0);
    arc_geo_add_position(&layout, nodes[2],  50.0, 100.0);
    arc_geo_add_position(&layout, nodes[3], 100.0,  50.0);

    ArcCyclicResult result = arc_geo_derive_cyclic_order(
        &g, &layout, r_ring, 100.0, 100.0);
    ASSERT(!result.valid, "node at center → undefined angle → rejection");

    arc_graph_free(&g);
    PASS();
}

/* Reflection: mirroring positions reverses successor order */
void test_cyclic_geo_reflection(void) {
    ArcGraph gA, gB;
    ArcNodeId nodesA[4], nodesB[4];
    ArcRegionId rA, rB;
    build_sensor_graph(&gA, nodesA, &rA);
    build_sensor_graph(&gB, nodesB, &rB);

    ArcGeoLayout layoutA, layoutB;
    arc_geo_init(&layoutA);
    arc_geo_init(&layoutB);

    /* A: normal positions */
    arc_geo_add_position(&layoutA, nodesA[0], 150.0, 100.0);
    arc_geo_add_position(&layoutA, nodesA[1], 100.0, 150.0);
    arc_geo_add_position(&layoutA, nodesA[2],  50.0, 100.0);
    arc_geo_add_position(&layoutA, nodesA[3], 100.0,  50.0);

    /* B: reflected across x-axis (negate y offset from center) */
    arc_geo_add_position(&layoutB, nodesB[0], 150.0, 100.0);
    arc_geo_add_position(&layoutB, nodesB[1], 100.0,  50.0);  /* was 150 → 50 */
    arc_geo_add_position(&layoutB, nodesB[2],  50.0, 100.0);
    arc_geo_add_position(&layoutB, nodesB[3], 100.0, 150.0);  /* was 50 → 150 */

    ArcCyclicResult resA = arc_geo_derive_cyclic_order(&gA, &layoutA, rA, 100.0, 100.0);
    ArcCyclicResult resB = arc_geo_derive_cyclic_order(&gB, &layoutB, rB, 100.0, 100.0);
    ASSERT(resA.valid && resB.valid, "both derivations succeed");

    /* In A: atan2 order is S3(-π/2), S0(0), S1(π/2), S2(π)
     *   → next(S0)=S1
     * In B: S1 and S3 are swapped. atan2 order: S1(-π/2), S0(0), S3(π/2), S2(π)
     *   → next(S0)=S3 */
    ASSERT(arc_cyclic_next(&resA.orders[0], nodesA[0]) == nodesA[1],
           "A: next(S0)=S1");
    ASSERT(arc_cyclic_next(&resB.orders[0], nodesB[0]) == nodesB[3],
           "B (reflected): next(S0)=S3 (reversal)");

    arc_graph_free(&gA);
    arc_graph_free(&gB);
    PASS();
}

/* ================================================================
 * Realistic fixture: sensor-ring neighbor consistency check
 *
 * 4 sensors around a platform. Each produces a reading.
 * adjacent_consistency: for each sensor, check if its reading is
 * consistent with its clockwise neighbor's reading.
 * Different ring orders → different neighbor pairs → different results.
 *
 * This is the E4 naturalness evidence target.
 * ================================================================ */
void test_cyclic_sensor_ring_realistic(void) {
    ArcGraph gA, gB;
    ArcNodeId nodesA[4], nodesB[4];
    ArcRegionId rA, rB;
    build_sensor_graph(&gA, nodesA, &rA);
    build_sensor_graph(&gB, nodesB, &rB);

    ASSERT(arc_graph_fingerprint(&gA) == arc_graph_fingerprint(&gB),
           "realistic: U(A)=U(B) precondition");

    ArcGeoLayout layoutA, layoutB;
    arc_geo_init(&layoutA);
    arc_geo_init(&layoutB);

    /* Layout A: clockwise from east: S0, S1, S2, S3 */
    arc_geo_add_position(&layoutA, nodesA[0], 150.0, 100.0);
    arc_geo_add_position(&layoutA, nodesA[1], 100.0, 150.0);
    arc_geo_add_position(&layoutA, nodesA[2],  50.0, 100.0);
    arc_geo_add_position(&layoutA, nodesA[3], 100.0,  50.0);

    /* Layout B: S1 and S2 swapped */
    arc_geo_add_position(&layoutB, nodesB[0], 150.0, 100.0);
    arc_geo_add_position(&layoutB, nodesB[2], 100.0, 150.0);
    arc_geo_add_position(&layoutB, nodesB[1],  50.0, 100.0);
    arc_geo_add_position(&layoutB, nodesB[3], 100.0,  50.0);

    ArcCyclicResult resA = arc_geo_derive_cyclic_order(&gA, &layoutA, rA, 100.0, 100.0);
    ArcCyclicResult resB = arc_geo_derive_cyclic_order(&gB, &layoutB, rB, 100.0, 100.0);
    ASSERT(resA.valid && resB.valid, "both derivations succeed");

    /* Simulate neighbor-consistency check:
     * For each sensor, collect (sensor, next_clockwise(sensor)) pairs.
     * Count how many pairs involve S0 and S1 as neighbors. */
    bool a_s0s1_adj = arc_cyclic_adjacent(&resA.orders[0], nodesA[0], nodesA[1]);
    bool b_s0s1_adj = arc_cyclic_adjacent(&resB.orders[0], nodesB[0], nodesB[1]);

    ASSERT(a_s0s1_adj, "A: S0,S1 are neighbors (adjacent sensors)");
    ASSERT(!b_s0s1_adj, "B: S0,S1 are NOT neighbors (S2 is between them)");

    /* This difference is semantically meaningful: if S0 and S1 produce
     * correlated readings, the consistency check in A would flag it
     * (they're adjacent → expected), while B would not (they're not
     * adjacent → correlation is suspicious). */

    arc_graph_free(&gA);
    arc_graph_free(&gB);
    PASS();
}
