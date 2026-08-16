#ifndef ARCANA_FORMAT_H
#define ARCANA_FORMAT_H

#include "../common/arcana_common.h"

/*
 * Arcana .mgc bytecode file format.
 *
 * Layout:
 *   Header (12 bytes)
 *   Constant Pool
 *   Function Table
 *   Code Section
 *   Debug Section (optional)
 */

/* Magic: "ARCA" */
#define ARC_MAGIC_0 'A'
#define ARC_MAGIC_1 'R'
#define ARC_MAGIC_2 'C'
#define ARC_MAGIC_3 'A'

#define ARC_VERSION_MAJOR 0
#define ARC_VERSION_MINOR 1

/* --- Constant pool entry tags --- */
typedef enum {
    ARC_CONST_NULL   = 0,
    ARC_CONST_BOOL   = 1,
    ARC_CONST_I64    = 2,
    ARC_CONST_F64    = 3,
    ARC_CONST_STRING = 4,
} ArcConstTag;

/* --- Runtime constant value --- */
typedef struct {
    ArcConstTag tag;
    union {
        bool     b;
        int64_t  i64;
        double   f64;
        struct { char* data; uint32_t len; } str;
    } as;
} ArcConstant;

/* --- Constant pool --- */
typedef struct {
    ArcConstant* entries;
    uint16_t     count;
    uint16_t     cap;
} ArcConstPool;

void arc_const_pool_init(ArcConstPool* pool);
void arc_const_pool_free(ArcConstPool* pool);
uint16_t arc_const_pool_add_null(ArcConstPool* pool);
uint16_t arc_const_pool_add_bool(ArcConstPool* pool, bool v);
uint16_t arc_const_pool_add_i64(ArcConstPool* pool, int64_t v);
uint16_t arc_const_pool_add_f64(ArcConstPool* pool, double v);
uint16_t arc_const_pool_add_string(ArcConstPool* pool, const char* s, uint32_t len);

/* --- Upvalue descriptor (for closures) --- */
typedef struct {
    bool     is_local;  /* true = captures a local in enclosing scope */
    uint16_t index;     /* local slot (if is_local) or upvalue idx in enclosing func */
} ArcUpvalueDesc;

/* --- Function record --- */
typedef struct {
    uint16_t name_const_idx;  /* index into constant pool for name string */
    uint8_t  arity;
    uint16_t local_count;
    uint16_t max_stack;
    uint32_t code_offset;     /* byte offset into code section */
    uint32_t code_length;     /* byte count */
    uint8_t  upvalue_count;   /* number of captured upvalues */
    ArcUpvalueDesc* upvalues; /* array of upvalue descriptors (NULL if 0) */
} ArcFuncRecord;

/* --- Function table --- */
typedef struct {
    ArcFuncRecord* funcs;
    uint16_t       count;
    uint16_t       cap;
} ArcFuncTable;

void arc_func_table_init(ArcFuncTable* t);
void arc_func_table_free(ArcFuncTable* t);
uint16_t arc_func_table_add(ArcFuncTable* t, ArcFuncRecord rec);

/* --- Debug entry: maps bytecode range to source element --- */
typedef struct {
    uint16_t      func_idx;
    uint32_t      bc_start;
    uint32_t      bc_end;
    ArcElementId  element_id;
} ArcDebugEntry;

typedef struct {
    ArcDebugEntry* entries;
    uint32_t       count;
    uint32_t       cap;
} ArcDebugTable;

void arc_debug_table_init(ArcDebugTable* t);
void arc_debug_table_free(ArcDebugTable* t);
void arc_debug_table_add(ArcDebugTable* t, ArcDebugEntry e);

/* --- Complete bytecode image --- */
typedef struct {
    ArcConstPool  constants;
    ArcFuncTable  functions;
    uint8_t*      code;
    uint32_t      code_len;
    ArcDebugTable debug;
} ArcBytecodeImage;

void arc_image_init(ArcBytecodeImage* img);
void arc_image_free(ArcBytecodeImage* img);

/* Serialize to .mgc binary */
ArcStatus arc_image_write(const ArcBytecodeImage* img, uint8_t** out, size_t* out_len);

/* Deserialize from .mgc binary */
ArcStatus arc_image_read(const uint8_t* data, size_t len, ArcBytecodeImage* out);

#endif /* ARCANA_FORMAT_H */
