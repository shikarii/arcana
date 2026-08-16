#ifndef ARCANA_TYPECHECK_H
#define ARCANA_TYPECHECK_H

#include "../semantic_graph/semantic_graph.h"
#include "../compiler/diagnostics.h"

/*
 * Arcana Type Checker — validates type constraints on a semantic graph
 * before bytecode compilation.
 *
 * This is a lightweight inference pass, not a full Hindley-Milner system.
 * It walks expression trees and checks:
 *   - Arithmetic operands are numeric (i64/f64)
 *   - Bitwise operands are i64
 *   - Comparison operands are compatible
 *   - Logical operands produce booleans
 *   - Cast inputs are valid
 *   - Container operations have correct operand types
 *
 * Types:
 *   ARC_TYPE_UNKNOWN  — not yet inferred
 *   ARC_TYPE_NULL     — null literal
 *   ARC_TYPE_BOOL     — boolean
 *   ARC_TYPE_I64      — 64-bit integer
 *   ARC_TYPE_F64      — 64-bit float
 *   ARC_TYPE_STRING   — string
 *   ARC_TYPE_ARRAY    — array
 *   ARC_TYPE_MAP      — map
 *   ARC_TYPE_CLOSURE  — closure/function
 *   ARC_TYPE_ANY      — dynamic/unresolvable
 */

typedef enum {
    ARC_TYPE_UNKNOWN,
    ARC_TYPE_NULL,
    ARC_TYPE_BOOL,
    ARC_TYPE_I64,
    ARC_TYPE_F64,
    ARC_TYPE_STRING,
    ARC_TYPE_ARRAY,
    ARC_TYPE_MAP,
    ARC_TYPE_CLOSURE,
    ARC_TYPE_ANY,
} ArcType;

/* Result of type checking */
typedef struct {
    ArcDiagList  diagnostics;
    ArcType*     node_types;     /* inferred type per node (indexed by node ID) */
    uint32_t     node_count;
    bool         success;
} ArcTypeCheckResult;

/* Run type checking on a graph */
ArcTypeCheckResult arc_typecheck(const ArcGraph* graph);
void arc_typecheck_result_free(ArcTypeCheckResult* r);

/* Get the name of a type (for diagnostics) */
const char* arc_type_name(ArcType t);

#endif /* ARCANA_TYPECHECK_H */
