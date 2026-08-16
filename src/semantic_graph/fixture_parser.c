#include "fixture_parser.h"
#include <stdarg.h>
#include <ctype.h>

#ifdef _MSC_VER
#define arc_strdup _strdup
#else
#define arc_strdup strdup
#endif

/* --- Internal parse state --- */

typedef struct {
    /* Maps user IDs (r0, n0, etc.) to internal IDs */
    struct { char name[32]; ArcRegionId id; } regions[256];
    uint32_t region_count;

    struct { char name[32]; ArcNodeId id; } nodes[256];
    uint32_t node_count;

    /* Port lookup: node_name.role -> port_id */
    struct { char node[32]; char role[32]; ArcPortId id; ArcNodeId owner; } ports[1024];
    uint32_t port_count;

    ArcGraph* graph;
    char error[256];
    bool had_error;
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
    strncpy(s->ports[s->port_count].node, node_name, 31);
    s->ports[s->port_count].node[31] = '\0';
    strncpy(s->ports[s->port_count].role, role, 31);
    s->ports[s->port_count].role[31] = '\0';
    s->ports[s->port_count].id = pid;
    s->ports[s->port_count].owner = owner;
    s->port_count++;
}

/* Skip whitespace */
static const char* skip_ws(const char* p) {
    while (*p && (*p == ' ' || *p == '\t')) p++;
    return p;
}

/* Read a token (word) into buf, return pointer after token */
static const char* read_token(const char* p, char* buf, size_t bufsz) {
    p = skip_ws(p);
    size_t i = 0;
    while (*p && !isspace((unsigned char)*p) && i < bufsz - 1) buf[i++] = *p++;
    buf[i] = '\0';
    return p;
}

/* Map kind string to ArcRegionKind */
static ArcRegionKind parse_region_kind(const char* s) {
    if (strcmp(s, "module") == 0 || strcmp(s, "scope") == 0) return ARC_REGION_MODULE;
    if (strcmp(s, "function") == 0) return ARC_REGION_FUNCTION;
    if (strcmp(s, "block") == 0) return ARC_REGION_BLOCK;
    if (strcmp(s, "then") == 0) return ARC_REGION_THEN;
    if (strcmp(s, "else") == 0) return ARC_REGION_ELSE;
    if (strcmp(s, "loop_body") == 0) return ARC_REGION_LOOP_BODY;
    return ARC_REGION_MODULE;
}

/* Map kind string to ArcNodeKind, extract attribute into attr_buf */
static ArcNodeKind parse_node_kind(const char* s, char* attr_buf, size_t attr_sz) {
    attr_buf[0] = '\0';
    /* Check for kind(attr) pattern */
    const char* paren = strchr(s, '(');
    char kind[64] = {0};
    if (paren) {
        size_t klen = (size_t)(paren - s);
        if (klen >= sizeof(kind)) klen = sizeof(kind) - 1;
        memcpy(kind, s, klen); kind[klen] = '\0';
        const char* end = strchr(paren + 1, ')');
        if (end) {
            size_t alen = (size_t)(end - paren - 1);
            if (alen >= attr_sz) alen = attr_sz - 1;
            memcpy(attr_buf, paren + 1, alen); attr_buf[alen] = '\0';
        }
    } else {
        strncpy(kind, s, sizeof(kind) - 1);
    }

    if (strcmp(kind, "const_int") == 0) return ARC_NODE_CONST_INT;
    if (strcmp(kind, "const_float") == 0) return ARC_NODE_CONST_FLOAT;
    if (strcmp(kind, "const_bool") == 0) return ARC_NODE_CONST_BOOL;
    if (strcmp(kind, "const_null") == 0) return ARC_NODE_CONST_NULL;
    if (strcmp(kind, "add") == 0) return ARC_NODE_ADD;
    if (strcmp(kind, "sub") == 0) return ARC_NODE_SUB;
    if (strcmp(kind, "mul") == 0) return ARC_NODE_MUL;
    if (strcmp(kind, "div") == 0) return ARC_NODE_DIV;
    if (strcmp(kind, "mod") == 0) return ARC_NODE_MOD;
    if (strcmp(kind, "neg") == 0) return ARC_NODE_NEG;
    if (strcmp(kind, "eq") == 0) return ARC_NODE_EQ;
    if (strcmp(kind, "neq") == 0) return ARC_NODE_NEQ;
    if (strcmp(kind, "lt") == 0) return ARC_NODE_LT;
    if (strcmp(kind, "le") == 0) return ARC_NODE_LE;
    if (strcmp(kind, "gt") == 0) return ARC_NODE_GT;
    if (strcmp(kind, "ge") == 0) return ARC_NODE_GE;
    if (strcmp(kind, "not") == 0) return ARC_NODE_NOT;
    if (strcmp(kind, "if") == 0) return ARC_NODE_IF;
    if (strcmp(kind, "while") == 0) return ARC_NODE_WHILE;
    if (strcmp(kind, "let") == 0) return ARC_NODE_LET;
    if (strcmp(kind, "var_ref") == 0) return ARC_NODE_VAR_REF;
    if (strcmp(kind, "assign") == 0) return ARC_NODE_ASSIGN;
    if (strcmp(kind, "func_def") == 0) return ARC_NODE_FUNC_DEF;
    if (strcmp(kind, "func_call") == 0) return ARC_NODE_FUNC_CALL;
    if (strcmp(kind, "param") == 0) return ARC_NODE_PARAM;
    if (strcmp(kind, "return") == 0) return ARC_NODE_RETURN;
    if (strcmp(kind, "print") == 0) return ARC_NODE_PRINT;
    if (strcmp(kind, "root_output") == 0) return ARC_NODE_ROOT_OUTPUT;
    if (strcmp(kind, "sequence") == 0) return ARC_NODE_SEQUENCE;
    return ARC_NODE_CONST_NULL; /* fallback */
}

