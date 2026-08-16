#ifndef ARCANA_FIXTURE_PARSER_H
#define ARCANA_FIXTURE_PARSER_H

#include "semantic_graph.h"

/*
 * Parse a text fixture into an ArcGraph.
 *
 * Format:
 *   region <id> <kind> [parent=<id>]
 *   node <id> <kind>[(<attr>)] in <region_id> [ports=[p1,p2,...]] [cyclic=[p1,p2,...]]
 *   edge <id> <node_id>.<port_role> -> <node_id>.<port_role>
 *   root <node_id>.<port_role>
 *
 * Example:
 *   region r0 module
 *   node n0 const_int(5) in r0
 *   node n1 const_int(10) in r0
 *   node n2 add in r0 ports=[lhs,rhs,out] cyclic=[lhs,rhs,out]
 *   edge e0 n0.out -> n2.lhs
 *   edge e1 n1.out -> n2.rhs
 *   root n2.out
 */

typedef struct {
    ArcGraph   graph;
    char       error[256];
    bool       success;
} ArcFixtureResult;

/* Parse fixture text (null-terminated) into a graph */
ArcFixtureResult arc_fixture_parse(const char* text);

/* Parse fixture from file */
ArcFixtureResult arc_fixture_parse_file(const char* path);

#endif /* ARCANA_FIXTURE_PARSER_H */
