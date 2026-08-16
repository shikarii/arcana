#ifndef ARCANA_INTERPRETER_H
#define ARCANA_INTERPRETER_H

#include "../semantic_graph/semantic_graph.h"
#include "../vm/value.h"

/*
 * Differential Reference Interpreter — directly evaluates an ArcGraph.
 *
 * This provides executable reference semantics against which the
 * compiler -> bytecode -> VM pipeline can be compared.
 */

#define ARC_INTERP_STACK_MAX  256
#define ARC_INTERP_VARS_MAX   256
#define ARC_INTERP_FRAMES_MAX 64

typedef struct {
    ArcValue   result;
    char       error[256];
    bool       success;
    FILE*      output;    /* for print, NULL = stdout */
} ArcInterpResult;

/* Evaluate a semantic graph directly, returning the output value */
ArcInterpResult arc_interpret(const ArcGraph* graph);

#endif /* ARCANA_INTERPRETER_H */
