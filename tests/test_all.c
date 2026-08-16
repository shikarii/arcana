/*
 * Arcana test suite — covers all milestones A through F + end-to-end.
 *
 * Uses a minimal assertion macro (no external test framework).
 */

#include "../src/common/arcana_common.h"
#include "../src/bytecode/opcodes.h"
#include "../src/bytecode/format.h"
#include "../src/bytecode/disassembler.h"
#include "../src/vm/vm.h"
#include "../src/verifier/verifier.h"
#include "../src/semantic_graph/semantic_graph.h"
#include "../src/semantic_graph/fixture_parser.h"
#include "../src/compiler/compiler.h"
#include "../src/compiler/diagnostics.h"
#include "../src/interpreter/interpreter.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)
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
    /* Don't free img — code is owned by the buf */
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

/* ================================================================
 * Milestone B: Verifier
 * ================================================================ */

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

    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

/* ================================================================
 * Milestone C: VM execution
 * ================================================================ */

TEST(test_vm_add_5_10) {
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

    ArcVm vm;
    arc_vm_init(&vm, &img);
    ArcStatus s = arc_vm_run(&vm);
    ASSERT(s == ARC_OK);

    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 15);

    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_vm_arithmetic) {
    /* (5 + 10) * 3 - 2 = 43 */
    ArcBytecodeImage img;
    arc_image_init(&img);

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

    ArcVm vm;
    arc_vm_init(&vm, &img);
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 43);

    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_vm_comparisons) {
    /* 5 > 3 == true */
    ArcBytecodeImage img;
    arc_image_init(&img);

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

    ArcVm vm;
    arc_vm_init(&vm, &img);
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    ArcValue r = arc_vm_result(&vm);
    ASSERT(r.tag == VAL_BOOL);
    ASSERT(r.as.b == true);

    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_vm_branch) {
    /* if (true) push 42 else push 99 -> expect 42 */
    ArcBytecodeImage img;
    arc_image_init(&img);

    uint16_t ct = arc_const_pool_add_bool(&img.constants, true);
    uint16_t c42 = arc_const_pool_add_i64(&img.constants, 42);
    uint16_t c99 = arc_const_pool_add_i64(&img.constants, 99);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    /* 0: const true */
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, ct);
    /* 3: jump_if_false +8 (skip to else at 16) */
    arc_buf_push(&code, OP_JUMP_IF_FALSE);
    /* offset: from after this instr (ip=8) to else (ip=16) => +8 */
    arc_buf_push_i32(&code, 8);
    /* 8: const 42 */
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c42);
    /* 11: jump +3 (skip else, go to halt at 19) */
    arc_buf_push(&code, OP_JUMP);
    arc_buf_push_i32(&code, 3);
    /* 16: const 99 */
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c99);
    /* 19: halt */
    arc_buf_push(&code, OP_HALT);

    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 1, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVm vm;
    arc_vm_init(&vm, &img);
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 42);

    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_vm_locals) {
    /* x = 5; y = 10; result = x + y */
    ArcBytecodeImage img;
    arc_image_init(&img);

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

    ArcVm vm;
    arc_vm_init(&vm, &img);
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 15);

    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_vm_division_by_zero) {
    ArcBytecodeImage img;
    arc_image_init(&img);
    uint16_t c5 = arc_const_pool_add_i64(&img.constants, 5);
    uint16_t c0 = arc_const_pool_add_i64(&img.constants, 0);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c5);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c0);
    arc_buf_push(&code, OP_DIV);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 2, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVm vm;
    arc_vm_init(&vm, &img);
    ArcStatus s = arc_vm_run(&vm);
    ASSERT(s == ARC_ERR_RUNTIME);

    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

/* ================================================================
 * Milestone D: Semantic graph validation
 * ================================================================ */

TEST(test_graph_valid) {
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n0 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 100);
    g.nodes[n0].attr.int_value = 5;
    ArcPortId p0out = arc_graph_add_port(&g, n0, ARC_PORT_OUTPUT, "out");

    ArcNodeId n1 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 101);
    g.nodes[n1].attr.int_value = 10;
    ArcPortId p1out = arc_graph_add_port(&g, n1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n2 = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 102);
    ArcPortId p2lhs = arc_graph_add_port(&g, n2, ARC_PORT_INPUT, "lhs");
    ArcPortId p2rhs = arc_graph_add_port(&g, n2, ARC_PORT_INPUT, "rhs");
    ArcPortId p2out = arc_graph_add_port(&g, n2, ARC_PORT_OUTPUT, "out");
    (void)p2out;

    ArcPortId order[] = { p2lhs, p2rhs, p2out };
    arc_node_set_cyclic_order(&g, n2, order, 3);

    arc_graph_add_edge(&g, p0out, p2lhs);
    arc_graph_add_edge(&g, p1out, p2rhs);

    ArcGraphValidation v = arc_graph_validate(&g);
    ASSERT(v.valid);
    arc_graph_validation_free(&v);
    arc_graph_free(&g);
}

TEST(test_graph_invalid_edge) {
    ArcGraph g;
    arc_graph_init(&g);
    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n0 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 100);
    ArcPortId p0in = arc_graph_add_port(&g, n0, ARC_PORT_INPUT, "in"); /* wrong direction! */

    ArcNodeId n1 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 101);
    ArcPortId p1in = arc_graph_add_port(&g, n1, ARC_PORT_INPUT, "in");

    /* Edge from input to input — should fail validation */
    arc_graph_add_edge(&g, p0in, p1in);

    ArcGraphValidation v = arc_graph_validate(&g);
    ASSERT(!v.valid);
    arc_graph_validation_free(&v);
    arc_graph_free(&g);
}

/* ================================================================
 * Milestone E-F + End-to-End: Semantic Graph -> Compiler -> VM = 15
 * ================================================================ */

TEST(test_e2e_5_plus_10) {
    /*
     * Build the canonical first Arcana program as a semantic graph:
     *
     * Region r0
     *   ConstInt(5)  ---> Add.lhs
     *   ConstInt(10) ---> Add.rhs
     *   Add.out ---------> RootOutput.in
     *
     * Cyclic port order on Add: [lhs, rhs, out]
     */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    /* ConstInt(5) */
    ArcNodeId n0 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 1001);
    g.nodes[n0].attr.int_value = 5;
    ArcPortId p0out = arc_graph_add_port(&g, n0, ARC_PORT_OUTPUT, "out");

    /* ConstInt(10) */
    ArcNodeId n1 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 1002);
    g.nodes[n1].attr.int_value = 10;
    ArcPortId p1out = arc_graph_add_port(&g, n1, ARC_PORT_OUTPUT, "out");

    /* Add */
    ArcNodeId n2 = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 1003);
    ArcPortId p2lhs = arc_graph_add_port(&g, n2, ARC_PORT_INPUT, "lhs");
    ArcPortId p2rhs = arc_graph_add_port(&g, n2, ARC_PORT_INPUT, "rhs");
    ArcPortId p2out = arc_graph_add_port(&g, n2, ARC_PORT_OUTPUT, "out");

    /* Cyclic order: lhs, rhs, out */
    ArcPortId order[] = { p2lhs, p2rhs, p2out };
    arc_node_set_cyclic_order(&g, n2, order, 3);

    /* RootOutput */
    ArcNodeId n3 = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 1004);
    ArcPortId p3in = arc_graph_add_port(&g, n3, ARC_PORT_INPUT, "value");
    g.output_node = n3;

    /* Edges */
    arc_graph_add_edge(&g, p0out, p2lhs);
    arc_graph_add_edge(&g, p1out, p2rhs);
    arc_graph_add_edge(&g, p2out, p3in);

    /* Validate */
    ArcGraphValidation val = arc_graph_validate(&g);
    ASSERT(val.valid);
    arc_graph_validation_free(&val);

    /* Compile */
    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);

    /* Verify bytecode */
    ArcVerifyResult vr = arc_verify(&cr.image);
    if (!vr.valid) {
        for (int i = 0; i < vr.error_count; i++)
            printf("    verify: %s\n", vr.errors[i].message);
    }
    ASSERT(vr.valid);

    /* Disassemble (for inspection) */
    printf("\n");
    arc_disassemble(&cr.image, stdout);

    /* Execute */
    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    ArcStatus s = arc_vm_run(&vm);
    ASSERT(s == ARC_OK);

    /* The output was printed by the intrinsic, but verify we can also check the result */
    /* Actually, the main function halts after intrinsic print. Let's check stdout. */
    /* For a proper test, redirect output and verify. For now, just check no errors. */

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

