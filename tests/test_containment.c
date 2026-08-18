/*
 * Geometric containment tests — Experiment 2A.
 *
 * Tests that inside(x, R) derives region membership from geometry,
 * and that the existing architecture verifier consumes derived membership.
 *
 * Adversarial cases:
 *   - Boundary ambiguity rejection
 *   - Nested circles (innermost wins)
 *   - Node move across boundary changes membership
 *   - Deformation without crossing preserves semantics
 *   - Geometry/semantic-graph consistency
 *   - Full pipeline: geometry + transitive effect + architectural rejection
 */
#include "test_harness.h"
#include "../src/geometry/containment.h"
#include "../src/verifier/architecture.h"
#include <string.h>
#include <math.h>

/* Helper: build a circle at (cx, cy) with radius r for region rid */
static ArcCircle make_circle(ArcRegionId rid, double cx, double cy, double r) {
    ArcCircle c;
    memset(&c, 0, sizeof(c));
    c.region = rid;
    c.center.x = cx;
    c.center.y = cy;
    c.radius = r;
    return c;
}

static ArcPos make_pos(double x, double y) {
    ArcPos p;
    p.x = x;
    p.y = y;
    return p;
}

/* --- Unit: point-in-circle containment --- */
TEST(test_geo_point_inside) {
    ArcCircle c = make_circle(0, 100, 100, 50);
    ASSERT(arc_geo_containment(&c, make_pos(110, 110), 2.0) == ARC_CONTAIN_INSIDE);
}

TEST(test_geo_point_outside) {
    ArcCircle c = make_circle(0, 100, 100, 50);
    ASSERT(arc_geo_containment(&c, make_pos(200, 200), 2.0) == ARC_CONTAIN_OUTSIDE);
}

TEST(test_geo_point_on_boundary) {
    ArcCircle c = make_circle(0, 100, 100, 50);
    /* Point at distance exactly 50 from center */
    ASSERT(arc_geo_containment(&c, make_pos(150, 100), 2.0) == ARC_CONTAIN_AMBIGUOUS);
}

TEST(test_geo_point_near_boundary) {
    ArcCircle c = make_circle(0, 100, 100, 50);
    /* Point at distance 49.5 from center — within epsilon=2 of boundary */
    ASSERT(arc_geo_containment(&c, make_pos(149.5, 100), 2.0) == ARC_CONTAIN_AMBIGUOUS);
}

/* --- Unit: derive regions from geometry --- */
TEST(test_geo_derive_basic) {
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    ArcRegionId r1 = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    g.root_region = r0;

    ArcNodeId n = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);
    ASSERT(g.nodes[n].region == r0);

    ArcGeoLayout layout;
    arc_geo_init(&layout);
    arc_geo_add_circle(&layout, r1, 100, 100, 50);
    arc_geo_add_position(&layout, n, 110, 110);

    ArcGeoResult result = arc_geo_derive_regions(&g, &layout);
    ASSERT(result.valid);
    ASSERT(result.member_count == 1);
    ASSERT(result.members[0].node == n);
    ASSERT(result.members[0].region == r1);
    ASSERT(g.nodes[n].region == r1);

    /* Declared fan-out: all 6 potential consequences */
    ASSERT(result.fanout_membership == 1);
    ASSERT(result.fanout_scope == 1);
    ASSERT(result.fanout_effect_owner == 1);
    ASSERT(result.fanout_capability == 1);
    ASSERT(result.fanout_dep_owner == 1);
    ASSERT(result.fanout_visibility == 1);

    /* Consumed fan-out: no arch spec -> only membership consumed */
    arc_geo_measure_consumed_fanout(&result, &g, NULL);
    ASSERT(result.consumed_membership == 1);
    ASSERT(result.consumed_scope == 0);         /* no resolver */
    ASSERT(result.consumed_effect_owner == 0);  /* no sealed region */
    ASSERT(result.consumed_capability == 0);    /* no sealed region */
    ASSERT(result.consumed_dep_owner == 0);     /* no cross-boundary edges */
    ASSERT(result.consumed_visibility == 0);    /* no resolver */

    arc_graph_free(&g);
}

