cmake_minimum_required(VERSION 3.16)
project(arcana C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Compiler flags ---
if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Werror -Wno-unused-parameter)
endif()

# --- Library: arcana_platform ---
add_library(arcana_platform STATIC
    src/platform/platform.c
)
target_include_directories(arcana_platform PUBLIC src)

# --- Library: arcana_bytecode ---
add_library(arcana_bytecode STATIC
    src/bytecode/format.c
    src/bytecode/disassembler.c
)
target_include_directories(arcana_bytecode PUBLIC src)

# --- Library: arcana_runtime ---
add_library(arcana_runtime STATIC
    src/runtime/object.c
    src/runtime/gc.c
    src/vm/value_ops.c
)
target_include_directories(arcana_runtime PUBLIC src)

# --- Library: arcana_vm ---
add_library(arcana_vm STATIC
    src/vm/vm.c
)
target_link_libraries(arcana_vm PUBLIC arcana_bytecode arcana_runtime)

# --- Library: arcana_verifier ---
add_library(arcana_verifier STATIC
    src/verifier/verifier.c
)
target_link_libraries(arcana_verifier PUBLIC arcana_bytecode)

# --- Library: arcana_semantic_graph ---
add_library(arcana_semantic_graph STATIC
    src/semantic_graph/semantic_graph.c
    src/semantic_graph/fixture_parser.c
)
target_include_directories(arcana_semantic_graph PUBLIC src)

# --- Library: arcana_hir ---
add_library(arcana_hir STATIC
    src/hir/hir.c
)
target_include_directories(arcana_hir PUBLIC src)

# --- Library: arcana_mir ---
add_library(arcana_mir STATIC
    src/mir/mir.c
)
target_include_directories(arcana_mir PUBLIC src)

# --- Library: arcana_semantic ---
add_library(arcana_semantic STATIC
    src/semantic/semantic.c
)
target_link_libraries(arcana_semantic PUBLIC arcana_semantic_graph arcana_hir)

# --- Library: arcana_compiler ---
add_library(arcana_compiler STATIC
    src/compiler/compiler.c
)
target_link_libraries(arcana_compiler PUBLIC arcana_bytecode arcana_semantic_graph)

# --- Library: arcana_interpreter ---
add_library(arcana_interpreter STATIC
    src/interpreter/interpreter.c
)
target_link_libraries(arcana_interpreter PUBLIC arcana_semantic_graph)

# --- Test executable ---
add_executable(arcana_tests
    tests/test_all.c
)
target_link_libraries(arcana_tests
    arcana_bytecode
    arcana_vm
    arcana_verifier
    arcana_semantic_graph
    arcana_compiler
    arcana_interpreter
    arcana_hir
    arcana_mir
    arcana_semantic
    arcana_platform
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