/* Determine default ports for a node kind */
static void add_default_ports(ParseState* s, const char* node_name, ArcNodeId nid, ArcNodeKind kind) {
    ArcGraph* g = s->graph;
    switch (kind) {
    case ARC_NODE_CONST_INT: case ARC_NODE_CONST_FLOAT:
    case ARC_NODE_CONST_BOOL: case ARC_NODE_CONST_NULL:
    case ARC_NODE_VAR_REF: {
        ArcPortId out = arc_graph_add_port(g, nid, ARC_PORT_OUTPUT, "out");
        register_port(s, node_name, "out", out, nid);
        break;
    }
    case ARC_NODE_ADD: case ARC_NODE_SUB: case ARC_NODE_MUL:
    case ARC_NODE_DIV: case ARC_NODE_MOD: case ARC_NODE_EQ:
    case ARC_NODE_NEQ: case ARC_NODE_LT: case ARC_NODE_LE:
    case ARC_NODE_GT: case ARC_NODE_GE: {
        ArcPortId lhs = arc_graph_add_port(g, nid, ARC_PORT_INPUT, "lhs");
        ArcPortId rhs = arc_graph_add_port(g, nid, ARC_PORT_INPUT, "rhs");
        ArcPortId out = arc_graph_add_port(g, nid, ARC_PORT_OUTPUT, "out");
        register_port(s, node_name, "lhs", lhs, nid);
        register_port(s, node_name, "rhs", rhs, nid);
        register_port(s, node_name, "out", out, nid);
        ArcPortId order[] = { lhs, rhs, out };
        arc_node_set_cyclic_order(g, nid, order, 3);
        break;
    }
    case ARC_NODE_NEG: case ARC_NODE_NOT: {
        ArcPortId in = arc_graph_add_port(g, nid, ARC_PORT_INPUT, "value");
        ArcPortId out = arc_graph_add_port(g, nid, ARC_PORT_OUTPUT, "out");
        register_port(s, node_name, "value", in, nid);
        register_port(s, node_name, "out", out, nid);
        break;
    }
    case ARC_NODE_LET: case ARC_NODE_ASSIGN: {
        ArcPortId val = arc_graph_add_port(g, nid, ARC_PORT_INPUT, "value");
        register_port(s, node_name, "value", val, nid);
        break;
    }
    case ARC_NODE_PRINT: case ARC_NODE_ROOT_OUTPUT: {
        ArcPortId val = arc_graph_add_port(g, nid, ARC_PORT_INPUT, "value");
        register_port(s, node_name, "value", val, nid);
        break;
    }
    case ARC_NODE_RETURN: {
        ArcPortId val = arc_graph_add_port(g, nid, ARC_PORT_INPUT, "value");
        register_port(s, node_name, "value", val, nid);
        break;
    }
    case ARC_NODE_IF: {
        ArcPortId cond = arc_graph_add_port(g, nid, ARC_PORT_INPUT, "cond");
        register_port(s, node_name, "cond", cond, nid);
        break;
    }
    case ARC_NODE_WHILE: {
        ArcPortId cond = arc_graph_add_port(g, nid, ARC_PORT_INPUT, "cond");
        register_port(s, node_name, "cond", cond, nid);
        break;
    }
    case ARC_NODE_FUNC_CALL: {
        ArcPortId out = arc_graph_add_port(g, nid, ARC_PORT_OUTPUT, "out");
        register_port(s, node_name, "out", out, nid);
        break;
    }
    default: break;
    }
}

