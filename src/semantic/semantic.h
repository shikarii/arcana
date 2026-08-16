#ifndef ARCANA_SEMANTIC_H
#define ARCANA_SEMANTIC_H

#include "../semantic_graph/semantic_graph.h"
#include "../hir/hir.h"
#include "../compiler/diagnostics.h"

/* Result of semantic analysis: graph -> HIR */
typedef struct {
    HirModule    module;
    ArcDiagList  diagnostics;
    bool         success;
} ArcSemanticResult;

/* Lower an ArcGraph into HIR with symbol/scope resolution */
ArcSemanticResult arc_semantic_lower(const ArcGraph* graph);
void arc_semantic_result_free(ArcSemanticResult* r);

#endif
