/*
 * Arcana test suite -- driver file.
 *
 * Defines global counters and main(). All test functions are defined
 * in per-module files and declared here as extern.
 */
#include "test_harness.h"

int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

/* --- test_runtime.c --- */
extern void test_arena_basic(void);
extern void test_arena_many_allocs(void);
extern void test_arena_large_alloc(void);
extern void test_typecheck_arithmetic_valid(void);
extern void test_typecheck_bitwise_error(void);
extern void test_typecheck_string_concat(void);
extern void test_typecheck_cast_result_type(void);
extern void test_typecheck_empty_graph(void);
extern void test_type_name(void);
extern void test_record_create_and_fields(void);
extern void test_record_gc_tracing(void);
extern void test_record_print(void);
extern void test_diag_type_code_format(void);

/* --- test_bytecode.c --- */
extern void test_opcode_mnemonic(void);
extern void test_opcode_operand_bytes(void);
extern void test_opcode_stack_effects(void);
extern void test_const_pool(void);
extern void test_image_serialize_roundtrip(void);
extern void test_malformed_binary(void);
extern void test_truncated_binary(void);
extern void test_serialization_roundtrip_with_debug(void);
extern void test_const_pool_dedup(void);
extern void test_property_roundtrip_random(void);

/* --- test_verifier.c --- */
extern void test_verifier_valid(void);
extern void test_verifier_invalid_const_index(void);
extern void test_verifier_stack_underflow(void);
extern void test_verifier_bad_jump_target(void);
extern void test_verifier_stack_consistency(void);

/* --- test_vm.c --- */
extern void test_vm_add_5_10(void);
extern void test_vm_arithmetic(void);
extern void test_vm_comparisons(void);
extern void test_vm_branch(void);
extern void test_vm_locals(void);
extern void test_vm_division_by_zero(void);
extern void test_vm_stack_overflow(void);
extern void test_clock_intrinsic(void);
extern void test_vm_globals(void);
extern void test_vm_string_const(void);
extern void test_vm_string_concat(void);
extern void test_vm_str_len(void);
extern void test_vm_str_slice(void);
extern void test_vm_str_index(void);
extern void test_vm_bitwise_and(void);
extern void test_vm_bitwise_or_xor(void);
extern void test_vm_bitwise_not(void);
extern void test_vm_shift(void);
extern void test_vm_cast_i64(void);
extern void test_vm_cast_f64(void);
extern void test_vm_cast_str(void);
extern void test_vm_cast_str_from_string(void);

/* --- test_vm_collections.c --- */
extern void test_vm_array_new(void);
extern void test_vm_index_get_set(void);
extern void test_vm_array_length(void);
extern void test_vm_map_new(void);
extern void test_vm_map_get(void);
extern void test_vm_try_catch_basic(void);
extern void test_vm_try_end_no_throw(void);
extern void test_vm_unhandled_throw(void);
extern void test_vm_intrinsic_type(void);
extern void test_vm_intrinsic_assert_pass(void);
extern void test_vm_intrinsic_assert_fail(void);
extern void test_vm_intrinsic_len(void);
extern void test_vm_intrinsic_push(void);
extern void test_vm_intrinsic_keys(void);
extern void test_upvalue_format_roundtrip(void);
extern void test_vm_closure_upvalue(void);

/* --- test_gc.c --- */
extern void test_gc_collect_unreachable(void);
extern void test_gc_preserve_reachable(void);
extern void test_gc_array_tracing(void);
extern void test_gc_map_tracing(void);
extern void test_gc_stress(void);
extern void test_gc_stress_mode(void);

/* --- test_graph.c --- */
extern void test_graph_valid(void);
extern void test_graph_invalid_edge(void);
extern void test_graph_validation_bidirectional_port(void);
extern void test_hir_construct_and_dump(void);
extern void test_hir_function_with_params(void);
extern void test_hir_validation_valid(void);
extern void test_hir_validation_null_expr(void);
extern void test_mir_construct_and_dump(void);
extern void test_mir_branching(void);
extern void test_mir_validation_missing_terminator(void);
extern void test_semantic_5_plus_10(void);
extern void test_semantic_undefined_variable(void);
extern void test_semantic_null_graph(void);
extern void test_semantic_function_lowering(void);