/* Parse a ports=[...] specification and add custom input ports */
static const char* parse_ports_spec(ParseState* s, const char* node_name, ArcNodeId nid,
                                     const char* p) {
    if (strncmp(p, "ports=[", 7) != 0) return p;
    p += 7;
    while (*p && *p != ']') {
        char role[32]; size_t ri = 0;
        while (*p && *p != ',' && *p != ']' && ri < 31) role[ri++] = *p++;
        role[ri] = '\0';
        if (*p == ',') p++;
        /* Only add if not already registered */
        if (find_port(s, node_name, role) == ARC_INVALID_ID) {
            ArcPortDir dir = ARC_PORT_OUTPUT;
            if (strcmp(role, "lhs") == 0 || strcmp(role, "rhs") == 0 ||
                strcmp(role, "value") == 0 || strcmp(role, "cond") == 0 ||
                strncmp(role, "arg", 3) == 0)
                dir = ARC_PORT_INPUT;
            ArcPortId pid = arc_graph_add_port(s->graph, nid, dir, role);
            register_port(s, node_name, role, pid, nid);
        }
    }
    if (*p == ']') p++;
    return p;
}

/* Parse a cyclic=[...] specification */
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

/* Parse node attribute specifications like then_region=r1, body_region=r2, arity=1 */
static const char* parse_node_attrs(ParseState* s, ArcNodeId nid, ArcNodeKind kind,
                                     const char* node_name, const char* p) {
    while (*p) {
        p = skip_ws(p);
        if (!*p || *p == '\n' || *p == '\r' || *p == '#') break;

        if (strncmp(p, "ports=", 6) == 0) { p = parse_ports_spec(s, node_name, nid, p); continue; }
        if (strncmp(p, "cyclic=", 7) == 0) { p = parse_cyclic_spec(s, node_name, nid, p); continue; }

        /* key=value attrs */
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
        }
        else if (strcmp(key, "arity") == 0) {
            if (kind == ARC_NODE_FUNC_DEF) n->attr.func.arity = (uint8_t)atoi(val);
        }
    }
    return p;
}

