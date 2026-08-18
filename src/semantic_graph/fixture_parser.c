#include "fixture_parser.h"
#include "fixture_kinds.h"
#include <stdarg.h>
#include <ctype.h>

/* --- Internal parse state --- */

typedef struct {
    struct { char name[32]; ArcRegionId id; } regions[256];
    uint32_t region_count;

    struct { char name[32]; ArcNodeId id; } nodes[256];
    uint32_t node_count;

    struct { char node[64]; char role[64]; ArcPortId id; ArcNodeId owner; } ports[1024];
    uint32_t port_count;

    ArcGraph* graph;
    ArcArchSpec* arch;
    ArcGeoLayout* geo;
    char error[256];
    bool had_error;
    bool has_arch;
    bool has_geo;
} ParseState;

static void perr(ParseState* s, const char* fmt, ...) {
    if (s->had_error) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(s->error, sizeof(s->error), fmt, ap);
    va_end(ap);
    s->had_error = true;
}

static ArcRegionId find_region(ParseState* s, const char* name) {
    for (uint32_t i = 0; i < s->region_count; i++)
        if (strcmp(s->regions[i].name, name) == 0) return s->regions[i].id;
    return ARC_INVALID_ID;
}

static ArcNodeId find_node(ParseState* s, const char* name) {
    for (uint32_t i = 0; i < s->node_count; i++)
        if (strcmp(s->nodes[i].name, name) == 0) return s->nodes[i].id;
    return ARC_INVALID_ID;
}

static ArcPortId find_port(ParseState* s, const char* node_name, const char* role) {
    for (uint32_t i = 0; i < s->port_count; i++)
        if (strcmp(s->ports[i].node, node_name) == 0 && strcmp(s->ports[i].role, role) == 0)
            return s->ports[i].id;
    return ARC_INVALID_ID;
}

static void register_port(ParseState* s, const char* node_name, const char* role,
                           ArcPortId pid, ArcNodeId owner) {
    if (s->port_count >= 1024) { perr(s, "too many ports"); return; }
    snprintf(s->ports[s->port_count].node, 64, "%s", node_name);
    snprintf(s->ports[s->port_count].role, 64, "%s", role);
    s->ports[s->port_count].id = pid;
    s->ports[s->port_count].owner = owner;
    s->port_count++;
}

static const char* skip_ws(const char* p) {
    while (*p && (*p == ' ' || *p == '\t')) p++;
    return p;
}

static const char* read_token(const char* p, char* buf, size_t bufsz) {
    p = skip_ws(p);
    size_t i = 0;
    while (*p && !isspace((unsigned char)*p) && i < bufsz - 1) buf[i++] = *p++;
    buf[i] = '\0';
    return p;
}

/* Kind table and node attr parsing extracted to fixture_kinds.c */

/* --- Add default output-only port --- */
static void add_output_port(ParseState* s, const char* node_name, ArcNodeId nid) {
    ArcPortId out = arc_graph_add_port(s->graph, nid, ARC_PORT_OUTPUT, "out");
    register_port(s, node_name, "out", out, nid);
}

/* --- Add default binary operator ports (lhs, rhs, out) with cyclic order --- */
static void add_binary_ports(ParseState* s, const char* node_name, ArcNodeId nid) {
    ArcGraph* g = s->graph;
    ArcPortId lhs = arc_graph_add_port(g, nid, ARC_PORT_INPUT, "lhs");
    ArcPortId rhs = arc_graph_add_port(g, nid, ARC_PORT_INPUT, "rhs");
    ArcPortId out = arc_graph_add_port(g, nid, ARC_PORT_OUTPUT, "out");
    register_port(s, node_name, "lhs", lhs, nid);
    register_port(s, node_name, "rhs", rhs, nid);
    register_port(s, node_name, "out", out, nid);
    ArcPortId order[] = { lhs, rhs, out };
    arc_node_set_cyclic_order(g, nid, order, 3);
}

/* --- Add default unary operator ports (value, out) --- */
static void add_unary_ports(ParseState* s, const char* node_name, ArcNodeId nid) {
    ArcGraph* g = s->graph;
    ArcPortId in = arc_graph_add_port(g, nid, ARC_PORT_INPUT, "value");
    ArcPortId out = arc_graph_add_port(g, nid, ARC_PORT_OUTPUT, "out");
    register_port(s, node_name, "value", in, nid);
    register_port(s, node_name, "out", out, nid);
}