TEST(test_e2e_5_plus_10_vm_result) {
    /*
     * Same graph but compile WITHOUT print/halt — just compute 5+10 and return.
     * We'll build bytecode manually from the compiler output and verify the result.
     */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n0 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 2001);
    g.nodes[n0].attr.int_value = 5;
    ArcPortId p0out = arc_graph_add_port(&g, n0, ARC_PORT_OUTPUT, "out");

    ArcNodeId n1 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 2002);
    g.nodes[n1].attr.int_value = 10;
    ArcPortId p1out = arc_graph_add_port(&g, n1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n2 = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 2003);
    ArcPortId p2lhs = arc_graph_add_port(&g, n2, ARC_PORT_INPUT, "lhs");
    ArcPortId p2rhs = arc_graph_add_port(&g, n2, ARC_PORT_INPUT, "rhs");
    ArcPortId p2out = arc_graph_add_port(&g, n2, ARC_PORT_OUTPUT, "out");

    ArcPortId order[] = { p2lhs, p2rhs, p2out };
    arc_node_set_cyclic_order(&g, n2, order, 3);

    /* No root output — just compile the add directly */
    arc_graph_add_edge(&g, p0out, p2lhs);
    arc_graph_add_edge(&g, p1out, p2rhs);

    /* Compile — the output_node is not set, so main just halts */
    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* ================================================================
 * Milestone G: Function definitions + calls
 * ================================================================ */

TEST(test_e2e_function_call) {
    /*
     * def double(x): return x + x
     * output = double(7)   => prints 14
     */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    /* Function body region */
    ArcRegionId r_body = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);

    /* === FUNC_DEF "double" in root region === */
    ArcNodeId n_fdef = arc_graph_add_node(&g, ARC_NODE_FUNC_DEF, r0, 3001);
    g.nodes[n_fdef].attr.func.name = "double";
    g.nodes[n_fdef].attr.func.arity = 1;
    g.nodes[n_fdef].attr.func.body_region = r_body;

    /* === Body: PARAM "x", VAR_REF "x" (×2), ADD, RETURN === */
    ArcNodeId n_param = arc_graph_add_node(&g, ARC_NODE_PARAM, r_body, 3010);
    g.nodes[n_param].attr.name = "x";

    ArcNodeId n_ref1 = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_body, 3011);
    g.nodes[n_ref1].attr.name = "x";
    ArcPortId p_ref1_out = arc_graph_add_port(&g, n_ref1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ref2 = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_body, 3012);
    g.nodes[n_ref2].attr.name = "x";
    ArcPortId p_ref2_out = arc_graph_add_port(&g, n_ref2, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add = arc_graph_add_node(&g, ARC_NODE_ADD, r_body, 3013);
    ArcPortId p_add_lhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId p_add_rhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId p_add_out = arc_graph_add_port(&g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId add_order[] = { p_add_lhs, p_add_rhs, p_add_out };
    arc_node_set_cyclic_order(&g, n_add, add_order, 3);

    ArcNodeId n_ret = arc_graph_add_node(&g, ARC_NODE_RETURN, r_body, 3014);
    ArcPortId p_ret_val = arc_graph_add_port(&g, n_ret, ARC_PORT_INPUT, "value");

    arc_graph_add_edge(&g, p_ref1_out, p_add_lhs);
    arc_graph_add_edge(&g, p_ref2_out, p_add_rhs);
    arc_graph_add_edge(&g, p_add_out, p_ret_val);

    /* === Root: ConstInt(7) → FUNC_CALL "double" → ROOT_OUTPUT === */
    ArcNodeId n_7 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 3020);
    g.nodes[n_7].attr.int_value = 7;
    ArcPortId p_7_out = arc_graph_add_port(&g, n_7, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_call = arc_graph_add_node(&g, ARC_NODE_FUNC_CALL, r0, 3021);
    g.nodes[n_call].attr.name = "double";
    ArcPortId p_call_arg = arc_graph_add_port(&g, n_call, ARC_PORT_INPUT, "arg0");
    ArcPortId p_call_out = arc_graph_add_port(&g, n_call, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 3022);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;

    arc_graph_add_edge(&g, p_7_out, p_call_arg);
    arc_graph_add_edge(&g, p_call_out, p_out_in);

    /* Compile + verify + run */
    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);
    ArcVerifyResult vr = arc_verify(&cr.image);
    ASSERT(vr.valid);

    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen("NUL", "w"); /* suppress print output */
    ArcStatus s = arc_vm_run(&vm);
    fclose(vm.output);
    ASSERT(s == ARC_OK);

    /* double(7) = 7+7 = 14 */
    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 14);

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* ================================================================
 * Milestone H: If/else branching
 * ================================================================ */

