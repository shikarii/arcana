#ifndef ARCANA_SEMANTIC_INTERNAL_H
#define ARCANA_SEMANTIC_INTERNAL_H

/*
 * semantic_internal.h -- shared types and declarations for the semantic
 * lowering pass, split across semantic.c and semantic_expr.c.
 */

#include "semantic.h"
#include "scope.h"
#include <stdarg.h>

/* ===================================================================
 * Lowering context — carries mutable state through the walk.
 * =================================================================== */

typedef struct {
    const ArcGraph* graph;
    HirModule*      module;
    ArcDiagList*    diags;
    ArcScope        scope;
    bool            had_error;

    struct { const char* name; uint16_t idx; } func_table[256];
    uint16_t func_table_count;
} SemCtx;

/* --- Diagnostics --- */
void sem_error(SemCtx* ctx, ArcDiagCode code,
               ArcNodeId node_id, ArcElementId elem,
               const char* fmt, ...);

/* --- Function table --- */
void register_func(SemCtx* ctx, const char* name, uint16_t idx);
int  find_func(const SemCtx* ctx, const char* name);

/* --- Graph navigation --- */
ArcNodeId  find_source_node(const ArcGraph* g, ArcPortId input_port);
ArcPortId  find_port_by_role(const ArcGraph* g, ArcNodeId node_id,
                              const char* role, ArcPortDir dir);
bool       is_expr_node(ArcNodeKind k);
HirExprKind node_kind_to_binary_expr(ArcNodeKind k);

/* --- Lowering --- */
HirExpr* lower_expr(SemCtx* ctx, ArcNodeId node_id);
HirExpr* lower_value_from_port(SemCtx* ctx, ArcNodeId node_id,
                                const char* role, const char* alt_role);
void     lower_stmt(SemCtx* ctx, ArcNodeId node_id, HirBlock* block);
void     lower_region_stmts(SemCtx* ctx, ArcRegionId region_id, HirBlock* block);

#endif