/* --- Add a single input port with given role --- */
static void add_input_port(ParseState* s, const char* node_name, ArcNodeId nid,
                            const char* role) {
    ArcPortId val = arc_graph_add_port(s->graph, nid, ARC_PORT_INPUT, role);
    register_port(s, node_name, role, val, nid);
}

static void add_default_ports(ParseState* s, const char* node_name, ArcNodeId nid,
                               ArcNodeKind kind) {
    switch (kind) {
    case ARC_NODE_CONST_INT: case ARC_NODE_CONST_FLOAT:
    case ARC_NODE_CONST_BOOL: case ARC_NODE_CONST_NULL:
    case ARC_NODE_CONST_STRING: case ARC_NODE_VAR_REF:
        add_output_port(s, node_name, nid); break;
    case ARC_NODE_ADD: case ARC_NODE_SUB: case ARC_NODE_MUL:
    case ARC_NODE_DIV: case ARC_NODE_MOD: case ARC_NODE_EQ:
    case ARC_NODE_NEQ: case ARC_NODE_LT: case ARC_NODE_LE:
    case ARC_NODE_GT: case ARC_NODE_GE:
    case ARC_NODE_AND: case ARC_NODE_OR:
    case ARC_NODE_BIT_AND: case ARC_NODE_BIT_OR: case ARC_NODE_BIT_XOR:
    case ARC_NODE_SHL: case ARC_NODE_SHR:
    case ARC_NODE_INDEX_GET: case ARC_NODE_STR_INDEX:
        add_binary_ports(s, node_name, nid); break;
    case ARC_NODE_NEG: case ARC_NODE_NOT: case ARC_NODE_BIT_NOT:
    case ARC_NODE_CAST_I64: case ARC_NODE_CAST_F64: case ARC_NODE_CAST_STR:
    case ARC_NODE_STR_LEN: case ARC_NODE_LENGTH:
        add_unary_ports(s, node_name, nid); break;
    case ARC_NODE_LET: case ARC_NODE_ASSIGN:
    case ARC_NODE_PRINT: case ARC_NODE_ROOT_OUTPUT:
    case ARC_NODE_RETURN: case ARC_NODE_THROW:
    case ARC_NODE_IF: case ARC_NODE_WHILE:
        add_input_port(s, node_name, nid,
                       (kind == ARC_NODE_IF || kind == ARC_NODE_WHILE) ? "cond" : "value");
        break;
    case ARC_NODE_FUNC_CALL: case ARC_NODE_INTRINSIC_CALL:
        add_output_port(s, node_name, nid); break;
    case ARC_NODE_ARRAY_LITERAL: case ARC_NODE_MAP_LITERAL:
        add_output_port(s, node_name, nid); break;
    case ARC_NODE_CLOSURE:
        add_input_port(s, node_name, nid, "func");
        add_output_port(s, node_name, nid); break;
    case ARC_NODE_INDEX_SET: {
        add_input_port(s, node_name, nid, "container");
        add_input_port(s, node_name, nid, "key");
        add_input_port(s, node_name, nid, "value");
        break;
    }
    case ARC_NODE_STR_SLICE: {
        add_input_port(s, node_name, nid, "value");
        add_input_port(s, node_name, nid, "start");
        add_input_port(s, node_name, nid, "end");
        add_output_port(s, node_name, nid); break;
    }
    case ARC_NODE_RECORD_NEW:
        add_output_port(s, node_name, nid); break;
    case ARC_NODE_FIELD_GET:
        add_input_port(s, node_name, nid, "value");
        add_output_port(s, node_name, nid); break;
    case ARC_NODE_FIELD_SET:
        add_input_port(s, node_name, nid, "record");
        add_input_port(s, node_name, nid, "value");
        add_output_port(s, node_name, nid); break;
    case ARC_NODE_BREAK_IF:
        add_input_port(s, node_name, nid, "cond"); break;
    case ARC_NODE_THREAD_SPAWN:
        add_output_port(s, node_name, nid); break;
    case ARC_NODE_THREAD_JOIN:
        add_unary_ports(s, node_name, nid); break;
    case ARC_NODE_MUTEX_NEW: case ARC_NODE_CHAN_NEW:
        add_output_port(s, node_name, nid); break;
    case ARC_NODE_MUTEX_LOCK: case ARC_NODE_MUTEX_UNLOCK:
        add_input_port(s, node_name, nid, "value"); break;
    case ARC_NODE_CHAN_SEND:
        add_binary_ports(s, node_name, nid); break;
    case ARC_NODE_CHAN_RECV:
        add_unary_ports(s, node_name, nid); break;
    case ARC_NODE_TRY: case ARC_NODE_CYCLE: break;
    default: break;
    }
}

