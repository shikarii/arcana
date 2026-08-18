#include "cyclic_order.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Cyclic Order — Experiment 2B implementation.
 *
 * Primitive R: region member ordering derived from angular position.
 * This is NOT vertex rotation (Primitive V). The ordering is among
 * the members of a region, not among edges incident on a hub node.
 */

/* === Query API === */

ArcNodeId arc_cyclic_next(const ArcCyclicOrder* order, ArcNodeId node) {
    for (uint32_t i = 0; i < order->count; i++) {
        if (order->members[i] == node)
            return order->members[(i + 1) % order->count];
    }
    return ARC_INVALID_ID;
}

ArcNodeId arc_cyclic_prev(const ArcCyclicOrder* order, ArcNodeId node) {
    for (uint32_t i = 0; i < order->count; i++) {
        if (order->members[i] == node)
            return order->members[(i + order->count - 1) % order->count];
    }
    return ARC_INVALID_ID;
}

bool arc_cyclic_adjacent(const ArcCyclicOrder* order, ArcNodeId a, ArcNodeId b) {
    return arc_cyclic_next(order, a) == b || arc_cyclic_prev(order, a) == b;
}

/* === Manual cyclic order (Phase 2B.0) === */

void arc_cyclic_set(ArcCyclicOrder* order, ArcRegionId region,
                    const ArcNodeId* members, uint32_t count) {
    order->region = region;
    order->count = count < ARC_CYCLIC_MAX_MEMBERS ? count : ARC_CYCLIC_MAX_MEMBERS;
    memcpy(order->members, members, order->count * sizeof(ArcNodeId));
}

/* === Geometry-derived cyclic order (Phase 2B.1) === */

/* Entry for sorting members by angle */
typedef struct {
    ArcNodeId node;
    double    angle;
} AngleEntry;