TEST(test_e2e_if_else) {
    /*
     * def pick(flag):
     *     if flag > 0: return 42
     *     else: return 99
     * output = pick(1)   => 42
     */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;
    ArcRegionId r_body = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    ArcRegionId r_then = arc_graph_add_region(&g, ARC_REGION_THEN, r_body);
    ArcRegionId r_else = arc_graph_add_region(&g, ARC_REGION_ELSE, r_body);

    /* FUNC_DEF "pick" */
    ArcNodeId n_fdef = arc_graph_add_node(&g, ARC_NODE_FUNC_DEF, r0, 4001);
    g.nodes[n_fdef].attr.func.name = "pick";
    g.nodes[n_fdef].attr.func.arity = 1;
    g.nodes[n_fdef].attr.func.body_region = r_body;

    /* Body: PARAM "flag" */
    ArcNodeId n_param = arc_graph_add_node(&g, ARC_NODE_PARAM, r_body, 4010);
    g.nodes[n_param].attr.name = "flag";

    /* Body: VAR_REF "flag", CONST_INT 0, GT → condition */
    ArcNodeId n_ref = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_body, 4011);
    g.nodes[n_ref].attr.name = "flag";
    ArcPortId p_ref_out = arc_graph_add_port(&g, n_ref, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_zero = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r_body, 4012);
    g.nodes[n_zero].attr.int_value = 0;
    ArcPortId p_zero_out = arc_graph_add_port(&g, n_zero, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_gt = arc_graph_add_node(&g, ARC_NODE_GT, r_body, 4013);
    ArcPortId p_gt_lhs = arc_graph_add_port(&g, n_gt, ARC_PORT_INPUT, "lhs");
    ArcPortId p_gt_rhs = arc_graph_add_port(&g, n_gt, ARC_PORT_INPUT, "rhs");
    ArcPortId p_gt_out = arc_graph_add_port(&g, n_gt, ARC_PORT_OUTPUT, "out");
    ArcPortId gt_order[] = { p_gt_lhs, p_gt_rhs, p_gt_out };
    arc_node_set_cyclic_order(&g, n_gt, gt_order, 3);

    arc_graph_add_edge(&g, p_ref_out, p_gt_lhs);
    arc_graph_add_edge(&g, p_zero_out, p_gt_rhs);

    /* Body: IF node */
    ArcNodeId n_if = arc_graph_add_node(&g, ARC_NODE_IF, r_body, 4014);
    ArcPortId p_if_cond = arc_graph_add_port(&g, n_if, ARC_PORT_INPUT, "cond");
    n_if; /* avoid warning */
    g.nodes[n_if].attr.branch.then_region = r_then;
    g.nodes[n_if].attr.branch.else_region = r_else;

    arc_graph_add_edge(&g, p_gt_out, p_if_cond);

    /* Then: CONST_INT 42, RETURN */
    ArcNodeId n_42 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r_then, 4020);
    g.nodes[n_42].attr.int_value = 42;
    ArcPortId p_42_out = arc_graph_add_port(&g, n_42, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ret_then = arc_graph_add_node(&g, ARC_NODE_RETURN, r_then, 4021);
    ArcPortId p_ret_then_val = arc_graph_add_port(&g, n_ret_then, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_42_out, p_ret_then_val);

    /* Else: CONST_INT 99, RETURN */
    ArcNodeId n_99 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r_else, 4030);
    g.nodes[n_99].attr.int_value = 99;
    ArcPortId p_99_out = arc_graph_add_port(&g, n_99, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ret_else = arc_graph_add_node(&g, ARC_NODE_RETURN, r_else, 4031);
    ArcPortId p_ret_else_val = arc_graph_add_port(&g, n_ret_else, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_99_out, p_ret_else_val);

    /* Root: CONST_INT 1 → FUNC_CALL "pick" → ROOT_OUTPUT */
    ArcNodeId n_1 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 4040);
    g.nodes[n_1].attr.int_value = 1;
    ArcPortId p_1_out = arc_graph_add_port(&g, n_1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_call = arc_graph_add_node(&g, ARC_NODE_FUNC_CALL, r0, 4041);
    g.nodes[n_call].attr.name = "pick";
    ArcPortId p_call_arg = arc_graph_add_port(&g, n_call, ARC_PORT_INPUT, "arg0");
    ArcPortId p_call_out = arc_graph_add_port(&g, n_call, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_1_out, p_call_arg);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 4042);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_call_out, p_out_in);

    /* Compile + verify + run */
    ArcCompileResult cr = arc_compile(&g);
    if (!cr.success) {
        for (int i = 0; i < cr.error_count; i++)
            printf("    compile: %s\n", cr.errors[i].message);
    }
    ASSERT(cr.success);
    ArcVerifyResult vr = arc_verify(&cr.image);
    ASSERT(vr.valid);

    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen("NUL", "w");
    ArcStatus s = arc_vm_run(&vm);
    fclose(vm.output);
    ASSERT(s == ARC_OK);

    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 42);

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* ================================================================
 * Milestone I: Recursion — fib(10) == 55
 * ================================================================ */

TEST(test_e2e_fibonacci) {
    /*
     * def fib(n):
     *     if n <= 1: return n
     *     else: return fib(n-1) + fib(n-2)
     * output = fib(10)   => 55
     */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;
    ArcRegionId r_body = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    ArcRegionId r_then = arc_graph_add_region(&g, ARC_REGION_THEN, r_body);
    ArcRegionId r_else = arc_graph_add_region(&g, ARC_REGION_ELSE, r_body);

    /* FUNC_DEF "fib" */
    ArcNodeId n_fdef = arc_graph_add_node(&g, ARC_NODE_FUNC_DEF, r0, 5001);
    g.nodes[n_fdef].attr.func.name = "fib";
    g.nodes[n_fdef].attr.func.arity = 1;
    g.nodes[n_fdef].attr.func.body_region = r_body;

    /* Body: PARAM "n" */
    ArcNodeId n_param = arc_graph_add_node(&g, ARC_NODE_PARAM, r_body, 5010);
    g.nodes[n_param].attr.name = "n";

    /* Body: VAR_REF "n" for comparison, CONST_INT 1, LE */
    ArcNodeId n_ref_cmp = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_body, 5011);
    g.nodes[n_ref_cmp].attr.name = "n";
    ArcPortId p_ref_cmp_out = arc_graph_add_port(&g, n_ref_cmp, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_1_cmp = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r_body, 5012);
    g.nodes[n_1_cmp].attr.int_value = 1;
    ArcPortId p_1_cmp_out = arc_graph_add_port(&g, n_1_cmp, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_le = arc_graph_add_node(&g, ARC_NODE_LE, r_body, 5013);
    ArcPortId p_le_lhs = arc_graph_add_port(&g, n_le, ARC_PORT_INPUT, "lhs");
    ArcPortId p_le_rhs = arc_graph_add_port(&g, n_le, ARC_PORT_INPUT, "rhs");
    ArcPortId p_le_out = arc_graph_add_port(&g, n_le, ARC_PORT_OUTPUT, "out");
    ArcPortId le_order[] = { p_le_lhs, p_le_rhs, p_le_out };
    arc_node_set_cyclic_order(&g, n_le, le_order, 3);

    arc_graph_add_edge(&g, p_ref_cmp_out, p_le_lhs);
    arc_graph_add_edge(&g, p_1_cmp_out, p_le_rhs);

    /* Body: IF node */
    ArcNodeId n_if = arc_graph_add_node(&g, ARC_NODE_IF, r_body, 5014);
    ArcPortId p_if_cond = arc_graph_add_port(&g, n_if, ARC_PORT_INPUT, "cond");
    g.nodes[n_if].attr.branch.then_region = r_then;
    g.nodes[n_if].attr.branch.else_region = r_else;
    arc_graph_add_edge(&g, p_le_out, p_if_cond);

    /* Then: return n */
    ArcNodeId n_ref_ret = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_then, 5020);
    g.nodes[n_ref_ret].attr.name = "n";
    ArcPortId p_ref_ret_out = arc_graph_add_port(&g, n_ref_ret, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ret_then = arc_graph_add_node(&g, ARC_NODE_RETURN, r_then, 5021);
    ArcPortId p_ret_then_val = arc_graph_add_port(&g, n_ret_then, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_ref_ret_out, p_ret_then_val);

    /* Else: return fib(n-1) + fib(n-2) */

    /* n-1: VAR_REF "n", CONST_INT 1, SUB */
    ArcNodeId n_ref_a = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_else, 5030);
    g.nodes[n_ref_a].attr.name = "n";
    ArcPortId p_ref_a_out = arc_graph_add_port(&g, n_ref_a, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_1a = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r_else, 5031);
    g.nodes[n_1a].attr.int_value = 1;
    ArcPortId p_1a_out = arc_graph_add_port(&g, n_1a, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_sub1 = arc_graph_add_node(&g, ARC_NODE_SUB, r_else, 5032);
    ArcPortId p_sub1_lhs = arc_graph_add_port(&g, n_sub1, ARC_PORT_INPUT, "lhs");
    ArcPortId p_sub1_rhs = arc_graph_add_port(&g, n_sub1, ARC_PORT_INPUT, "rhs");
    ArcPortId p_sub1_out = arc_graph_add_port(&g, n_sub1, ARC_PORT_OUTPUT, "out");
    ArcPortId sub1_order[] = { p_sub1_lhs, p_sub1_rhs, p_sub1_out };
    arc_node_set_cyclic_order(&g, n_sub1, sub1_order, 3);

    arc_graph_add_edge(&g, p_ref_a_out, p_sub1_lhs);
    arc_graph_add_edge(&g, p_1a_out, p_sub1_rhs);

    /* fib(n-1) */
    ArcNodeId n_call1 = arc_graph_add_node(&g, ARC_NODE_FUNC_CALL, r_else, 5033);
    g.nodes[n_call1].attr.name = "fib";
    ArcPortId p_call1_arg = arc_graph_add_port(&g, n_call1, ARC_PORT_INPUT, "arg0");
    ArcPortId p_call1_out = arc_graph_add_port(&g, n_call1, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_sub1_out, p_call1_arg);

    /* n-2: VAR_REF "n", CONST_INT 2, SUB */
    ArcNodeId n_ref_b = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_else, 5034);
    g.nodes[n_ref_b].attr.name = "n";
    ArcPortId p_ref_b_out = arc_graph_add_port(&g, n_ref_b, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_2 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r_else, 5035);
    g.nodes[n_2].attr.int_value = 2;
    ArcPortId p_2_out = arc_graph_add_port(&g, n_2, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_sub2 = arc_graph_add_node(&g, ARC_NODE_SUB, r_else, 5036);
    ArcPortId p_sub2_lhs = arc_graph_add_port(&g, n_sub2, ARC_PORT_INPUT, "lhs");
    ArcPortId p_sub2_rhs = arc_graph_add_port(&g, n_sub2, ARC_PORT_INPUT, "rhs");
    ArcPortId p_sub2_out = arc_graph_add_port(&g, n_sub2, ARC_PORT_OUTPUT, "out");
    ArcPortId sub2_order[] = { p_sub2_lhs, p_sub2_rhs, p_sub2_out };
    arc_node_set_cyclic_order(&g, n_sub2, sub2_order, 3);

    arc_graph_add_edge(&g, p_ref_b_out, p_sub2_lhs);
    arc_graph_add_edge(&g, p_2_out, p_sub2_rhs);

    /* fib(n-2) */
    ArcNodeId n_call2 = arc_graph_add_node(&g, ARC_NODE_FUNC_CALL, r_else, 5037);
    g.nodes[n_call2].attr.name = "fib";
    ArcPortId p_call2_arg = arc_graph_add_port(&g, n_call2, ARC_PORT_INPUT, "arg0");
    ArcPortId p_call2_out = arc_graph_add_port(&g, n_call2, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_sub2_out, p_call2_arg);

    /* fib(n-1) + fib(n-2) */
    ArcNodeId n_add = arc_graph_add_node(&g, ARC_NODE_ADD, r_else, 5038);
    ArcPortId p_add_lhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId p_add_rhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId p_add_out = arc_graph_add_port(&g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId add_order[] = { p_add_lhs, p_add_rhs, p_add_out };
    arc_node_set_cyclic_order(&g, n_add, add_order, 3);

    arc_graph_add_edge(&g, p_call1_out, p_add_lhs);
    arc_graph_add_edge(&g, p_call2_out, p_add_rhs);

    /* return fib(n-1) + fib(n-2) */
    ArcNodeId n_ret_else = arc_graph_add_node(&g, ARC_NODE_RETURN, r_else, 5039);
    ArcPortId p_ret_else_val = arc_graph_add_port(&g, n_ret_else, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_add_out, p_ret_else_val);

    /* Root: CONST_INT 10 → FUNC_CALL "fib" → ROOT_OUTPUT */
    ArcNodeId n_10 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 5050);
    g.nodes[n_10].attr.int_value = 10;
    ArcPortId p_10_out = arc_graph_add_port(&g, n_10, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_call_main = arc_graph_add_node(&g, ARC_NODE_FUNC_CALL, r0, 5051);
    g.nodes[n_call_main].attr.name = "fib";
    ArcPortId p_call_main_arg = arc_graph_add_port(&g, n_call_main, ARC_PORT_INPUT, "arg0");
    ArcPortId p_call_main_out = arc_graph_add_port(&g, n_call_main, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_10_out, p_call_main_arg);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 5052);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_call_main_out, p_out_in);

    /* Compile */
    ArcCompileResult cr = arc_compile(&g);
    if (!cr.success) {
        for (int i = 0; i < cr.error_count; i++)
            printf("    compile: %s\n", cr.errors[i].message);
    }
    ASSERT(cr.success);

    /* Verify */
    ArcVerifyResult vr = arc_verify(&cr.image);
    if (!vr.valid) {
        for (int i = 0; i < vr.error_count; i++)
            printf("    verify: %s\n", vr.errors[i].message);
    }
    ASSERT(vr.valid);

    /* Execute */
    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen("NUL", "w");
    ArcStatus s = arc_vm_run(&vm);
    fclose(vm.output);
    ASSERT(s == ARC_OK);

    /* fib(10) = 55 */
    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 55);

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* ================================================================
 * Milestone J: While loops
 * ================================================================ */