/* Parse ports=[...] */
static const char* parse_ports_spec(ParseState* s, const char* node_name, ArcNodeId nid,
                                     const char* p) {
    if (strncmp(p, "ports=[", 7) != 0) return p;
    p += 7;
    while (*p && *p != ']') {
        char role[32]; size_t ri = 0;
        while (*p && *p != ',' && *p != ']' && ri < 31) role[ri++] = *p++;
        role[ri] = '\0';
        if (*p == ',') p++;
        if (find_port(s, node_name, role) == ARC_INVALID_ID) {
            ArcPortDir dir = ARC_PORT_OUTPUT;
            if (strcmp(role, "lhs") == 0 || strcmp(role, "rhs") == 0 ||
                strcmp(role, "value") == 0 || strcmp(role, "cond") == 0 ||
                strcmp(role, "container") == 0 || strcmp(role, "key") == 0 ||
                strcmp(role, "start") == 0 || strcmp(role, "end") == 0 ||
                strcmp(role, "func") == 0 || strcmp(role, "record") == 0 ||
                strncmp(role, "arg", 3) == 0 || strncmp(role, "item", 4) == 0)
                dir = ARC_PORT_INPUT;
            ArcPortId pid = arc_graph_add_port(s->graph, nid, dir, role);
            register_port(s, node_name, role, pid, nid);
        }
    }
    if (*p == ']') p++;
    return p;
}

/* Parse cyclic=[...] */
static const char* parse_cyclic_spec(ParseState* s, const char* node_name, ArcNodeId nid,
                                      const char* p) {
    if (strncmp(p, "cyclic=[", 8) != 0) return p;
    p += 8;
    ArcPortId order[32]; uint32_t count = 0;
    while (*p && *p != ']' && count < 32) {
        char role[32]; size_t ri = 0;
        while (*p && *p != ',' && *p != ']' && ri < 31) role[ri++] = *p++;
        role[ri] = '\0';
        if (*p == ',') p++;
        ArcPortId pid = find_port(s, node_name, role);
        if (pid != ARC_INVALID_ID) order[count++] = pid;
    }
    if (*p == ']') p++;
    if (count > 0) arc_node_set_cyclic_order(s->graph, nid, order, count);
    return p;
}

/* Parse key=value attrs */
static const char* parse_node_attrs(ParseState* s, ArcNodeId nid, ArcNodeKind kind,
                                     const char* node_name, const char* p) {
    while (*p) {
        p = skip_ws(p);
        if (!*p || *p == '\n' || *p == '\r' || *p == '#') break;

        if (strncmp(p, "ports=", 6) == 0) { p = parse_ports_spec(s, node_name, nid, p); continue; }
        if (strncmp(p, "cyclic=", 7) == 0) { p = parse_cyclic_spec(s, node_name, nid, p); continue; }

        char key[64] = {0}, val[64] = {0};
        size_t ki = 0;
        while (*p && *p != '=' && !isspace((unsigned char)*p) && ki < 63) key[ki++] = *p++;
        key[ki] = '\0';
        if (*p == '=') {
            p++;
            size_t vi = 0;
            while (*p && !isspace((unsigned char)*p) && vi < 63) val[vi++] = *p++;
            val[vi] = '\0';
        }

        ArcNode* n = arc_graph_node(s->graph, nid);
        if (strcmp(key, "then_region") == 0) n->attr.branch.then_region = find_region(s, val);
        else if (strcmp(key, "else_region") == 0) n->attr.branch.else_region = find_region(s, val);
        else if (strcmp(key, "body_region") == 0) {
            if (kind == ARC_NODE_WHILE) n->attr.loop.body_region = find_region(s, val);
            else if (kind == ARC_NODE_FUNC_DEF) n->attr.func.body_region = find_region(s, val);
            else if (kind == ARC_NODE_CYCLE) n->attr.cycle.body_region = find_region(s, val);
        }
        else if (strcmp(key, "try_region") == 0) n->attr.try_catch.try_region = find_region(s, val);
        else if (strcmp(key, "catch_region") == 0) n->attr.try_catch.catch_region = find_region(s, val);
        else if (strcmp(key, "arity") == 0) {
            if (kind == ARC_NODE_FUNC_DEF) n->attr.func.arity = (uint8_t)atoi(val);
        }
        else if (strcmp(key, "count") == 0) n->attr.collection.count = (uint16_t)atoi(val);
        else if (strcmp(key, "id") == 0) n->attr.intrinsic.id = (uint16_t)atoi(val);
    }
    return p;
}