/* --- Fixture: basic containment from file --- */
TEST(test_L4_01_geo_basic_containment) {
    char path[512]; fixture_path(path, sizeof(path), "L4_01_geo_basic_containment.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);
    ASSERT(fr.has_geo);

    ArcGeoResult gr = arc_geo_derive_regions(&fr.graph, &fr.geo);
    ASSERT(gr.valid);
    ASSERT(gr.member_count == 3);

    for (uint32_t i = 0; i < gr.member_count; i++) {
        ASSERT(gr.members[i].region == 1); /* r_plan is region id 1 */
    }

    ASSERT(arc_geo_verify_consistency(&fr.graph, &gr));
    arc_graph_free(&fr.graph);
}

/* --- Fixture: nested circles, innermost wins --- */
TEST(test_L4_02_geo_nested_circles) {
    char path[512]; fixture_path(path, sizeof(path), "L4_02_geo_nested_circles.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);
    ASSERT(fr.has_geo);

    ArcGeoResult gr = arc_geo_derive_regions(&fr.graph, &fr.geo);
    ASSERT(gr.valid);

    ArcRegionId r_percep = 2; /* r0=0, r_root=1, r_percep=2 */
    for (uint32_t i = 0; i < gr.member_count; i++) {
        ASSERT(gr.members[i].region == r_percep);
    }
    arc_graph_free(&fr.graph);
}

/* --- Fixture: boundary ambiguity rejection --- */
TEST(test_L4_03_geo_boundary_ambiguous) {
    char path[512]; fixture_path(path, sizeof(path), "L4_03_geo_boundary_ambiguous.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);
    ASSERT(fr.success);
    ASSERT(fr.has_geo);

    ArcGeoResult gr = arc_geo_derive_regions(&fr.graph, &fr.geo);
    ASSERT(!gr.valid);
    ASSERT(gr.error_count > 0);
    ASSERT(strstr(gr.errors[0].message, "ambiguous") != NULL);
    arc_graph_free(&fr.graph);
}

/* --- Fixture: pipeline with hidden effect, geometry-derived membership --- */
TEST(test_L4_04_geo_pipeline_hidden_effect) {
    char path[512]; fixture_path(path, sizeof(path), "L4_04_geo_pipeline_hidden_effect.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);

    ASSERT(fr.success);
    ASSERT(fr.has_geo);
    ASSERT(fr.has_arch);

    /* Step 1: derive regions from geometry */
    ArcGeoResult gr = arc_geo_derive_regions(&fr.graph, &fr.geo);
    ASSERT(gr.valid);

    /* Step 2: feed derived membership into architecture verifier */
    ArcArchResult ar = arc_arch_verify(&fr.graph, &fr.arch);

    /* Planner calls sub_helper -> print (Logger) but has no Logger capability */
    ASSERT(!ar.valid);
    ASSERT(ar.error_count > 0);
    ASSERT(strstr(ar.errors[0].message, "Logger") != NULL);

    /* Declared fan-out: 5 positioned nodes × 6 categories */
    ASSERT(gr.fanout_membership == 5);
    ASSERT(gr.fanout_scope == 5);
    ASSERT(gr.fanout_effect_owner == 5);

    /* Consumed fan-out: real downstream consumption */
    arc_geo_measure_consumed_fanout(&gr, &fr.graph, &fr.arch);
    ASSERT(gr.consumed_membership == 5);         /* all 5 derived */
    ASSERT(gr.consumed_effect_owner >= 2);       /* helper+route in sealed r_plan */
    ASSERT(gr.consumed_capability >= 2);         /* sealed ancestor for r_plan nodes */
    ASSERT(gr.consumed_dep_owner >= 1);          /* cross-boundary edges exist */
    ASSERT(gr.consumed_scope == 0);              /* no scope resolver */
    ASSERT(gr.consumed_visibility == 0);         /* no visibility resolver */

    arc_graph_free(&fr.graph);
}

/* --- Fixture: pipeline with capability declared — should pass --- */
TEST(test_L4_05_geo_pipeline_with_capability) {
    char path[512]; fixture_path(path, sizeof(path), "L4_05_geo_pipeline_with_capability.graph");
    ArcFixtureResult fr = arc_fixture_parse_file(path);

    ASSERT(fr.success);

    ArcGeoResult gr = arc_geo_derive_regions(&fr.graph, &fr.geo);
    ASSERT(gr.valid);

    ArcArchResult ar = arc_arch_verify(&fr.graph, &fr.arch);
    ASSERT(ar.valid);
    ASSERT(ar.error_count == 0);
    arc_graph_free(&fr.graph);
}

/* --- Case A: move effectful node across boundary --- */
TEST(test_geo_move_across_boundary) {
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    ArcRegionId r_a = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    ArcRegionId r_b = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    g.root_region = r0;

    ArcNodeId n = arc_graph_add_node(&g, ARC_NODE_PRINT, r0, 0);

    ArcGeoLayout layout;
    arc_geo_init(&layout);
    arc_geo_add_circle(&layout, r_a, 100, 100, 50);
    arc_geo_add_circle(&layout, r_b, 300, 100, 50);

    arc_geo_add_position(&layout, n, 110, 100);
    ArcGeoResult res1 = arc_geo_derive_regions(&g, &layout);
    ASSERT(res1.valid);
    ASSERT(g.nodes[n].region == r_a);

    layout.positions[0].pos.x = 310;
    ArcGeoResult res2 = arc_geo_derive_regions(&g, &layout);
    ASSERT(res2.valid);
    ASSERT(g.nodes[n].region == r_b);

    arc_graph_free(&g);
}

/* --- Case D: deformation without crossing preserves semantics --- */
TEST(test_geo_deformation_stable) {
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    ArcRegionId r1 = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    g.root_region = r0;

    ArcNodeId n = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);

    ArcGeoLayout before_layout;
    arc_geo_init(&before_layout);
    arc_geo_add_circle(&before_layout, r1, 100, 100, 50);
    arc_geo_add_position(&before_layout, n, 110, 110);

    ArcGeoLayout after_layout;
    arc_geo_init(&after_layout);
    arc_geo_add_circle(&after_layout, r1, 120, 130, 60);
    arc_geo_add_position(&after_layout, n, 110, 110);

    ASSERT(arc_geo_deformation_stable(&g, &before_layout, &after_layout));
    arc_graph_free(&g);
}

/* --- Case E: move unrelated node within same region — no semantic change --- */
TEST(test_geo_move_within_region) {
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    ArcRegionId r1 = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    g.root_region = r0;

    ArcNodeId n = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);

    ArcGeoLayout layout;
    arc_geo_init(&layout);
    arc_geo_add_circle(&layout, r1, 100, 100, 50);
    arc_geo_add_position(&layout, n, 110, 110);

    ArcGeoResult res_a = arc_geo_derive_regions(&g, &layout);
    ASSERT(res_a.valid);
    ArcRegionId region_before = g.nodes[n].region;

    layout.positions[0].pos.x = 80;
    layout.positions[0].pos.y = 90;
    ArcGeoResult res_b = arc_geo_derive_regions(&g, &layout);
    ASSERT(res_b.valid);
    ASSERT(g.nodes[n].region == region_before);

    arc_graph_free(&g);
}

/* --- Case F: consistency check fails on disagreement --- */
TEST(test_geo_consistency_disagreement) {
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    ArcRegionId r1 = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    g.root_region = r0;

    ArcNodeId n = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);

    ArcGeoLayout layout;
    arc_geo_init(&layout);
    arc_geo_add_circle(&layout, r1, 100, 100, 50);
    arc_geo_add_position(&layout, n, 110, 110);

    ArcGeoResult result = arc_geo_derive_regions(&g, &layout);
    ASSERT(result.valid);

    g.nodes[n].region = r0; /* break consistency */
    ASSERT(!arc_geo_verify_consistency(&g, &result));
    arc_graph_free(&g);
}

/* --- Semantic fan-out measurement --- */
TEST(test_geo_fanout_measurement) {
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    ArcRegionId r1 = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    g.root_region = r0;

    int i;
    for (i = 0; i < 5; i++)
        arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);

    ArcGeoLayout layout;
    arc_geo_init(&layout);
    arc_geo_add_circle(&layout, r1, 100, 100, 80);

    for (i = 0; i < 5; i++)
        arc_geo_add_position(&layout, (ArcNodeId)i, 80.0 + (double)i * 10, 100);

    ArcGeoResult result = arc_geo_derive_regions(&g, &layout);
    ASSERT(result.valid);
    ASSERT(result.member_count == 5);

    /* Declared fan-out: 6 potential categories × 5 nodes */
    ASSERT(result.fanout_membership == 5);
    ASSERT(result.fanout_scope == 5);
    ASSERT(result.fanout_effect_owner == 5);
    ASSERT(result.fanout_capability == 5);
    ASSERT(result.fanout_dep_owner == 5);
    ASSERT(result.fanout_visibility == 5);

    /* Consumed fan-out: no arch spec, no edges -> only membership */
    arc_geo_measure_consumed_fanout(&result, &g, NULL);
    ASSERT(result.consumed_membership == 5);
    ASSERT(result.consumed_effect_owner == 0);  /* no sealed regions */
    ASSERT(result.consumed_capability == 0);
    ASSERT(result.consumed_dep_owner == 0);     /* no edges */
    ASSERT(result.consumed_scope == 0);
    ASSERT(result.consumed_visibility == 0);
    /* Declared leverage = 6×, consumed leverage = 1× (membership only) */
    /* Real leverage requires downstream consumers (architecture verifier) */

    arc_graph_free(&g);
}

