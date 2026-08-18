#ifndef ARCANA_CYCLIC_ORDER_H
#define ARCANA_CYCLIC_ORDER_H

#include "../common/arcana_common.h"
#include "../semantic_graph/semantic_graph.h"
#include "containment.h"

/*
 * Arcana Cyclic Order — Experiment 2B
 *
 * Primitive R: cyclic ordering of region members derived from 2D geometry.
 *
 * The cyclic order ρ = (m0, m1, ..., mk) determines successor/predecessor
 * relationships among the members of a region. Different orderings yield
 * different semantics (next_clockwise, adjacency) while the underlying
 * abstract graph (node kinds, edge connectivity, regions) remains identical.
 *
 * Phase 2B.0: ρ set manually via arc_cyclic_set() — scaffolding only.
 * Phase 2B.1: ρ derived from atan2(pos - center) — the real experiment.
 *
 * Angular ambiguity: members within ANGLE_EPSILON radians of each other
 * are rejected as angularly indeterminate (analogous to containment epsilon).
 */

#define ARC_CYCLIC_MAX_MEMBERS  64
#define ARC_CYCLIC_MAX_ORDERS   32
#define ARC_CYCLIC_ANGLE_EPSILON 0.05  /* ~2.9 degrees */

/* --- Cyclic order for one region --- */
typedef struct {
    ArcRegionId region;
    ArcNodeId   members[ARC_CYCLIC_MAX_MEMBERS];
    uint32_t    count;
} ArcCyclicOrder;

/* --- Result of cyclic order derivation --- */
typedef struct {
    ArcCyclicOrder orders[ARC_CYCLIC_MAX_ORDERS];
    uint32_t       count;
    bool           valid;
    char           error[256];
} ArcCyclicResult;

/* === Query API === */

/* Next member clockwise after node. Returns ARC_INVALID_ID if not found. */
ArcNodeId arc_cyclic_next(const ArcCyclicOrder* order, ArcNodeId node);

/* Previous member (counter-clockwise) from node. */
ArcNodeId arc_cyclic_prev(const ArcCyclicOrder* order, ArcNodeId node);

/* Are a and b adjacent in the cyclic order? */
bool arc_cyclic_adjacent(const ArcCyclicOrder* order, ArcNodeId a, ArcNodeId b);

/* === Manual cyclic order (Phase 2B.0) === */

void arc_cyclic_set(ArcCyclicOrder* order, ArcRegionId region,
                    const ArcNodeId* members, uint32_t count);

/* === Geometry-derived cyclic order (Phase 2B.1) === */

/* Derive cyclic member order for a region from 2D positions.
 * Members are sorted by atan2(pos.y - cy, pos.x - cx).
 * Rejects if two members are within ANGLE_EPSILON of each other.
 * Rejects if a member is too close to the center (radius < 1.0). */
ArcCyclicResult arc_geo_derive_cyclic_order(
    const ArcGraph* g, const ArcGeoLayout* layout,
    ArcRegionId region, double cx, double cy);

/* === Forgetful fingerprint === */

/* Compute embedding-stripped fingerprint of graph topology.
 * Strips: positions, circles, cyclic orders, coordinates.
 * Preserves: node kinds, edge direction, connectivity, regions, port roles.
 * Two graphs with identical abstract topology have equal fingerprints. */
uint64_t arc_graph_fingerprint(const ArcGraph* g);

#endif /* ARCANA_CYCLIC_ORDER_H */
