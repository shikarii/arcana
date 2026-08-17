/*
 * Arcana test suite -- VM tests for arrays, maps, exceptions,
 * intrinsics, and closures/upvalues.
 * Split from test_vm.c to stay under 600-line file limit.
 */
#include "test_harness.h"

/* Shared helpers (duplicated from test_vm.c for link independence) */
static ArcVm run_bytecode(ArcBytecodeImage* img, ArcBuf* code, uint16_t local_count, uint16_t max_stack) {
    uint16_t name = arc_const_pool_add_string(&img->constants, "main", 4);
    img->code = code->data; img->code_len = (uint32_t)code->len;
    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = local_count,
                         .max_stack = max_stack, .code_offset = 0, .code_length = img->code_len };
    arc_func_table_add(&img->functions, fr);
    ArcVm vm;
    arc_vm_init(&vm, img);
    vm.output = fopen(DEV_NULL, "w");
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
 * Arrays
 * ================================================================ */

TEST(test_vm_array_new) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c1 = arc_const_pool_add_i64(&img.constants, 10);
    uint16_t c2 = arc_const_pool_add_i64(&img.constants, 20);
    uint16_t c3 = arc_const_pool_add_i64(&img.constants, 30);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c1);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c2);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c3);
    arc_buf_push(&code, OP_ARRAY_NEW); arc_buf_push_u16(&code, 3);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 4);
    ArcValue r = arc_vm_result(&vm);
    ASSERT(ARC_IS_ARRAY(r));
    ArcObjArray* arr = ARC_AS_ARRAY(r);
    ASSERT(arr->count == 3);
    ASSERT_EQ_I64(arr->items[0].as.i64, 10);
    ASSERT_EQ_I64(arr->items[1].as.i64, 20);
    ASSERT_EQ_I64(arr->items[2].as.i64, 30);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_index_get_set) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c1 = arc_const_pool_add_i64(&img.constants, 10);
    uint16_t c2 = arc_const_pool_add_i64(&img.constants, 20);
    uint16_t c0 = arc_const_pool_add_i64(&img.constants, 0);
    uint16_t c99 = arc_const_pool_add_i64(&img.constants, 99);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c1);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c2);
    arc_buf_push(&code, OP_ARRAY_NEW); arc_buf_push_u16(&code, 2);
    arc_buf_push(&code, OP_STORE_LOCAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_LOAD_LOCAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c0);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c99);
    arc_buf_push(&code, OP_INDEX_SET);
    arc_buf_push(&code, OP_LOAD_LOCAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c0);
    arc_buf_push(&code, OP_INDEX_GET);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 1, 4);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 99);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_array_length) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c1 = arc_const_pool_add_i64(&img.constants, 1);
    uint16_t c2 = arc_const_pool_add_i64(&img.constants, 2);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c1);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c2);
    arc_buf_push(&code, OP_ARRAY_NEW); arc_buf_push_u16(&code, 2);
    arc_buf_push(&code, OP_LENGTH);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 3);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 2);
    cleanup_vm_test(&vm, &img, &code);
}

/* ================================================================
 * Maps
 * ================================================================ */

TEST(test_vm_map_new) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t ck = arc_const_pool_add_string(&img.constants, "x", 1);
    uint16_t cv = arc_const_pool_add_i64(&img.constants, 42);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ck);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cv);
    arc_buf_push(&code, OP_MAP_NEW); arc_buf_push_u16(&code, 1);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 3);
    ArcValue r = arc_vm_result(&vm);
    ASSERT(ARC_IS_MAP(r));
    ArcObjMap* map = ARC_AS_MAP(r);
    ASSERT(map->count == 1);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_map_get) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t ck = arc_const_pool_add_string(&img.constants, "x", 1);
    uint16_t cv = arc_const_pool_add_i64(&img.constants, 42);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ck);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cv);
    arc_buf_push(&code, OP_MAP_NEW); arc_buf_push_u16(&code, 1);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ck);
    arc_buf_push(&code, OP_INDEX_GET);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 3);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 42);
    cleanup_vm_test(&vm, &img, &code);
}

