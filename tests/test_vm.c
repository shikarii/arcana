/*
 * Arcana test suite -- pure VM execution tests (hand-assembled bytecode):
 * arithmetic, comparisons, branches, locals, globals, strings, bitwise, casts.
 * Arrays, maps, exceptions, intrinsics, closures are in test_vm_collections.c.
 */
#include "test_harness.h"

/* Helper: build a single-function image, run it, return result.
 * Caller must call arc_vm_destroy() on the VM when done. */
static ArcVm run_bytecode(ArcBytecodeImage* img, ArcBuf* code, uint16_t local_count, uint16_t max_stack) {
    uint16_t name = arc_const_pool_add_string(&img->constants, "main", 4);
    img->code = code->data; img->code_len = (uint32_t)code->len;
    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = local_count,
                         .max_stack = max_stack, .code_offset = 0, .code_length = img->code_len };
    arc_func_table_add(&img->functions, fr);
    ArcVm vm;
    arc_vm_init(&vm, img);
    vm.output = fopen(DEV_NULL, "w");  /* suppress output */
    arc_vm_run(&vm);
    return vm;
}

static void cleanup_vm_test(ArcVm* vm, ArcBytecodeImage* img, ArcBuf* code) {
    if (vm->output && vm->output != stdout) fclose(vm->output);
    arc_vm_destroy(vm);
    if (code) ARC_FREE(code->data);
    arc_const_pool_free(&img->constants);
    arc_func_table_free(&img->functions);
    arc_debug_table_free(&img->debug);
}

/* ================================================================
 * Milestone C: VM execution
 * ================================================================ */

TEST(test_vm_add_5_10) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c0 = arc_const_pool_add_i64(&img.constants, 5);
    uint16_t c1 = arc_const_pool_add_i64(&img.constants, 10);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c0);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c1);
    arc_buf_push(&code, OP_ADD);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 2, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVm vm; arc_vm_init(&vm, &img);
    ArcStatus s = arc_vm_run(&vm);
    ASSERT(s == ARC_OK);
    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 15);

    arc_vm_destroy(&vm);
    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_vm_arithmetic) {
    /* (5 + 10) * 3 - 2 = 43 */
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c5 = arc_const_pool_add_i64(&img.constants, 5);
    uint16_t c10 = arc_const_pool_add_i64(&img.constants, 10);
    uint16_t c3 = arc_const_pool_add_i64(&img.constants, 3);
    uint16_t c2 = arc_const_pool_add_i64(&img.constants, 2);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c5);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c10);
    arc_buf_push(&code, OP_ADD);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c3);
    arc_buf_push(&code, OP_MUL);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c2);
    arc_buf_push(&code, OP_SUB);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 3, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVm vm; arc_vm_init(&vm, &img);
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 43);

    arc_vm_destroy(&vm);
    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_vm_comparisons) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c5 = arc_const_pool_add_i64(&img.constants, 5);
    uint16_t c3 = arc_const_pool_add_i64(&img.constants, 3);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c5);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c3);
    arc_buf_push(&code, OP_GT);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 2, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVm vm; arc_vm_init(&vm, &img);
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    ArcValue r = arc_vm_result(&vm);
    ASSERT(r.tag == VAL_BOOL);
    ASSERT(r.as.b == true);

    arc_vm_destroy(&vm);
    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_vm_branch) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t ct = arc_const_pool_add_bool(&img.constants, true);
    uint16_t c42 = arc_const_pool_add_i64(&img.constants, 42);
    uint16_t c99 = arc_const_pool_add_i64(&img.constants, 99);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ct);
    arc_buf_push(&code, OP_JUMP_IF_FALSE); arc_buf_push_i32(&code, 8);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c42);
    arc_buf_push(&code, OP_JUMP); arc_buf_push_i32(&code, 3);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c99);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 1, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVm vm; arc_vm_init(&vm, &img);
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 42);

    arc_vm_destroy(&vm);
    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_vm_locals) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c5 = arc_const_pool_add_i64(&img.constants, 5);
    uint16_t c10 = arc_const_pool_add_i64(&img.constants, 10);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c5);
    arc_buf_push(&code, OP_STORE_LOCAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c10);
    arc_buf_push(&code, OP_STORE_LOCAL); arc_buf_push_u16(&code, 1);
    arc_buf_push(&code, OP_LOAD_LOCAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_LOAD_LOCAL); arc_buf_push_u16(&code, 1);
    arc_buf_push(&code, OP_ADD);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 2,
                         .max_stack = 4, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVm vm; arc_vm_init(&vm, &img);
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 15);

    arc_vm_destroy(&vm);
    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_vm_division_by_zero) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c5 = arc_const_pool_add_i64(&img.constants, 5);
    uint16_t c0 = arc_const_pool_add_i64(&img.constants, 0);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c5);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c0);
    arc_buf_push(&code, OP_DIV);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ASSERT(vm.error.code == ARC_ERR_RUNTIME);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_stack_overflow) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c42 = arc_const_pool_add_i64(&img.constants, 42);
    ArcBuf code; arc_buf_init(&code);
    for (int i = 0; i < 1025; i++) {
        arc_buf_push(&code, OP_CONST);
        arc_buf_push_u16(&code, c42);
    }
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 1025);
    ASSERT(vm.error.code == ARC_ERR_RUNTIME);
    ASSERT(strstr(vm.error.message, "stack overflow") != NULL);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_clock_intrinsic) {
    ArcBytecodeImage img; arc_image_init(&img);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_INTRINSIC);
    arc_buf_push_u16(&code, ARC_INTRINSIC_CLOCK);
    arc_buf_push(&code, 0); arc_buf_push(&code, 0);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 1);
    ASSERT(vm.error.code == ARC_OK);
    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_F64);
    ASSERT(result.as.f64 >= 0.0);
    cleanup_vm_test(&vm, &img, &code);
}

