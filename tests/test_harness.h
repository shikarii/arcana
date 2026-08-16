#ifndef ARCANA_TEST_HARNESS_H
#define ARCANA_TEST_HARNESS_H

#include "../src/common/arcana_common.h"
#include "../src/bytecode/opcodes.h"
#include "../src/bytecode/format.h"
#include "../src/bytecode/disassembler.h"
#include "../src/vm/vm.h"
#include "../src/runtime/object.h"
#include "../src/runtime/gc.h"
#include "../src/verifier/verifier.h"
#include "../src/semantic_graph/semantic_graph.h"
#include "../src/semantic_graph/fixture_parser.h"
#include "../src/compiler/compiler.h"
#include "../src/compiler/diagnostics.h"
#include "../src/common/arena.h"
#include "../src/typecheck/typecheck.h"
#include "../src/interpreter/interpreter.h"
#include "../src/hir/hir.h"
#include "../src/mir/mir.h"
#include "../src/semantic/semantic.h"
#include "../src/platform/platform.h"

/* Portable null device path */
#ifdef _WIN32
#define DEV_NULL "NUL"
#else
#define DEV_NULL "/dev/null"
#endif

/* Global test counters (defined in test_main.c) */
extern int tests_run;
extern int tests_passed;
extern int tests_failed;

/* Build a path relative to this source file's directory */
static inline void fixture_path(char* buf, size_t buflen, const char* name) {
    const char* src = __FILE__;
    const char* sep = src;
    for (const char* p = src; *p; p++) {
        if (*p == '/' || *p == '\\') sep = p;
    }
    size_t dirlen = (sep == src) ? 0 : (size_t)(sep - src + 1);
    snprintf(buf, buflen, "%.*sfixtures/%s", (int)dirlen, src, name);
}

#define TEST(name) void name(void)
#define RUN(name) do { \
    printf("  %-50s", #name); \
    name(); \
    printf(" PASS\n"); \
    tests_run++; tests_passed++; \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf(" FAIL\n    assertion failed: %s\n    at %s:%d\n", #cond, __FILE__, __LINE__); \
        tests_run++; tests_failed++; return; \
    } \
} while(0)

#define ASSERT_EQ_I64(a, b) do { \
    int64_t _a = (a), _b = (b); \
    if (_a != _b) { \
        printf(" FAIL\n    expected %lld, got %lld\n    at %s:%d\n", \
               (long long)_b, (long long)_a, __FILE__, __LINE__); \
        tests_run++; tests_failed++; return; \
    } \
} while(0)

#endif /* ARCANA_TEST_HARNESS_H */
