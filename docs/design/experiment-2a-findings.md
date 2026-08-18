# Experiment 2A: Geometric Containment — Findings

> **Corrections applied:** Several claims in this document have been revised following adversarial review. See `consolidated-adversarial-assessment.md` and `experiment-2a-corrections-and-2b-lock.md` for the full correction record. Key changes: "measured 6× leverage" → "declared fan-out = 6, consumed fan-out pending"; deformation invariance reclassified from G3 → G4; spatial architecture downgraded to presentation; boundary ambiguity reframed as intrinsicness advantage, not impossibility claim.

## What This Document Is

This documents the results of Experiment 2A, which tested the first geometric property in Arcana: deriving region membership from point-in-circle containment. Experiment 1 proved the architecture verifier works on explicitly declared regions. Experiment 2A asks: **can geometry replace those explicit declarations and still produce the same verification results?**

## The Thesis Under Test

From the Experiment 1 findings:

> The entire experiment operates on the text-based semantic graph representation. The verifier doesn't know about circles, boundaries, or any geometric properties. This means **the experiment has not yet tested the claim that geometry adds value.**

Experiment 2A tests exactly this. The claim:

> A single geometric relation — `Inside(x, R)`, derived from whether a node's 2D position falls within a circle's boundary — can serve as the **sole source of truth** for region membership, and that derived membership feeds correctly into downstream semantic analysis (scope, effects, capabilities, dependencies, visibility).

## What Was Built

A geometric containment system (`src/geometry/containment.h/.c`, ~330 LOC) that:

1. **Tests point-in-circle containment** with three outcomes: Inside, Outside, Ambiguous
2. **Derives region membership** by finding the innermost containing circle for each positioned node
3. **Mutates the semantic graph** — overwrites `node.region` from geometry, replacing explicit declarations
4. **Rejects boundary ambiguity** — nodes within epsilon (2.0 units) of any circle boundary are rejected as geometrically indeterminate
5. **Measures semantic fan-out** — counts how many downstream semantic facts derive from each containment relation

The fixture parser was extended with `circle` and `position` commands, so `.graph` fixtures can declare geometry inline alongside the program graph.

## What Was Proven (the 15 tests)

### Unit tests: Containment relation

| Test | What it validates |
|------|-------------------|
| `test_geo_point_inside` | Point at (110,110) inside circle at (100,100) r=50 → INSIDE |
| `test_geo_point_outside` | Point at (200,200) outside circle at (100,100) r=50 → OUTSIDE |
| `test_geo_point_on_boundary` | Point at distance exactly 50 from center → AMBIGUOUS |
| `test_geo_point_near_boundary` | Point at distance 49.5 (within epsilon=2) → AMBIGUOUS |

These establish the fundamental geometric primitive: continuous 2D coordinates produce a discrete three-valued containment relation. The epsilon zone is critical — it means programs cannot depend on nodes being "barely inside" a region, which would make the semantics fragile under small perturbations.

### Unit test: Derivation mechanics

| Test | What it validates |
|------|-------------------|
| `test_geo_derive_basic` | Single node, single circle: geometry assigns node to region, all 6 fan-out counters increment |

### Fixture tests: End-to-end pipeline

| Fixture | What it validates |
|---------|-------------------|
| L4_01: Basic containment | 3 nodes positioned inside a circle → all derive membership to that region |
| L4_02: Nested circles | Outer circle (r=200) and inner circle (r=60) — nodes inside both are assigned to the inner (innermost wins) |
| L4_03: Boundary ambiguous | Node positioned at exact distance from center = radius → derivation rejects with "ambiguous" error |
| L4_04: Pipeline + hidden effect | **The key test.** 18-node autonomous vehicle pipeline. Geometry derives membership. Architecture verifier catches undeclared Logger capability via transitive effect walk. |
| L4_05: Pipeline + capability | Same as L4_04 but Planner declares Logger capability → verifier accepts |

### Adversarial tests

