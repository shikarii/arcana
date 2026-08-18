#include "architecture.h"
#include "../bytecode/opcodes.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/*
 * Architecture verifier — Experiment 1.
 *
 * For each sealed region:
 *   1. Walk all nodes transitively reachable inside the region
 *   2. Union their effects (including called functions)
 *   3. Check: Effects(R) <= Capabilities(R)
 *
 * For each edge crossing a sealed boundary:
 *   4. Check: a declared channel exists between the two regions
 */

/* --- Init / builders --- */

void arc_arch_spec_init(ArcArchSpec* spec) {
    memset(spec, 0, sizeof(*spec));
}

void arc_arch_spec_add_sealed(ArcArchSpec* spec, ArcRegionId r, ArcEffectSet caps) {
    if (spec->region_count >= ARC_ARCH_MAX_REGIONS) return;
    ArcArchRegionSpec* rs = &spec->regions[spec->region_count++];
    rs->region = r;
    rs->sealed = true;
    rs->capabilities = caps;
}

void arc_arch_spec_add_channel(ArcArchSpec* spec, ArcRegionId from, ArcRegionId to) {
    if (spec->channel_count >= ARC_ARCH_MAX_CHANNELS) return;
    ArcArchChannel* ch = &spec->channels[spec->channel_count++];
    ch->from_region = from;
    ch->to_region = to;
}

/* --- Effect lookup --- */

ArcEffectSet arc_node_effect(ArcNodeKind kind) {
    switch (kind) {
    case ARC_NODE_PRINT:        return ARC_EFFECT_LOGGER;
    case ARC_NODE_CHAN_SEND:
    case ARC_NODE_CHAN_RECV:
    case ARC_NODE_CHAN_NEW:
    case ARC_NODE_MUTEX_NEW:
    case ARC_NODE_MUTEX_LOCK:
    case ARC_NODE_MUTEX_UNLOCK:
    case ARC_NODE_THREAD_SPAWN:
    case ARC_NODE_THREAD_JOIN:
        return ARC_EFFECT_PURE; /* concurrency primitives are architectural, not effects */
    default:
        return ARC_EFFECT_PURE;
    }
}

ArcEffectSet arc_intrinsic_effect(uint16_t id) {
    switch (id) {
    case ARC_INTRINSIC_PRINT:    return ARC_EFFECT_LOGGER;
    case ARC_INTRINSIC_CLOCK:    return ARC_EFFECT_CLOCK;
    case ARC_INTRINSIC_INPUT:    return ARC_EFFECT_INPUT;
    default:                     return ARC_EFFECT_PURE;
    }
}

ArcEffectSet arc_effect_parse(const char* name) {
    if (strcmp(name, "Clock") == 0)      return ARC_EFFECT_CLOCK;
    if (strcmp(name, "Logger") == 0)     return ARC_EFFECT_LOGGER;
    if (strcmp(name, "Network") == 0)    return ARC_EFFECT_NETWORK;
    if (strcmp(name, "Filesystem") == 0) return ARC_EFFECT_FILESYSTEM;
    if (strcmp(name, "Hardware") == 0)   return ARC_EFFECT_HARDWARE;
    if (strcmp(name, "Input") == 0)      return ARC_EFFECT_INPUT;
    return 0;
}

const char* arc_effect_name(ArcEffect e) {
    switch (e) {
    case ARC_EFFECT_CLOCK:      return "Clock";
    case ARC_EFFECT_LOGGER:     return "Logger";
    case ARC_EFFECT_NETWORK:    return "Network";
    case ARC_EFFECT_FILESYSTEM: return "Filesystem";
    case ARC_EFFECT_HARDWARE:   return "Hardware";
    case ARC_EFFECT_INPUT:      return "Input";
    default:                    return "Pure";
    }
}

/* --- Internal helpers --- */

