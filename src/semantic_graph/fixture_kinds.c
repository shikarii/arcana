#include "fixture_kinds.h"
#include <stdlib.h>
#include <string.h>

/* --- Region kind from text --- */

ArcRegionKind arc_fk_region_kind(const char* s) {
    if (strcmp(s, "module") == 0 || strcmp(s, "scope") == 0) return ARC_REGION_MODULE;
    if (strcmp(s, "function") == 0) return ARC_REGION_FUNCTION;
    if (strcmp(s, "block") == 0) return ARC_REGION_BLOCK;
    if (strcmp(s, "then") == 0) return ARC_REGION_THEN;
    if (strcmp(s, "else") == 0) return ARC_REGION_ELSE;
    if (strcmp(s, "loop_body") == 0) return ARC_REGION_LOOP_BODY;
    if (strcmp(s, "cycle") == 0) return ARC_REGION_CYCLE;
    return ARC_REGION_MODULE;
}

/* --- Table-driven node kind lookup --- */

static const struct { const char* name; ArcNodeKind kind; } kind_table[] = {
    {"const_int",ARC_NODE_CONST_INT}, {"const_float",ARC_NODE_CONST_FLOAT},
    {"const_bool",ARC_NODE_CONST_BOOL}, {"const_null",ARC_NODE_CONST_NULL},
    {"const_string",ARC_NODE_CONST_STRING},
    {"add",ARC_NODE_ADD}, {"sub",ARC_NODE_SUB}, {"mul",ARC_NODE_MUL},
    {"div",ARC_NODE_DIV}, {"mod",ARC_NODE_MOD}, {"neg",ARC_NODE_NEG},
    {"eq",ARC_NODE_EQ}, {"neq",ARC_NODE_NEQ}, {"lt",ARC_NODE_LT},
    {"le",ARC_NODE_LE}, {"gt",ARC_NODE_GT}, {"ge",ARC_NODE_GE},
    {"not",ARC_NODE_NOT}, {"and",ARC_NODE_AND}, {"or",ARC_NODE_OR},
    {"if",ARC_NODE_IF}, {"while",ARC_NODE_WHILE},
    {"let",ARC_NODE_LET}, {"var_ref",ARC_NODE_VAR_REF}, {"assign",ARC_NODE_ASSIGN},
    {"func_def",ARC_NODE_FUNC_DEF}, {"func_call",ARC_NODE_FUNC_CALL},
    {"param",ARC_NODE_PARAM}, {"return",ARC_NODE_RETURN},
    {"print",ARC_NODE_PRINT}, {"root_output",ARC_NODE_ROOT_OUTPUT},
    {"sequence",ARC_NODE_SEQUENCE}, {"cycle",ARC_NODE_CYCLE}, {"break_if",ARC_NODE_BREAK_IF},
    {"bit_and",ARC_NODE_BIT_AND}, {"bit_or",ARC_NODE_BIT_OR},
    {"bit_xor",ARC_NODE_BIT_XOR}, {"bit_not",ARC_NODE_BIT_NOT},
    {"shl",ARC_NODE_SHL}, {"shr",ARC_NODE_SHR},
    {"cast_i64",ARC_NODE_CAST_I64}, {"cast_f64",ARC_NODE_CAST_F64},
    {"cast_str",ARC_NODE_CAST_STR},
    {"array_literal",ARC_NODE_ARRAY_LITERAL}, {"map_literal",ARC_NODE_MAP_LITERAL},
    {"index_get",ARC_NODE_INDEX_GET}, {"index_set",ARC_NODE_INDEX_SET},
    {"str_len",ARC_NODE_STR_LEN}, {"str_slice",ARC_NODE_STR_SLICE},
    {"str_index",ARC_NODE_STR_INDEX}, {"length",ARC_NODE_LENGTH},
    {"try",ARC_NODE_TRY}, {"throw",ARC_NODE_THROW},
    {"closure",ARC_NODE_CLOSURE}, {"intrinsic_call",ARC_NODE_INTRINSIC_CALL},
    {"record_new",ARC_NODE_RECORD_NEW}, {"field_get",ARC_NODE_FIELD_GET},
    {"field_set",ARC_NODE_FIELD_SET},
    {"thread_spawn",ARC_NODE_THREAD_SPAWN}, {"thread_join",ARC_NODE_THREAD_JOIN},
    {"mutex_new",ARC_NODE_MUTEX_NEW}, {"mutex_lock",ARC_NODE_MUTEX_LOCK},
    {"mutex_unlock",ARC_NODE_MUTEX_UNLOCK}, {"chan_new",ARC_NODE_CHAN_NEW},
    {"chan_send",ARC_NODE_CHAN_SEND}, {"chan_recv",ARC_NODE_CHAN_RECV},
    {NULL, ARC_NODE_CONST_NULL}
};

