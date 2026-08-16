#ifndef ARCANA_OPCODES_H
#define ARCANA_OPCODES_H

/*
 * Arcana Bytecode Opcode Definitions — SINGLE SOURCE OF TRUTH
 *
 * Every consumer (VM, verifier, assembler, disassembler) includes this file.
 * Do not duplicate opcode numbers elsewhere.
 *
 * Encoding: each instruction is 1 opcode byte + 0..N operand bytes.
 * Operands are little-endian.
 *
 * FORMAT(name, opcode_byte, operand_bytes, stack_pops, stack_pushes, mnemonic)
 */

#define ARC_OPCODES(X) \
    /* --- Constants --- */ \
    X(OP_CONST,         0x01, 2, 0, 1, "const")         /* u16 const_idx */ \
    /* --- Stack --- */ \
    X(OP_POP,           0x02, 0, 1, 0, "pop")           \
    X(OP_DUP,           0x03, 0, 1, 2, "dup")           \
    /* --- Locals --- */ \
    X(OP_LOAD_LOCAL,    0x10, 2, 0, 1, "load_local")    /* u16 slot */ \
    X(OP_STORE_LOCAL,   0x11, 2, 1, 0, "store_local")   /* u16 slot */ \
    /* --- Globals --- */ \
    X(OP_LOAD_GLOBAL,   0x12, 2, 0, 1, "load_global")   /* u16 idx */ \
    X(OP_STORE_GLOBAL,  0x13, 2, 1, 0, "store_global")  /* u16 idx */ \
    /* --- Integer arithmetic --- */ \
    X(OP_ADD,           0x20, 0, 2, 1, "add")           \
    X(OP_SUB,           0x21, 0, 2, 1, "sub")           \
    X(OP_MUL,           0x22, 0, 2, 1, "mul")           \
    X(OP_DIV,           0x23, 0, 2, 1, "div")           \
    X(OP_MOD,           0x24, 0, 2, 1, "mod")           \
    X(OP_NEG,           0x25, 0, 1, 1, "neg")           \
    /* --- Comparison --- */ \
    X(OP_EQ,            0x30, 0, 2, 1, "eq")            \
    X(OP_NEQ,           0x31, 0, 2, 1, "neq")           \
    X(OP_LT,            0x32, 0, 2, 1, "lt")            \
    X(OP_LE,            0x33, 0, 2, 1, "le")            \
    X(OP_GT,            0x34, 0, 2, 1, "gt")            \
    X(OP_GE,            0x35, 0, 2, 1, "ge")            \
    /* --- Logic --- */ \
    X(OP_NOT,           0x36, 0, 1, 1, "not")           \
    /* --- Branching (relative i32 offset from next instruction) --- */ \
    X(OP_JUMP,          0x40, 4, 0, 0, "jump")          /* i32 offset */ \
    X(OP_JUMP_IF_FALSE, 0x41, 4, 1, 0, "jump_if_false") /* i32 offset */ \
    X(OP_JUMP_IF_TRUE,  0x42, 4, 1, 0, "jump_if_true")  /* i32 offset */ \
    /* --- Functions --- */ \
    X(OP_CALL,          0x50, 4, -1, 1, "call")         /* u16 func_idx, u8 argc */ \
    X(OP_RETURN,        0x51, 0, 1, 0, "return")        \
    /* --- Intrinsics --- */ \
    X(OP_INTRINSIC,     0xF0, 4, -1, 0, "intrinsic")    /* u16 intrinsic_id, u8 argc */ \
    /* --- Control --- */ \
    X(OP_HALT,          0xFF, 0, 0, 0, "halt")

/* Generate the enum */
typedef enum {
#define ARC_OP_ENUM(name, byte, operands, pops, pushes, mnemonic) name = byte,
    ARC_OPCODES(ARC_OP_ENUM)
#undef ARC_OP_ENUM
} ArcOpcode;

/* Operand byte count for an opcode */
static inline int arc_op_operand_bytes(uint8_t op) {
    switch (op) {
#define ARC_OP_OPERANDS(name, byte, operands, pops, pushes, mnemonic) \
        case byte: return operands;
        ARC_OPCODES(ARC_OP_OPERANDS)
#undef ARC_OP_OPERANDS
        default: return -1;
    }
}

/* Mnemonic string for an opcode */
static inline const char* arc_op_mnemonic(uint8_t op) {
    switch (op) {
#define ARC_OP_MNEMONIC(name, byte, operands, pops, pushes, mnemonic) \
        case byte: return mnemonic;
        ARC_OPCODES(ARC_OP_MNEMONIC)
#undef ARC_OP_MNEMONIC
        default: return "???";
    }
}

/* Stack pops (-1 means variable, depends on argc operand) */
static inline int arc_op_pops(uint8_t op) {
    switch (op) {
#define ARC_OP_POPS(name, byte, operands, pops, pushes, mnemonic) \
        case byte: return pops;
        ARC_OPCODES(ARC_OP_POPS)
#undef ARC_OP_POPS
        default: return -1;
    }
}

/* Stack pushes */
static inline int arc_op_pushes(uint8_t op) {
    switch (op) {
#define ARC_OP_PUSHES(name, byte, operands, pops, pushes, mnemonic) \
        case byte: return pushes;
        ARC_OPCODES(ARC_OP_PUSHES)
#undef ARC_OP_PUSHES
        default: return -1;
    }
}

/* Well-known intrinsic IDs */
typedef enum {
    ARC_INTRINSIC_PRINT = 0,
    ARC_INTRINSIC_CLOCK = 1,
    ARC_INTRINSIC_COUNT
} ArcIntrinsicId;

#endif /* ARCANA_OPCODES_H */
