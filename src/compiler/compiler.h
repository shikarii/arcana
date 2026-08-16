#ifndef ARCANA_COMPILER_H
#define ARCANA_COMPILER_H

#include "../semantic_graph/semantic_graph.h"
#include "../bytecode/format.h"

typedef struct {
    char message[256];
} ArcCompileError;

typedef struct {
    ArcBytecodeImage image;
    ArcCompileError* errors;
    int              error_count;
    int              error_cap;
    bool             success;
} ArcCompileResult;

/* Compile a semantic graph into a bytecode image.
 * This performs the full pipeline: graph -> HIR -> MIR -> bytecode.
 * For v0, HIR/MIR are implicit (single-pass lowering). */
ArcCompileResult arc_compile(const ArcGraph* graph);

void arc_compile_result_free(ArcCompileResult* r);

#endif /* ARCANA_COMPILER_H */