ArcNodeKind arc_fk_lookup_kind(const char* name) {
    for (int i = 0; kind_table[i].name; i++)
        if (strcmp(kind_table[i].name, name) == 0) return kind_table[i].kind;
    return ARC_NODE_CONST_NULL;
}

ArcNodeKind arc_fk_parse_node_kind(const char* s, char* attr_buf, size_t attr_sz) {
    attr_buf[0] = '\0';
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
        snprintf(kind, sizeof(kind), "%s", s);
    }
    return arc_fk_lookup_kind(kind);
}

/* --- Set node attributes from parsed kind + attribute string --- */

void arc_fk_set_node_attr(ArcNode* n, ArcNodeKind kind, const char* attr_buf) {
    switch (kind) {
    case ARC_NODE_CONST_INT: n->attr.int_value = atoll(attr_buf); break;
    case ARC_NODE_CONST_FLOAT: n->attr.float_value = atof(attr_buf); break;
    case ARC_NODE_CONST_BOOL: n->attr.bool_value = (strcmp(attr_buf, "true") == 0); break;
    case ARC_NODE_LET: case ARC_NODE_VAR_REF: case ARC_NODE_ASSIGN:
    case ARC_NODE_PARAM:
        n->attr.name = arc_strdup(attr_buf); break;
    case ARC_NODE_IF:
        n->attr.branch.then_region = ARC_INVALID_ID;
        n->attr.branch.else_region = ARC_INVALID_ID;
        break;
    case ARC_NODE_FUNC_DEF:
        n->attr.func.name = arc_strdup(attr_buf);
        n->attr.func.arity = 0;
        n->attr.func.body_region = ARC_INVALID_ID;
        break;
    case ARC_NODE_FUNC_CALL: case ARC_NODE_THREAD_SPAWN:
    case ARC_NODE_RECORD_NEW: case ARC_NODE_FIELD_GET: case ARC_NODE_FIELD_SET:
        n->attr.name = arc_strdup(attr_buf); break;
    case ARC_NODE_CHAN_NEW:
        n->attr.collection.count = attr_buf[0] ? (uint16_t)atoi(attr_buf) : 1; break;
    case ARC_NODE_CONST_STRING:
        n->attr.string_value.data = arc_strdup(attr_buf);
        n->attr.string_value.len = (uint32_t)strlen(attr_buf); break;
    case ARC_NODE_ARRAY_LITERAL: case ARC_NODE_MAP_LITERAL:
        n->attr.collection.count = attr_buf[0] ? (uint16_t)atoi(attr_buf) : 0; break;
    case ARC_NODE_INTRINSIC_CALL:
        n->attr.intrinsic.id = attr_buf[0] ? (uint16_t)atoi(attr_buf) : 0; break;
    case ARC_NODE_TRY:
        n->attr.try_catch.try_region = ARC_INVALID_ID;
        n->attr.try_catch.catch_region = ARC_INVALID_ID; break;
    default: break;
    }
}
