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

        /* Declared (potential) fan-out — 6 categories per derivation */
        result.fanout_membership++;
        result.fanout_scope++;
        result.fanout_effect_owner++;
        result.fanout_capability++;
        result.fanout_dep_owner++;
        result.fanout_visibility++;
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

/* --- Fan-out consumption measurement --- */

/* Walk up region tree to find a sealed ancestor in the arch spec */
static bool has_sealed_ancestor(const ArcGraph* g, const ArcArchSpec* arch,
                                ArcRegionId rid) {
    ArcRegionId cur = rid;
    while (cur != ARC_INVALID_ID) {
        for (uint32_t i = 0; i < arch->region_count; i++) {
            if (arch->regions[i].region == cur && arch->regions[i].sealed)
                return true;
        }
        if (cur >= g->region_count) break;
        cur = g->regions[cur].parent;
    }
    return false;
}

/* Check if any edge incident on node crosses a region boundary */
static bool has_cross_boundary_edge(const ArcGraph* g, ArcNodeId nid) {
    ArcRegionId node_rid = g->nodes[nid].region;
    for (uint32_t ei = 0; ei < g->edge_count; ei++) {
        const ArcSemEdge* e = &g->edges[ei];
        ArcNodeId from_owner = g->ports[e->from].owner;
        ArcNodeId to_owner = g->ports[e->to].owner;
        if (from_owner == nid || to_owner == nid) {
            ArcNodeId other = (from_owner == nid) ? to_owner : from_owner;
            if (g->nodes[other].region != node_rid) return true;
        }
    }
    return false;
}

/*
 * Measure which fan-out categories were actually consumed by downstream
 * analysis. Populates the consumed_* fields in result.
 *
 * For each derived node:
 *   membership:   always consumed (the derivation itself is the fact)
 *   effect_owner: consumed if a sealed ancestor exists (effect collection
 *                 walks the region's members to attribute effects)
 *   capability:   consumed if a sealed ancestor exists (capability check
 *                 uses sealed ancestry to enforce capability requirements)
 *   dep_owner:    consumed if any incident edge crosses a region boundary
 *                 (verify_boundaries reads node.region for each edge endpoint)
 *   scope:        not yet consumed (no scope resolver implemented)
 *   visibility:   not yet consumed (no visibility resolver implemented)
 */
void arc_geo_measure_consumed_fanout(ArcGeoResult* result,
                                     const ArcGraph* g,
                                     const ArcArchSpec* arch) {
    result->consumed_membership = 0;
    result->consumed_scope = 0;
    result->consumed_effect_owner = 0;
    result->consumed_capability = 0;
    result->consumed_dep_owner = 0;
    result->consumed_visibility = 0;

    for (uint32_t i = 0; i < result->member_count; i++) {
        ArcNodeId nid = result->members[i].node;
        ArcRegionId rid = result->members[i].region;
        if (nid >= g->node_count) continue;
        (void)rid;

        /* Membership: always consumed */
        result->consumed_membership++;

        /* Effect ownership + capability context: consumed if sealed ancestor */
        if (arch && has_sealed_ancestor(g, arch, g->nodes[nid].region)) {
            result->consumed_effect_owner++;
            result->consumed_capability++;
        }

        /* Dependency ownership: consumed if cross-boundary edge exists */
        if (arch && has_cross_boundary_edge(g, nid))
            result->consumed_dep_owner++;

        /* Scope: no resolver implemented */
        /* Visibility: no resolver implemented */
    }
}
