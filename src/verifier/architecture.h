#ifndef ARCANA_ARCHITECTURE_H
#define ARCANA_ARCHITECTURE_H

#include "../common/arcana_common.h"
#include "../semantic_graph/semantic_graph.h"

/*
 * Arcana Architecture Verifier — Experiment 1
 *
 * Validates Visible Dependency Completeness for sealed regions:
 *   1. Effects(R) <= Capabilities(R) for each sealed region
 *   2. Cross-boundary edges require declared channels
 *
 * Fixed effect enum — no user-defined effects, no polymorphism.
 * Transitive effect analysis via graph walk.
 */

/* --- Effect bitmask --- */
typedef enum {
    ARC_EFFECT_PURE       = 0,
    ARC_EFFECT_CLOCK      = (1 << 0),
    ARC_EFFECT_LOGGER     = (1 << 1),
    ARC_EFFECT_NETWORK    = (1 << 2),
    ARC_EFFECT_FILESYSTEM = (1 << 3),
    ARC_EFFECT_HARDWARE   = (1 << 4),
    ARC_EFFECT_INPUT      = (1 << 5),
} ArcEffect;

typedef uint32_t ArcEffectSet;

#define ARC_EFFECT_NAME_COUNT 6

/* --- Architecture specification for a region --- */
typedef struct {
    ArcRegionId region;
    bool        sealed;
    ArcEffectSet capabilities;
} ArcArchRegionSpec;

/* --- Declared channel between regions --- */
typedef struct {
    ArcRegionId from_region;
    ArcRegionId to_region;
} ArcArchChannel;

/* --- Full architecture specification (overlay on ArcGraph) --- */
#define ARC_ARCH_MAX_REGIONS  64
#define ARC_ARCH_MAX_CHANNELS 64

typedef struct {
    ArcArchRegionSpec regions[ARC_ARCH_MAX_REGIONS];
    uint32_t          region_count;
    ArcArchChannel    channels[ARC_ARCH_MAX_CHANNELS];
    uint32_t          channel_count;
} ArcArchSpec;

/* --- Verification result --- */
typedef struct {
    char        message[256];
    ArcRegionId region;
} ArcArchError;

#define ARC_ARCH_MAX_ERRORS 32

typedef struct {
    ArcArchError errors[ARC_ARCH_MAX_ERRORS];
    int          error_count;
    bool         valid;
} ArcArchResult;

/* --- API --- */

void         arc_arch_spec_init(ArcArchSpec* spec);
void         arc_arch_spec_add_sealed(ArcArchSpec* spec, ArcRegionId r, ArcEffectSet caps);
void         arc_arch_spec_add_channel(ArcArchSpec* spec, ArcRegionId from, ArcRegionId to);

ArcArchResult arc_arch_verify(const ArcGraph* g, const ArcArchSpec* spec);

/* Effect lookup for nodes and intrinsics */
ArcEffectSet  arc_node_effect(ArcNodeKind kind);
ArcEffectSet  arc_intrinsic_effect(uint16_t intrinsic_id);

/* Parse effect name to bitmask (returns 0 on unknown) */
ArcEffectSet  arc_effect_parse(const char* name);
const char*   arc_effect_name(ArcEffect e);

#endif /* ARCANA_ARCHITECTURE_H */
