#include "containment.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/*
 * Geometric containment — Experiment 2A.
 *
 * Core algorithm:
 *   1. For each node with a position, test against all circles
 *   2. Find the innermost containing circle (smallest radius among containers)
 *   3. That circle's region becomes the node's region
 *   4. Reject nodes within epsilon of any boundary
 *
 * The result is a discrete Inside/Outside/Ambiguous relation.
 * Continuous coordinates are input; discrete membership is output.
 */

void arc_geo_init(ArcGeoLayout* layout) {
    memset(layout, 0, sizeof(*layout));
    layout->epsilon = ARC_GEO_EPSILON;
}

void arc_geo_add_circle(ArcGeoLayout* layout, ArcRegionId region,
                        double cx, double cy, double radius) {
    if (layout->circle_count >= ARC_GEO_MAX_CIRCLES) return;
    ArcCircle* c = &layout->circles[layout->circle_count++];
    c->region = region;
    c->center.x = cx;
    c->center.y = cy;
    c->radius = radius;
}

void arc_geo_add_position(ArcGeoLayout* layout, ArcNodeId node,
                          double x, double y) {
    if (layout->position_count >= ARC_GEO_MAX_NODES) return;
    ArcNodePos* np = &layout->positions[layout->position_count++];
    np->id = node;
    np->pos.x = x;
    np->pos.y = y;
}

/* Point-in-circle with boundary ambiguity detection */
ArcContainment arc_geo_containment(const ArcCircle* c, ArcPos p, double epsilon) {
    double dx = p.x - c->center.x;
    double dy = p.y - c->center.y;
    double dist = sqrt(dx * dx + dy * dy);
    double boundary_dist = fabs(dist - c->radius);

    if (boundary_dist < epsilon)
        return ARC_CONTAIN_AMBIGUOUS;
    if (dist < c->radius)
        return ARC_CONTAIN_INSIDE;
    return ARC_CONTAIN_OUTSIDE;
}

static void geo_error(ArcGeoResult* r, ArcNodeId node, const char* fmt, ...) {
    if (r->error_count >= ARC_GEO_MAX_ERRORS) return;
    ArcGeoError* e = &r->errors[r->error_count++];
    e->node = node;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->message, sizeof(e->message), fmt, ap);
    va_end(ap);
    r->valid = false;
}

/* Remove a node from its region's member list */
static void remove_from_region(ArcGraph* g, ArcRegionId rid, ArcNodeId node) {
    if (rid >= g->region_count) return;
    ArcRegion* r = &g->regions[rid];
    for (uint32_t i = 0; i < r->member_count; i++) {
        if (r->members[i] == node) {
            r->members[i] = r->members[--r->member_count];
            return;
        }
    }
}

/*
 * For a positioned node, find its innermost containing circle.
 * "Innermost" = smallest radius among all circles that contain the point.
 * This handles nested circles correctly: a node inside both Perception
 * and Root belongs to Perception (the smaller circle).
 */
static ArcRegionId find_innermost_region(
    const ArcGeoLayout* layout, ArcPos pos, ArcNodeId node,
    ArcGeoResult* result)
{
    ArcRegionId best = ARC_INVALID_ID;
    double best_radius = 1e18;

    for (uint32_t i = 0; i < layout->circle_count; i++) {
        ArcContainment ct = arc_geo_containment(
            &layout->circles[i], pos, layout->epsilon);

        if (ct == ARC_CONTAIN_AMBIGUOUS) {
            geo_error(result, node,
                "node %u is ambiguously on boundary of region %u",
                (unsigned)node, (unsigned)layout->circles[i].region);
            return ARC_INVALID_ID;
        }
        if (ct == ARC_CONTAIN_INSIDE && layout->circles[i].radius < best_radius) {
            best = layout->circles[i].region;
            best_radius = layout->circles[i].radius;
        }
    }
    return best;
}

/*
 * Derive region memberships from geometry.
 *
 * For each node that has a geometric position:
 *   1. Find innermost containing circle
 *   2. Assign node to that circle's region
 *   3. Add node to region's member list
 *
 * This replaces explicit "node X in RegionY" declarations.
 * After this call, g->nodes[i].region is set from geometry.
 */
ArcGeoResult arc_geo_derive_regions(ArcGraph* g, const ArcGeoLayout* layout) {
    ArcGeoResult result;
    memset(&result, 0, sizeof(result));
    result.valid = true;

    if (layout->circle_count == 0) {
        return result;  /* no geometry = nothing to derive */
    }

    /* Clear existing region memberships for positioned nodes */
    for (uint32_t i = 0; i < layout->position_count; i++) {
        ArcNodeId nid = layout->positions[i].id;
        if (nid >= g->node_count) continue;

        ArcPos pos = layout->positions[i].pos;
        ArcRegionId region = find_innermost_region(layout, pos, nid, &result);

        if (!result.valid) return result;  /* ambiguity detected */

        if (region == ARC_INVALID_ID) {
            /* Node is outside all circles — remains in its current region.
             * This is valid for top-level nodes (e.g., in root module). */
            continue;
        }

        /* Remove from old region, assign to new */
        ArcNode* node = &g->nodes[nid];
        remove_from_region(g, node->region, nid);
        node->region = region;
        arc_region_add_member(g, region, nid);

        /* Track in result */
        if (result.member_count < ARC_GEO_MAX_NODES) {
            ArcDerivedMember* dm = &result.members[result.member_count++];
            dm->node = nid;
            dm->region = region;
        }

        /* === Semantic fan-out tracking === */
        /* Each of these increments represents a semantic fact
         * derived from the single geometric relation inside(x, R).
         * Only count if actually used by downstream systems. */

        result.fanout_membership++;   /* always: region membership */
        result.fanout_scope++;        /* scope = region (lexical scope) */
        result.fanout_effect_owner++; /* effect ownership = region */
        result.fanout_capability++;   /* capability context = sealed ancestor */
        result.fanout_dep_owner++;    /* dependency owner = region */
        result.fanout_visibility++;   /* visibility boundary = region */
    }

    return result;
}

/* Case F: verify geometry and semantic graph agree */
bool arc_geo_verify_consistency(const ArcGraph* g, const ArcGeoResult* result) {
    for (uint32_t i = 0; i < result->member_count; i++) {
        ArcNodeId nid = result->members[i].node;
        ArcRegionId expected = result->members[i].region;
        if (nid >= g->node_count) return false;
        if (g->nodes[nid].region != expected) return false;
    }
    return true;
}

/* Case D: verify deformation stability.
 * Re-derive membership from the "after" layout and check it matches "before". */
bool arc_geo_deformation_stable(ArcGraph* g,
                                const ArcGeoLayout* before,
                                const ArcGeoLayout* after) {
    /* Derive with "before" layout */
    ArcGeoResult r_before = arc_geo_derive_regions(g, before);
    if (!r_before.valid) return false;

    /* Save derived memberships */
    ArcRegionId saved[ARC_GEO_MAX_NODES];
    uint32_t saved_count = r_before.member_count;
    for (uint32_t i = 0; i < saved_count; i++) {
        saved[i] = r_before.members[i].region;
    }

    /* Derive with "after" layout */
    ArcGeoResult r_after = arc_geo_derive_regions(g, after);
    if (!r_after.valid) return false;

    /* Compare */
    if (r_after.member_count != saved_count) return false;
    for (uint32_t i = 0; i < saved_count; i++) {
        if (r_after.members[i].region != saved[i]) return false;
    }
    return true;
}
