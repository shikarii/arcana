#ifndef ARCANA_VM_H
#define ARCANA_VM_H

#include "value.h"
#include "../bytecode/format.h"
#include "../bytecode/opcodes.h"

#define ARC_STACK_MAX   1024
#define ARC_FRAMES_MAX  256
#define ARC_GLOBALS_MAX 256

/* Call frame */
typedef struct {
    uint16_t func_idx;
    uint32_t return_ip;
    uint32_t base_slot;     /* operand stack base for this frame's locals */
} ArcFrame;

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

#endif /* ARCANA_VM_H */