/* --- Parse "region" command --- */
static void parse_cmd_region(ParseState* s, const char* line) {
    char name[32], kind_str[32];
    line = read_token(line, name, sizeof(name));
    line = read_token(line, kind_str, sizeof(kind_str));

    ArcRegionKind kind = arc_fk_region_kind(kind_str);
    ArcRegionId parent = ARC_INVALID_ID;

    line = skip_ws(line);
    if (strncmp(line, "parent=", 7) == 0) {
        char pname[32]; line = read_token(line + 7, pname, sizeof(pname));
        parent = find_region(s, pname);
    }

    ArcRegionId rid = arc_graph_add_region(s->graph, kind, parent);
    if (s->region_count == 0) s->graph->root_region = rid;
    snprintf(s->regions[s->region_count].name, 32, "%s", name);
    s->regions[s->region_count].id = rid;
    s->region_count++;
}

/* set_node_attr extracted to fixture_kinds.c */

/* --- Parse "node" command --- */
static void parse_cmd_node(ParseState* s, const char* line) {
    char name[32], kind_str[64];
    line = read_token(line, name, sizeof(name));
    line = read_token(line, kind_str, sizeof(kind_str));

    char attr_buf[64];
    ArcNodeKind kind = arc_fk_parse_node_kind(kind_str, attr_buf, sizeof(attr_buf));

    char in_kw[16], region_name[32] = {0};
    line = read_token(line, in_kw, sizeof(in_kw));
    if (strcmp(in_kw, "in") == 0)
        line = read_token(line, region_name, sizeof(region_name));

    ArcRegionId rid = find_region(s, region_name);
    if (rid == ARC_INVALID_ID) { perr(s, "unknown region '%s'", region_name); return; }

    ArcNodeId nid = arc_graph_add_node(s->graph, kind, rid, 0);
    ArcNode* n = arc_graph_node(s->graph, nid);
    arc_fk_set_node_attr(n, kind, attr_buf);

    snprintf(s->nodes[s->node_count].name, 32, "%s", name);
    s->nodes[s->node_count].id = nid;
    s->node_count++;

    add_default_ports(s, name, nid, kind);
    parse_node_attrs(s, nid, kind, name, line);
}