/* Parse one line */
static void parse_line(ParseState* s, const char* line) {
    if (s->had_error) return;
    line = skip_ws(line);
    if (!*line || *line == '#' || *line == ';') return; /* comment/empty */

    char cmd[32];
    line = read_token(line, cmd, sizeof(cmd));

    if (strcmp(cmd, "region") == 0) {
        char name[32], kind_str[32];
        line = read_token(line, name, sizeof(name));
        line = read_token(line, kind_str, sizeof(kind_str));

        ArcRegionKind kind = parse_region_kind(kind_str);
        ArcRegionId parent = ARC_INVALID_ID;

        /* Check for parent=<id> */
        line = skip_ws(line);
        if (strncmp(line, "parent=", 7) == 0) {
            char pname[32]; line = read_token(line + 7, pname, sizeof(pname));
            parent = find_region(s, pname);
        }

        ArcRegionId rid = arc_graph_add_region(s->graph, kind, parent);
        if (s->region_count == 0) s->graph->root_region = rid;
        strncpy(s->regions[s->region_count].name, name, 31);
        s->regions[s->region_count].name[31] = '\0';
        s->regions[s->region_count].id = rid;
        s->region_count++;

    } else if (strcmp(cmd, "node") == 0) {
        char name[32], kind_str[64];
        line = read_token(line, name, sizeof(name));
        line = read_token(line, kind_str, sizeof(kind_str));

        char attr_buf[64];
        ArcNodeKind kind = parse_node_kind(kind_str, attr_buf, sizeof(attr_buf));

        /* Expect "in <region>" */
        char in_kw[16], region_name[32] = {0};
        line = read_token(line, in_kw, sizeof(in_kw));
        if (strcmp(in_kw, "in") == 0) {
            line = read_token(line, region_name, sizeof(region_name));
        }

        ArcRegionId rid = find_region(s, region_name);
        if (rid == ARC_INVALID_ID) { perr(s, "unknown region '%s'", region_name); return; }

        ArcNodeId nid = arc_graph_add_node(s->graph, kind, rid, 0);

        /* Set attribute based on kind */
        ArcNode* n = arc_graph_node(s->graph, nid);
        switch (kind) {
        case ARC_NODE_CONST_INT: n->attr.int_value = atoll(attr_buf); break;
        case ARC_NODE_CONST_FLOAT: n->attr.float_value = atof(attr_buf); break;
        case ARC_NODE_CONST_BOOL: n->attr.bool_value = (strcmp(attr_buf, "true") == 0); break;
        case ARC_NODE_LET: case ARC_NODE_VAR_REF: case ARC_NODE_ASSIGN:
        case ARC_NODE_PARAM:
            n->attr.name = arc_strdup(attr_buf); break;
        case ARC_NODE_FUNC_DEF:
            n->attr.func.name = arc_strdup(attr_buf);
            n->attr.func.arity = 0;
            n->attr.func.body_region = ARC_INVALID_ID;
            break;
        case ARC_NODE_FUNC_CALL:
            n->attr.name = arc_strdup(attr_buf); break;
        default: break;
        }

        strncpy(s->nodes[s->node_count].name, name, 31);
        s->nodes[s->node_count].name[31] = '\0';
        s->nodes[s->node_count].id = nid;
        s->node_count++;

        /* Add default ports for this node kind */
        add_default_ports(s, name, nid, kind);

        /* Parse remaining attributes on the line (ports=, cyclic=, etc.) */
        parse_node_attrs(s, nid, kind, name, line);

    } else if (strcmp(cmd, "edge") == 0) {
        char eid_name[32], from_spec[64], arrow[16], to_spec[64];
        line = read_token(line, eid_name, sizeof(eid_name));
        line = read_token(line, from_spec, sizeof(from_spec));
        line = read_token(line, arrow, sizeof(arrow)); /* "->" */
        line = read_token(line, to_spec, sizeof(to_spec));

        /* Parse node.role */
        char from_node[32], from_role[32], to_node[32], to_role[32];
        char* dot;

        dot = strchr(from_spec, '.');
        if (!dot) { perr(s, "edge from missing '.'"); return; }
        *dot = '\0';
        strncpy(from_node, from_spec, 31); from_node[31] = '\0';
        strncpy(from_role, dot + 1, 31); from_role[31] = '\0';

        dot = strchr(to_spec, '.');
        if (!dot) { perr(s, "edge to missing '.'"); return; }
        *dot = '\0';
        strncpy(to_node, to_spec, 31); to_node[31] = '\0';
        strncpy(to_role, dot + 1, 31); to_role[31] = '\0';

        ArcPortId from_port = find_port(s, from_node, from_role);
        ArcPortId to_port = find_port(s, to_node, to_role);

        /* Auto-create arg ports for func_call */
        if (to_port == ARC_INVALID_ID && strncmp(to_role, "arg", 3) == 0) {
            ArcNodeId tn = find_node(s, to_node);
            if (tn != ARC_INVALID_ID) {
                to_port = arc_graph_add_port(s->graph, tn, ARC_PORT_INPUT, to_role);
                register_port(s, to_node, to_role, to_port, tn);
            }
        }

        if (from_port == ARC_INVALID_ID) { perr(s, "unknown port %s.%s", from_node, from_role); return; }
        if (to_port == ARC_INVALID_ID) { perr(s, "unknown port %s.%s", to_node, to_role); return; }

        arc_graph_add_edge(s->graph, from_port, to_port);

    } else if (strcmp(cmd, "root") == 0) {
        char spec[64];
        line = read_token(line, spec, sizeof(spec));
        char* dot = strchr(spec, '.');
        if (dot) {
            *dot = '\0';
            ArcNodeId nid = find_node(s, spec);
            if (nid != ARC_INVALID_ID) {
                /* Find or create ROOT_OUTPUT that connects to this port */
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
    } else {
        perr(s, "unknown command '%s'", cmd);
    }
}

ArcFixtureResult arc_fixture_parse(const char* text) {
    ArcFixtureResult result;
    memset(&result, 0, sizeof(result));
    result.success = true;

    arc_graph_init(&result.graph);

    ParseState state = {0};
    state.graph = &result.graph;

    /* Process line by line */
    const char* p = text;
    while (*p) {
        /* Extract one line */
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
            strncpy(result.error, state.error, sizeof(result.error) - 1);
            return result;
        }
    }

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
