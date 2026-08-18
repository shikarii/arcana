#ifndef ARCANA_FIXTURE_KINDS_H
#define ARCANA_FIXTURE_KINDS_H

#include "semantic_graph.h"

#ifdef _MSC_VER
#define arc_strdup _strdup
#else
#define arc_strdup strdup
#endif

ArcRegionKind arc_fk_region_kind(const char* s);
ArcNodeKind   arc_fk_lookup_kind(const char* name);
ArcNodeKind   arc_fk_parse_node_kind(const char* s, char* attr_buf, size_t attr_sz);
void          arc_fk_set_node_attr(ArcNode* n, ArcNodeKind kind, const char* attr_buf);

#endif /* ARCANA_FIXTURE_KINDS_H */