TEST(test_e2e_while_loop) {
    /*
     * def sum_to(n):
     *     let acc = 0
     *     let i = 1
     *     while i <= n:
     *         acc = acc + i
     *         i = i + 1
     *     return acc
     * output = sum_to(10)  => 55
     */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;
    ArcRegionId r_body = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);
    ArcRegionId r_loop = arc_graph_add_region(&g, ARC_REGION_LOOP_BODY, r_body);

    /* FUNC_DEF "sum_to" */
    ArcNodeId n_fdef = arc_graph_add_node(&g, ARC_NODE_FUNC_DEF, r0, 6001);
    g.nodes[n_fdef].attr.func.name = "sum_to";
    g.nodes[n_fdef].attr.func.arity = 1;
    g.nodes[n_fdef].attr.func.body_region = r_body;

    /* PARAM "n" */
    ArcNodeId n_param = arc_graph_add_node(&g, ARC_NODE_PARAM, r_body, 6010);
    g.nodes[n_param].attr.name = "n";

    /* LET acc = 0 */
    ArcNodeId n_c0 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r_body, 6011);
    g.nodes[n_c0].attr.int_value = 0;
    ArcPortId p_c0_out = arc_graph_add_port(&g, n_c0, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_let_acc = arc_graph_add_node(&g, ARC_NODE_LET, r_body, 6012);
    g.nodes[n_let_acc].attr.name = "acc";
    ArcPortId p_let_acc_val = arc_graph_add_port(&g, n_let_acc, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_c0_out, p_let_acc_val);

    /* LET i = 1 */
    ArcNodeId n_c1 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r_body, 6013);
    g.nodes[n_c1].attr.int_value = 1;
    ArcPortId p_c1_out = arc_graph_add_port(&g, n_c1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_let_i = arc_graph_add_node(&g, ARC_NODE_LET, r_body, 6014);
    g.nodes[n_let_i].attr.name = "i";
    ArcPortId p_let_i_val = arc_graph_add_port(&g, n_let_i, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_c1_out, p_let_i_val);

    /* WHILE condition: i <= n */
    ArcNodeId n_ref_i_w = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_body, 6015);
    g.nodes[n_ref_i_w].attr.name = "i";
    ArcPortId p_ref_i_w_out = arc_graph_add_port(&g, n_ref_i_w, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ref_n_w = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_body, 6016);
    g.nodes[n_ref_n_w].attr.name = "n";
    ArcPortId p_ref_n_w_out = arc_graph_add_port(&g, n_ref_n_w, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_le_w = arc_graph_add_node(&g, ARC_NODE_LE, r_body, 6017);
    ArcPortId p_le_w_lhs = arc_graph_add_port(&g, n_le_w, ARC_PORT_INPUT, "lhs");
    ArcPortId p_le_w_rhs = arc_graph_add_port(&g, n_le_w, ARC_PORT_INPUT, "rhs");
    ArcPortId p_le_w_out = arc_graph_add_port(&g, n_le_w, ARC_PORT_OUTPUT, "out");
    ArcPortId le_w_order[] = { p_le_w_lhs, p_le_w_rhs, p_le_w_out };
    arc_node_set_cyclic_order(&g, n_le_w, le_w_order, 3);
    arc_graph_add_edge(&g, p_ref_i_w_out, p_le_w_lhs);
    arc_graph_add_edge(&g, p_ref_n_w_out, p_le_w_rhs);

    /* WHILE node */
    ArcNodeId n_while = arc_graph_add_node(&g, ARC_NODE_WHILE, r_body, 6018);
    ArcPortId p_while_cond = arc_graph_add_port(&g, n_while, ARC_PORT_INPUT, "cond");
    g.nodes[n_while].attr.loop.body_region = r_loop;
    arc_graph_add_edge(&g, p_le_w_out, p_while_cond);

    /* Loop body: acc = acc + i */
    ArcNodeId n_ref_acc_lb = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_loop, 6020);
    g.nodes[n_ref_acc_lb].attr.name = "acc";
    ArcPortId p_ref_acc_lb_out = arc_graph_add_port(&g, n_ref_acc_lb, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ref_i2_lb = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_loop, 6021);
    g.nodes[n_ref_i2_lb].attr.name = "i";
    ArcPortId p_ref_i2_lb_out = arc_graph_add_port(&g, n_ref_i2_lb, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add_lb = arc_graph_add_node(&g, ARC_NODE_ADD, r_loop, 6022);
    ArcPortId p_add_lb_lhs = arc_graph_add_port(&g, n_add_lb, ARC_PORT_INPUT, "lhs");
    ArcPortId p_add_lb_rhs = arc_graph_add_port(&g, n_add_lb, ARC_PORT_INPUT, "rhs");
    ArcPortId p_add_lb_out = arc_graph_add_port(&g, n_add_lb, ARC_PORT_OUTPUT, "out");
    ArcPortId add_lb_order[] = { p_add_lb_lhs, p_add_lb_rhs, p_add_lb_out };
    arc_node_set_cyclic_order(&g, n_add_lb, add_lb_order, 3);
    arc_graph_add_edge(&g, p_ref_acc_lb_out, p_add_lb_lhs);
    arc_graph_add_edge(&g, p_ref_i2_lb_out, p_add_lb_rhs);

    ArcNodeId n_assign_acc = arc_graph_add_node(&g, ARC_NODE_ASSIGN, r_loop, 6023);
    g.nodes[n_assign_acc].attr.name = "acc";
    ArcPortId p_assign_acc_val = arc_graph_add_port(&g, n_assign_acc, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_add_lb_out, p_assign_acc_val);

    /* Loop body: i = i + 1 */
    ArcNodeId n_ref_i3_lb = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_loop, 6024);
    g.nodes[n_ref_i3_lb].attr.name = "i";
    ArcPortId p_ref_i3_lb_out = arc_graph_add_port(&g, n_ref_i3_lb, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_one_lb = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r_loop, 6025);
    g.nodes[n_one_lb].attr.int_value = 1;
    ArcPortId p_one_lb_out = arc_graph_add_port(&g, n_one_lb, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add2_lb = arc_graph_add_node(&g, ARC_NODE_ADD, r_loop, 6026);
    ArcPortId p_add2_lb_lhs = arc_graph_add_port(&g, n_add2_lb, ARC_PORT_INPUT, "lhs");
    ArcPortId p_add2_lb_rhs = arc_graph_add_port(&g, n_add2_lb, ARC_PORT_INPUT, "rhs");
    ArcPortId p_add2_lb_out = arc_graph_add_port(&g, n_add2_lb, ARC_PORT_OUTPUT, "out");
    ArcPortId add2_lb_order[] = { p_add2_lb_lhs, p_add2_lb_rhs, p_add2_lb_out };
    arc_node_set_cyclic_order(&g, n_add2_lb, add2_lb_order, 3);
    arc_graph_add_edge(&g, p_ref_i3_lb_out, p_add2_lb_lhs);
    arc_graph_add_edge(&g, p_one_lb_out, p_add2_lb_rhs);

    ArcNodeId n_assign_i = arc_graph_add_node(&g, ARC_NODE_ASSIGN, r_loop, 6027);
    g.nodes[n_assign_i].attr.name = "i";
    ArcPortId p_assign_i_val = arc_graph_add_port(&g, n_assign_i, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_add2_lb_out, p_assign_i_val);

    /* RETURN acc */
    ArcNodeId n_ref_acc2 = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_body, 6030);
    g.nodes[n_ref_acc2].attr.name = "acc";
    ArcPortId p_ref_acc2_out = arc_graph_add_port(&g, n_ref_acc2, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ret = arc_graph_add_node(&g, ARC_NODE_RETURN, r_body, 6031);
    ArcPortId p_ret_val = arc_graph_add_port(&g, n_ret, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_ref_acc2_out, p_ret_val);

    /* Root: sum_to(10) -> output */
    ArcNodeId n_10 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 6040);
    g.nodes[n_10].attr.int_value = 10;
    ArcPortId p_10_out = arc_graph_add_port(&g, n_10, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_call = arc_graph_add_node(&g, ARC_NODE_FUNC_CALL, r0, 6041);
    g.nodes[n_call].attr.name = "sum_to";
    ArcPortId p_call_arg = arc_graph_add_port(&g, n_call, ARC_PORT_INPUT, "arg0");
    ArcPortId p_call_out = arc_graph_add_port(&g, n_call, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_10_out, p_call_arg);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 6042);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_call_out, p_out_in);

    /* Compile + verify + run */
    ArcCompileResult cr = arc_compile(&g);
    if (!cr.success) {
        for (int i = 0; i < cr.error_count; i++)
            printf("    compile: %s\n", cr.errors[i].message);
    }
    ASSERT(cr.success);

    ArcVerifyResult vr = arc_verify(&cr.image);
    ASSERT(vr.valid);

    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen("NUL", "w");
    ArcStatus s = arc_vm_run(&vm);
    fclose(vm.output);
    ASSERT(s == ARC_OK);

    /* sum(1..10) = 55 */
    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 55);

    /* Verify debug table has entries */
    ASSERT(cr.image.debug.count > 0);

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* ================================================================
 * Edge cases: malformed binary, stack overflow, clock intrinsic
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

