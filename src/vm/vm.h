#ifndef ARCANA_VM_H
#define ARCANA_VM_H

#include "value.h"
#include "../bytecode/format.h"
#include "../bytecode/opcodes.h"
#include "../runtime/gc.h"

#define ARC_STACK_MAX   1024
#define ARC_FRAMES_MAX  256
#define ARC_GLOBALS_MAX 256
#define ARC_HANDLER_MAX 64

/* Call frame */
typedef struct {
    uint16_t func_idx;
    uint32_t return_ip;
    uint32_t base_slot;     /* operand stack base for this frame's locals */
} ArcFrame;

/* Exception handler */
typedef struct {
    uint32_t catch_ip;      /* IP of catch block */
    uint32_t stack_height;  /* sp to restore on throw */
    uint32_t frame_depth;   /* fp to restore on throw */
} ArcHandler;

/* VM runtime error */
typedef struct {
    ArcStatus code;
    char      message[256];
    uint32_t  ip;
    uint16_t  func_idx;
} ArcVmError;

/* VM instance */
typedef struct {
    const ArcBytecodeImage* image;

    /* Operand stack */
    ArcValue  stack[ARC_STACK_MAX];
    uint32_t  sp;

    /* Call frames */
    ArcFrame  frames[ARC_FRAMES_MAX];
    uint32_t  fp;       /* frame pointer (count of active frames) */

    /* Globals */
    ArcValue  globals[ARC_GLOBALS_MAX];
    uint16_t  global_count;

    /* Instruction pointer (byte offset into image->code) */
    uint32_t  ip;

    bool      halted;
    ArcVmError error;

    /* Exception handler stack */
    ArcHandler handlers[ARC_HANDLER_MAX];
    uint32_t   handler_count;

    /* Garbage collector */
    ArcGC     gc;

    /* Open upvalue chain (for closures) */
    ArcObjUpvalue* open_upvalues;

    /* Output capture for testing (NULL = stdout) */
    FILE*     output;

    /* Trace mode */
    bool      trace;
} ArcVm;

/* Initialize VM with a loaded bytecode image */
void arc_vm_init(ArcVm* vm, const ArcBytecodeImage* image);

/* Execute from function index 0 (main) until halt or error */
ArcStatus arc_vm_run(ArcVm* vm);

/* Get the top-of-stack value after execution */
ArcValue arc_vm_result(const ArcVm* vm);

/* Clean up VM resources (frees all GC objects) */
void arc_vm_destroy(ArcVm* vm);

#endif /* ARCANA_VM_H */
