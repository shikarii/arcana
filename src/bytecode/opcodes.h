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
    /* --- Type conversion --- */ \
    X(OP_CAST_I64,      0x14, 0, 1, 1, "cast_i64")      /* any → i64 */ \
    X(OP_CAST_F64,      0x15, 0, 1, 1, "cast_f64")      /* any → f64 */ \
    X(OP_CAST_STR,      0x16, 0, 1, 1, "cast_str")      /* any → string */ \
    /* --- Arithmetic --- */ \
    X(OP_ADD,           0x20, 0, 2, 1, "add")           \
    X(OP_SUB,           0x21, 0, 2, 1, "sub")           \
    X(OP_MUL,           0x22, 0, 2, 1, "mul")           \
    X(OP_DIV,           0x23, 0, 2, 1, "div")           \
    X(OP_MOD,           0x24, 0, 2, 1, "mod")           \
    X(OP_NEG,           0x25, 0, 1, 1, "neg")           \
    /* --- String ops --- */ \
    X(OP_STR_LEN,       0x26, 0, 1, 1, "str_len")       /* [str] → [i64] */ \
    X(OP_STR_SLICE,     0x27, 0, 3, 1, "str_slice")     /* [str, start, end] → [str] */ \
    X(OP_STR_INDEX,     0x28, 0, 2, 1, "str_index")     /* [str, idx] → [str(1char)] */ \
    /* --- Bitwise --- */ \
    X(OP_BIT_AND,       0x29, 0, 2, 1, "bit_and")       /* [a, b] → [a & b] (i64) */ \
    X(OP_BIT_OR,        0x2A, 0, 2, 1, "bit_or")        \
    X(OP_BIT_XOR,       0x2B, 0, 2, 1, "bit_xor")       \
    X(OP_BIT_NOT,       0x2C, 0, 1, 1, "bit_not")       /* [a] → [~a] */ \
    X(OP_SHL,           0x2D, 0, 2, 1, "shl")           \
    X(OP_SHR,           0x2E, 0, 2, 1, "shr")           \
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
    /* --- Exception handling --- */ \
    X(OP_TRY_BEGIN,     0x43, 4, 0, 0, "try_begin")     /* i32 catch_offset */ \
    X(OP_TRY_END,       0x44, 0, 0, 0, "try_end")       \
    X(OP_THROW,         0x45, 0, 1, 0, "throw")         \
    /* --- Functions --- */ \
    X(OP_CALL,          0x50, 4, -1, 1, "call")         /* u16 func_idx, u8 argc */ \
    X(OP_RETURN,        0x51, 0, 1, 0, "return")        \
    /* --- Closures --- */ \
    X(OP_CLOSURE,       0x52, 2, 0, 1, "closure")       /* u16 func_idx */ \
    X(OP_GET_UPVAL,     0x53, 2, 0, 1, "get_upvalue")   /* u16 upval_idx */ \
    X(OP_SET_UPVAL,     0x54, 2, 1, 0, "set_upvalue")   /* u16 upval_idx */ \
    X(OP_CLOSE_UPVAL,   0x55, 0, 0, 0, "close_upvalue") \
    /* --- Collections --- */ \
    X(OP_ARRAY_NEW,     0x60, 2, -1, 1, "array_new")    /* u16 count */ \
    X(OP_INDEX_GET,     0x61, 0, 2, 1, "index_get")     /* [container, key] → [value] */ \
    X(OP_INDEX_SET,     0x62, 0, 3, 0, "index_set")     /* [container, key, value] → [] */ \
    X(OP_LENGTH,        0x63, 0, 1, 1, "length")        /* [container] → [i64] */ \
    X(OP_MAP_NEW,       0x64, 2, -1, 1, "map_new")      /* u16 pair_count */ \
    /* --- Records --- */ \
    X(OP_RECORD_NEW,    0x65, 2, 0, 1, "record_new")   /* u16 type_name_const_idx */ \
    X(OP_FIELD_GET,     0x66, 2, 1, 1, "field_get")    /* u16 field_name_idx; [rec] → [val] */ \
    X(OP_FIELD_SET,     0x67, 2, 2, 1, "field_set")    /* u16 field_name_idx; [rec, val] → [rec] */ \
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
    ARC_INTRINSIC_PRINT    = 0,
    ARC_INTRINSIC_CLOCK    = 1,
    ARC_INTRINSIC_TYPE     = 2,
    ARC_INTRINSIC_ASSERT   = 3,
    ARC_INTRINSIC_TOSTRING = 4,
    ARC_INTRINSIC_INPUT    = 5,
    ARC_INTRINSIC_LEN      = 6,
    ARC_INTRINSIC_PUSH     = 7,
    ARC_INTRINSIC_KEYS     = 8,
    ARC_INTRINSIC_COUNT
} ArcIntrinsicId;

#endif /* ARCANA_OPCODES_H */
