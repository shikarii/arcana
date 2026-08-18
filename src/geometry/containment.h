#ifndef ARCANA_CONTAINMENT_H
#define ARCANA_CONTAINMENT_H

#include "../common/arcana_common.h"
#include "../semantic_graph/semantic_graph.h"
#include "../verifier/architecture.h"

/*
 * Arcana Geometric Containment — Experiment 2A
 *
 * Derives region membership from circle geometry:
 *   inside(x, R)  ->  region membership, scope, effect ownership,
 *                      capability context, dependency ownership, visibility
 *
 * The circle is the sole source of truth for membership.
 * No duplicate "in R" declarations allowed.
 *
 * Boundary ambiguity: nodes within epsilon of a circle boundary
 * are rejected as geometrically ambiguous.
 */

/* --- 2D position --- */
typedef struct {
    double x;
    double y;
} ArcPos;

/* --- Geometric circle (defines a region boundary) --- */
typedef struct {
    ArcRegionId region;    /* which semantic region this circle defines */
    ArcPos      center;
    double      radius;
} ArcCircle;

/* --- Node position (geometric placement of a node) --- */
typedef struct {
    ArcNodeId id;
    ArcPos    pos;
} ArcNodePos;

/* --- Containment relation (discrete, derived from geometry) --- */
typedef enum {
    ARC_CONTAIN_INSIDE,
    ARC_CONTAIN_OUTSIDE,
    ARC_CONTAIN_AMBIGUOUS,   /* within epsilon of boundary */
} ArcContainment;

/* --- Geometric layout (overlay on ArcGraph) --- */
#define ARC_GEO_MAX_CIRCLES  32
#define ARC_GEO_MAX_NODES    256
#define ARC_GEO_EPSILON      2.0   /* boundary ambiguity zone */

typedef struct {
    ArcCircle   circles[ARC_GEO_MAX_CIRCLES];
    uint32_t    circle_count;

    ArcNodePos  positions[ARC_GEO_MAX_NODES];
    uint32_t    position_count;

    double      epsilon;   /* boundary rejection threshold */
} ArcGeoLayout;

/* --- Derived membership result --- */
typedef struct {
    ArcNodeId   node;
    ArcRegionId region;      /* innermost containing circle's region */
} ArcDerivedMember;

typedef struct {
    char message[256];
    ArcNodeId node;
} ArcGeoError;

#define ARC_GEO_MAX_ERRORS 32

typedef struct {
    ArcDerivedMember members[ARC_GEO_MAX_NODES];
    uint32_t         member_count;

    ArcGeoError      errors[ARC_GEO_MAX_ERRORS];
    uint32_t         error_count;
    bool             valid;

    /* Semantic fan-out: declared (potential) */
    uint32_t         fanout_membership;    /* always = member_count */
    uint32_t         fanout_scope;         /* declared: scope = region */
    uint32_t         fanout_effect_owner;  /* declared: effects attributed to region */
    uint32_t         fanout_capability;    /* declared: capability context = sealed ancestor */
    uint32_t         fanout_dep_owner;     /* declared: edge boundary checks use region */
    uint32_t         fanout_visibility;    /* declared: visibility boundary = region */

    /* Semantic fan-out: consumed (empirically observed) */
    uint32_t         consumed_membership;   /* always = member_count */
    uint32_t         consumed_scope;        /* consumed by scope resolver */
    uint32_t         consumed_effect_owner; /* consumed by effect collection */
    uint32_t         consumed_capability;   /* consumed by capability check */
    uint32_t         consumed_dep_owner;    /* consumed by boundary verification */
    uint32_t         consumed_visibility;   /* consumed by visibility resolver */
} ArcGeoResult;

/* === API === */

void arc_geo_init(ArcGeoLayout* layout);
void arc_geo_add_circle(ArcGeoLayout* layout, ArcRegionId region,
                        double cx, double cy, double radius);
void arc_geo_add_position(ArcGeoLayout* layout, ArcNodeId node,
                          double x, double y);

/* Point-in-circle test with boundary rejection */
ArcContainment arc_geo_containment(const ArcCircle* c, ArcPos p, double epsilon);

/* Derive all region memberships from geometry.
 * Populates graph->nodes[i].region from containment.
 * Returns result with derived members and any errors. */
ArcGeoResult arc_geo_derive_regions(ArcGraph* g, const ArcGeoLayout* layout);

/* Verify that geometry and semantic graph agree (Case F).
 * Returns false if any node's semantic region disagrees with geometry. */
bool arc_geo_verify_consistency(const ArcGraph* g, const ArcGeoResult* result);

/* Check that deformation preserves semantics (Case D).
 * Re-derives membership from modified layout and compares. */
bool arc_geo_deformation_stable(ArcGraph* g,
                                const ArcGeoLayout* before,
                                const ArcGeoLayout* after);

/* Measure which fan-out categories were actually consumed by downstream
 * analysis. Call AFTER arc_arch_verify(). Populates the consumed_* fields
 * in the result. arch may be NULL if no architecture spec exists. */
void arc_geo_measure_consumed_fanout(ArcGeoResult* result,
                                     const ArcGraph* g,
                                     const ArcArchSpec* arch);

#endif /* ARCANA_CONTAINMENT_H */