static int compare_angles(const void* a, const void* b) {
    double da = ((const AngleEntry*)a)->angle;
    double db = ((const AngleEntry*)b)->angle;
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

/* Find a node's position in the geo layout. Returns false if not found. */
static bool find_node_pos(const ArcGeoLayout* layout, ArcNodeId node, ArcPos* out) {
    for (uint32_t i = 0; i < layout->position_count; i++) {
        if (layout->positions[i].id == node) {
            *out = layout->positions[i].pos;
            return true;
        }
    }
    return false;
}

ArcCyclicResult arc_geo_derive_cyclic_order(
    const ArcGraph* g, const ArcGeoLayout* layout,
    ArcRegionId region, double cx, double cy)
{
    ArcCyclicResult result;
    memset(&result, 0, sizeof(result));
    result.valid = true;

    if (region >= g->region_count) {
        result.valid = false;
        snprintf(result.error, sizeof(result.error), "invalid region %u", (unsigned)region);
        return result;
    }

    const ArcRegion* reg = &g->regions[region];
    AngleEntry entries[ARC_CYCLIC_MAX_MEMBERS];
    uint32_t entry_count = 0;

    /* Collect positioned members and compute their angles */
    for (uint32_t i = 0; i < reg->member_count; i++) {
        ArcNodeId nid = reg->members[i];
        ArcPos pos;
        if (!find_node_pos(layout, nid, &pos)) continue;

        double dx = pos.x - cx;
        double dy = pos.y - cy;
        double dist = sqrt(dx * dx + dy * dy);

        /* Reject members too close to center — angle undefined */
        if (dist < 1.0) {
            result.valid = false;
            snprintf(result.error, sizeof(result.error),
                "node %u too close to ring center (dist=%.2f)", (unsigned)nid, dist);
            return result;
        }

        if (entry_count >= ARC_CYCLIC_MAX_MEMBERS) break;
        entries[entry_count].node = nid;
        entries[entry_count].angle = atan2(dy, dx);
        entry_count++;
    }

    if (entry_count < 2) {
        result.valid = false;
        snprintf(result.error, sizeof(result.error),
            "region %u has fewer than 2 positioned members", (unsigned)region);
        return result;
    }

    /* Sort by angle (clockwise from positive x-axis) */
    qsort(entries, entry_count, sizeof(AngleEntry), compare_angles);

    /* Check angular ambiguity — any two adjacent members too close? */
    for (uint32_t i = 0; i < entry_count; i++) {
        uint32_t j = (i + 1) % entry_count;
        double diff = fabs(entries[j].angle - entries[i].angle);
        /* Handle wrap-around for last→first comparison */
        if (i == entry_count - 1)
            diff = fabs((entries[0].angle + 2.0 * M_PI) - entries[i].angle);
        if (diff < ARC_CYCLIC_ANGLE_EPSILON) {
            result.valid = false;
            snprintf(result.error, sizeof(result.error),
                "angular ambiguity: nodes %u and %u within %.4f rad",
                (unsigned)entries[i].node, (unsigned)entries[j].node, diff);
            return result;
        }
    }

    /* Populate the cyclic order */
    ArcCyclicOrder* order = &result.orders[0];
    order->region = region;
    order->count = entry_count;
    for (uint32_t i = 0; i < entry_count; i++)
        order->members[i] = entries[i].node;
    result.count = 1;

    return result;
}

/* === Forgetful fingerprint === */

/*
 * FNV-1a 64-bit hash.
 * Strips all geometric embedding; preserves abstract graph topology.
 *
 * Hashed elements (in deterministic order):
 *   1. Node count, edge count, region count
 *   2. For each node (by ID order): kind, region
 *   3. For each edge (by ID order): from_port_owner kind, to_port_owner kind,
 *      from_port_role hash, to_port_role hash
 *   4. For each region (by ID order): kind, parent
 */

#define FNV_OFFSET 0xcbf29ce484222325ULL
#define FNV_PRIME  0x100000001b3ULL

static uint64_t fnv_hash_u32(uint64_t h, uint32_t val) {
    for (int i = 0; i < 4; i++) {
        h ^= (val >> (i * 8)) & 0xFF;
        h *= FNV_PRIME;
    }
    return h;
}

static uint64_t fnv_hash_str(uint64_t h, const char* s) {
    if (!s) { h ^= 0xFF; h *= FNV_PRIME; return h; }
    while (*s) { h ^= (uint8_t)*s++; h *= FNV_PRIME; }
    return h;
}

uint64_t arc_graph_fingerprint(const ArcGraph* g) {
    uint64_t h = FNV_OFFSET;

    /* Counts */
    h = fnv_hash_u32(h, g->node_count);
    h = fnv_hash_u32(h, g->edge_count);
    h = fnv_hash_u32(h, g->region_count);

    /* Nodes: kind + region (ordered by node ID) */
    for (uint32_t i = 0; i < g->node_count; i++) {
        h = fnv_hash_u32(h, (uint32_t)g->nodes[i].kind);
        h = fnv_hash_u32(h, g->nodes[i].region);
    }

    /* Edges: from/to port owner kinds + port roles (ordered by edge ID) */
    for (uint32_t i = 0; i < g->edge_count; i++) {
        ArcPortId from_pid = g->edges[i].from;
        ArcPortId to_pid = g->edges[i].to;
        if (from_pid < g->port_count && to_pid < g->port_count) {
            const ArcPort* fp = &g->ports[from_pid];
            const ArcPort* tp = &g->ports[to_pid];
            if (fp->owner < g->node_count)
                h = fnv_hash_u32(h, (uint32_t)g->nodes[fp->owner].kind);
            if (tp->owner < g->node_count)
                h = fnv_hash_u32(h, (uint32_t)g->nodes[tp->owner].kind);
            h = fnv_hash_str(h, fp->role);
            h = fnv_hash_str(h, tp->role);
        }
    }

    /* Regions: kind + parent (ordered by region ID) */
    for (uint32_t i = 0; i < g->region_count; i++) {
        h = fnv_hash_u32(h, (uint32_t)g->regions[i].kind);
        h = fnv_hash_u32(h, g->regions[i].parent);
    }

    return h;
}