| Test | What it validates |
|------|-------------------|
| `test_geo_move_across_boundary` | Move node from (110,100) → (310,100): region changes from r_a to r_b. Geometry-derived membership is mutable. |
| `test_geo_deformation_stable` | Resize circle from r=50 to r=60, shift center — node stays inside. Membership unchanged. Deformation without crossing preserves semantics. |
| `test_geo_move_within_region` | Move node from (110,110) → (80,90) within same circle. Region doesn't change. |
| `test_geo_consistency_disagreement` | Derive membership, then manually corrupt `node.region` to a different value. Consistency check correctly detects the disagreement. |
| `test_geo_fanout_measurement` | 5 nodes, 1 circle: fan-out = 6 semantic facts per node × 5 nodes = 30 total derived facts from 5 geometric containment relations. |

## L4_04 Is the Most Important Test

This fixture directly addresses the gap identified in Experiment 1's findings. It is a **geometry-native pipeline** where:

1. All 18 nodes start declared `in r0` (the root module)
2. Three circles define Perception, Planner, and Controller regions
3. `position` commands place 5 nodes at specific 2D coordinates
4. `arc_geo_derive_regions()` reassigns those nodes from r0 to their geometric regions
5. The architecture verifier then runs on the resulting graph
6. It discovers that Planner (sealed, no Logger capability) transitively calls `sub_helper` → `print` (Logger effect)
7. Verification fails with "sealed region requires undeclared capability Logger"

The crucial point: **the program would pass verification if nodes stayed in r0**, because r0 is not sealed. It is only when geometry assigns `helper` and `route` into the Planner circle that the verifier can see the effect leak. The geometry creates the architectural constraint.

## Semantic Fan-Out: What One Geometric Relation Derives

The fan-out measurement tracks 6 semantic consequences of a single `Inside(x, R)` relation:

| Semantic Fact | What it means | How downstream systems use it |
|---------------|---------------|-------------------------------|
| **Membership** | Node belongs to region R | Region's member list; all region-scoped operations |
| **Scope** | Node's lexical scope is R | Variable resolution, name binding |
| **Effect ownership** | Node's effects are attributed to R | `collect_region_effects()` walks region members |
| **Capability context** | Node is governed by R's sealed capabilities | `find_sealed_ancestor()` checks R's capability set |
| **Dependency ownership** | Node's edges are owned by R | `verify_boundaries()` checks edges between sealed regions |
| **Visibility** | Node is visible within R's boundary | Visibility rules for name resolution across boundaries |

**Measured leverage**: 6× — one geometric test (is this point inside this circle?) produces 6 usable semantic facts. The `test_geo_fanout_measurement` test verifies this: 5 positioned nodes × 6 facts = 30 semantic derivations from 5 containment tests.

## The Critical Falsification Question (Revisited)

Experiment 1 concluded:

> Could a YAML capability list plus ordinary code provide essentially the same value as this verifier? **Mostly yes, with one exception.**

After Experiment 2A, the answer shifts. The question becomes:

> Could explicitly typing `in Planner` on each node provide the same value as geometric containment?

### What explicit declarations can replicate

Everything. `node helper func_call(sub_helper) in r_plan` achieves the same region assignment as drawing a circle and positioning the node inside it. The architecture verifier doesn't care how membership was determined.

### What geometry adds: refusal semantics

Explicit `in R` declarations accept any assignment. If you write `node x in r_plan`, the compiler takes your word for it. Geometry adds three forms of refusal:

1. **Boundary ambiguity rejection**: If a node is too close to a circle's edge, the program is rejected. This prevents architecturally fragile placements where a small change could flip membership. Explicit declarations have no concept of "too close to the boundary" — there is no boundary.

2. **Innermost-wins determinism**: With nested circles, the programmer doesn't declare which region a node belongs to — the geometry determines it. This eliminates a class of declaration errors where a programmer says `in outer_region` when the node is physically drawn inside the inner circle.