/* --- test_pipeline.c --- */
extern void test_e2e_5_plus_10(void);
extern void test_e2e_5_plus_10_vm_result(void);
extern void test_compile_fail_diagnostic(void);
extern void test_golden_disassembly(void);
extern void test_compiler_short_circuit_and(void);
extern void test_compiler_short_circuit_and_false(void);
extern void test_compiler_short_circuit_or(void);
extern void test_compiler_const_string(void);
extern void test_compiler_bitwise_and(void);
extern void test_compiler_cast_i64(void);
extern void test_compiler_str_len(void);
extern void test_compiler_try_throw(void);
extern void test_compiler_new_node_kinds_exist(void);
extern void test_fixture_parse_add(void);
extern void test_fixture_parse_file(void);
extern void test_fixture_parse_malformed(void);

/* --- test_const_fold.c --- */
extern void test_fold_int_arithmetic(void);
extern void test_fold_int_comparison(void);
extern void test_fold_unary_neg(void);
extern void test_fold_nested_expr(void);

/* --- test_pipeline_e2e.c --- */
extern void test_e2e_function_call(void);
extern void test_e2e_if_else(void);
extern void test_e2e_fibonacci(void);
extern void test_e2e_while_loop(void);
extern void test_e2e_not_operator(void);
extern void test_e2e_not_via_fixture(void);

/* --- test_infra.c --- */
extern void test_platform_strdup(void);
extern void test_platform_clock(void);
extern void test_platform_file_io(void);
extern void test_diag_code_formatting(void);
extern void test_diag_backward_compat(void);
extern void test_string_values(void);
extern void test_structured_diagnostics(void);
extern void test_ref_interp_5_plus_10(void);
extern void test_ref_interp_function(void);
extern void test_ref_interp_not(void);

/* --- test_error_recovery.c --- */
extern void test_semantic_multi_error(void);
extern void test_semantic_poison_no_cascade(void);
extern void test_hir_poison_dump(void);
extern void test_hir_poison_validation(void);
extern void test_semantic_mixed_errors(void);

/* --- test_cycle.c --- */
extern void test_cycle_sum_5(void);
extern void test_cycle_sum_0(void);
extern void test_cycle_sum_1(void);
extern void test_cycle_sum_100(void);
extern void test_cycle_interp_agreement(void);
extern void test_cycle_semantic_lowering(void);
extern void test_cycle_scope_resolution(void);

/* ================================================================
 * Sub-runner functions (keep main() under 60 lines)
 * ================================================================ */

static void run_bytecode_tests(void) {
    printf("[Milestone A: Opcodes]\n");
    RUN(test_opcode_mnemonic);
    RUN(test_opcode_operand_bytes);
    RUN(test_opcode_stack_effects);

    printf("\n[Milestone B: Format + Verifier]\n");
    RUN(test_const_pool);
    RUN(test_image_serialize_roundtrip);
    RUN(test_verifier_valid);
    RUN(test_verifier_invalid_const_index);
    RUN(test_verifier_stack_underflow);

    printf("\n[Edge Cases]\n");
    RUN(test_malformed_binary);
    RUN(test_truncated_binary);
    RUN(test_serialization_roundtrip_with_debug);
    RUN(test_const_pool_dedup);

    printf("\n[Property Tests]\n");
    RUN(test_property_roundtrip_random);

    printf("\n[Verifier Stack Consistency]\n");
    RUN(test_verifier_stack_consistency);
    RUN(test_verifier_bad_jump_target);
}

static void run_vm_tests(void) {
    printf("\n[Milestone C: VM]\n");
    RUN(test_vm_add_5_10);
    RUN(test_vm_arithmetic);
    RUN(test_vm_comparisons);
    RUN(test_vm_branch);
    RUN(test_vm_locals);
    RUN(test_vm_division_by_zero);
    RUN(test_vm_stack_overflow);
    RUN(test_clock_intrinsic);

    printf("\n[Globals]\n");
    RUN(test_vm_globals);

    printf("\n[String Values + Ops]\n");
    RUN(test_vm_string_const);
    RUN(test_vm_string_concat);
    RUN(test_vm_str_len);
    RUN(test_vm_str_slice);
    RUN(test_vm_str_index);

    printf("\n[Bitwise Operations]\n");
    RUN(test_vm_bitwise_and);
    RUN(test_vm_bitwise_or_xor);
    RUN(test_vm_bitwise_not);
    RUN(test_vm_shift);

    printf("\n[Type Casts]\n");
    RUN(test_vm_cast_i64);
    RUN(test_vm_cast_f64);
    RUN(test_vm_cast_str);
    RUN(test_vm_cast_str_from_string);
}

