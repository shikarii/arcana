/*
 * Arcana Tooling Service — implementation.
 */
#include "arcana_service.h"
#include "../semantic_graph/semantic_graph.h"
#include "../semantic_graph/fixture_parser.h"
#include "../compiler/compiler.h"
#include "../verifier/verifier.h"
#include "../vm/vm.h"

#ifdef _WIN32
#define DEV_NULL "NUL"
#else
#define DEV_NULL "/dev/null"
#endif

/* Internal result structure (opaque to callers) */
struct ArcServiceResult {
    ArcServiceStage  stage;
    bool             ok;
    char             error[256];

    /* Owned resources (freed by arc_service_free) */
    ArcGraph            graph;
    bool                has_graph;
    ArcTypeCheckResult  tc;
    bool                has_tc;
    ArcCompileResult    cr;
    bool                has_cr;
    ArcVm               vm;
    bool                has_vm;

    /* Accumulated diagnostics */
    ArcDiagList         diagnostics;

    /* Execution result */
    ArcValue            result;
};

/* --- Internal helpers --- */

static ArcServiceResult* result_new(void) {
    ArcServiceResult* r = ARC_ALLOC(ArcServiceResult, 1);
    memset(r, 0, sizeof(*r));
    arc_diag_init(&r->diagnostics);
    r->result = arc_val_null();
    return r;
}

static void result_fail(ArcServiceResult* r, ArcServiceStage s,
                         const char* msg) {
    r->stage = s;
    r->ok = false;
    snprintf(r->error, sizeof(r->error), "%s", msg);
}

/* Copy diagnostics from a source list into the result */
static void merge_diags(ArcServiceResult* r, const ArcDiagList* src) {
    for (uint32_t i = 0; i < src->count; i++) {
        ArcDiagnostic* d = arc_diag_add(&r->diagnostics);
        *d = src->items[i];
    }
}

/* --- Stage: Parse --- */
static bool stage_parse(ArcServiceResult* r, const char* text) {
    ArcFixtureResult fr = arc_fixture_parse(text);
    if (!fr.success) {
        result_fail(r, ARC_STAGE_PARSE, fr.error);
        return false;
    }
    r->graph = fr.graph;
    r->has_graph = true;
    r->stage = ARC_STAGE_PARSE;
    return true;
}

/* --- Stage: Validate --- */
static bool stage_validate(ArcServiceResult* r) {
    ArcGraphValidation v = arc_graph_validate(&r->graph);
    if (!v.valid) {
        const char* msg = (v.count > 0) ? v.errors[0].message
                                         : "graph validation failed";
        result_fail(r, ARC_STAGE_VALIDATE, msg);
        /* Copy validation errors as diagnostics */
        for (int i = 0; i < v.count; i++) {
            ArcDiagnostic* d = arc_diag_add(&r->diagnostics);
            d->severity = ARC_DIAG_ERROR;
            d->code = ARC_DIAG_GRAPH_0005;
            snprintf(d->message, sizeof(d->message), "%s",
                     v.errors[i].message);
        }
        arc_graph_validation_free(&v);
        return false;
    }
    arc_graph_validation_free(&v);
    r->stage = ARC_STAGE_VALIDATE;
    return true;
}

/* --- Stage: Typecheck --- */
static bool stage_typecheck(ArcServiceResult* r) {
    r->tc = arc_typecheck(&r->graph);
    r->has_tc = true;
    merge_diags(r, &r->tc.diagnostics);
    if (!r->tc.success) {
        result_fail(r, ARC_STAGE_TYPECHECK, "type check failed");
        return false;
    }
    r->stage = ARC_STAGE_TYPECHECK;
    return true;
}

/* --- Stage: Compile --- */
static bool stage_compile(ArcServiceResult* r) {
    r->cr = arc_compile(&r->graph);
    r->has_cr = true;
    if (!r->cr.success) {
        const char* msg = (r->cr.error_count > 0)
            ? r->cr.errors[0].message : "compilation failed";
        result_fail(r, ARC_STAGE_COMPILE, msg);
        return false;
    }
    r->stage = ARC_STAGE_COMPILE;
    return true;
}

/* --- Stage: Verify --- */
static bool stage_verify(ArcServiceResult* r) {
    ArcVerifyResult vr = arc_verify(&r->cr.image);
    if (!vr.valid) {
        const char* msg = (vr.error_count > 0)
            ? vr.errors[0].message : "verification failed";
        result_fail(r, ARC_STAGE_VERIFY, msg);
        return false;
    }
    r->stage = ARC_STAGE_VERIFY;
    return true;
}

/* --- Stage: Run --- */
static bool stage_run(ArcServiceResult* r) {
    arc_vm_init(&r->vm, &r->cr.image);
    r->has_vm = true;
    r->vm.output = fopen(DEV_NULL, "w");
    ArcStatus s = arc_vm_run(&r->vm);
    fclose(r->vm.output);
    r->vm.output = NULL;
    if (s != ARC_OK) {
        result_fail(r, ARC_STAGE_RUN, r->vm.error.message);
        return false;
    }
    r->result = arc_vm_result(&r->vm);
    r->stage = ARC_STAGE_RUN;
    return true;
}

/* --- Public API --- */

ArcServiceResult* arc_service_run(const char* graph_text) {
    ArcServiceResult* r = result_new();
    if (!stage_parse(r, graph_text)) return r;
    if (!stage_validate(r)) return r;
    (void)stage_typecheck(r);
    if (!stage_compile(r)) return r;
    if (!stage_verify(r)) return r;
    if (!stage_run(r)) return r;
    r->ok = true;
    return r;
}

ArcServiceResult* arc_service_check(const char* graph_text) {
    ArcServiceResult* r = result_new();
    if (!stage_parse(r, graph_text)) return r;
    if (!stage_validate(r)) return r;
    if (!stage_typecheck(r)) return r;
    r->ok = true;
    return r;
}

ArcServiceResult* arc_service_compile(const char* graph_text) {
    ArcServiceResult* r = result_new();
    if (!stage_parse(r, graph_text)) return r;
    if (!stage_validate(r)) return r;
    (void)stage_typecheck(r);
    if (!stage_compile(r)) return r;
    if (!stage_verify(r)) return r;
    r->ok = true;
    return r;
}

/* --- Accessors --- */

bool arc_service_ok(const ArcServiceResult* r) { return r->ok; }

ArcServiceStage arc_service_stage(const ArcServiceResult* r) {
    return r->stage;
}

ArcValue arc_service_value(const ArcServiceResult* r) { return r->result; }

const ArcDiagList* arc_service_diagnostics(const ArcServiceResult* r) {
    return &r->diagnostics;
}

const ArcTypeCheckResult* arc_service_types(const ArcServiceResult* r) {
    return r->has_tc ? &r->tc : NULL;
}

const char* arc_service_error(const ArcServiceResult* r) {
    return r->error;
}

/* --- Cleanup --- */

void arc_service_free(ArcServiceResult* r) {
    if (!r) return;
    if (r->has_vm) arc_vm_destroy(&r->vm);
    if (r->has_cr) arc_compile_result_free(&r->cr);
    if (r->has_tc) arc_typecheck_result_free(&r->tc);
    if (r->has_graph) arc_graph_free(&r->graph);
    arc_diag_free(&r->diagnostics);
    ARC_FREE(r);
}
