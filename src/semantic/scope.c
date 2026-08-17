/*
 * scope.c -- Lexical scope resolution implementation.
 *
 * Manages a stack of scopes, each mapping variable names to local slot
 * indices.  When a scope is popped, its bindings become invisible but
 * slots are reclaimed.
 */

#include "scope.h"
#include <string.h>
#include <assert.h>

void arc_scope_init(ArcScope* s) {
    s->local_count = 0;
    s->scope_depth = 0;
}

void arc_scope_push(ArcScope* s) {
    assert(s->scope_depth < SCOPE_MAX_DEPTH);
    s->scope_starts[s->scope_depth++] = s->local_count;
}

void arc_scope_pop(ArcScope* s) {
    assert(s->scope_depth > 0);
    s->local_count = s->scope_starts[--s->scope_depth];
}

int arc_scope_find(const ArcScope* s, const char* name) {
    for (int i = (int)s->local_count - 1; i >= 0; i--) {
        if (strcmp(s->locals[i].name, name) == 0)
            return (int)s->locals[i].slot;
    }
    return -1;
}

uint16_t arc_scope_add(ArcScope* s, const char* name) {
    assert(s->local_count < SCOPE_MAX_LOCALS);
    uint16_t slot = s->local_count;
    s->locals[s->local_count].name = name;
    s->locals[s->local_count].slot = slot;
    s->local_count++;
    return slot;
}