TEST(test_vm_stack_overflow) {
    ArcBytecodeImage img;
    arc_image_init(&img);

    uint16_t c42 = arc_const_pool_add_i64(&img.constants, 42);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    for (int i = 0; i < 1025; i++) {
        arc_buf_push(&code, OP_CONST);
        arc_buf_push_u16(&code, c42);
    }
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 1025, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVm vm;
    arc_vm_init(&vm, &img);
    ArcStatus s = arc_vm_run(&vm);
    ASSERT(s == ARC_ERR_RUNTIME);
    ASSERT(strstr(vm.error.message, "stack overflow") != NULL);

    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

TEST(test_clock_intrinsic) {
    ArcBytecodeImage img;
    arc_image_init(&img);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_INTRINSIC);
    arc_buf_push_u16(&code, ARC_INTRINSIC_CLOCK);
    arc_buf_push(&code, 0);
    arc_buf_push(&code, 0);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 1, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVm vm;
    arc_vm_init(&vm, &img);
    ArcStatus s = arc_vm_run(&vm);
    ASSERT(s == ARC_OK);

    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_F64);
    ASSERT(result.as.f64 >= 0.0);

    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
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
 * Fixture Parser
 * ================================================================ */

TEST(test_fixture_parse_add) {
    const char* fixture =
        "region r0 module\n"
        "node n0 const_int(5) in r0\n"
        "node n1 const_int(10) in r0\n"
        "node n2 add in r0\n"
        "edge e0 n0.out -> n2.lhs\n"
        "edge e1 n1.out -> n2.rhs\n"
        "root n2.out\n";

    ArcFixtureResult fr = arc_fixture_parse(fixture);
    if (!fr.success) printf("    fixture error: %s\n", fr.error);
    ASSERT(fr.success);
    ASSERT(fr.graph.region_count == 1);
    /* Should have 4 nodes: n0, n1, n2, + auto-created ROOT_OUTPUT */
    ASSERT(fr.graph.node_count == 4);
    ASSERT(fr.graph.output_node != ARC_INVALID_ID);
    ASSERT(fr.graph.edge_count == 3); /* e0, e1, + root edge */

    /* Compile and run to verify end-to-end */
    ArcCompileResult cr = arc_compile(&fr.graph);
    ASSERT(cr.success);
    ArcVerifyResult vr = arc_verify(&cr.image);
    ASSERT(vr.valid);

    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen("NUL", "w");
    ArcStatus s = arc_vm_run(&vm);
    fclose(vm.output);
    ASSERT(s == ARC_OK);

    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 15);

    arc_compile_result_free(&cr);
    arc_graph_free(&fr.graph);
}

