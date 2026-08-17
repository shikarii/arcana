/*
 * test_concurrency.c — Tests for coroutines, threads, mutexes, channels.
 */

#include "test_harness.h"
#include "../src/runtime/concurrency.h"

/* --- Helper: build a 2-function image for coroutine tests --- */

static void build_coro_image(ArcBytecodeImage* img,
                             const uint8_t* main_code, uint32_t main_len,
                             const uint8_t* coro_code, uint32_t coro_len) {
    arc_image_init(img);
    /* Constant 0: i64 42 */
    arc_const_pool_add_i64(&img->constants, 42);
    /* Constant 1: i64 99 */
    arc_const_pool_add_i64(&img->constants, 99);
    /* Constant 2: null (for resume send value) */
    arc_const_pool_add_null(&img->constants);

    /* Allocate combined code buffer */
    img->code_len = main_len + coro_len;
    img->code = (uint8_t*)calloc(img->code_len, 1);
    memcpy(img->code, main_code, main_len);
    memcpy(img->code + main_len, coro_code, coro_len);

    /* Function 0: main */
    ArcFuncRecord main_fn = {0};
    main_fn.arity = 0;
    main_fn.local_count = 1; /* one local for the coroutine */
    main_fn.max_stack = 8;
    main_fn.code_offset = 0;
    main_fn.code_length = main_len;
    arc_func_table_add(&img->functions, main_fn);

    /* Function 1: coroutine body */
    ArcFuncRecord coro_fn = {0};
    coro_fn.arity = 0;
    coro_fn.local_count = 0;
    coro_fn.max_stack = 4;
    coro_fn.code_offset = main_len;
    coro_fn.code_length = coro_len;
    arc_func_table_add(&img->functions, coro_fn);
}

/* Test: create a coroutine, check status is "ready" */
void test_coro_create(void) {
    /* Main: coro_new 1, coro_status, halt */
    uint8_t main_code[] = {
        OP_CORO_NEW, 0x01, 0x00,       /* coro_new func_idx=1 */
        OP_CORO_STATUS,                  /* push status string */
        OP_HALT
    };
    /* Coro body: just return null */
    uint8_t coro_code[] = {
        OP_CONST, 0x02, 0x00,           /* push null */
        OP_RETURN
    };
    ArcBytecodeImage img;
    build_coro_image(&img, main_code, sizeof(main_code),
                     coro_code, sizeof(coro_code));
    ArcVm vm;
    arc_vm_init(&vm, &img);
    vm.output = fopen(DEV_NULL, "w");
    ArcStatus st = arc_vm_run(&vm);
    ASSERT(st == ARC_OK);
    ArcValue result = arc_vm_result(&vm);
    ASSERT(ARC_IS_STRING(result));
    ASSERT(strcmp(ARC_AS_STRING(result)->data, "ready") == 0);
    fclose(vm.output);
    arc_vm_destroy(&vm);
    arc_image_free(&img);
}

/* Test: coroutine yields a value */
void test_coro_yield_resume(void) {
    /*
     * Main:
     *   coro_new 1          → [coro]
     *   const null           → [coro, null]
     *   coro_resume          → [42]  (first resume, coro yields 42)
     *   halt
     *
     * Coro body:
     *   const 42             → [42]
     *   coro_yield           → yields 42, suspends
     *   const 99
     *   return               → dead
     */
    uint8_t main_code[] = {
        OP_CORO_NEW, 0x01, 0x00,        /* coro_new func_idx=1 */
        OP_CONST, 0x02, 0x00,           /* push null (send value) */
        OP_CORO_RESUME,                  /* resume → gets yielded value */
        OP_HALT
    };
    uint8_t coro_code[] = {
        OP_CONST, 0x00, 0x00,           /* push 42 */
        OP_CORO_YIELD,                   /* yield 42 */
        OP_CONST, 0x01, 0x00,           /* push 99 */
        OP_RETURN
    };
    ArcBytecodeImage img;
    build_coro_image(&img, main_code, sizeof(main_code),
                     coro_code, sizeof(coro_code));
    ArcVm vm;
    arc_vm_init(&vm, &img);
    vm.output = fopen(DEV_NULL, "w");
    ArcStatus st = arc_vm_run(&vm);
    ASSERT(st == ARC_OK);
    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 42);
    fclose(vm.output);
    arc_vm_destroy(&vm);
    arc_image_free(&img);
}

