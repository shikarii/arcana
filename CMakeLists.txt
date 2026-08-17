cmake_minimum_required(VERSION 3.16)
project(arcana C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Compiler flags ---
if(MSVC)
    add_compile_options(
        /W4 /WX          # high warnings + treat as errors
        /wd4200          # C4200: flexible array members (valid C17)
        /wd4127          # C4127: conditional expression is constant (do-while macros)
    )
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(
        -Wall -Wextra -Werror -Wpedantic
        -Wshadow                 # warn on shadowed variables
        -Wstrict-prototypes      # require full prototypes
        -Wold-style-definition   # reject K&R-style function defs
        -Wdouble-promotion      # warn on float→double implicit promotion
        -Wformat                 # printf format checking (not =2; -Wformat-nonliteral false-positives on variadic wrappers)
        -Wnull-dereference       # warn on null pointer deref paths
        -Wno-unused-parameter    # allow unused params (common in callbacks)
    )
endif()

# Extra strictness for library code (not tests — test functions lack prototypes by design)
set(ARCANA_LIB_STRICT "")
if(NOT MSVC)
    set(ARCANA_LIB_STRICT -Wmissing-prototypes)
endif()

# --- All library targets ---
set(ARCANA_LIBS "")

add_library(arcana_common STATIC src/common/arena.c)
target_include_directories(arcana_common PUBLIC src)
list(APPEND ARCANA_LIBS arcana_common)

add_library(arcana_platform STATIC src/platform/platform.c)
target_include_directories(arcana_platform PUBLIC src)
list(APPEND ARCANA_LIBS arcana_platform)

add_library(arcana_bytecode STATIC src/bytecode/format.c src/bytecode/disassembler.c)
target_include_directories(arcana_bytecode PUBLIC src)
list(APPEND ARCANA_LIBS arcana_bytecode)

add_library(arcana_runtime STATIC src/runtime/object.c src/runtime/gc.c src/vm/value_ops.c)
target_include_directories(arcana_runtime PUBLIC src)
list(APPEND ARCANA_LIBS arcana_runtime)

add_library(arcana_vm STATIC src/vm/vm.c src/vm/vm_ops.c)
target_link_libraries(arcana_vm PUBLIC arcana_bytecode arcana_runtime)
list(APPEND ARCANA_LIBS arcana_vm)

add_library(arcana_verifier STATIC src/verifier/verifier.c)
target_link_libraries(arcana_verifier PUBLIC arcana_bytecode)
list(APPEND ARCANA_LIBS arcana_verifier)

add_library(arcana_semantic_graph STATIC
    src/semantic_graph/semantic_graph.c src/semantic_graph/fixture_parser.c)
target_include_directories(arcana_semantic_graph PUBLIC src)
list(APPEND ARCANA_LIBS arcana_semantic_graph)

add_library(arcana_hir STATIC src/hir/hir.c)
target_include_directories(arcana_hir PUBLIC src)
list(APPEND ARCANA_LIBS arcana_hir)

add_library(arcana_mir STATIC src/mir/mir.c)
target_include_directories(arcana_mir PUBLIC src)
list(APPEND ARCANA_LIBS arcana_mir)

add_library(arcana_semantic STATIC
    src/semantic/scope.c src/semantic/semantic.c src/semantic/semantic_expr.c)
target_link_libraries(arcana_semantic PUBLIC arcana_semantic_graph arcana_hir)
list(APPEND ARCANA_LIBS arcana_semantic)

add_library(arcana_typecheck STATIC src/typecheck/typecheck.c)
target_link_libraries(arcana_typecheck PUBLIC arcana_semantic_graph)
list(APPEND ARCANA_LIBS arcana_typecheck)

add_library(arcana_compiler STATIC src/compiler/compiler.c src/compiler/compile_nodes.c)
target_link_libraries(arcana_compiler PUBLIC arcana_bytecode arcana_semantic_graph arcana_semantic)
list(APPEND ARCANA_LIBS arcana_compiler)

add_library(arcana_interpreter STATIC src/interpreter/interpreter.c)
target_link_libraries(arcana_interpreter PUBLIC arcana_semantic_graph)
list(APPEND ARCANA_LIBS arcana_interpreter)

# Apply -Wmissing-prototypes to all library targets (not tests)
foreach(lib ${ARCANA_LIBS})
    target_compile_options(${lib} PRIVATE ${ARCANA_LIB_STRICT})
endforeach()

# --- Test executable ---
add_executable(arcana_tests
    tests/test_all.c
    tests/test_runtime.c
    tests/test_vm.c
    tests/test_gc.c
    tests/test_pipeline.c
    tests/test_bytecode.c
    tests/test_verifier.c
    tests/test_graph.c
    tests/test_infra.c
    tests/test_pipeline_e2e.c
    tests/test_vm_collections.c
    tests/test_error_recovery.c
    tests/test_cycle.c
    tests/test_closures.c
    tests/test_challenge_L0.c
)
target_link_libraries(arcana_tests
    arcana_bytecode
    arcana_vm
    arcana_verifier
    arcana_semantic_graph
    arcana_compiler
    arcana_typecheck
    arcana_interpreter
    arcana_hir
    arcana_mir
    arcana_semantic
    arcana_platform
    arcana_common
)

# --- CLI tools ---
add_executable(arcana-run tools/arcana-run.c)
target_link_libraries(arcana-run arcana_bytecode arcana_vm arcana_verifier)

add_executable(arcana-dis tools/arcana-dis.c)
target_link_libraries(arcana-dis arcana_bytecode)

add_executable(arcana-verify tools/arcana-verify.c)
target_link_libraries(arcana-verify arcana_bytecode arcana_verifier)

# --- Assembler ---
add_executable(arcana-asm tools/arcana-asm.c)
target_link_libraries(arcana-asm arcana_bytecode)

# --- Compiler CLI ---
add_executable(arcana-compile tools/arcana-compile.c)
target_link_libraries(arcana-compile
    arcana_semantic_graph arcana_compiler arcana_bytecode arcana_verifier
    arcana_hir arcana_mir arcana_semantic
)

# --- Benchmark (optional, not part of test suite) ---
add_executable(arcana-bench benchmarks/bench_compile.c)
target_link_libraries(arcana-bench
    arcana_semantic_graph arcana_compiler arcana_bytecode
)

enable_testing()
add_test(NAME arcana_tests COMMAND arcana_tests)

# --- LOC limit enforcement (600 lines/file, 60 lines/function) ---
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME check_limits
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/check-limits.py" --root "${CMAKE_SOURCE_DIR}")