static void run_vm_collection_tests(void) {
    printf("\n[Arrays]\n");
    RUN(test_vm_array_new);
    RUN(test_vm_index_get_set);
    RUN(test_vm_array_length);

    printf("\n[Maps]\n");
    RUN(test_vm_map_new);
    RUN(test_vm_map_get);

    printf("\n[Exception Handling]\n");
    RUN(test_vm_try_catch_basic);
    RUN(test_vm_try_end_no_throw);
    RUN(test_vm_unhandled_throw);

    printf("\n[Extended Intrinsics]\n");
    RUN(test_vm_intrinsic_type);
    RUN(test_vm_intrinsic_assert_pass);
    RUN(test_vm_intrinsic_assert_fail);
    RUN(test_vm_intrinsic_len);
    RUN(test_vm_intrinsic_push);
    RUN(test_vm_intrinsic_keys);

    printf("\n[Closure Upvalues]\n");
    RUN(test_upvalue_format_roundtrip);
    RUN(test_vm_closure_upvalue);
}

static void run_graph_tests(void) {
    printf("\n[Milestone D: Semantic Graph]\n");
    RUN(test_graph_valid);
    RUN(test_graph_invalid_edge);
    RUN(test_graph_validation_bidirectional_port);

    printf("\n[HIR]\n");
    RUN(test_hir_construct_and_dump);
    RUN(test_hir_function_with_params);
    RUN(test_hir_validation_valid);
    RUN(test_hir_validation_null_expr);

    printf("\n[MIR]\n");
    RUN(test_mir_construct_and_dump);
    RUN(test_mir_branching);
    RUN(test_mir_validation_missing_terminator);

    printf("\n[Semantic Analysis]\n");
    RUN(test_semantic_5_plus_10);
    RUN(test_semantic_undefined_variable);
    RUN(test_semantic_null_graph);
    RUN(test_semantic_function_lowering);
}

static void run_pipeline_tests(void) {
    printf("\n[Milestone E-F: End-to-End]\n");
    RUN(test_e2e_5_plus_10);
    RUN(test_e2e_5_plus_10_vm_result);

    printf("\n[Milestone G: Functions]\n");
    RUN(test_e2e_function_call);

    printf("\n[Milestone H: If/Else]\n");
    RUN(test_e2e_if_else);

    printf("\n[Milestone I: Recursion]\n");
    RUN(test_e2e_fibonacci);

    printf("\n[Milestone J: While Loops]\n");
    RUN(test_e2e_while_loop);

    printf("\n[NOT Operator]\n");
    RUN(test_e2e_not_operator);
    RUN(test_e2e_not_via_fixture);

    printf("\n[Compile-Fail Diagnostics]\n");
    RUN(test_compile_fail_diagnostic);

    printf("\n[Golden Disassembly]\n");
    RUN(test_golden_disassembly);

    printf("\n[Short-Circuit Booleans]\n");
    RUN(test_compiler_short_circuit_and);
    RUN(test_compiler_short_circuit_and_false);
    RUN(test_compiler_short_circuit_or);

    printf("\n[Compiler: Strings + Bitwise + Casts]\n");
    RUN(test_compiler_const_string);
    RUN(test_compiler_bitwise_and);
    RUN(test_compiler_cast_i64);
    RUN(test_compiler_str_len);

    printf("\n[Compiler: Try/Throw]\n");
    RUN(test_compiler_try_throw);

    printf("\n[Fixture Parser]\n");
    RUN(test_fixture_parse_add);
    RUN(test_fixture_parse_file);
    RUN(test_fixture_parse_malformed);

    printf("\n[Constant Folding]\n");
    RUN(test_fold_int_arithmetic);
    RUN(test_fold_int_comparison);
    RUN(test_fold_unary_neg);
    RUN(test_fold_nested_expr);

    printf("\n[New Node Kinds]\n");
    RUN(test_compiler_new_node_kinds_exist);
}