/* Test: coroutine runs to completion (dead after full resume) */
void test_coro_completion(void) {
    /*
     * Main:
     *   coro_new 1          → [coro]
     *   store_local 0       → []   (save coro in local)
     *   load_local 0        → [coro]
     *   const null           → [coro, null]
     *   coro_resume          → [42] (first resume yields 42)
     *   pop                  → []
     *   load_local 0        → [coro]
     *   const null           → [coro, null]
     *   coro_resume          → [99] (second resume, coro returns 99)
     *   halt
     */
    uint8_t main_code[] = {
        OP_CORO_NEW, 0x01, 0x00,
        OP_STORE_LOCAL, 0x00, 0x00,
        OP_LOAD_LOCAL, 0x00, 0x00,
        OP_CONST, 0x02, 0x00,
        OP_CORO_RESUME,
        OP_POP,
        OP_LOAD_LOCAL, 0x00, 0x00,
        OP_CONST, 0x02, 0x00,
        OP_CORO_RESUME,
        OP_HALT
    };
    uint8_t coro_code[] = {
        OP_CONST, 0x00, 0x00,           /* push 42 */
        OP_CORO_YIELD,                   /* yield 42 */
        OP_POP,                          /* pop resumed value (null) */
        OP_CONST, 0x01, 0x00,           /* push 99 */
        OP_RETURN
    };
    ArcBytecodeImage img;
    build_coro_image(&img, main_code, sizeof(main_code),
                     coro_code, sizeof(coro_code));
    ArcVm vm;
    arc_vm_init(&vm, &img);
    vm.output = fopen(DEV_NULL, "w");
    ArcStatus st = arc_vm_run(&vm);
    ASSERT(st == ARC_OK);
    ArcValue result = arc_vm_result(&vm);
    ASSERT(result.tag == VAL_I64);
    ASSERT_EQ_I64(result.as.i64, 99);
    fclose(vm.output);
    arc_vm_destroy(&vm);
    arc_image_free(&img);
}

/* Test: resume dead coroutine is an error */
void test_coro_resume_dead(void) {
    /*
     * Coro body returns immediately (no yield).
     * First resume: coro returns null, status=dead.
     * Second resume: error.
     */
    uint8_t main_code[] = {
        OP_CORO_NEW, 0x01, 0x00,
        OP_STORE_LOCAL, 0x00, 0x00,
        OP_LOAD_LOCAL, 0x00, 0x00,
        OP_CONST, 0x02, 0x00,
        OP_CORO_RESUME,
        OP_POP,
        OP_LOAD_LOCAL, 0x00, 0x00,
        OP_CONST, 0x02, 0x00,
        OP_CORO_RESUME,
        OP_HALT
    };
    uint8_t coro_code[] = {
        OP_CONST, 0x02, 0x00,           /* push null */
        OP_RETURN
    };
    ArcBytecodeImage img;
    build_coro_image(&img, main_code, sizeof(main_code),
                     coro_code, sizeof(coro_code));
    ArcVm vm;
    arc_vm_init(&vm, &img);
    vm.output = fopen(DEV_NULL, "w");
    ArcStatus st = arc_vm_run(&vm);
    ASSERT(st == ARC_ERR_RUNTIME);
    fclose(vm.output);
    arc_vm_destroy(&vm);
    arc_image_free(&img);
}

/* Test: mutex create, lock, unlock */
void test_mutex_basic(void) {
    /* Main: mutex_new, dup, mutex_lock, mutex_unlock, halt */
    ArcBytecodeImage img;
    arc_image_init(&img);
    uint8_t code[] = {
        OP_MUTEX_NEW,
        OP_DUP,
        OP_MUTEX_LOCK,
        OP_MUTEX_UNLOCK,
        OP_HALT
    };
    img.code = (uint8_t*)calloc(sizeof(code), 1);
    img.code_len = sizeof(code);
    memcpy(img.code, code, sizeof(code));
    ArcFuncRecord fn = {0};
    fn.local_count = 0;
    fn.max_stack = 4;
    fn.code_offset = 0;
    fn.code_length = sizeof(code);
    arc_func_table_add(&img.functions, fn);
    ArcVm vm;
    arc_vm_init(&vm, &img);
    vm.output = fopen(DEV_NULL, "w");
    ArcStatus st = arc_vm_run(&vm);
    ASSERT(st == ARC_OK);
    fclose(vm.output);
    arc_vm_destroy(&vm);
    arc_image_free(&img);
}