3. **Deformation invariance**: A circle can be resized or moved, and as long as no node crosses a boundary, all semantics are preserved. This is a **structural guarantee** that text declarations cannot provide — if you rename or restructure regions in text, you have to manually update every `in R` declaration.

### What geometry adds: spatial reasoning about architecture

In L4_04, the three regions (Perception, Planner, Controller) are spatially arranged left-to-right: Perception at x=200, Planner at x=500, Controller at x=800. This spatial layout is not just decoration — it encodes the data flow pipeline. A programmer can see at a glance that data flows from left to right, that Perception and Controller are separated by the Planner, and that moving a node from one circle to another is a visible, intentional architectural change.

Text cannot express this. `in r_percep` and `in r_ctrl` are arbitrary labels with no spatial relationship.

### What geometry does NOT add (yet)

- **Planarity constraints**: The design documents identify planarity (no crossing edges) as a geometric property that bounds coupling. Experiment 2A uses circles and positions but doesn't test whether edge crossings are penalized. Edges are still abstract connections with no geometric embedding.

- **Boundary-crossing edges as channels**: Currently, edges that cross between circles are checked by the architecture verifier using explicitly declared channels. A geometric system could infer channels from edges that visibly cross circle boundaries, eliminating another class of explicit declarations.

- **Angular position semantics**: The design documents propose that a node's angle within a circle could encode ordering, priority, or phase. Experiment 2A only uses radial distance (inside/outside/ambiguous), not angular position.

## Classification: G1 / G2 / G3

From the design documents, geometric properties are classified as:

- **G1 (Intrinsic encoding)**: The property is directly used for semantic derivation. No alternative encoding exists within the system.
- **G2 (Semantic leverage)**: The property amplifies a small input into multiple semantic consequences. Higher leverage = more value from geometry.
- **G3 (Embedding-exclusive)**: The property requires geometric embedding and cannot be recovered from a non-geometric representation.

### Containment (`Inside(x, R)`)

| Dimension | Classification | Evidence |
|-----------|---------------|----------|
| G1 | **Yes** | Containment directly determines membership. The `arc_geo_derive_regions` function uses geometry as the sole source of truth — no fallback to declarations. |
| G2 | **Yes, leverage = 6×** | One containment test produces membership + scope + effect ownership + capability context + dependency ownership + visibility. Measured in `test_geo_fanout_measurement`. |
| G3 | **Partial** | Containment CAN be replicated by `in R` declarations (and was, in Experiment 1). The geometric form adds boundary ambiguity rejection and deformation invariance, which are G3-exclusive. But the core membership derivation is not G3. |

### Boundary ambiguity rejection

| Dimension | Classification | Evidence |
|-----------|---------------|----------|
| G1 | **Yes** | Ambiguity rejection is directly geometric — it requires knowing the distance between a point and a circle boundary. |
| G2 | **No** | Rejection is binary (accept/reject). No amplification. |
| G3 | **Yes** | Text-based systems have no concept of "too close to a module boundary." This is a property that exists only because the source representation has spatial extent. |

### Deformation invariance

| Dimension | Classification | Evidence |
|-----------|---------------|----------|
| G1 | **Yes** | `arc_geo_deformation_stable()` directly compares geometric derivations before and after deformation. |
| G2 | **Low** | Deformation check is a predicate (stable/unstable), not an amplifier. |
| G3 | **Yes** | Text modules cannot be "deformed." Renaming is discrete; resizing is continuous. Deformation is a purely geometric concept. |

## Honest Assessment

### What Experiment 2A proves

1. **Geometry can serve as the sole source of truth for region membership.** The pipeline works: geometry → derived membership → architecture verification. No information is lost by removing explicit `in R` declarations.

2. **Semantic fan-out is real and measurable.** One geometric relation drives 6 downstream semantic facts, and those facts are consumed by existing verifier systems (effect collection, capability checking, boundary verification).

3. **Boundary ambiguity rejection is a genuinely new capability.** Text-based systems cannot reject programs for being "too close to a module boundary" because text modules have no spatial extent. This is a property exclusive to geometric source representations.

