#ifndef ARCANA_DIAGNOSTICS_H
#define ARCANA_DIAGNOSTICS_H

#include "../common/arcana_common.h"

/*
 * Structured diagnostics for the Arcana compiler.
 *
 * Each diagnostic includes severity, error code, message,
 * primary source reference, related refs, and optional notes.
 */

typedef enum {
    ARC_DIAG_ERROR,
    ARC_DIAG_WARNING,
    ARC_DIAG_NOTE,
} ArcDiagSeverity;

typedef enum {
    ARC_ERR_NONE = 0,
    ARC_ERR_UNKNOWN_NODE_KIND,
    ARC_ERR_MISSING_PORT,
    ARC_ERR_DISCONNECTED_INPUT,
    ARC_ERR_UNDEFINED_VARIABLE,
    ARC_ERR_UNDEFINED_FUNCTION,
    ARC_ERR_ARITY_MISMATCH,
    ARC_ERR_INVALID_NODE_ID,
    ARC_ERR_INVALID_REGION,
    ARC_ERR_UNSUPPORTED_NODE,
    ARC_ERR_GRAPH_VALIDATION,
} ArcDiagCode;

/* Source reference — points back to a drawing element */
typedef struct {
    ArcElementId element_id;    /* stable source element identity */
    ArcNodeId    node_id;       /* semantic graph node */
    const char*  port_role;     /* optional: which port */
} ArcSourceRef;

/* Related source reference */
typedef struct {
    ArcSourceRef ref;
    char         label[64];     /* e.g., "defined here", "used here" */
} ArcRelatedRef;

/* A single diagnostic */
typedef struct {
    ArcDiagSeverity  severity;
    ArcDiagCode      code;
    char             message[256];
    ArcSourceRef     primary;
    ArcRelatedRef    related[4];
    uint32_t         related_count;
    char             note[256];     /* optional help text */
} ArcDiagnostic;

/* Diagnostic list */
typedef struct {
    ArcDiagnostic* items;
    uint32_t       count;
    uint32_t       cap;
} ArcDiagList;

static inline void arc_diag_init(ArcDiagList* d) {
    d->items = NULL; d->count = 0; d->cap = 0;
}

static inline void arc_diag_free(ArcDiagList* d) {
    ARC_FREE(d->items); d->items = NULL; d->count = d->cap = 0;
}

static inline ArcDiagnostic* arc_diag_add(ArcDiagList* d) {
    if (d->count >= d->cap) {
        d->cap = d->cap < 16 ? 16 : d->cap * 2;
        d->items = ARC_REALLOC(d->items, ArcDiagnostic, d->cap);
    }
    ArcDiagnostic* diag = &d->items[d->count++];
    memset(diag, 0, sizeof(*diag));
    return diag;
}

/* Print diagnostics to stream */
static inline void arc_diag_print(const ArcDiagList* d, FILE* out) {
    static const char* sev_str[] = { "error", "warning", "note" };
    for (uint32_t i = 0; i < d->count; i++) {
        const ArcDiagnostic* diag = &d->items[i];
        fprintf(out, "%s[%d]: %s\n", sev_str[diag->severity], diag->code, diag->message);
        if (diag->primary.element_id != 0)
            fprintf(out, "  --> element %llu", (unsigned long long)diag->primary.element_id);
        if (diag->primary.node_id != ARC_INVALID_ID)
            fprintf(out, " (node %u)", diag->primary.node_id);
        if (diag->primary.port_role)
            fprintf(out, " port '%s'", diag->primary.port_role);
        if (diag->primary.element_id != 0 || diag->primary.node_id != ARC_INVALID_ID)
            fprintf(out, "\n");
        for (uint32_t j = 0; j < diag->related_count; j++) {
            const ArcRelatedRef* r = &diag->related[j];
            fprintf(out, "  = %s: element %llu\n", r->label,
                    (unsigned long long)r->ref.element_id);
        }
        if (diag->note[0])
            fprintf(out, "  = note: %s\n", diag->note);
    }
}

#endif /* ARCANA_DIAGNOSTICS_H */