/* ================================================================
 * Exception Handling
 * ================================================================ */

TEST(test_vm_try_catch_basic) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c42 = arc_const_pool_add_i64(&img.constants, 42);
    uint16_t c99 = arc_const_pool_add_i64(&img.constants, 99);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_TRY_BEGIN);
    uint32_t try_ip = (uint32_t)code.len;
    arc_buf_push_i32(&code, 0);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c42);
    arc_buf_push(&code, OP_THROW);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c99);
    arc_buf_push(&code, OP_HALT);
    uint32_t catch_ip = (uint32_t)code.len;
    arc_buf_push(&code, OP_HALT);
    int32_t offset = (int32_t)catch_ip - (int32_t)(try_ip + 4);
    code.data[try_ip] = (uint8_t)(offset & 0xFF);
    code.data[try_ip+1] = (uint8_t)((offset >> 8) & 0xFF);
    code.data[try_ip+2] = (uint8_t)((offset >> 16) & 0xFF);
    code.data[try_ip+3] = (uint8_t)((offset >> 24) & 0xFF);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 42);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_try_end_no_throw) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c99 = arc_const_pool_add_i64(&img.constants, 99);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_TRY_BEGIN);
    uint32_t try_ip = (uint32_t)code.len;
    arc_buf_push_i32(&code, 0);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c99);
    arc_buf_push(&code, OP_TRY_END);
    arc_buf_push(&code, OP_HALT);
    uint32_t catch_ip = (uint32_t)code.len;
    arc_buf_push(&code, OP_HALT);
    int32_t offset = (int32_t)catch_ip - (int32_t)(try_ip + 4);
    code.data[try_ip] = (uint8_t)(offset & 0xFF);
    code.data[try_ip+1] = (uint8_t)((offset >> 8) & 0xFF);
    code.data[try_ip+2] = (uint8_t)((offset >> 16) & 0xFF);
    code.data[try_ip+3] = (uint8_t)((offset >> 24) & 0xFF);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 99);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_unhandled_throw) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c1 = arc_const_pool_add_i64(&img.constants, 1);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c1);
    arc_buf_push(&code, OP_THROW);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 1);
    ASSERT(vm.error.code == ARC_ERR_RUNTIME);
    ASSERT(strstr(vm.error.message, "unhandled") != NULL);
    cleanup_vm_test(&vm, &img, &code);
}

/* ================================================================
 * Intrinsics: type, assert, len, push, keys
 * ================================================================ */

