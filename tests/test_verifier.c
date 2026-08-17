/*
 * Arcana test suite — bytecode verifier tests.
 * Split from test_all.c for modularity.
 */
#include "test_harness.h"

TEST(test_verifier_valid) {
    ArcBytecodeImage img;
    arc_image_init(&img);

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

    ArcVerifyResult vr = arc_verify(&img);
    ASSERT(vr.valid);
    ASSERT(vr.error_count == 0);

    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_verifier_invalid_const_index) {
    ArcBytecodeImage img;
    arc_image_init(&img);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, 99); /* invalid! */
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 1, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVerifyResult vr = arc_verify(&img);
    ASSERT(!vr.valid);
    ASSERT(vr.error_count > 0);

    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_verifier_stack_underflow) {
    ArcBytecodeImage img;
    arc_image_init(&img);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_ADD); /* stack is empty! */
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 0, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVerifyResult vr = arc_verify(&img);
    ASSERT(!vr.valid);

    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_verifier_bad_jump_target) {
    ArcBytecodeImage img;
    arc_image_init(&img);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_JUMP);
    arc_buf_push_i32(&code, 999);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 0, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVerifyResult vr = arc_verify(&img);
    ASSERT(!vr.valid);

    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_verifier_stack_consistency) {
    /* The verifier should check stack height consistency at jump targets.
     * Build valid code with a conditional — should pass verification. */
    ArcBytecodeImage img;
    arc_image_init(&img);

    uint16_t c_true = arc_const_pool_add_bool(&img.constants, true);
    uint16_t c_42 = arc_const_pool_add_i64(&img.constants, 42);
    uint16_t c_99 = arc_const_pool_add_i64(&img.constants, 99);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    /* offset 0: CONST true  (3 bytes)  sp: 0->1 */
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c_true);
    /* offset 3: JUMP_IF_FALSE +8  (5 bytes)  sp: 1->0, target=16 */
    arc_buf_push(&code, OP_JUMP_IF_FALSE); arc_buf_push_i32(&code, 8);
    /* offset 8: CONST 42  (3 bytes)  sp: 0->1 */
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c_42);
    /* offset 11: JUMP +3  (5 bytes)  sp: 1->1, target=19 */
    arc_buf_push(&code, OP_JUMP); arc_buf_push_i32(&code, 3);
    /* offset 16: CONST 99  (3 bytes)  sp: 0->1 */
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c_99);
    /* offset 19: HALT  sp: 1 */
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 1, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVerifyResult vr = arc_verify(&img);
    ASSERT(vr.valid);

    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}
