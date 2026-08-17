#include "semantic_graph.h"
#include <stdarg.h>

void arc_graph_init(ArcGraph* g) {
    memset(g, 0, sizeof(*g));
    g->root_region = ARC_INVALID_ID;
    g->output_node = ARC_INVALID_ID;
}

void arc_graph_free(ArcGraph* g) {
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->owns_strings) {
            ArcNodeKind k = g->nodes[i].kind;
            if (k == ARC_NODE_LET || k == ARC_NODE_VAR_REF ||
                k == ARC_NODE_ASSIGN || k == ARC_NODE_PARAM ||
                k == ARC_NODE_FUNC_CALL ||
                k == ARC_NODE_RECORD_NEW || k == ARC_NODE_FIELD_GET ||
                k == ARC_NODE_FIELD_SET)
                ARC_FREE((void*)g->nodes[i].attr.name);
            else if (k == ARC_NODE_FUNC_DEF)
                ARC_FREE((void*)g->nodes[i].attr.func.name);
            else if (k == ARC_NODE_CONST_STRING)
                ARC_FREE((void*)g->nodes[i].attr.string_value.data);
        }
        ARC_FREE(g->nodes[i].ports);
        ARC_FREE(g->nodes[i].cyclic_order);
    }
    for (uint32_t i = 0; i < g->region_count; i++) {
        ARC_FREE(g->regions[i].members);
    }
    ARC_FREE(g->nodes);
    ARC_FREE(g->ports);
    ARC_FREE(g->edges);
    ARC_FREE(g->regions);
    memset(g, 0, sizeof(*g));
}

ArcRegionId arc_graph_add_region(ArcGraph* g, ArcRegionKind kind, ArcRegionId parent) {
    if (g->region_count >= g->region_cap) {
        g->region_cap = g->region_cap < 8 ? 8 : g->region_cap * 2;
        g->regions = ARC_REALLOC(g->regions, ArcRegion, g->region_cap);
    }
    ArcRegionId id = g->region_count++;
    g->regions[id] = (ArcRegion){
        .id = id, .parent = parent, .kind = kind,
        .members = NULL, .member_count = 0, .member_cap = 0
    };
    return id;
}

ArcNodeId arc_graph_add_node(ArcGraph* g, ArcNodeKind kind, ArcRegionId region, ArcElementId src) {
    if (g->node_count >= g->node_cap) {
        g->node_cap = g->node_cap < 16 ? 16 : g->node_cap * 2;
        g->nodes = ARC_REALLOC(g->nodes, ArcNode, g->node_cap);
    }
    ArcNodeId id = g->node_count++;
    memset(&g->nodes[id], 0, sizeof(ArcNode));
    g->nodes[id].id = id;
    g->nodes[id].kind = kind;
    g->nodes[id].region = region;
    g->nodes[id].source_id = src;

    /* Auto-add to region */
    arc_region_add_member(g, region, id);
    return id;
}

ArcPortId arc_graph_add_port(ArcGraph* g, ArcNodeId owner, ArcPortDir dir, const char* role) {
    if (g->port_count >= g->port_cap) {
        g->port_cap = g->port_cap < 32 ? 32 : g->port_cap * 2;
        g->ports = ARC_REALLOC(g->ports, ArcPort, g->port_cap);
    }
    ArcPortId id = g->port_count++;
    g->ports[id] = (ArcPort){ .id = id, .owner = owner, .dir = dir, .role = role };

    /* Add to node's port list */
    ArcNode* n = &g->nodes[owner];
    if (n->port_count >= n->port_cap) {
        n->port_cap = n->port_cap < 4 ? 4 : n->port_cap * 2;
        n->ports = ARC_REALLOC(n->ports, ArcPortId, n->port_cap);
    }
    n->ports[n->port_count++] = id;
    return id;
}

ArcEdgeId arc_graph_add_edge(ArcGraph* g, ArcPortId from, ArcPortId to) {
    if (g->edge_count >= g->edge_cap) {
        g->edge_cap = g->edge_cap < 16 ? 16 : g->edge_cap * 2;
        g->edges = ARC_REALLOC(g->edges, ArcSemEdge, g->edge_cap);
    }
    ArcEdgeId id = g->edge_count++;
    g->edges[id] = (ArcSemEdge){ .id = id, .from = from, .to = to };
    return id;
}

void arc_node_set_cyclic_order(ArcGraph* g, ArcNodeId node, const ArcPortId* order, uint32_t count) {
    ArcNode* n = &g->nodes[node];
    ARC_FREE(n->cyclic_order);
    n->cyclic_order = ARC_ALLOC(ArcPortId, count);
    memcpy(n->cyclic_order, order, count * sizeof(ArcPortId));
    n->cyclic_count = count;
}

