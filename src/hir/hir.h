#ifndef ARCANA_HIR_H
#define ARCANA_HIR_H

#include "../common/arcana_common.h"

/*
 * Arcana HIR (High-level Intermediate Representation)
 *
 * HIR is the first explicit compiler IR after the semantic graph.
 * It preserves semantic structure (functions, scopes, expressions)
 * in a form that is easier to analyze than the graph topology.
 *
 * Topology-specific concepts (cyclic port order, region containment)
 * have been resolved into explicit operand order and lexical scoping.
 *
 * Invariants:
 *   - All variables are resolved to scope-local indices
 *   - Operand order is explicit (no topology lookup needed)
 *   - Function calls reference function IDs, not graph names
 *   - Source provenance (element_id) is preserved on every node
 */

/* HIR expression kinds */
typedef enum {
    HIR_CONST_INT,
    HIR_CONST_FLOAT,
    HIR_CONST_BOOL,
    HIR_CONST_NULL,
    HIR_ADD, HIR_SUB, HIR_MUL, HIR_DIV, HIR_MOD,
    HIR_NEG, HIR_NOT,
    HIR_EQ, HIR_NEQ, HIR_LT, HIR_LE, HIR_GT, HIR_GE,
    HIR_VAR_LOAD,       /* read a local variable */
    HIR_CALL,           /* function call */
    HIR_INTRINSIC,      /* intrinsic call */
    HIR_POISON,         /* error recovery: placeholder for failed lowering */
} HirExprKind;

/* HIR statement kinds */
typedef enum {
    HIR_STMT_EXPR,      /* expression as statement (value discarded) */
    HIR_STMT_LET,       /* let binding: var_idx = expr */
    HIR_STMT_ASSIGN,    /* assign: var_idx = expr */
    HIR_STMT_RETURN,    /* return expr */
    HIR_STMT_IF,        /* if (cond) { then_stmts } else { else_stmts } */
    HIR_STMT_WHILE,     /* while (cond) { body_stmts } */
    HIR_STMT_PRINT,     /* print expr */
} HirStmtKind;

/* Forward declarations */
typedef struct HirExpr HirExpr;
typedef struct HirStmt HirStmt;
typedef struct HirBlock HirBlock;

/* Expression node */
struct HirExpr {
    HirExprKind   kind;
    ArcElementId  source_id;     /* provenance back to drawing */

    union {
        int64_t   int_val;       /* CONST_INT */
        double    float_val;     /* CONST_FLOAT */
        bool      bool_val;      /* CONST_BOOL */
        /* CONST_NULL has no data */

        struct { HirExpr* lhs; HirExpr* rhs; } binary;  /* ADD..GE */
        struct { HirExpr* operand; } unary;              /* NEG, NOT */
        uint16_t  var_idx;                               /* VAR_LOAD */
        struct {
            uint16_t  func_idx;
            HirExpr** args;
            uint8_t   argc;
        } call;                                          /* CALL */
        struct {
            uint16_t  intrinsic_id;
            HirExpr** args;
            uint8_t   argc;
        } intrinsic;                                     /* INTRINSIC */
    } as;
};

/* Statement block (array of statements) */
struct HirBlock {
    HirStmt*  stmts;
    uint32_t  count;
    uint32_t  cap;
};

/* Statement node */
struct HirStmt {
    HirStmtKind   kind;
    ArcElementId  source_id;

    union {
        HirExpr*  expr;                                  /* EXPR */
        struct { uint16_t var_idx; HirExpr* value; } let;  /* LET */
        struct { uint16_t var_idx; HirExpr* value; } assign; /* ASSIGN */
        struct { HirExpr* value; } ret;                  /* RETURN */
        struct {
            HirExpr*  cond;
            HirBlock  then_block;
            HirBlock  else_block;
        } if_stmt;                                       /* IF */
        struct {
            HirExpr*  cond;
            HirBlock  body;
        } while_stmt;                                    /* WHILE */
        struct { HirExpr* value; } print;                /* PRINT */
    } as;
};

/* HIR function */
typedef struct {
    char        name[64];
    uint8_t     arity;
    uint16_t    local_count;    /* total locals including params */
    HirBlock    body;
    ArcElementId source_id;
} HirFunction;

/* HIR module (top-level compilation unit) */
typedef struct {
    HirFunction* functions;
    uint32_t     func_count;
    uint32_t     func_cap;

    HirBlock     main_body;     /* top-level statements */
    ArcNodeId    output_node;   /* graph output node id (if any) */
} HirModule;

/* === API === */

void hir_module_init(HirModule* m);
void hir_module_free(HirModule* m);

/* Pretty-print */
void hir_dump(const HirModule* m, FILE* out);
void hir_dump_expr(const HirExpr* e, FILE* out, int indent);
void hir_dump_block(const HirBlock* b, FILE* out, int indent);

/* Allocation helpers */
HirExpr* hir_expr_new(HirExprKind kind, ArcElementId src);
void     hir_expr_free(HirExpr* e);
HirStmt* hir_block_add(HirBlock* b, HirStmtKind kind, ArcElementId src);

/* Validation */
typedef struct {
    char     message[256];
    uint32_t func_idx;
} HirError;

typedef struct {
    HirError* errors;
    uint32_t  count;
    uint32_t  cap;
    bool      valid;
} HirValidation;

HirValidation hir_validate(const HirModule* m);
void hir_validation_free(HirValidation* v);

#endif /* ARCANA_HIR_H */
