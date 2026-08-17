/*
 * Arcana test suite — opcode metadata, constant pool, format,
 * and serialization tests.
 * Split from test_all.c for modularity.
 */
#include "test_harness.h"

/* ================================================================
 * Milestone A: Opcode definitions
 * ================================================================ */

TEST(test_opcode_mnemonic) {
    ASSERT(strcmp(arc_op_mnemonic(OP_CONST), "const") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_ADD), "add") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_HALT), "halt") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_CALL), "call") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_RETURN), "return") == 0);
    ASSERT(strcmp(arc_op_mnemonic(0xAA), "???") == 0); /* unknown */
}

TEST(test_opcode_operand_bytes) {
    ASSERT(arc_op_operand_bytes(OP_CONST) == 2);
    ASSERT(arc_op_operand_bytes(OP_ADD) == 0);
    ASSERT(arc_op_operand_bytes(OP_JUMP) == 4);
    ASSERT(arc_op_operand_bytes(OP_HALT) == 0);
    ASSERT(arc_op_operand_bytes(OP_CALL) == 4);
    ASSERT(arc_op_operand_bytes(0xAA) == -1); /* unknown */
}

TEST(test_opcode_stack_effects) {
    ASSERT(arc_op_pops(OP_ADD) == 2);
    ASSERT(arc_op_pushes(OP_ADD) == 1);
    ASSERT(arc_op_pops(OP_CONST) == 0);
    ASSERT(arc_op_pushes(OP_CONST) == 1);
    ASSERT(arc_op_pops(OP_HALT) == 0);
    ASSERT(arc_op_pushes(OP_HALT) == 0);
}

/* ================================================================
 * Milestone B: Constant pool, format, serialize/deserialize
 * ================================================================ */

TEST(test_const_pool) {
    ArcConstPool pool;
    arc_const_pool_init(&pool);

    uint16_t i0 = arc_const_pool_add_i64(&pool, 42);
    uint16_t i1 = arc_const_pool_add_i64(&pool, -7);
    uint16_t i2 = arc_const_pool_add_string(&pool, "hello", 5);

    ASSERT(i0 == 0);
    ASSERT(i1 == 1);
    ASSERT(i2 == 2);
    ASSERT(pool.count == 3);
    ASSERT(pool.entries[0].tag == ARC_CONST_I64);
    ASSERT(pool.entries[0].as.i64 == 42);
    ASSERT(pool.entries[2].tag == ARC_CONST_STRING);
    ASSERT(strcmp(pool.entries[2].as.str.data, "hello") == 0);

    arc_const_pool_free(&pool);
}

TEST(test_image_serialize_roundtrip) {
    /* Build a simple image: const 5, const 10, add, intrinsic print 1, halt */
    ArcBytecodeImage img;
    arc_image_init(&img);

    uint16_t c0 = arc_const_pool_add_i64(&img.constants, 5);
    uint16_t c1 = arc_const_pool_add_i64(&img.constants, 10);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c0);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c1);
    arc_buf_push(&code, OP_ADD);
    arc_buf_push(&code, OP_INTRINSIC); arc_buf_push_u16(&code, ARC_INTRINSIC_PRINT);
    arc_buf_push(&code, 1); arc_buf_push(&code, 0);
    arc_buf_push(&code, OP_HALT);

    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 2, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    /* Serialize */
    uint8_t* bytes; size_t blen;
    ArcStatus s = arc_image_write(&img, &bytes, &blen);
    ASSERT(s == ARC_OK);
    ASSERT(blen > 12); /* at least header */

    /* Check magic */
    ASSERT(bytes[0] == 'A' && bytes[1] == 'R' && bytes[2] == 'C' && bytes[3] == 'A');

    /* Deserialize */
    ArcBytecodeImage img2;
    s = arc_image_read(bytes, blen, &img2);
    ASSERT(s == ARC_OK);
    ASSERT(img2.constants.count == 3);
    ASSERT(img2.constants.entries[0].as.i64 == 5);
    ASSERT(img2.constants.entries[1].as.i64 == 10);
    ASSERT(img2.functions.count == 1);
    ASSERT(img2.code_len == img.code_len);

    /* Verify code bytes match */
    for (uint32_t i = 0; i < img.code_len; i++)
        ASSERT(img2.code[i] == img.code[i]);

    ARC_FREE(bytes);
    arc_image_free(&img2);
    /* Don't free img -- code is owned by the buf */
    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

/* ================================================================
 * Edge cases: malformed binary
 * ================================================================ */

TEST(test_malformed_binary) {
    uint8_t garbage[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03 };
    ArcBytecodeImage img;
    ArcStatus s = arc_image_read(garbage, sizeof(garbage), &img);
    ASSERT(s == ARC_ERR_FORMAT);
}

TEST(test_truncated_binary) {
    uint8_t trunc[] = { 'A', 'R', 'C', 'A', 0, 1 };
    ArcBytecodeImage img;
    ArcStatus s = arc_image_read(trunc, sizeof(trunc), &img);
    ASSERT(s == ARC_ERR_FORMAT);
}