static void run_infra_tests(void) {
    printf("\n[Platform]\n");
    RUN(test_platform_strdup);
    RUN(test_platform_clock);
    RUN(test_platform_file_io);

    printf("\n[Diagnostic Codes]\n");
    RUN(test_diag_code_formatting);
    RUN(test_diag_backward_compat);

    printf("\n[String Values]\n");
    RUN(test_string_values);

    printf("\n[Structured Diagnostics]\n");
    RUN(test_structured_diagnostics);

    printf("\n[Reference Interpreter]\n");
    RUN(test_ref_interp_5_plus_10);
    RUN(test_ref_interp_function);
    RUN(test_ref_interp_not);
}

static void run_gc_tests(void) {
    printf("\n[GC]\n");
    RUN(test_gc_collect_unreachable);
    RUN(test_gc_preserve_reachable);
    RUN(test_gc_array_tracing);
    RUN(test_gc_map_tracing);
    RUN(test_gc_stress);

    printf("\n[GC Stress Mode]\n");
    RUN(test_gc_stress_mode);
}

static void run_runtime_tests(void) {
    printf("\n[Arena Allocator]\n");
    RUN(test_arena_basic);
    RUN(test_arena_many_allocs);
    RUN(test_arena_large_alloc);

    printf("\n[Type Checker]\n");
    RUN(test_typecheck_arithmetic_valid);
    RUN(test_typecheck_bitwise_error);
    RUN(test_typecheck_string_concat);
    RUN(test_typecheck_cast_result_type);
    RUN(test_typecheck_empty_graph);
    RUN(test_type_name);

    printf("\n[Records]\n");
    RUN(test_record_create_and_fields);
    RUN(test_record_gc_tracing);
    RUN(test_record_print);

    printf("\n[Diagnostic Type Codes]\n");
    RUN(test_diag_type_code_format);
}

static void run_error_recovery_tests(void) {
    printf("\n[Error Recovery]\n");
    RUN(test_semantic_multi_error);
    RUN(test_semantic_poison_no_cascade);
    RUN(test_hir_poison_dump);
    RUN(test_hir_poison_validation);
    RUN(test_semantic_mixed_errors);
}

static void run_cycle_tests(void) {
    printf("\n[Topology-Derived Iteration (CYCLE)]\n");
    RUN(test_cycle_sum_5);
    RUN(test_cycle_sum_0);
    RUN(test_cycle_sum_1);
    RUN(test_cycle_sum_100);
    RUN(test_cycle_interp_agreement);
    RUN(test_cycle_semantic_lowering);
    RUN(test_cycle_scope_resolution);
}

/* --- test_closures.c --- */
extern void test_closure_compile_smoke(void);
extern void test_closure_upvalue_one_capture(void);
extern void test_closure_regular_func_no_upvalues(void);
extern void test_closure_target_skipped(void);

static void run_closure_tests(void) {
    printf("\n[Closures]\n");
    RUN(test_closure_compile_smoke);
    RUN(test_closure_upvalue_one_capture);
    RUN(test_closure_regular_func_no_upvalues);
    RUN(test_closure_target_skipped);
}

/* --- test_challenge_L0.c --- */
extern void test_L0_01_constant(void);
extern void test_L0_02_addition(void);
extern void test_L0_03_arithmetic(void);
extern void test_L0_04_branch(void);
extern void test_L0_05_loop(void);
extern void test_L0_06_function(void);
extern void test_L0_07_nested_calls(void);
extern void test_L0_08_fib_recursive(void);
extern void test_L0_09_fib_iterative(void);
extern void test_L0_10_stress(void);
extern void test_L0_11_comparisons(void);
extern void test_L0_12_invalid_rejection(void);

static void run_challenge_L0_tests(void) {
    printf("\n[Challenge Corpus Level 0: Machine Sanity]\n");
    RUN(test_L0_01_constant);
    RUN(test_L0_02_addition);
    RUN(test_L0_03_arithmetic);
    RUN(test_L0_04_branch);
    RUN(test_L0_05_loop);
    RUN(test_L0_06_function);
    RUN(test_L0_07_nested_calls);
    RUN(test_L0_08_fib_recursive);
    RUN(test_L0_09_fib_iterative);
    RUN(test_L0_10_stress);
    RUN(test_L0_11_comparisons);
    RUN(test_L0_12_invalid_rejection);
}