TEST(test_vm_intrinsic_type) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t ci = arc_const_pool_add_i64(&img.constants, 5);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ci);
    arc_buf_push(&code, OP_INTRINSIC);
    arc_buf_push_u16(&code, ARC_INTRINSIC_TYPE);
    arc_buf_push(&code, 1); arc_buf_push(&code, 0);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ArcValue r = arc_vm_result(&vm);
    ASSERT(ARC_IS_STRING(r));
    ASSERT(strcmp(ARC_AS_STRING(r)->data, "i64") == 0);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_intrinsic_assert_pass) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t ct = arc_const_pool_add_bool(&img.constants, true);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ct);
    arc_buf_push(&code, OP_INTRINSIC);
    arc_buf_push_u16(&code, ARC_INTRINSIC_ASSERT);
    arc_buf_push(&code, 1); arc_buf_push(&code, 0);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ASSERT(vm.error.code == ARC_OK);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_intrinsic_assert_fail) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t cf = arc_const_pool_add_bool(&img.constants, false);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cf);
    arc_buf_push(&code, OP_INTRINSIC);
    arc_buf_push_u16(&code, ARC_INTRINSIC_ASSERT);
    arc_buf_push(&code, 1); arc_buf_push(&code, 0);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ASSERT(vm.error.code == ARC_ERR_RUNTIME);
    ASSERT(strstr(vm.error.message, "assertion failed") != NULL);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_intrinsic_len) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t cs = arc_const_pool_add_string(&img.constants, "test", 4);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cs);
    arc_buf_push(&code, OP_INTRINSIC);
    arc_buf_push_u16(&code, ARC_INTRINSIC_LEN);
    arc_buf_push(&code, 1); arc_buf_push(&code, 0);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 2);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 4);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_intrinsic_push) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t c1 = arc_const_pool_add_i64(&img.constants, 10);
    uint16_t c2 = arc_const_pool_add_i64(&img.constants, 20);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c1);
    arc_buf_push(&code, OP_ARRAY_NEW); arc_buf_push_u16(&code, 1);
    arc_buf_push(&code, OP_STORE_LOCAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_LOAD_LOCAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c2);
    arc_buf_push(&code, OP_INTRINSIC);
    arc_buf_push_u16(&code, ARC_INTRINSIC_PUSH);
    arc_buf_push(&code, 2); arc_buf_push(&code, 0);
    arc_buf_push(&code, OP_POP);
    arc_buf_push(&code, OP_LOAD_LOCAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_LENGTH);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 1, 4);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 2);
    cleanup_vm_test(&vm, &img, &code);
}

TEST(test_vm_intrinsic_keys) {
    ArcBytecodeImage img; arc_image_init(&img);
    uint16_t ck1 = arc_const_pool_add_string(&img.constants, "a", 1);
    uint16_t cv1 = arc_const_pool_add_i64(&img.constants, 1);
    uint16_t ck2 = arc_const_pool_add_string(&img.constants, "b", 1);
    uint16_t cv2 = arc_const_pool_add_i64(&img.constants, 2);
    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ck1);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cv1);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ck2);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cv2);
    arc_buf_push(&code, OP_MAP_NEW); arc_buf_push_u16(&code, 2);
    arc_buf_push(&code, OP_INTRINSIC);
    arc_buf_push_u16(&code, ARC_INTRINSIC_KEYS);
    arc_buf_push(&code, 1); arc_buf_push(&code, 0);
    arc_buf_push(&code, OP_LENGTH);
    arc_buf_push(&code, OP_HALT);
    ArcVm vm = run_bytecode(&img, &code, 0, 5);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 2);
    cleanup_vm_test(&vm, &img, &code);
}

/* ================================================================
 * Upvalue/Closure
 * ================================================================ */

TEST(test_upvalue_format_roundtrip) {
    ArcBytecodeImage img;
    arc_image_init(&img);

    uint16_t name_idx = arc_const_pool_add_string(&img.constants, "closure_fn", 10);
    ArcFuncRecord fr = {
        .name_const_idx = name_idx, .arity = 0, .local_count = 1,
        .max_stack = 4, .code_offset = 0, .code_length = 0,
        .upvalue_count = 2, .upvalues = NULL
    };
    fr.upvalues = ARC_ALLOC(ArcUpvalueDesc, 2);
    fr.upvalues[0] = (ArcUpvalueDesc){ .is_local = true, .index = 3 };
    fr.upvalues[1] = (ArcUpvalueDesc){ .is_local = false, .index = 0 };
    arc_func_table_add(&img.functions, fr);

    uint16_t main_name = arc_const_pool_add_string(&img.constants, "main", 4);
    ArcFuncRecord main_fr = {
        .name_const_idx = main_name, .arity = 0, .local_count = 0,
        .max_stack = 1, .code_offset = 0, .code_length = 1,
        .upvalue_count = 0, .upvalues = NULL
    };
    arc_func_table_add(&img.functions, main_fr);

    img.code = ARC_ALLOC(uint8_t, 1);
    img.code[0] = OP_HALT;
    img.code_len = 1;

    uint8_t* data; size_t len;
    ASSERT(arc_image_write(&img, &data, &len) == ARC_OK);

    ArcBytecodeImage img2;
    ASSERT(arc_image_read(data, len, &img2) == ARC_OK);
    ASSERT(img2.functions.count == 2);
    ASSERT(img2.functions.funcs[0].upvalue_count == 2);
    ASSERT(img2.functions.funcs[0].upvalues[0].is_local == true);
    ASSERT(img2.functions.funcs[0].upvalues[0].index == 3);
    ASSERT(img2.functions.funcs[0].upvalues[1].is_local == false);
    ASSERT(img2.functions.funcs[0].upvalues[1].index == 0);
    ASSERT(img2.functions.funcs[1].upvalue_count == 0);

    ARC_FREE(data);
    arc_image_free(&img);
    arc_image_free(&img2);
}

