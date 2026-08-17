#ifndef ARCANA_SCOPE_H
#define ARCANA_SCOPE_H

/*
 * scope.h -- Lexical scope resolution for the semantic analysis pass.
 *
 * Maps variable names to local slot indices within nested scopes.
 * Used by the semantic lowering pass to resolve LET, VAR_REF, and ASSIGN
 * nodes to concrete slot numbers before HIR construction.
 */

#include "../common/arcana_common.h"

#define SCOPE_MAX_LOCALS 256
#define SCOPE_MAX_DEPTH   64

typedef struct {
    const char* name;
    uint16_t    slot;
} ScopeLocal;

typedef struct {
    ScopeLocal locals[SCOPE_MAX_LOCALS];
    uint16_t   local_count;
    uint16_t   scope_starts[SCOPE_MAX_DEPTH];
    uint16_t   scope_depth;
} ArcScope;

void     arc_scope_init(ArcScope* s);
void     arc_scope_push(ArcScope* s);
void     arc_scope_pop(ArcScope* s);
int      arc_scope_find(const ArcScope* s, const char* name);
uint16_t arc_scope_add(ArcScope* s, const char* name);

#endif /* ARCANA_SCOPE_H */