static void add_error(ArcArchResult* r, ArcRegionId reg, const char* fmt, ...) {
    if (r->error_count >= ARC_ARCH_MAX_ERRORS) return;
    ArcArchError* e = &r->errors[r->error_count++];
    e->region = reg;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->message, sizeof(e->message), fmt, ap);
    va_end(ap);
    r->valid = false;
}

/* Find which sealed region (if any) a node's region belongs to */
static const ArcArchRegionSpec* find_sealed_ancestor(
    const ArcGraph* g, const ArcArchSpec* spec, ArcRegionId rid)
{
    ArcRegionId cur = rid;
    while (cur != ARC_INVALID_ID) {
        for (uint32_t i = 0; i < spec->region_count; i++) {
            if (spec->regions[i].region == cur && spec->regions[i].sealed)
                return &spec->regions[i];
        }
        cur = g->regions[cur].parent;
    }
    return NULL;
}

/* Find func_def node by name in the graph */
static ArcNodeId find_func_def(const ArcGraph* g, const char* name) {
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].kind == ARC_NODE_FUNC_DEF &&
            g->nodes[i].attr.func.name &&
            strcmp(g->nodes[i].attr.func.name, name) == 0)
            return g->nodes[i].id;
    }
    return ARC_INVALID_ID;
}

/* Collect transitive effects of all nodes in a region (and child regions) */
static ArcEffectSet collect_region_effects(
    const ArcGraph* g, ArcRegionId rid,
    bool* visited_regions, bool* visited_funcs, uint32_t max_regions)
{
    if (rid >= max_regions || visited_regions[rid]) return ARC_EFFECT_PURE;
    visited_regions[rid] = true;

    ArcEffectSet effects = ARC_EFFECT_PURE;
    const ArcRegion* region = &g->regions[rid];

    for (uint32_t m = 0; m < region->member_count; m++) {
        ArcNodeId nid = region->members[m];
        if (nid >= g->node_count) continue;
        const ArcNode* node = &g->nodes[nid];

        /* Direct effect of this node */
        effects |= arc_node_effect(node->kind);

        /* Intrinsic call effect */
        if (node->kind == ARC_NODE_INTRINSIC_CALL)
            effects |= arc_intrinsic_effect(node->attr.intrinsic.id);

        /* Function call — trace into body */
        if (node->kind == ARC_NODE_FUNC_CALL && node->attr.name) {
            ArcNodeId fid = find_func_def(g, node->attr.name);
            if (fid != ARC_INVALID_ID && !visited_funcs[fid]) {
                visited_funcs[fid] = true;
                ArcRegionId body = g->nodes[fid].attr.func.body_region;
                if (body != ARC_INVALID_ID)
                    effects |= collect_region_effects(g, body,
                        visited_regions, visited_funcs, max_regions);
            }
        }

        /* Thread spawn — trace into spawned function body */
        if (node->kind == ARC_NODE_THREAD_SPAWN && node->attr.name) {
            ArcNodeId fid = find_func_def(g, node->attr.name);
            if (fid != ARC_INVALID_ID && !visited_funcs[fid]) {
                visited_funcs[fid] = true;
                ArcRegionId body = g->nodes[fid].attr.func.body_region;
                if (body != ARC_INVALID_ID)
                    effects |= collect_region_effects(g, body,
                        visited_regions, visited_funcs, max_regions);
            }
        }

        /* Nodes with child regions (if, while, cycle, try) */
        if (node->kind == ARC_NODE_IF) {
            if (node->attr.branch.then_region != ARC_INVALID_ID)
                effects |= collect_region_effects(g, node->attr.branch.then_region,
                    visited_regions, visited_funcs, max_regions);
            if (node->attr.branch.else_region != ARC_INVALID_ID)
                effects |= collect_region_effects(g, node->attr.branch.else_region,
                    visited_regions, visited_funcs, max_regions);
        }
        if (node->kind == ARC_NODE_WHILE && node->attr.loop.body_region != ARC_INVALID_ID)
            effects |= collect_region_effects(g, node->attr.loop.body_region,
                visited_regions, visited_funcs, max_regions);
        if (node->kind == ARC_NODE_CYCLE && node->attr.cycle.body_region != ARC_INVALID_ID)
            effects |= collect_region_effects(g, node->attr.cycle.body_region,
                visited_regions, visited_funcs, max_regions);
        if (node->kind == ARC_NODE_TRY) {
            if (node->attr.try_catch.try_region != ARC_INVALID_ID)
                effects |= collect_region_effects(g, node->attr.try_catch.try_region,
                    visited_regions, visited_funcs, max_regions);
            if (node->attr.try_catch.catch_region != ARC_INVALID_ID)
                effects |= collect_region_effects(g, node->attr.try_catch.catch_region,
                    visited_regions, visited_funcs, max_regions);
        }
    }

    /* Also walk child regions (regions whose parent is this region) */
    for (uint32_t i = 0; i < g->region_count; i++) {
        if (g->regions[i].parent == rid)
            effects |= collect_region_effects(g, i,
                visited_regions, visited_funcs, max_regions);
    }

    return effects;
}