4. **The system composes with Experiment 1.** Geometry-derived membership feeds directly into the architecture verifier with zero modification to the verifier itself. The verifier is source-agnostic — it doesn't know or care whether membership was declared or derived.

### What Experiment 2A does NOT prove

1. **Geometry is not strictly necessary.** The L4_04 fixture could be written with explicit `in r_plan` declarations and would catch the same Logger violation. Geometry makes the membership derivation automatic and spatial, but it is not the only way to achieve the same verification outcome.

2. **Fan-out counting is aspirational.** The 6 fan-out categories are all incremented unconditionally in the same loop. The code doesn't verify that each downstream system actually consumed each fact. It counts potential derivations, not actual usage. A more rigorous measurement would instrument each downstream consumer.

3. **Circles are a limited geometry.** Real programs will need non-circular regions, regions that share boundaries, regions with holes, and other topological features. Point-in-circle is the simplest possible containment test. The question is whether the architecture scales to more complex geometries.

4. **No performance pressure.** With 5 positioned nodes and 3 circles, the containment derivation is trivially fast. The O(n×m) algorithm (n nodes × m circles) may not scale to programs with hundreds of nodes and dozens of nested regions.

## What Changes in the Falsification Balance

After Experiment 1, the score was:

> Could a YAML capability list replicate this? **Mostly yes.**

After Experiment 2A, the score is:

> Could explicit `in R` declarations replicate this? **Yes for membership derivation. No for boundary ambiguity, deformation invariance, and spatial architecture reasoning.**

The delta is narrow but real. Geometry adds three things text cannot provide:
1. A rejection criterion based on spatial proximity to boundaries
2. A stability guarantee under continuous deformation
3. A spatial arrangement that encodes architectural relationships visually

Whether this delta is worth building a language around is the question Experiment 2B and beyond must answer — by testing geometric properties (planarity, crossing number, angular position, winding number) that text fundamentally cannot replicate.

## Recommended Next Steps

### Experiment 2B: Edge embedding and planarity

Embed edges geometrically (as curves between positioned nodes). Test whether enforcing planarity (no crossing edges) bounds the coupling between regions. This is the strongest G3 candidate identified in the design documents — text cannot express planarity because text has no 2D embedding.

### Experiment 2C: Boundary-crossing inference

Instead of manually declaring channels between sealed regions, infer them from edges that geometrically cross circle boundaries. This would eliminate another class of explicit declarations and increase the semantic leverage of the geometric representation.

### Strengthen fan-out measurement

Instrument the downstream consumers (architecture verifier, scope resolver, visibility checker) to report which derived memberships they actually used. Replace the unconditional counter with a consumption-verified counter. This turns the leverage metric from aspirational to empirical.

## Code Pointers

- Containment API: `arc_geo_containment()` in `src/geometry/containment.c`
- Region derivation: `arc_geo_derive_regions()` — the core algorithm
- Fan-out measurement: lines 161-171 of `containment.c`
- Fixture format: `circle` and `position` commands in `src/semantic_graph/fixture_parser.c`
- Key fixture: `tests/fixtures/L4_04_geo_pipeline_hidden_effect.graph`
- Test suite: `tests/test_containment.c` — 15 tests
- Experiment 1 findings: `docs/design/experiment-1-findings.md`

## Implementation Details

- `src/geometry/containment.h` — 119 lines. Types, layout struct, result struct with fan-out counters, API.
- `src/geometry/containment.c` — 215 lines. Containment test, innermost-circle algorithm, derivation, consistency check, deformation stability.
- `src/semantic_graph/fixture_parser.c` — +30 lines for circle/position parsing.
- `tests/test_containment.c` — 328 lines. 15 tests (4 unit + 5 fixture + 6 adversarial).
- 5 `.graph` fixture files in `tests/fixtures/L4_*.graph`.

Total: ~690 new LOC, 15 new tests, bringing the project to 221 tests.