/* Build inner function code and main code for closure test */
static void build_closure_code(ArcBuf* main_code, ArcBuf* inner_code, ArcBuf* combined,
                               uint16_t ci_10, uint32_t* main_len, uint32_t* inner_off) {
    arc_buf_init(inner_code);
    arc_buf_push(inner_code, OP_GET_UPVAL);
    arc_buf_push_u16(inner_code, 0);
    arc_buf_push(inner_code, OP_RETURN);

    arc_buf_init(main_code);
    arc_buf_push(main_code, OP_CONST);
    arc_buf_push_u16(main_code, ci_10);
    arc_buf_push(main_code, OP_STORE_LOCAL);
    arc_buf_push_u16(main_code, 0);
    arc_buf_push(main_code, OP_HALT);

    *main_len = (uint32_t)main_code->len;
    *inner_off = *main_len;

    arc_buf_init(combined);
    for (size_t i = 0; i < main_code->len; i++) arc_buf_push(combined, main_code->data[i]);
    for (size_t i = 0; i < inner_code->len; i++) arc_buf_push(combined, inner_code->data[i]);
}

TEST(test_vm_closure_upvalue) {
    ArcBytecodeImage img;
    arc_image_init(&img);

    uint16_t ci_10 = arc_const_pool_add_i64(&img.constants, 10);
    arc_const_pool_add_null(&img.constants);
    uint16_t main_name = arc_const_pool_add_string(&img.constants, "main", 4);
    uint16_t inner_name = arc_const_pool_add_string(&img.constants, "inner", 5);

    ArcBuf main_code, inner_code, code;
    uint32_t main_len, inner_off;
    build_closure_code(&main_code, &inner_code, &code, ci_10, &main_len, &inner_off);

    img.code = code.data;
    img.code_len = (uint32_t)code.len;

    ArcFuncRecord main_fr = {
        .name_const_idx = main_name, .arity = 0, .local_count = 1,
        .max_stack = 4, .code_offset = 0, .code_length = main_len,
        .upvalue_count = 0, .upvalues = NULL
    };
    arc_func_table_add(&img.functions, main_fr);

    ArcUpvalueDesc* descs = ARC_ALLOC(ArcUpvalueDesc, 1);
    descs[0] = (ArcUpvalueDesc){ .is_local = true, .index = 0 };
    ArcFuncRecord inner_fr = {
        .name_const_idx = inner_name, .arity = 0, .local_count = 0,
        .max_stack = 2, .code_offset = inner_off, .code_length = (uint32_t)inner_code.len,
        .upvalue_count = 1, .upvalues = descs
    };
    arc_func_table_add(&img.functions, inner_fr);

    ArcVm vm; arc_vm_init(&vm, &img);
    vm.output = fopen(DEV_NULL, "w");
    ASSERT(arc_vm_run(&vm) == ARC_OK);

    ArcValue result = vm.stack[0];
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 10);

    fclose(vm.output); vm.output = NULL;
    arc_vm_destroy(&vm);
    ARC_FREE(main_code.data);
    ARC_FREE(inner_code.data);
    ARC_FREE(code.data);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}
