/*
 * Arcana Tooling Service — unified entry point for external tools.
 *
 * Provides a clean, opaque API over the full compiler pipeline:
 *   parse → validate → typecheck → compile → verify → run
 *
 * Consumers (VS Code extension, CLI, REPL) call these functions
 * with graph text and receive results + diagnostics.
 */
#ifndef ARCANA_SERVICE_H
#define ARCANA_SERVICE_H

#include "../common/arcana_common.h"
#include "../vm/value.h"
#include "../compiler/diagnostics.h"
#include "../typecheck/typecheck.h"

/* How far the pipeline progressed before stopping */
typedef enum {
    ARC_STAGE_NONE,
    ARC_STAGE_PARSE,
    ARC_STAGE_VALIDATE,
    ARC_STAGE_TYPECHECK,
    ARC_STAGE_COMPILE,
    ARC_STAGE_VERIFY,
    ARC_STAGE_RUN,
} ArcServiceStage;

/* Opaque result handle — query with accessors, free when done */
typedef struct ArcServiceResult ArcServiceResult;

/* --- Pipeline entry points --- */

/* Full pipeline: parse → validate → typecheck → compile → verify → run.
 * Returns result with the execution value or the first stage that failed. */
ArcServiceResult* arc_service_run(const char* graph_text);

/* Check only: parse → validate → typecheck.
 * For IDE diagnostics without compiling or running. */
ArcServiceResult* arc_service_check(const char* graph_text);

/* Compile only: parse → validate → typecheck → compile → verify.
 * For pre-compilation without execution. */
ArcServiceResult* arc_service_compile(const char* graph_text);

/* --- Result accessors --- */

/* Did all requested stages succeed? */
bool arc_service_ok(const ArcServiceResult* r);

/* Which stage was reached (last successful or failed stage)? */
ArcServiceStage arc_service_stage(const ArcServiceResult* r);

/* Execution result (only meaningful after ARC_STAGE_RUN) */
ArcValue arc_service_value(const ArcServiceResult* r);

/* Accumulated diagnostics from all stages */
const ArcDiagList* arc_service_diagnostics(const ArcServiceResult* r);

/* Type information per node (only after ARC_STAGE_TYPECHECK) */
const ArcTypeCheckResult* arc_service_types(const ArcServiceResult* r);

/* Human-readable error summary (empty string if ok) */
const char* arc_service_error(const ArcServiceResult* r);

/* --- Cleanup --- */

void arc_service_free(ArcServiceResult* r);

#endif /* ARCANA_SERVICE_H */
