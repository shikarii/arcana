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

/*
 * Stable diagnostic codes — format: ARC-<CATEGORY>-<NUMBER>
 *
 * Categories:
 *   GRAPH (1xxx) — semantic graph structure errors
 *   SEM   (2xxx) — semantic analysis errors
 *   TYPE  (3xxx) — type checking errors (reserved)
 *   CODE  (4xxx) — bytecode generation errors
 *   VM    (5xxx) — runtime errors (reserved)
 *
 * Once assigned, a number MUST NOT be reused or reassigned.
 */
typedef enum {
    ARC_DIAG_NONE               = 0,

    /* ARC-GRAPH-00xx: graph structure */
    ARC_DIAG_GRAPH_0001         = 1001,  /* invalid node ID */
    ARC_DIAG_GRAPH_0002         = 1002,  /* invalid region */
    ARC_DIAG_GRAPH_0003         = 1003,  /* missing port */
    ARC_DIAG_GRAPH_0004         = 1004,  /* disconnected input */
    ARC_DIAG_GRAPH_0005         = 1005,  /* validation failure */
    ARC_DIAG_GRAPH_0006         = 1006,  /* cyclic region ancestry */
    ARC_DIAG_GRAPH_0007         = 1007,  /* duplicate ID */
    ARC_DIAG_GRAPH_0008         = 1008,  /* invalid edge endpoint */

    /* ARC-SEM-00xx: semantic analysis */
    ARC_DIAG_SEM_0001           = 2001,  /* undefined variable */
    ARC_DIAG_SEM_0002           = 2002,  /* undefined function */
    ARC_DIAG_SEM_0003           = 2003,  /* arity mismatch */
    ARC_DIAG_SEM_0004           = 2004,  /* unsupported node kind */

    /* ARC-CODE-00xx: code generation */
    ARC_DIAG_CODE_0001          = 4001,  /* bytecode emission failure */

    /* Backward-compatible aliases for existing code */
    ARC_ERR_NONE                = 0,
    ARC_ERR_UNKNOWN_NODE_KIND   = 2004,
    ARC_ERR_MISSING_PORT        = 1003,
    ARC_ERR_DISCONNECTED_INPUT  = 1004,
    ARC_ERR_UNDEFINED_VARIABLE  = 2001,
    ARC_ERR_UNDEFINED_FUNCTION  = 2002,
    ARC_ERR_ARITY_MISMATCH     = 2003,
    ARC_ERR_INVALID_NODE_ID     = 1001,
    ARC_ERR_INVALID_REGION      = 1002,
    ARC_ERR_UNSUPPORTED_NODE    = 2004,
    ARC_ERR_GRAPH_VALIDATION    = 1005,
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

/* Format a diagnostic code as ARC-CATEGORY-NNNN */
static inline const char* arc_diag_code_str(ArcDiagCode code) {
    if (code == 0) return "ARC-0000";
    static char buf[20];
    if (code >= 1001 && code <= 1999)
        snprintf(buf, sizeof(buf), "ARC-GRAPH-%04d", code - 1000);
    else if (code >= 2001 && code <= 2999)
        snprintf(buf, sizeof(buf), "ARC-SEM-%04d", code - 2000);
    else if (code >= 4001 && code <= 4999)
        snprintf(buf, sizeof(buf), "ARC-CODE-%04d", code - 4000);
    else if (code >= 5001 && code <= 5999)
        snprintf(buf, sizeof(buf), "ARC-VM-%04d", code - 5000);
    else
        snprintf(buf, sizeof(buf), "ARC-%04d", code);
    return buf;
}

/* Print diagnostics to stream */
static inline void arc_diag_print(const ArcDiagList* d, FILE* out) {
    static const char* sev_str[] = { "error", "warning", "note" };
    for (uint32_t i = 0; i < d->count; i++) {
        const ArcDiagnostic* diag = &d->items[i];
        fprintf(out, "%s[%s]: %s\n", sev_str[diag->severity],
                arc_diag_code_str(diag->code), diag->message);
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
