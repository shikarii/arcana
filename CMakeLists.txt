cmake_minimum_required(VERSION 3.16)
project(arcana C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Compiler flags ---
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Werror -Wno-unused-parameter)
endif()

# --- Library: arcana_bytecode ---
add_library(arcana_bytecode STATIC
    src/bytecode/format.c
    src/bytecode/disassembler.c
)
target_include_directories(arcana_bytecode PUBLIC src)

# --- Library: arcana_vm ---
add_library(arcana_vm STATIC
    src/vm/vm.c
)
target_link_libraries(arcana_vm PUBLIC arcana_bytecode)

# --- Library: arcana_verifier ---
add_library(arcana_verifier STATIC
    src/verifier/verifier.c
)
target_link_libraries(arcana_verifier PUBLIC arcana_bytecode)

# --- Library: arcana_semantic_graph ---
add_library(arcana_semantic_graph STATIC
    src/semantic_graph/semantic_graph.c
)
target_include_directories(arcana_semantic_graph PUBLIC src)

# --- Library: arcana_compiler ---
add_library(arcana_compiler STATIC
    src/compiler/compiler.c
)
target_link_libraries(arcana_compiler PUBLIC arcana_bytecode arcana_semantic_graph)

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
)

enable_testing()
add_test(NAME arcana_tests COMMAND arcana_tests)