/* --- Parse "edge" command --- */
static void parse_cmd_edge(ParseState* s, const char* line) {
    char eid_name[32], from_spec[64], arrow[16], to_spec[64];
    line = read_token(line, eid_name, sizeof(eid_name));
    line = read_token(line, from_spec, sizeof(from_spec));
    line = read_token(line, arrow, sizeof(arrow));
    line = read_token(line, to_spec, sizeof(to_spec));

    char from_node[64], from_role[64], to_node[64], to_role[64];
    char* dot;

    dot = strchr(from_spec, '.');
    if (!dot) { perr(s, "edge from missing '.'"); return; }
    *dot = '\0';
    snprintf(from_node, sizeof(from_node), "%s", from_spec);
    snprintf(from_role, sizeof(from_role), "%s", dot + 1);

    dot = strchr(to_spec, '.');
    if (!dot) { perr(s, "edge to missing '.'"); return; }
    *dot = '\0';
    snprintf(to_node, sizeof(to_node), "%s", to_spec);
    snprintf(to_role, sizeof(to_role), "%s", dot + 1);

    ArcPortId from_port = find_port(s, from_node, from_role);
    ArcPortId to_port = find_port(s, to_node, to_role);

    if (to_port == ARC_INVALID_ID &&
        (strncmp(to_role, "arg", 3) == 0 || strncmp(to_role, "item", 4) == 0)) {
        ArcNodeId tn = find_node(s, to_node);
        if (tn != ARC_INVALID_ID) {
            to_port = arc_graph_add_port(s->graph, tn, ARC_PORT_INPUT, to_role);
            register_port(s, to_node, to_role, to_port, tn);
        }
    }

    if (from_port == ARC_INVALID_ID) { perr(s, "unknown port %s.%s", from_node, from_role); return; }
    if (to_port == ARC_INVALID_ID) { perr(s, "unknown port %s.%s", to_node, to_role); return; }

    arc_graph_add_edge(s->graph, from_port, to_port);
}

/* --- Parse "root" command --- */
static void parse_cmd_root(ParseState* s, const char* line) {
    char spec[64];
    line = read_token(line, spec, sizeof(spec));
    char* dot = strchr(spec, '.');
    if (dot) {
        *dot = '\0';
        ArcNodeId nid = find_node(s, spec);
        if (nid != ARC_INVALID_ID) {
            ArcPortId src_port = find_port(s, spec, dot + 1);
            if (src_port != ARC_INVALID_ID) {
                ArcNode* src_node = arc_graph_node(s->graph, nid);
                ArcNodeId root_node = arc_graph_add_node(s->graph, ARC_NODE_ROOT_OUTPUT,
                    src_node->region, 0);
                ArcPortId root_in = arc_graph_add_port(s->graph, root_node, ARC_PORT_INPUT, "value");
                arc_graph_add_edge(s->graph, src_port, root_in);
                s->graph->output_node = root_node;
            }
        }
    } else {
        ArcNodeId nid = find_node(s, spec);
        if (nid != ARC_INVALID_ID) s->graph->output_node = nid;
    }
}

/* --- Architecture commands: sealed / capability / channel --- */

static ArcArchRegionSpec* find_arch_region(ArcArchSpec* a, ArcRegionId rid) {
    for (uint32_t i = 0; i < a->region_count; i++)
        if (a->regions[i].region == rid) return &a->regions[i];
    return NULL;
}

static void parse_cmd_sealed(ParseState* s, const char* line) {
    char rname[32]; read_token(line, rname, sizeof(rname));
    ArcRegionId rid = find_region(s, rname);
    if (rid == ARC_INVALID_ID) { perr(s, "sealed: unknown region '%s'", rname); return; }
    ArcArchRegionSpec* rs = find_arch_region(s->arch, rid);
    if (rs) { rs->sealed = true; return; }
    arc_arch_spec_add_sealed(s->arch, rid, ARC_EFFECT_PURE);
}

static void parse_cmd_capability(ParseState* s, const char* line) {
    char rname[32], ename[32];
    line = read_token(line, rname, sizeof(rname)); read_token(line, ename, sizeof(ename));
    ArcRegionId rid = find_region(s, rname);
    if (rid == ARC_INVALID_ID) { perr(s, "capability: unknown region '%s'", rname); return; }
    ArcEffectSet eff = arc_effect_parse(ename);
    if (eff == 0) { perr(s, "capability: unknown effect '%s'", ename); return; }
    ArcArchRegionSpec* rs = find_arch_region(s->arch, rid);
    if (rs) { rs->capabilities |= eff; return; }
    arc_arch_spec_add_sealed(s->arch, rid, eff);
}

static void parse_cmd_channel(ParseState* s, const char* line) {
    char fn[32], arrow[16], tn[32];
    line = read_token(line, fn, sizeof(fn));
    line = read_token(line, arrow, sizeof(arrow)); read_token(line, tn, sizeof(tn));
    ArcRegionId from = find_region(s, fn), to = find_region(s, tn);
    if (from == ARC_INVALID_ID) { perr(s, "channel: unknown region '%s'", fn); return; }
    if (to == ARC_INVALID_ID) { perr(s, "channel: unknown region '%s'", tn); return; }
    arc_arch_spec_add_channel(s->arch, from, to);
}