/* --- test_service.c --- */
extern void test_service_run_ok(void);
extern void test_service_run_add(void);
extern void test_service_run_parse_fail(void);
extern void test_service_check_ok(void);
extern void test_service_compile_ok(void);
extern void test_service_check_parse_fail(void);
extern void test_service_diagnostics(void);
extern void test_service_error_empty_on_success(void);

static void run_service_tests(void) {
    printf("\n[Tooling Service API]\n");
    RUN(test_service_run_ok);
    RUN(test_service_run_add);
    RUN(test_service_run_parse_fail);
    RUN(test_service_check_ok);
    RUN(test_service_compile_ok);
    RUN(test_service_check_parse_fail);
    RUN(test_service_diagnostics);
    RUN(test_service_error_empty_on_success);
}

/* --- test_challenge_L1.c --- */
extern void test_L1_01_factorial(void);
extern void test_L1_02_gcd(void);
extern void test_L1_03_power(void);
extern void test_L1_04_fizzbuzz(void);
extern void test_L1_05_sum_squares(void);
extern void test_L1_06_collatz(void);
extern void test_L1_07_is_prime(void);
extern void test_L1_08_mutual_recursion(void);
extern void test_L1_09_bubble_sort(void);
extern void test_L1_10_binary_search(void);
extern void test_L1_11_selection_sort(void);
extern void test_L1_12_max_subarray(void);
extern void test_L1_13_insertion_sort(void);
extern void test_L1_14_two_sum(void);
extern void test_L1_15_record_point(void);
extern void test_L1_16_linked_list(void);

static void run_challenge_L1_tests(void) {
    printf("\n[Challenge Corpus Level 1: Classic Algorithms]\n");
    RUN(test_L1_01_factorial);
    RUN(test_L1_02_gcd);
    RUN(test_L1_03_power);
    RUN(test_L1_04_fizzbuzz);
    RUN(test_L1_05_sum_squares);
    RUN(test_L1_06_collatz);
    RUN(test_L1_07_is_prime);
    RUN(test_L1_08_mutual_recursion);
    RUN(test_L1_09_bubble_sort);
    RUN(test_L1_10_binary_search);
    RUN(test_L1_11_selection_sort);
    RUN(test_L1_12_max_subarray);
    RUN(test_L1_13_insertion_sort);
    RUN(test_L1_14_two_sum);
    RUN(test_L1_15_record_point);
    RUN(test_L1_16_linked_list);
}

/* --- test_concurrency.c --- */
extern void test_coro_create(void);
extern void test_coro_yield_resume(void);
extern void test_coro_completion(void);
extern void test_coro_resume_dead(void);
extern void test_mutex_basic(void);
extern void test_channel_basic(void);
extern void test_channel_wrap(void);
extern void test_gc_coro_tracing(void);
extern void test_concurrency_opcodes(void);
extern void test_concurrency_type_names(void);

static void run_concurrency_tests(void) {
    printf("\n[Concurrency]\n");
    RUN(test_concurrency_opcodes);
    RUN(test_concurrency_type_names);
    RUN(test_coro_create);
    RUN(test_coro_yield_resume);
    RUN(test_coro_completion);
    RUN(test_coro_resume_dead);
    RUN(test_mutex_basic);
    RUN(test_channel_basic);
    RUN(test_channel_wrap);
    RUN(test_gc_coro_tracing);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== Arcana Test Suite ===\n\n");

    run_bytecode_tests();
    run_vm_tests();
    run_vm_collection_tests();
    run_graph_tests();
    run_pipeline_tests();
    run_infra_tests();
    run_gc_tests();
    run_runtime_tests();
    run_error_recovery_tests();
    run_cycle_tests();
    run_closure_tests();
    run_challenge_L0_tests();
    run_service_tests();
    run_challenge_L1_tests();
    run_concurrency_tests();

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FAILED", tests_failed);
    printf(" ===\n");

    return tests_failed > 0 ? 1 : 0;
}