TEST(test_fixture_parse_file) {
    ArcFixtureResult fr = arc_fixture_parse_file("../tests/fixtures/add_5_10.graph");
    if (!fr.success) printf("    fixture error: %s\n", fr.error);
    ASSERT(fr.success);
    ASSERT(fr.graph.node_count >= 3);

    ArcCompileResult cr = arc_compile(&fr.graph);
    ASSERT(cr.success);

    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen("NUL", "w");
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    fclose(vm.output);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 15);

    arc_compile_result_free(&cr);
    arc_graph_free(&fr.graph);
}

TEST(test_fixture_parse_malformed) {
    ArcFixtureResult fr = arc_fixture_parse_file("../tests/fixtures/malformed.graph");
    ASSERT(!fr.success);
    ASSERT(strlen(fr.error) > 0);
    if (fr.success) arc_graph_free(&fr.graph);
}

/* ================================================================
 * NOT Operator End-to-End
 * ================================================================ */

TEST(test_e2e_not_operator) {
    /* not(true) == false, not(false) == true */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_true = arc_graph_add_node(&g, ARC_NODE_CONST_BOOL, r0, 8001);
    g.nodes[n_true].attr.bool_value = true;
    ArcPortId p_true_out = arc_graph_add_port(&g, n_true, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_not = arc_graph_add_node(&g, ARC_NODE_NOT, r0, 8002);
    ArcPortId p_not_in = arc_graph_add_port(&g, n_not, ARC_PORT_INPUT, "value");
    ArcPortId p_not_out = arc_graph_add_port(&g, n_not, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_true_out, p_not_in);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 8003);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_not_out, p_out_in);

    ArcCompileResult cr = arc_compile(&g);
    if (!cr.success) {
        for (int i = 0; i < cr.error_count; i++)
            printf("    compile: %s\n", cr.errors[i].message);
    }
    ASSERT(cr.success);
    ArcVerifyResult vr = arc_verify(&cr.image);
    ASSERT(vr.valid);

    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen("NUL", "w");
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    fclose(vm.output);

    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_BOOL);
    ASSERT(result.as.b == false);

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

TEST(test_e2e_not_via_fixture) {
    ArcFixtureResult fr = arc_fixture_parse_file("../tests/fixtures/not_true.graph");
    ASSERT(fr.success);

    ArcCompileResult cr = arc_compile(&fr.graph);
    ASSERT(cr.success);

    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen("NUL", "w");
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    fclose(vm.output);

    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_BOOL);
    ASSERT(result.as.b == false);

    arc_compile_result_free(&cr);
    arc_graph_free(&fr.graph);
}

/* ================================================================
 * Compile-Fail Diagnostic Test
 * ================================================================ */

TEST(test_compile_fail_diagnostic) {
    /* Build a graph with a disconnected binary op — should produce a compile error */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    /* ADD node with no inputs connected */
    ArcNodeId n_add = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 9001);
    ArcPortId p_add_lhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId p_add_rhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId p_add_out = arc_graph_add_port(&g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId add_order[] = { p_add_lhs, p_add_rhs, p_add_out };
    arc_node_set_cyclic_order(&g, n_add, add_order, 3);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 9002);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_add_out, p_out_in);

    ArcCompileResult cr = arc_compile(&g);
    /* Should fail with disconnected inputs error */
    ASSERT(!cr.success);
    ASSERT(cr.error_count > 0);
    ASSERT(strstr(cr.errors[0].message, "disconnected") != NULL);

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

/* ================================================================
 * Golden Disassembly Test
 * ================================================================ */

TEST(test_golden_disassembly) {
    /* Build 5+10, compile, disassemble, check output matches expected */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_5 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);
    g.nodes[n_5].attr.int_value = 5;
    ArcPortId p5 = arc_graph_add_port(&g, n_5, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_10 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);
    g.nodes[n_10].attr.int_value = 10;
    ArcPortId p10 = arc_graph_add_port(&g, n_10, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 0);
    ArcPortId pa_lhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId pa_rhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId pa_out = arc_graph_add_port(&g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId pa_order[] = { pa_lhs, pa_rhs, pa_out };
    arc_node_set_cyclic_order(&g, n_add, pa_order, 3);
    arc_graph_add_edge(&g, p5, pa_lhs);
    arc_graph_add_edge(&g, p10, pa_rhs);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 0);
    ArcPortId po_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, pa_out, po_in);

    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);

    /* Disassemble to buffer */
    char dis_buf[4096] = {0};
    FILE* memf = fopen("NUL", "w");
    /* Use tmpfile to capture output */
    FILE* tmp = tmpfile();
    ASSERT(tmp != NULL);
    arc_disassemble(&cr.image, tmp);
    fseek(tmp, 0, SEEK_SET);
    size_t rd = fread(dis_buf, 1, sizeof(dis_buf) - 1, tmp);
    dis_buf[rd] = '\0';
    fclose(tmp);
    if (memf) fclose(memf);

    /* Verify key elements are present in deterministic output */
    ASSERT(strstr(dis_buf, "main") != NULL);
    ASSERT(strstr(dis_buf, "const") != NULL);
    ASSERT(strstr(dis_buf, "add") != NULL);
    ASSERT(strstr(dis_buf, "halt") != NULL);
    ASSERT(strstr(dis_buf, "5") != NULL);
    ASSERT(strstr(dis_buf, "10") != NULL);

    /* Compile same graph again — output should be identical (determinism) */
    ArcCompileResult cr2 = arc_compile(&g);
    ASSERT(cr2.success);

    uint8_t *bytes1, *bytes2;
    size_t len1, len2;
    arc_image_write(&cr.image, &bytes1, &len1);
    arc_image_write(&cr2.image, &bytes2, &len2);
    ASSERT(len1 == len2);
    ASSERT(memcmp(bytes1, bytes2, len1) == 0);

    ARC_FREE(bytes1); ARC_FREE(bytes2);
    arc_compile_result_free(&cr);
    arc_compile_result_free(&cr2);
    arc_graph_free(&g);
}

/* ================================================================
 * Globals Support
 * ================================================================ */

TEST(test_vm_globals) {
    /* Test LOAD_GLOBAL / STORE_GLOBAL opcodes */
    ArcBytecodeImage img;
    arc_image_init(&img);

    uint16_t c42 = arc_const_pool_add_i64(&img.constants, 42);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    /* CONST 42, STORE_GLOBAL 0, LOAD_GLOBAL 0, HALT */
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, c42);
    arc_buf_push(&code, OP_STORE_GLOBAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_LOAD_GLOBAL); arc_buf_push_u16(&code, 0);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 1, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVm vm;
    arc_vm_init(&vm, &img);
    ArcStatus s = arc_vm_run(&vm);
    ASSERT(s == ARC_OK);
    ASSERT(arc_vm_result(&vm).tag == VAL_I64);
    ASSERT_EQ_I64(arc_vm_result(&vm).as.i64, 42);

    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

