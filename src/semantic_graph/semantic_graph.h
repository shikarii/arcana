#ifndef ARCANA_SEMANTIC_GRAPH_H
#define ARCANA_SEMANTIC_GRAPH_H

#include "../common/arcana_common.h"

/*
 * Arcana Semantic Graph — the canonical compiler input.
 *
 * This represents normalized topology derived from the drawing:
 * - Regions (containment = lexical scope)
 * - Nodes (operations, literals, bindings)
 * - Ports (input/output connection points on nodes)
 * - Edges (directed data/value dependencies)
 * - Cyclic port order (determines operand/argument ordering)
 */

/* --- Node kinds --- */
typedef enum {
    ARC_NODE_CONST_INT,     /* literal integer */
    ARC_NODE_CONST_FLOAT,   /* literal float */
    ARC_NODE_CONST_BOOL,    /* literal bool */
    ARC_NODE_CONST_NULL,    /* null literal */
    ARC_NODE_ADD,           /* binary add */
    ARC_NODE_SUB,           /* binary sub */
    ARC_NODE_MUL,           /* binary mul */
    ARC_NODE_DIV,           /* binary div */
    ARC_NODE_MOD,           /* binary mod */
    ARC_NODE_NEG,           /* unary negate */
    ARC_NODE_EQ,            /* equality */
    ARC_NODE_NEQ,           /* inequality */
    ARC_NODE_LT,            /* less than */
    ARC_NODE_LE,            /* less or equal */
    ARC_NODE_GT,            /* greater than */
    ARC_NODE_GE,            /* greater or equal */
    ARC_NODE_NOT,           /* logical not */
    ARC_NODE_IF,            /* conditional: has then_region, else_region */
    ARC_NODE_WHILE,         /* loop: has cond, body_region */
    ARC_NODE_LET,           /* local binding: name in attributes */
    ARC_NODE_VAR_REF,       /* reference to a binding */
    ARC_NODE_ASSIGN,        /* assignment to a binding */
    ARC_NODE_FUNC_DEF,      /* function definition */
    ARC_NODE_FUNC_CALL,     /* function call */
    ARC_NODE_PARAM,         /* function parameter */
    ARC_NODE_RETURN,        /* return from function */
    ARC_NODE_PRINT,         /* intrinsic print */
    ARC_NODE_ROOT_OUTPUT,   /* root output / program result */
    ARC_NODE_SEQUENCE,      /* ordered statement sequence */
} ArcNodeKind;

/* --- Port direction --- */
typedef enum {
    ARC_PORT_INPUT,
    ARC_PORT_OUTPUT,
} ArcPortDir;

/* --- Port --- */
typedef struct {
    ArcPortId  id;
    ArcNodeId  owner;
    ArcPortDir dir;
    const char* role;   /* e.g., "lhs", "rhs", "out", "value", "cond" */
} ArcPort;

/* --- Edge --- */
typedef struct {
    ArcEdgeId  id;
    ArcPortId  from;    /* output port */
    ArcPortId  to;      /* input port */
} ArcSemEdge;

/* --- Region kind --- */
typedef enum {
    ARC_REGION_MODULE,      /* top-level module scope */
    ARC_REGION_FUNCTION,    /* function body scope */
    ARC_REGION_BLOCK,       /* nested block/scope */
    ARC_REGION_THEN,        /* then branch of if */
    ARC_REGION_ELSE,        /* else branch of if */
    ARC_REGION_LOOP_BODY,   /* loop body */
} ArcRegionKind;

/* --- Region --- */
typedef struct {
    ArcRegionId   id;
    ArcRegionId   parent;     /* ARC_INVALID_ID for root */
    ArcRegionKind kind;
    ArcNodeId*    members;    /* array of node IDs contained in this region */
    uint32_t      member_count;
    uint32_t      member_cap;
} ArcRegion;

/* --- Node --- */
typedef struct {
    ArcNodeId     id;
    ArcNodeKind   kind;
    ArcRegionId   region;       /* which region this node belongs to */
    ArcElementId  source_id;    /* stable source element identity */

    /* Ports */
    ArcPortId*    ports;
    uint32_t      port_count;
    uint32_t      port_cap;

    /* Cyclic port order (determines operand ordering) */
    ArcPortId*    cyclic_order;
    uint32_t      cyclic_count;

    /* Attributes (kind-specific) */
    union {
        int64_t     int_value;      /* CONST_INT */
        double      float_value;    /* CONST_FLOAT */
        bool        bool_value;     /* CONST_BOOL */
        const char* name;           /* LET, VAR_REF, ASSIGN, FUNC_DEF, PARAM */
        struct {
            const char* name;
            ArcRegionId body_region;
            uint8_t     arity;
        } func;                     /* FUNC_DEF */
        struct {
            ArcRegionId then_region;
            ArcRegionId else_region;
        } branch;                   /* IF */
        struct {
            ArcRegionId body_region;
        } loop;                     /* WHILE */
    } attr;
} ArcNode;

/* --- Program graph --- */
typedef struct {
    ArcNode*     nodes;
    uint32_t     node_count;
    uint32_t     node_cap;

    ArcPort*     ports;
    uint32_t     port_count;
    uint32_t     port_cap;

    ArcSemEdge*  edges;
    uint32_t     edge_count;
    uint32_t     edge_cap;

    ArcRegion*   regions;
    uint32_t     region_count;
    uint32_t     region_cap;

    ArcRegionId  root_region;

    /* Output node (what the program computes) */
    ArcNodeId    output_node;
} ArcGraph;

/* === API === */

void arc_graph_init(ArcGraph* g);
void arc_graph_free(ArcGraph* g);

ArcRegionId arc_graph_add_region(ArcGraph* g, ArcRegionKind kind, ArcRegionId parent);
ArcNodeId   arc_graph_add_node(ArcGraph* g, ArcNodeKind kind, ArcRegionId region, ArcElementId src);
ArcPortId   arc_graph_add_port(ArcGraph* g, ArcNodeId owner, ArcPortDir dir, const char* role);
ArcEdgeId   arc_graph_add_edge(ArcGraph* g, ArcPortId from, ArcPortId to);

void arc_node_set_cyclic_order(ArcGraph* g, ArcNodeId node, const ArcPortId* order, uint32_t count);
void arc_region_add_member(ArcGraph* g, ArcRegionId region, ArcNodeId node);

/* Lookup helpers */
ArcNode*   arc_graph_node(ArcGraph* g, ArcNodeId id);
ArcPort*   arc_graph_port(ArcGraph* g, ArcPortId id);
ArcRegion* arc_graph_region(ArcGraph* g, ArcRegionId id);

/* === Validation === */

typedef struct {
    char message[256];
} ArcGraphError;

typedef struct {
    ArcGraphError* errors;
    int            count;
    int            cap;
    bool           valid;
} ArcGraphValidation;

ArcGraphValidation arc_graph_validate(const ArcGraph* g);
void arc_graph_validation_free(ArcGraphValidation* v);

#endif /* ARCANA_SEMANTIC_GRAPH_H */