TEST(test_serialization_roundtrip_with_debug) {
    ArcBytecodeImage img;
    arc_image_init(&img);

    uint16_t c5 = arc_const_pool_add_i64(&img.constants, 5);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c5);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 1, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcDebugEntry de = { .func_idx = 0, .bc_start = 0, .bc_end = 3, .element_id = 42 };
    arc_debug_table_add(&img.debug, de);

    uint8_t* bytes; size_t blen;
    ASSERT(arc_image_write(&img, &bytes, &blen) == ARC_OK);

    ArcBytecodeImage img2;
    ASSERT(arc_image_read(bytes, blen, &img2) == ARC_OK);
    ASSERT(img2.debug.count == 1);
    ASSERT(img2.debug.entries[0].func_idx == 0);
    ASSERT(img2.debug.entries[0].bc_start == 0);
    ASSERT(img2.debug.entries[0].bc_end == 3);
    ASSERT(img2.debug.entries[0].element_id == 42);

    ARC_FREE(bytes);
    arc_image_free(&img2);
    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_const_pool_dedup) {
    ArcConstPool pool;
    arc_const_pool_init(&pool);

    uint16_t a = arc_const_pool_add_i64(&pool, 42);
    uint16_t b = arc_const_pool_add_i64(&pool, 42);
    ASSERT(a == b);
    ASSERT(pool.count == 1);

    uint16_t c = arc_const_pool_add_string(&pool, "hello", 5);
    uint16_t d = arc_const_pool_add_string(&pool, "hello", 5);
    ASSERT(c == d);
    ASSERT(pool.count == 2);

    arc_const_pool_free(&pool);
}

/* ================================================================
 * Property Tests
 * ================================================================ */

/* Compare two constant pools entry-by-entry */
static void assert_pools_equal(const ArcConstPool* a, const ArcConstPool* b) {
    ASSERT(a->count == b->count);
    for (uint16_t i = 0; i < a->count; i++) {
        ASSERT(b->entries[i].tag == a->entries[i].tag);
        switch (a->entries[i].tag) {
        case ARC_CONST_I64:
            ASSERT(b->entries[i].as.i64 == a->entries[i].as.i64); break;
        case ARC_CONST_F64:
            ASSERT(memcmp(&b->entries[i].as.f64, &a->entries[i].as.f64, 8) == 0); break;
        case ARC_CONST_STRING:
            ASSERT(b->entries[i].as.str.len == a->entries[i].as.str.len);
            ASSERT(memcmp(b->entries[i].as.str.data, a->entries[i].as.str.data,
                          a->entries[i].as.str.len) == 0); break;
        default: break;
        }
    }
}

/* Compare two debug tables entry-by-entry */
static void assert_debug_equal(const ArcDebugTable* a, const ArcDebugTable* b) {
    ASSERT(a->count == b->count);
    for (uint32_t i = 0; i < a->count; i++) {
        ASSERT(b->entries[i].func_idx == a->entries[i].func_idx);
        ASSERT(b->entries[i].bc_start == a->entries[i].bc_start);
        ASSERT(b->entries[i].bc_end == a->entries[i].bc_end);
        ASSERT(b->entries[i].element_id == a->entries[i].element_id);
    }
}

TEST(test_property_roundtrip_random) {
    /* Build an image with varied constants, serialize, deserialize, compare */
    ArcBytecodeImage img;
    arc_image_init(&img);

    uint16_t c_null = arc_const_pool_add_null(&img.constants);
    uint16_t c_true = arc_const_pool_add_bool(&img.constants, true);
    uint16_t c_false = arc_const_pool_add_bool(&img.constants, false);
    uint16_t c_i1 = arc_const_pool_add_i64(&img.constants, 0);
    uint16_t c_i2 = arc_const_pool_add_i64(&img.constants, -1);
    uint16_t c_i3 = arc_const_pool_add_i64(&img.constants, INT64_MAX);
    uint16_t c_i4 = arc_const_pool_add_i64(&img.constants, INT64_MIN);
    uint16_t c_f1 = arc_const_pool_add_f64(&img.constants, 3.14159);
    uint16_t c_f2 = arc_const_pool_add_f64(&img.constants, -0.0);
    uint16_t c_s1 = arc_const_pool_add_string(&img.constants, "test", 4);
    uint16_t c_s2 = arc_const_pool_add_string(&img.constants, "", 0);
    (void)c_null; (void)c_true; (void)c_false; (void)c_i1; (void)c_i2;
    (void)c_i3; (void)c_i4; (void)c_f1; (void)c_f2; (void)c_s1; (void)c_s2;

    uint16_t main_name = arc_const_pool_add_string(&img.constants, "main", 4);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c_i1);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = main_name, .arity = 0, .local_count = 0,
                         .max_stack = 1, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcDebugEntry de1 = { .func_idx = 0, .bc_start = 0, .bc_end = 3, .element_id = 12345 };
    ArcDebugEntry de2 = { .func_idx = 0, .bc_start = 3, .bc_end = 4, .element_id = 0xFFFFFFFFFFFFFFFFULL };
    arc_debug_table_add(&img.debug, de1);
    arc_debug_table_add(&img.debug, de2);

    uint8_t* bytes; size_t blen;
    ASSERT(arc_image_write(&img, &bytes, &blen) == ARC_OK);

    ArcBytecodeImage img2;
    ASSERT(arc_image_read(bytes, blen, &img2) == ARC_OK);

    assert_pools_equal(&img.constants, &img2.constants);
    ASSERT(img2.code_len == img.code_len);
    ASSERT(memcmp(img2.code, img.code, img.code_len) == 0);
    assert_debug_equal(&img.debug, &img2.debug);

    ARC_FREE(bytes);
    arc_image_free(&img2);
    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}