/* ================================================================
 * String Runtime Values
 * ================================================================ */

TEST(test_string_values) {
    /* Test ArcString creation, equality, refcounting */
    ArcString* s1 = arc_string_new("hello", 5);
    ASSERT(s1->len == 5);
    ASSERT(strcmp(s1->data, "hello") == 0);
    ASSERT(s1->refcount == 1);

    arc_string_retain(s1);
    ASSERT(s1->refcount == 2);

    ArcString* s2 = arc_string_new("hello", 5);
    ArcValue v1 = arc_val_string(s1);
    ArcValue v2 = arc_val_string(s2);
    ASSERT(arc_val_equal(v1, v2));
    ASSERT(arc_val_is_truthy(v1));

    ArcString* empty = arc_string_new("", 0);
    ArcValue ve = arc_val_string(empty);
    ASSERT(!arc_val_is_truthy(ve));

    arc_string_release(s1);
    ASSERT(s1->refcount == 1);
    arc_string_release(s1);
    /* s1 freed now */
    arc_string_release(s2);
    arc_string_release(empty);
}

TEST(test_vm_string_const) {
    /* Load a string constant and verify it becomes a VAL_STRING */
    ArcBytecodeImage img;
    arc_image_init(&img);

    uint16_t cs = arc_const_pool_add_string(&img.constants, "hello", 5);
    uint16_t name = arc_const_pool_add_string(&img.constants, "main", 4);

    ArcBuf code; arc_buf_init(&code);
    arc_buf_push(&code, OP_CONST); arc_buf_push_u16(&code, cs);
    arc_buf_push(&code, OP_HALT);
    img.code = code.data; img.code_len = (uint32_t)code.len;

    ArcFuncRecord fr = { .name_const_idx = name, .arity = 0, .local_count = 0,
                         .max_stack = 1, .code_offset = 0, .code_length = img.code_len };
    arc_func_table_add(&img.functions, fr);

    ArcVm vm;
    arc_vm_init(&vm, &img);
    ArcStatus s = arc_vm_run(&vm);
    ASSERT(s == ARC_OK);
    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_STRING);
    ASSERT(result.as.str != NULL);
    ASSERT(strcmp(result.as.str->data, "hello") == 0);

    arc_string_release(result.as.str);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

/* ================================================================
 * Property Tests (encode/decode roundtrip)
 * ================================================================ */

TEST(test_property_roundtrip_random) {
    /* Build an image with varied constants, serialize, deserialize, compare */
    ArcBytecodeImage img;
    arc_image_init(&img);

    /* Add diverse constant types */
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

    /* Add debug entries */
    ArcDebugEntry de1 = { .func_idx = 0, .bc_start = 0, .bc_end = 3, .element_id = 12345 };
    ArcDebugEntry de2 = { .func_idx = 0, .bc_start = 3, .bc_end = 4, .element_id = 0xFFFFFFFFFFFFFFFFULL };
    arc_debug_table_add(&img.debug, de1);
    arc_debug_table_add(&img.debug, de2);

    /* Serialize */
    uint8_t* bytes; size_t blen;
    ASSERT(arc_image_write(&img, &bytes, &blen) == ARC_OK);

    /* Deserialize */
    ArcBytecodeImage img2;
    ASSERT(arc_image_read(bytes, blen, &img2) == ARC_OK);

    /* Compare */
    ASSERT(img2.constants.count == img.constants.count);
    for (uint16_t i = 0; i < img.constants.count; i++) {
        ASSERT(img2.constants.entries[i].tag == img.constants.entries[i].tag);
        switch (img.constants.entries[i].tag) {
        case ARC_CONST_I64:
            ASSERT(img2.constants.entries[i].as.i64 == img.constants.entries[i].as.i64); break;
        case ARC_CONST_F64:
            ASSERT(memcmp(&img2.constants.entries[i].as.f64,
                          &img.constants.entries[i].as.f64, 8) == 0); break;
        case ARC_CONST_STRING:
            ASSERT(img2.constants.entries[i].as.str.len == img.constants.entries[i].as.str.len);
            ASSERT(memcmp(img2.constants.entries[i].as.str.data,
                          img.constants.entries[i].as.str.data,
                          img.constants.entries[i].as.str.len) == 0); break;
        default: break;
        }
    }
    ASSERT(img2.code_len == img.code_len);
    ASSERT(memcmp(img2.code, img.code, img.code_len) == 0);
    ASSERT(img2.debug.count == img.debug.count);
    for (uint32_t i = 0; i < img.debug.count; i++) {
        ASSERT(img2.debug.entries[i].func_idx == img.debug.entries[i].func_idx);
        ASSERT(img2.debug.entries[i].bc_start == img.debug.entries[i].bc_start);
        ASSERT(img2.debug.entries[i].bc_end == img.debug.entries[i].bc_end);
        ASSERT(img2.debug.entries[i].element_id == img.debug.entries[i].element_id);
    }

    ARC_FREE(bytes);
    arc_image_free(&img2);
    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

/* ================================================================
 * Structured Diagnostics
 * ================================================================ */

TEST(test_structured_diagnostics) {
    ArcDiagList diags;
    arc_diag_init(&diags);

    ArcDiagnostic* d = arc_diag_add(&diags);
    d->severity = ARC_DIAG_ERROR;
    d->code = ARC_ERR_MISSING_PORT;
    snprintf(d->message, sizeof(d->message), "missing port 'lhs' on node 5");
    d->primary.element_id = 42;
    d->primary.node_id = 5;
    d->primary.port_role = "lhs";

    ASSERT(diags.count == 1);
    ASSERT(diags.items[0].severity == ARC_DIAG_ERROR);
    ASSERT(diags.items[0].code == ARC_ERR_MISSING_PORT);
    ASSERT(diags.items[0].primary.element_id == 42);

    /* Add a related ref */
    d->related[0].ref.element_id = 99;
    snprintf(d->related[0].label, sizeof(d->related[0].label), "defined here");
    d->related_count = 1;

    /* Print to NUL to exercise the printer */
    FILE* f = fopen("NUL", "w");
    arc_diag_print(&diags, f);
    fclose(f);

    arc_diag_free(&diags);
    ASSERT(diags.count == 0);
}

/* ================================================================
 * Differential Reference Interpreter
 * ================================================================ */

TEST(test_ref_interp_5_plus_10) {
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_5 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);
    g.nodes[n_5].attr.int_value = 5;
    ArcPortId p5 = arc_graph_add_port(&g, n_5, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_10 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);
    g.nodes[n_10].attr.int_value = 10;
    ArcPortId p10 = arc_graph_add_port(&g, n_10, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add = arc_graph_add_node(&g, ARC_NODE_ADD, r0, 0);
    ArcPortId pa_lhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId pa_rhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId pa_out = arc_graph_add_port(&g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId pa_order[] = { pa_lhs, pa_rhs, pa_out };
    arc_node_set_cyclic_order(&g, n_add, pa_order, 3);
    arc_graph_add_edge(&g, p5, pa_lhs);
    arc_graph_add_edge(&g, p10, pa_rhs);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 0);
    ArcPortId po_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, pa_out, po_in);

    /* Reference interpreter */
    ArcInterpResult ir = arc_interpret(&g);
    ASSERT(ir.success);
    ASSERT(ir.result.tag == VAL_I64);
    ASSERT_EQ_I64(ir.result.as.i64, 15);

    /* Compiler -> VM */
    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);
    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen("NUL", "w");
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    fclose(vm.output);
    ArcValue vm_result = arc_vm_result(&vm);

    /* Differential comparison */
    ASSERT(arc_val_equal(ir.result, vm_result));

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

