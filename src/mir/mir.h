#ifndef ARCANA_MIR_H
#define ARCANA_MIR_H

#include "../common/arcana_common.h"

/*
 * Arcana MIR (Mid-level Intermediate Representation)
 *
 * MIR is a control-flow-graph representation with explicit basic blocks,
 * temporaries, and linearized operations. It sits between HIR and bytecode.
 *
 * Invariants:
 *   - Every block ends with a terminator (goto, branch, return, halt)
 *   - Temporaries are single-assignment within a block
 *   - No high-level control flow (if/while) — only branches
 *   - Source provenance preserved on every instruction
 */

/* Temporary value ID */
typedef uint16_t MirTemp;
#define MIR_TEMP_NONE UINT16_MAX

/* Block ID */
typedef uint16_t MirBlockId;
#define MIR_BLOCK_NONE UINT16_MAX

/* MIR operation kinds */
typedef enum {
    MIR_OP_CONST_INT,
    MIR_OP_CONST_FLOAT,
    MIR_OP_CONST_BOOL,
    MIR_OP_CONST_NULL,
    MIR_OP_ADD, MIR_OP_SUB, MIR_OP_MUL, MIR_OP_DIV, MIR_OP_MOD,
    MIR_OP_NEG, MIR_OP_NOT,
    MIR_OP_EQ, MIR_OP_NEQ, MIR_OP_LT, MIR_OP_LE, MIR_OP_GT, MIR_OP_GE,
    MIR_OP_LOAD_LOCAL,
    MIR_OP_STORE_LOCAL,
    MIR_OP_CALL,
    MIR_OP_INTRINSIC,
} MirOpKind;

/* MIR instruction (operation that produces a temporary) */
typedef struct {
    MirOpKind     kind;
    MirTemp       dest;         /* result temporary (MIR_TEMP_NONE for void) */
    ArcElementId  source_id;

    union {
        int64_t   int_val;
        double    float_val;
        bool      bool_val;
        struct { MirTemp lhs; MirTemp rhs; } binary;
        struct { MirTemp operand; } unary;
        uint16_t  local_idx;                             /* LOAD/STORE_LOCAL */
        struct { MirTemp value; uint16_t local_idx; } store;
        struct {
            uint16_t func_idx;
            MirTemp* args;
            uint8_t  argc;
        } call;
        struct {
            uint16_t intrinsic_id;
            MirTemp* args;
            uint8_t  argc;
        } intrinsic;
    } as;
} MirInstr;

/* Block terminator kinds */
typedef enum {
    MIR_TERM_GOTO,          /* unconditional jump */
    MIR_TERM_BRANCH,        /* conditional branch */
    MIR_TERM_RETURN,        /* return from function */
    MIR_TERM_HALT,          /* program end */
} MirTermKind;

typedef struct {
    MirTermKind  kind;
    ArcElementId source_id;
    union {
        MirBlockId target;                               /* GOTO */
        struct {
            MirTemp    cond;
            MirBlockId then_block;
            MirBlockId else_block;
        } branch;                                        /* BRANCH */
        MirTemp return_val;                              /* RETURN */
    } as;
} MirTerminator;

/* Basic block */
typedef struct {
    MirBlockId    id;
    MirInstr*     instrs;
    uint32_t      instr_count;
    uint32_t      instr_cap;
    MirTerminator term;
    bool          has_term;
} MirBlock;

/* MIR function */
typedef struct {
    char         name[64];
    uint8_t      arity;
    uint16_t     local_count;
    uint16_t     temp_count;   /* next available temp */
    MirBlock*    blocks;
    uint32_t     block_count;
    uint32_t     block_cap;
    MirBlockId   entry;        /* entry block ID */
    ArcElementId source_id;
} MirFunction;

/* MIR module */
typedef struct {
    MirFunction* functions;
    uint32_t     func_count;
    uint32_t     func_cap;
} MirModule;

/* === API === */

void mir_module_init(MirModule* m);
void mir_module_free(MirModule* m);

/* Construction */
MirFunction* mir_module_add_func(MirModule* m);
MirBlockId   mir_func_add_block(MirFunction* f);
MirInstr*    mir_block_add_instr(MirBlock* b);
MirTemp      mir_func_new_temp(MirFunction* f);

/* Pretty-print */
void mir_dump(const MirModule* m, FILE* out);
void mir_dump_func(const MirFunction* f, FILE* out);

/* Validation */
typedef struct {
    char     message[256];
    uint32_t func_idx;
    uint32_t block_idx;
} MirError;

typedef struct {
    MirError* errors;
    uint32_t  count;
    uint32_t  cap;
    bool      valid;
} MirValidation;

MirValidation mir_validate(const MirModule* m);
void mir_validation_free(MirValidation* v);

#endif /* ARCANA_MIR_H */