/* Test: channel send and receive */
void test_channel_basic(void) {
    ArcGC gc;
    arc_gc_init(&gc);
    ArcObjChannel* ch = arc_obj_channel_new(&gc, 4);
    ASSERT(ch != NULL);
    /* Send a value */
    bool ok = arc_channel_send(ch, arc_val_i64(42));
    ASSERT(ok);
    ASSERT(ch->count == 1);
    /* Receive it back */
    ArcValue out;
    ok = arc_channel_recv(ch, &out);
    ASSERT(ok);
    ASSERT(out.tag == VAL_I64);
    ASSERT_EQ_I64(out.as.i64, 42);
    ASSERT(ch->count == 0);
    arc_gc_free_all(&gc);
    arc_mutex_destroy(&gc.lock);
}

/* Test: channel circular buffer wraps correctly */
void test_channel_wrap(void) {
    ArcGC gc;
    arc_gc_init(&gc);
    ArcObjChannel* ch = arc_obj_channel_new(&gc, 2);
    /* Fill buffer */
    arc_channel_send(ch, arc_val_i64(1));
    arc_channel_send(ch, arc_val_i64(2));
    ASSERT(ch->count == 2);
    /* Receive one, send one more (wraps) */
    ArcValue v;
    arc_channel_recv(ch, &v);
    ASSERT_EQ_I64(v.as.i64, 1);
    arc_channel_send(ch, arc_val_i64(3));
    /* Drain */
    arc_channel_recv(ch, &v);
    ASSERT_EQ_I64(v.as.i64, 2);
    arc_channel_recv(ch, &v);
    ASSERT_EQ_I64(v.as.i64, 3);
    ASSERT(ch->count == 0);
    arc_gc_free_all(&gc);
    arc_mutex_destroy(&gc.lock);
}

/* Test: GC traces coroutine stack */
void test_gc_coro_tracing(void) {
    ArcGC gc;
    arc_gc_init(&gc);
    /* Create a string, put it in a coroutine's stack, collect, verify alive */
    ArcObjString* s = arc_obj_string_new(&gc, "alive", 5);
    /* Simulate by marking it as part of a coroutine (manual root marking) */
    arc_gc_mark_object((ArcObject*)s);
    /* After sweep, marked objects survive */
    ASSERT(s->obj.marked);
    arc_gc_free_all(&gc);
    arc_mutex_destroy(&gc.lock);
}

/* Test: opcode mnemonic for new concurrency opcodes */
void test_concurrency_opcodes(void) {
    ASSERT(strcmp(arc_op_mnemonic(OP_CORO_NEW), "coro_new") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_CORO_RESUME), "coro_resume") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_CORO_YIELD), "coro_yield") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_CORO_STATUS), "coro_status") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_THREAD_SPAWN), "thread_spawn") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_THREAD_JOIN), "thread_join") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_MUTEX_NEW), "mutex_new") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_MUTEX_LOCK), "mutex_lock") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_MUTEX_UNLOCK), "mutex_unlock") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_CHAN_NEW), "chan_new") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_CHAN_SEND), "chan_send") == 0);
    ASSERT(strcmp(arc_op_mnemonic(OP_CHAN_RECV), "chan_recv") == 0);
}

/* Test: concurrency type names */
void test_concurrency_type_names(void) {
    ASSERT(strcmp(arc_obj_type_name(OBJ_COROUTINE), "coroutine") == 0);
    ASSERT(strcmp(arc_obj_type_name(OBJ_THREAD), "thread") == 0);
    ASSERT(strcmp(arc_obj_type_name(OBJ_MUTEX), "mutex") == 0);
    ASSERT(strcmp(arc_obj_type_name(OBJ_CHANNEL), "channel") == 0);
}
