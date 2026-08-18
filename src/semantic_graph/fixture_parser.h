#ifndef ARCANA_FIXTURE_PARSER_H
#define ARCANA_FIXTURE_PARSER_H

#include "semantic_graph.h"
#include "../verifier/architecture.h"
#include "../geometry/containment.h"

/*
 * Parse a text fixture into an ArcGraph + optional architecture spec + geometry.
 *
 * Format:
 *   region <id> <kind> [parent=<id>]
 *   node <id> <kind>[(<attr>)] in <region_id> [ports=[p1,p2,...]] [cyclic=[p1,p2,...]]
 *   edge <id> <node_id>.<port_role> -> <node_id>.<port_role>
 *   root <node_id>.<port_role>
 *
 * Architecture extensions:
 *   sealed <region_id>
 *   capability <region_id> <effect_name>
 *   channel <region_id> -> <region_id>
 *
 * Geometry extensions (Experiment 2A):
 *   circle <region_name> <cx> <cy> <radius>
 *   position <node_name> <x> <y>
 */

typedef struct {
    ArcGraph      graph;
    ArcArchSpec   arch;
    ArcGeoLayout  geo;
    char          error[256];
    bool          success;
    bool          has_arch;    /* true if any sealed/capability/channel commands present */
    bool          has_geo;     /* true if any circle/position commands present */
} ArcFixtureResult;

/* Parse fixture text (null-terminated) into a graph */
ArcFixtureResult arc_fixture_parse(const char* text);

/* Parse fixture from file */
ArcFixtureResult arc_fixture_parse_file(const char* path);

#endif /* ARCANA_FIXTURE_PARSER_H */