/* --- Consumed fan-out with sealed region + cross-boundary edge --- */
TEST(test_geo_consumed_fanout_with_verifier) {
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    ArcRegionId r1 = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    g.root_region = r0;

    /* Print node (Logger effect) — will be derived into r1 */
    ArcNodeId n_print = arc_graph_add_node(&g, ARC_NODE_PRINT, r0, 0);
    /* Const node in r0 — feeds into print across boundary */
    ArcNodeId n_const = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);
    /* Edge: const -> print (will cross boundary after derivation) */
    ArcPortId p_out = arc_graph_add_port(&g, n_const, ARC_PORT_OUTPUT, "out");
    ArcPortId p_in = arc_graph_add_port(&g, n_print, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_out, p_in);

    /* Seal r1 with no capabilities */
    ArcArchSpec arch;
    arc_arch_spec_init(&arch);
    arc_arch_spec_add_sealed(&arch, r1, ARC_EFFECT_PURE);

    /* Geometry: only print node positioned inside r1 */
    ArcGeoLayout layout;
    arc_geo_init(&layout);
    arc_geo_add_circle(&layout, r1, 100, 100, 50);
    arc_geo_add_position(&layout, n_print, 110, 110);

    ArcGeoResult result = arc_geo_derive_regions(&g, &layout);
    ASSERT(result.valid);
    ASSERT(result.member_count == 1);
    ASSERT(g.nodes[n_print].region == r1);

    /* Verify: should fail (Logger undeclared in sealed r1) */
    ArcArchResult ar = arc_arch_verify(&g, &arch);
    ASSERT(!ar.valid);

    /* Measure consumed fan-out */
    arc_geo_measure_consumed_fanout(&result, &g, &arch);
    ASSERT(result.consumed_membership == 1);     /* always */
    ASSERT(result.consumed_effect_owner == 1);   /* sealed r1 walked */
    ASSERT(result.consumed_capability == 1);     /* sealed ancestor */
    ASSERT(result.consumed_dep_owner == 1);      /* edge crosses r0/r1 */
    ASSERT(result.consumed_scope == 0);          /* no resolver */
    ASSERT(result.consumed_visibility == 0);     /* no resolver */
    /* Consumed leverage = 4× (out of 6 declared) */

    arc_graph_free(&g);
}