void arc_region_add_member(ArcGraph* g, ArcRegionId region, ArcNodeId node) {
    ArcRegion* r = &g->regions[region];
    if (r->member_count >= r->member_cap) {
        r->member_cap = r->member_cap < 8 ? 8 : r->member_cap * 2;
        r->members = ARC_REALLOC(r->members, ArcNodeId, r->member_cap);
    }
    r->members[r->member_count++] = node;
}

ArcNode*   arc_graph_node(ArcGraph* g, ArcNodeId id)     { return id < g->node_count ? &g->nodes[id] : NULL; }
ArcPort*   arc_graph_port(ArcGraph* g, ArcPortId id)     { return id < g->port_count ? &g->ports[id] : NULL; }
ArcRegion* arc_graph_region(ArcGraph* g, ArcRegionId id) { return id < g->region_count ? &g->regions[id] : NULL; }

/* === Validation === */

static void vadd(ArcGraphValidation* v, const char* fmt, ...) {
    if (v->count >= v->cap) {
        v->cap = v->cap < 16 ? 16 : v->cap * 2;
        v->errors = ARC_REALLOC(v->errors, ArcGraphError, v->cap);
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(v->errors[v->count].message, sizeof(v->errors[v->count].message), fmt, ap);
    va_end(ap);
    v->count++;
    v->valid = false;
}

/* --- Validate regions: parentage acyclicity + sequential IDs --- */
static void validate_regions(const ArcGraph* g, ArcGraphValidation* v) {
    for (uint32_t i = 0; i < g->region_count; i++) {
        if (g->regions[i].id != i)
            vadd(v, "region at index %u has mismatched id %u", i, g->regions[i].id);
        if (g->regions[i].parent != ARC_INVALID_ID && g->regions[i].parent >= g->region_count)
            vadd(v, "region %u has invalid parent %u", i, g->regions[i].parent);
        ArcRegionId cur = g->regions[i].parent;
        for (uint32_t depth = 0; cur != ARC_INVALID_ID && depth < g->region_count; depth++) {
            if (cur == i) { vadd(v, "region %u has cyclic ancestry", i); break; }
            if (cur >= g->region_count) break;
            cur = g->regions[cur].parent;
        }
    }
}

/* --- Validate nodes: IDs, regions, cyclic order --- */
static void validate_nodes(const ArcGraph* g, ArcGraphValidation* v) {
    for (uint32_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i].id != i)
            vadd(v, "node at index %u has mismatched id %u", i, g->nodes[i].id);
        const ArcNode* n = &g->nodes[i];
        if (n->region >= g->region_count)
            vadd(v, "node %u has invalid region %u", i, n->region);
        if (n->cyclic_count > 0) {
            if (n->cyclic_count != n->port_count)
                vadd(v, "node %u cyclic order count %u != port count %u",
                     i, n->cyclic_count, n->port_count);
            for (uint32_t ci = 0; ci < n->cyclic_count; ci++) {
                bool found = false;
                for (uint32_t pi = 0; pi < n->port_count; pi++) {
                    if (n->cyclic_order[ci] == n->ports[pi]) { found = true; break; }
                }
                if (!found)
                    vadd(v, "node %u cyclic order entry %u references unknown port %u",
                         i, ci, n->cyclic_order[ci]);
            }
        }
    }
}

/* --- Validate edges and ports --- */
static void validate_edges_and_ports(const ArcGraph* g, ArcGraphValidation* v) {
    for (uint32_t i = 0; i < g->edge_count; i++) {
        const ArcSemEdge* e = &g->edges[i];
        if (e->id != i)
            vadd(v, "edge at index %u has mismatched id %u", i, e->id);
        if (e->from >= g->port_count)
            vadd(v, "edge %u has invalid source port %u", i, e->from);
        if (e->to >= g->port_count)
            vadd(v, "edge %u has invalid target port %u", i, e->to);
        if (e->from < g->port_count && g->ports[e->from].dir != ARC_PORT_OUTPUT)
            vadd(v, "edge %u source port %u is not an output", i, e->from);
        if (e->to < g->port_count && g->ports[e->to].dir != ARC_PORT_INPUT)
            vadd(v, "edge %u target port %u is not an input", i, e->to);
    }
    for (uint32_t i = 0; i < g->port_count; i++) {
        if (g->ports[i].id != i)
            vadd(v, "port at index %u has mismatched id %u", i, g->ports[i].id);
        if (g->ports[i].owner >= g->node_count)
            vadd(v, "port %u has invalid owner node %u", i, g->ports[i].owner);
    }
}

ArcGraphValidation arc_graph_validate(const ArcGraph* g) {
    ArcGraphValidation v = { .errors = NULL, .count = 0, .cap = 0, .valid = true };

    if (g->root_region == ARC_INVALID_ID)
        vadd(&v, "no root region defined");

    validate_regions(g, &v);
    validate_nodes(g, &v);
    validate_edges_and_ports(g, &v);

    return v;
}

void arc_graph_validation_free(ArcGraphValidation* v) {
    ARC_FREE(v->errors);
    v->errors = NULL; v->count = v->cap = 0;
}
