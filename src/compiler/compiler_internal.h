#ifndef ARCANA_COMPILER_INTERNAL_H
#define ARCANA_COMPILER_INTERNAL_H

#include "compiler.h"
#include "../bytecode/opcodes.h"
#include <stdarg.h>
#include <string.h>

/* --- Compiler state --- */
typedef struct { const char* name; uint16_t slot; } LocalVar;
typedef struct {
    const ArcGraph*   graph;
    ArcBytecodeImage  image;
    ArcBuf            code;
    ArcCompileError*  errors;
    int               error_count, error_cap;
    bool              had_error;
    LocalVar locals[256];
    uint16_t local_count, current_func;
    int      stack_depth, max_stack;
    uint16_t debug_func_idx;
    /* Closure capture analysis */
    LocalVar enclosing[256];
    uint16_t enclosing_count;
    struct { const char* name; bool is_local; uint16_t index; } upvals[64];
    uint8_t  upval_count;
} Compiler;

/* --- Error reporting --- */
static inline void cerr(Compiler* c, const char* fmt, ...) {
    if (c->error_count >= c->error_cap) {
        c->error_cap = c->error_cap < 16 ? 16 : c->error_cap * 2;
        c->errors = ARC_REALLOC(c->errors, ArcCompileError, c->error_cap);
    }
    va_list ap; va_start(ap, fmt);
    vsnprintf(c->errors[c->error_count].message,
              sizeof(c->errors[c->error_count].message), fmt, ap);
    va_end(ap);
    c->error_count++;
    c->had_error = true;
}

/* --- Code emission helpers --- */
static inline void emit_byte(Compiler* c, uint8_t b) { arc_buf_push(&c->code, b); }
static inline void emit_u16(Compiler* c, uint16_t v) { arc_buf_push_u16(&c->code, v); }
static inline void emit_i32(Compiler* c, int32_t v)  { arc_buf_push_i32(&c->code, v); }

static inline void emit_op(Compiler* c, uint8_t op) {
    emit_byte(c, op);
    int pops = arc_op_pops(op), pushes = arc_op_pushes(op);
    if (pops > 0) c->stack_depth -= pops;
    if (pushes > 0) c->stack_depth += pushes;
    if (c->stack_depth > c->max_stack) c->max_stack = c->stack_depth;
}

static inline void emit_const(Compiler* c, uint16_t idx) { emit_op(c, OP_CONST); emit_u16(c, idx); }

static inline uint32_t emit_jump(Compiler* c, uint8_t op) {
    emit_op(c, op);
    uint32_t pp = (uint32_t)c->code.len;
    emit_i32(c, 0);
    return pp;
}

static inline void patch_jump(Compiler* c, uint32_t pp) {
    uint32_t t = (uint32_t)c->code.len;
    int32_t off = (int32_t)t - (int32_t)(pp + 4);
    c->code.data[pp]     = (uint8_t)(((uint32_t)off) & 0xFF);
    c->code.data[pp + 1] = (uint8_t)((((uint32_t)off) >> 8) & 0xFF);
    c->code.data[pp + 2] = (uint8_t)((((uint32_t)off) >> 16) & 0xFF);
    c->code.data[pp + 3] = (uint8_t)((((uint32_t)off) >> 24) & 0xFF);
}

static inline void track_stack(Compiler* c, int pops, int pushes) {
    c->stack_depth -= pops;
    c->stack_depth += pushes;
    if (c->stack_depth > c->max_stack) c->max_stack = c->stack_depth;
}

/* --- Local variable resolution --- */
static inline int find_local(Compiler* c, const char* name) {
    for (int i = c->local_count - 1; i >= 0; i--)
        if (strcmp(c->locals[i].name, name) == 0) return c->locals[i].slot;
    return -1;
}

static inline uint16_t add_local(Compiler* c, const char* name) {
    uint16_t slot = c->local_count;
    c->locals[c->local_count].name = name;
    c->locals[c->local_count].slot = slot;
    c->local_count++;
    return slot;
}

/* --- Upvalue resolution (closure captures) --- */
static inline int find_upvalue(Compiler* c, const char* name) {
    for (int i = 0; i < c->upval_count; i++)
        if (strcmp(c->upvals[i].name, name) == 0) return i;
    for (int i = (int)c->enclosing_count - 1; i >= 0; i--)
        if (strcmp(c->enclosing[i].name, name) == 0) {
            int ui = c->upval_count++;
            c->upvals[ui].name = name;
            c->upvals[ui].is_local = true;
            c->upvals[ui].index = c->enclosing[i].slot;
            return ui;
        }
    return -1;
}

/* --- Graph traversal helpers --- */
static inline ArcNodeId find_source_node(const ArcGraph* g, ArcPortId input_port) {
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].to == input_port)
            return g->ports[g->edges[i].from].owner;
    }
    return ARC_INVALID_ID;
}

static inline ArcPortId find_port_by_role(const ArcGraph* g, ArcNodeId nid,
                                          const char* role, ArcPortDir dir) {
    const ArcNode* n = &g->nodes[nid];
    for (uint32_t i = 0; i < n->port_count; i++) {
        const ArcPort* p = &g->ports[n->ports[i]];
        if (p->dir == dir && p->role && strcmp(p->role, role) == 0) return p->id;
    }
    return ARC_INVALID_ID;
}

/* --- Cross-file function declarations --- */

/* Defined in compile_nodes.c, called by compiler.c */
void compile_node(Compiler* c, ArcNodeId node_id);
void compile_function(Compiler* c, ArcNodeId func_node_id);

/* Defined in semantic.c (arcana_semantic lib), shared with compiler */
bool is_expr_node(ArcNodeKind k);

#endif /* ARCANA_COMPILER_INTERNAL_H */