/* --- Geometry commands: circle / position (Experiment 2A) --- */

static void parse_cmd_circle(ParseState* s, const char* line) {
    char rname[32], cx_s[32], cy_s[32], r_s[32];
    line = read_token(line, rname, sizeof(rname));
    line = read_token(line, cx_s, sizeof(cx_s));
    line = read_token(line, cy_s, sizeof(cy_s));
    read_token(line, r_s, sizeof(r_s));
    ArcRegionId rid = find_region(s, rname);
    if (rid == ARC_INVALID_ID) { perr(s, "circle: unknown region '%s'", rname); return; }
    arc_geo_add_circle(s->geo, rid, atof(cx_s), atof(cy_s), atof(r_s));
}

static void parse_cmd_position(ParseState* s, const char* line) {
    char nname[32], x_s[32], y_s[32];
    line = read_token(line, nname, sizeof(nname));
    line = read_token(line, x_s, sizeof(x_s));
    read_token(line, y_s, sizeof(y_s));
    ArcNodeId nid = find_node(s, nname);
    if (nid == ARC_INVALID_ID) { perr(s, "position: unknown node '%s'", nname); return; }
    arc_geo_add_position(s->geo, nid, atof(x_s), atof(y_s));
}

static void parse_line(ParseState* s, const char* line) {
    if (s->had_error) return;
    line = skip_ws(line);
    if (!*line || *line == '#' || *line == ';') return;

    char cmd[32];
    line = read_token(line, cmd, sizeof(cmd));

    if (strcmp(cmd, "region") == 0)          parse_cmd_region(s, line);
    else if (strcmp(cmd, "node") == 0)       parse_cmd_node(s, line);
    else if (strcmp(cmd, "edge") == 0)       parse_cmd_edge(s, line);
    else if (strcmp(cmd, "root") == 0)       parse_cmd_root(s, line);
    else if (strcmp(cmd, "sealed") == 0)     { parse_cmd_sealed(s, line); s->has_arch = true; }
    else if (strcmp(cmd, "capability") == 0) { parse_cmd_capability(s, line); s->has_arch = true; }
    else if (strcmp(cmd, "channel") == 0)    { parse_cmd_channel(s, line); s->has_arch = true; }
    else if (strcmp(cmd, "circle") == 0)     { parse_cmd_circle(s, line); s->has_geo = true; }
    else if (strcmp(cmd, "position") == 0)   { parse_cmd_position(s, line); s->has_geo = true; }
    else                                     perr(s, "unknown command '%s'", cmd);
}

ArcFixtureResult arc_fixture_parse(const char* text) {
    ArcFixtureResult result;
    memset(&result, 0, sizeof(result));
    result.success = true;

    arc_graph_init(&result.graph);
    result.graph.owns_strings = true;
    arc_arch_spec_init(&result.arch);
    arc_geo_init(&result.geo);

    ParseState state = {0};
    state.graph = &result.graph;
    state.arch = &result.arch;
    state.geo = &result.geo;

    const char* p = text;
    while (*p) {
        char line[1024];
        size_t li = 0;
        while (*p && *p != '\n' && *p != '\r' && li < sizeof(line) - 1)
            line[li++] = *p++;
        line[li] = '\0';
        if (*p == '\r') p++;
        if (*p == '\n') p++;

        parse_line(&state, line);
        if (state.had_error) {
            result.success = false;
            snprintf(result.error, sizeof(result.error), "%s", state.error);
            return result;
        }
    }

    result.has_arch = state.has_arch;
    result.has_geo = state.has_geo;
    return result;
}

ArcFixtureResult arc_fixture_parse_file(const char* path) {
    ArcFixtureResult result;
    memset(&result, 0, sizeof(result));

    FILE* f = fopen(path, "r");
    if (!f) {
        result.success = false;
        snprintf(result.error, sizeof(result.error), "cannot open '%s'", path);
        return result;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = ARC_ALLOC(char, (size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);

    result = arc_fixture_parse(buf);
    ARC_FREE(buf);
    return result;
}