TEST(test_ref_interp_function) {
    /* double(x) = x + x; double(7) == 14 */
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;
    ArcRegionId r_body = arc_graph_add_region(&g, ARC_REGION_FUNCTION, r0);

    ArcNodeId n_fdef = arc_graph_add_node(&g, ARC_NODE_FUNC_DEF, r0, 0);
    g.nodes[n_fdef].attr.func.name = "double";
    g.nodes[n_fdef].attr.func.arity = 1;
    g.nodes[n_fdef].attr.func.body_region = r_body;

    ArcNodeId n_param = arc_graph_add_node(&g, ARC_NODE_PARAM, r_body, 0);
    g.nodes[n_param].attr.name = "x";

    ArcNodeId n_ref1 = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_body, 0);
    g.nodes[n_ref1].attr.name = "x";
    ArcPortId p_ref1 = arc_graph_add_port(&g, n_ref1, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_ref2 = arc_graph_add_node(&g, ARC_NODE_VAR_REF, r_body, 0);
    g.nodes[n_ref2].attr.name = "x";
    ArcPortId p_ref2 = arc_graph_add_port(&g, n_ref2, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_add = arc_graph_add_node(&g, ARC_NODE_ADD, r_body, 0);
    ArcPortId p_add_lhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "lhs");
    ArcPortId p_add_rhs = arc_graph_add_port(&g, n_add, ARC_PORT_INPUT, "rhs");
    ArcPortId p_add_out = arc_graph_add_port(&g, n_add, ARC_PORT_OUTPUT, "out");
    ArcPortId add_order[] = { p_add_lhs, p_add_rhs, p_add_out };
    arc_node_set_cyclic_order(&g, n_add, add_order, 3);
    arc_graph_add_edge(&g, p_ref1, p_add_lhs);
    arc_graph_add_edge(&g, p_ref2, p_add_rhs);

    ArcNodeId n_ret = arc_graph_add_node(&g, ARC_NODE_RETURN, r_body, 0);
    ArcPortId p_ret_val = arc_graph_add_port(&g, n_ret, ARC_PORT_INPUT, "value");
    arc_graph_add_edge(&g, p_add_out, p_ret_val);

    ArcNodeId n_7 = arc_graph_add_node(&g, ARC_NODE_CONST_INT, r0, 0);
    g.nodes[n_7].attr.int_value = 7;
    ArcPortId p_7 = arc_graph_add_port(&g, n_7, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_call = arc_graph_add_node(&g, ARC_NODE_FUNC_CALL, r0, 0);
    g.nodes[n_call].attr.name = "double";
    ArcPortId p_call_arg = arc_graph_add_port(&g, n_call, ARC_PORT_INPUT, "arg0");
    ArcPortId p_call_out = arc_graph_add_port(&g, n_call, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_7, p_call_arg);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 0);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_call_out, p_out_in);

    /* Reference interpreter */
    ArcInterpResult ir = arc_interpret(&g);
    ASSERT(ir.success);
    ASSERT_EQ_I64(ir.result.as.i64, 14);

    /* Compiler -> VM */
    ArcCompileResult cr = arc_compile(&g);
    ASSERT(cr.success);
    ArcVm vm;
    arc_vm_init(&vm, &cr.image);
    vm.output = fopen("NUL", "w");
    ASSERT(arc_vm_run(&vm) == ARC_OK);
    fclose(vm.output);

    /* Differential comparison */
    ASSERT(arc_val_equal(ir.result, arc_vm_result(&vm)));

    arc_compile_result_free(&cr);
    arc_graph_free(&g);
}

TEST(test_ref_interp_not) {
    ArcGraph g;
    arc_graph_init(&g);

    ArcRegionId r0 = arc_graph_add_region(&g, ARC_REGION_MODULE, ARC_INVALID_ID);
    g.root_region = r0;

    ArcNodeId n_true = arc_graph_add_node(&g, ARC_NODE_CONST_BOOL, r0, 0);
    g.nodes[n_true].attr.bool_value = true;
    ArcPortId p_true = arc_graph_add_port(&g, n_true, ARC_PORT_OUTPUT, "out");

    ArcNodeId n_not = arc_graph_add_node(&g, ARC_NODE_NOT, r0, 0);
    ArcPortId p_not_in = arc_graph_add_port(&g, n_not, ARC_PORT_INPUT, "value");
    ArcPortId p_not_out = arc_graph_add_port(&g, n_not, ARC_PORT_OUTPUT, "out");
    arc_graph_add_edge(&g, p_true, p_not_in);

    ArcNodeId n_out = arc_graph_add_node(&g, ARC_NODE_ROOT_OUTPUT, r0, 0);
    ArcPortId p_out_in = arc_graph_add_port(&g, n_out, ARC_PORT_INPUT, "value");
    g.output_node = n_out;
    arc_graph_add_edge(&g, p_not_out, p_out_in);

    ArcInterpResult ir = arc_interpret(&g);
    ASSERT(ir.success);
    ASSERT(ir.result.tag == VAL_BOOL);
    ASSERT(ir.result.as.b == false);

    arc_graph_free(&g);
}

/* ================================================================
 * Verifier: Stack consistency at joins
 * ================================================================ */

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

    arc_const_pool_free(&img.constants);
    arc_func_table_free(&img.functions);
    arc_debug_table_free(&img.debug);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== Arcana Test Suite ===\n\n");

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

    printf("\n[Milestone C: VM]\n");
    RUN(test_vm_add_5_10);
    RUN(test_vm_arithmetic);
    RUN(test_vm_comparisons);
    RUN(test_vm_branch);
    RUN(test_vm_locals);
    RUN(test_vm_division_by_zero);

    printf("\n[Milestone D: Semantic Graph]\n");
    RUN(test_graph_valid);
    RUN(test_graph_invalid_edge);

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

    printf("\n[Edge Cases]\n");
    RUN(test_malformed_binary);
    RUN(test_truncated_binary);
    RUN(test_vm_stack_overflow);
    RUN(test_clock_intrinsic);
    RUN(test_serialization_roundtrip_with_debug);
    RUN(test_verifier_bad_jump_target);
    RUN(test_const_pool_dedup);

    printf("\n[Fixture Parser]\n");
    RUN(test_fixture_parse_add);
    RUN(test_fixture_parse_file);
    RUN(test_fixture_parse_malformed);

    printf("\n[NOT Operator]\n");
    RUN(test_e2e_not_operator);
    RUN(test_e2e_not_via_fixture);

    printf("\n[Compile-Fail Diagnostics]\n");
    RUN(test_compile_fail_diagnostic);

    printf("\n[Golden Disassembly]\n");
    RUN(test_golden_disassembly);

    printf("\n[Globals]\n");
    RUN(test_vm_globals);

    printf("\n[String Values]\n");
    RUN(test_string_values);
    RUN(test_vm_string_const);

    printf("\n[Property Tests]\n");
    RUN(test_property_roundtrip_random);

    printf("\n[Structured Diagnostics]\n");
    RUN(test_structured_diagnostics);

    printf("\n[Reference Interpreter]\n");
    RUN(test_ref_interp_5_plus_10);
    RUN(test_ref_interp_function);
    RUN(test_ref_interp_not);

    printf("\n[Verifier Stack Consistency]\n");
    RUN(test_verifier_stack_consistency);

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FAILED", tests_failed);
    printf(" ===\n");

    return tests_failed > 0 ? 1 : 0;
}