/* ================================================================
 * Globals Support
 * ================================================================ */

TEST(test_vm_globals) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c42 = arc_const_pool_add_i64(&img.constants, 42);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c42);
    arc_buf_push(&code, OP_STORE_GLOBAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_LOAD_GLOBAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 1);
    ASSERT(vm.error.code == ARC_OK);
    ASSERT(arc_vm_result(&vm).tag == VAL_I64);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 42);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_string_const) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t cs = arc_const_pool_add_string(&img.constants, "hello", 5);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cs);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 1);
    ASSERT(vm.error.code == ARC_OK);
    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_OBJ);
    ASSERT(result.as.obj != NULL);
    ASSERT(ARC_OBJ_TYPE(result.as.obj) == OBJ_STRING);
    ASSERT(strcmp(ARC_AS_STRING(result)->data, "hello") == 0);
    cleanup_vm_test(&vm, &img, &code);
}

/* --- String ops --- */

TEST(test_vm_string_concat) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c1 = arc_const_pool_add_string(&img.constants, "hello ", 6);
    uint16_t c2 = arc_const_pool_add_string(&img.constants, "world", 5);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c1);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c2);
    arc_buf_push(&code, OP_ADD);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ArcValue r = arc_vm_result(&vm);
    ASSERT(ARC_IS_STRING(r));
    ASSERT(strcmp(ARC_AS_STRING(r)->data, "hello world") == 0);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_str_len) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t cs = arc_const_pool_add_string(&img.constants, "abcde", 5);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cs);
    arc_buf_push(&code, OP_STR_LEN);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 5);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_str_slice) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t cs = arc_const_pool_add_string(&img.constants, "hello world", 11);
    uint16_t c0 = arc_const_pool_add_i64(&img.constants, 0);
    uint16_t c5 = arc_const_pool_add_i64(&img.constants, 5);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cs);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c0);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c5);
    arc_buf_push(&code, OP_STR_SLICE);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 3);
    ArcValue r = arc_vm_result(&vm);
    ASSERT(ARC_IS_STRING(r));
    ASSERT(strcmp(ARC_AS_STRING(r)->data, "hello") == 0);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_str_index) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t cs = arc_const_pool_add_string(&img.constants, "abc", 3);
    uint16_t c1 = arc_const_pool_add_i64(&img.constants, 1);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cs);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c1);
    arc_buf_push(&code, OP_STR_INDEX);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ArcValue r = arc_vm_result(&vm);
    ASSERT(ARC_IS_STRING(r));
    ASSERT(strcmp(ARC_AS_STRING(r)->data, "b") == 0);
    cleanup_vm_test(&vm, &img, &code);
}

/* --- Bitwise ops --- */

TEST(test_vm_bitwise_and) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t ca = arc_const_pool_add_i64(&img.constants, 0xFF);
    uint16_t cb = arc_const_pool_add_i64(&img.constants, 0x0F);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ca);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cb);
    arc_buf_push(&code, OP_BIT_AND);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 0x0F);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_bitwise_or_xor) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t ca = arc_const_pool_add_i64(&img.constants, 0xA0);
    uint16_t cb = arc_const_pool_add_i64(&img.constants, 0x0B);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ca);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cb);
    arc_buf_push(&code, OP_BIT_OR);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 0xAB);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_bitwise_not) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t ca = arc_const_pool_add_i64(&img.constants, 0);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ca);
    arc_buf_push(&code, OP_BIT_NOT);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 1);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, -1);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_shift) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c1 = arc_const_pool_add_i64(&img.constants, 1);
    uint16_t c4 = arc_const_pool_add_i64(&img.constants, 4);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c1);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c4);
    arc_buf_push(&code, OP_SHL);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 16);
    cleanup_vm_test(&vm, &img, &code);
}

/* --- Type casts --- */

TEST(test_vm_cast_i64) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t cf = arc_const_pool_add_f64(&img.constants, 3.7);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cf);
    arc_buf_push(&code, OP_CAST_I64);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 1);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 3);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_cast_f64) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t ci = arc_const_pool_add_i64(&img.constants, 42);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ci);
    arc_buf_push(&code, OP_CAST_F64);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 1);
    ArcValue r = arc_vm_result(&vm);
    ASSERT(r.tag == VAL_F64);
    ASSERT(r.as.f64 == 42.0);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_cast_str) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t ci = arc_const_pool_add_i64(&img.constants, 123);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ci);
    arc_buf_push(&code, OP_CAST_STR);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 1);
    ArcValue r = arc_vm_result(&vm);
    ASSERT(ARC_IS_STRING(r));
    ASSERT(strcmp(ARC_AS_STRING(r)->data, "123") == 0);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_cast_str_from_string) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t cs = arc_const_pool_add_string(&img.constants, "42", 2);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cs);
    arc_buf_push(&code, OP_CAST_I64);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 1);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 42);
    cleanup_vm_test(&vm, &img, &code);
}