/* Check if a channel is declared between two regions (either direction) */
static bool has_channel(const ArcArchSpec* spec, ArcRegionId a, ArcRegionId b) {
    for (uint32_t i = 0; i < spec->channel_count; i++) {
        if ((spec->channels[i].from_region == a && spec->channels[i].to_region == b) ||
            (spec->channels[i].from_region == b && spec->channels[i].to_region == a))
            return true;
    }
    return false;
}

/* Phase 1: check Effects(R) <= Capabilities(R) for each sealed region */
static void verify_effects(const ArcGraph* g, const ArcArchSpec* spec, ArcArchResult* result) {
    for (uint32_t i = 0; i < spec->region_count; i++) {
        if (!spec->regions[i].sealed) continue;
        ArcRegionId rid = spec->regions[i].region;
        if (rid >= g->region_count) continue;
        bool* vr = ARC_ALLOC(bool, g->region_count);
        bool* vf = ARC_ALLOC(bool, g->node_count);
        ArcEffectSet effects = collect_region_effects(g, rid, vr, vf, g->region_count);
        ARC_FREE(vr); ARC_FREE(vf);
        ArcEffectSet undeclared = effects & ~spec->regions[i].capabilities;
        for (int bit = 0; bit < 8 && undeclared; bit++) {
            ArcEffect e = (ArcEffect)(1 << bit);
            if (undeclared & e)
                add_error(result, rid, "sealed region requires undeclared capability %s",
                    arc_effect_name(e));
        }
    }
}

/* Phase 2: check cross-boundary edges have declared channels */
static void verify_boundaries(const ArcGraph* g, const ArcArchSpec* spec, ArcArchResult* result) {
    for (uint32_t ei = 0; ei < g->edge_count; ei++) {
        const ArcSemEdge* edge = &g->edges[ei];
        ArcRegionId from_rid = g->nodes[g->ports[edge->from].owner].region;
        ArcRegionId to_rid = g->nodes[g->ports[edge->to].owner].region;
        if (from_rid == to_rid) continue;
        const ArcArchRegionSpec* fs = find_sealed_ancestor(g, spec, from_rid);
        const ArcArchRegionSpec* ts = find_sealed_ancestor(g, spec, to_rid);
        if (!fs || !ts || fs->region == ts->region) continue;
        if (!has_channel(spec, fs->region, ts->region))
            add_error(result, fs->region,
                "undeclared dependency crosses sealed boundary to region %u",
                (unsigned)ts->region);
    }
}

ArcArchResult arc_arch_verify(const ArcGraph* g, const ArcArchSpec* spec) {
    ArcArchResult result;
    memset(&result, 0, sizeof(result));
    result.valid = true;
    verify_effects(g, spec, &result);
    verify_boundaries(g, spec, &result);
    return result;
}