/* --- Design invariant: crossing != authorization --- */
/* A geometric boundary crossing must NOT automatically authorize
 * communication between sealed regions. The architecture verifier
 * must still reject undeclared cross-boundary dependencies. */
TEST(test_geo_crossing_not_authorization) {
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    ArcRegionId r_a = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    ArcRegionId r_b = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    g.root_region = r0;

    /* Two nodes, each in a different sealed region */
    ArcNodeId n_src = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);
    ArcNodeId n_dst = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);

    /* Edge that will cross the boundary after geometric derivation */
    ArcPortId p_out = arc_graph_add_port(&g, n_src, ARC_PORT_OUTPUT, "out");
    ArcPortId p_in = arc_graph_add_port(&g, n_dst, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_out, p_in);

    /* Geometry: each node inside its respective circle */
    ArcGeoLayout layout;
    arc_geo_init(&layout);
    arc_geo_add_circle(&layout, r_a, 100, 100, 50);
    arc_geo_add_circle(&layout, r_b, 300, 100, 50);
    arc_geo_add_position(&layout, n_src, 110, 100);
    arc_geo_add_position(&layout, n_dst, 310, 100);

    ArcGeoResult gr = arc_geo_derive_regions(&g, &layout);
    ASSERT(gr.valid);
    ASSERT(g.nodes[n_src].region == r_a);
    ASSERT(g.nodes[n_dst].region == r_b);

    /* Seal both regions, NO channel declared between them */
    ArcArchSpec arch;
    arc_arch_spec_init(&arch);
    arc_arch_spec_add_sealed(&arch, r_a, ARC_EFFECT_PURE);
    arc_arch_spec_add_sealed(&arch, r_b, ARC_EFFECT_PURE);

    /* The edge visibly crosses the boundary — but crossing is NOT
     * authorization. The verifier must still reject this. */
    ArcArchResult ar = arc_arch_verify(&g, &arch);
    ASSERT(!ar.valid);
    ASSERT(ar.error_count > 0);
    ASSERT(strstr(ar.errors[0].message, "undeclared dependency") != NULL);

    /* Now add a channel — should pass */
    arc_arch_spec_add_channel(&arch, r_a, r_b);
    ArcArchResult ar2 = arc_arch_verify(&g, &arch);
    ASSERT(ar2.valid);

    arc_graph_free(&g);
}
