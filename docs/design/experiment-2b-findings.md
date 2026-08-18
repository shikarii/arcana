# Experiment 2B: Cyclic Order — Findings

## What This Document Is

This documents the results of Experiment 2B, which tested the second geometric property in Arcana: deriving cyclic member order from angular position. Experiment 2A proved that containment (Inside/Outside) can replace explicit `in R` declarations. Experiment 2B asks: **can the angular arrangement of nodes around a center point produce semantic consequences that text cannot replicate?**

## The Thesis Under Test

From the Experiment 2A findings:

> Angular position semantics: The design documents propose that a node's angle within a circle could encode ordering, priority, or phase. Experiment 2A only uses radial distance (inside/outside/ambiguous), not angular position.

Experiment 2B tests exactly this. The claim:

> The cyclic order ρ = (m₀, m₁, ..., mₖ) of region members, derived from their angular positions via atan2(pos - center), determines successor/predecessor/adjacency relationships that text-based systems cannot express. Changing the physical arrangement of nodes — without changing nodes, edges, or regions — changes the program's semantics.

The geometric primitive under test is **Primitive R** (region member order), not Primitive V (vertex rotation system). The ordering is among the members of a region, not among edges incident on a hub node.

## What Was Built

A cyclic order system (`src/geometry/cyclic_order.h/.c`, ~300 LOC) that:

1. **Derives cyclic member order from atan2** — given positioned nodes in a region and a center point, sorts by angle to produce ρ
2. **Queries successor/predecessor/adjacency** — `arc_cyclic_next()`, `arc_cyclic_prev()`, `arc_cyclic_adjacent()`
3. **Rejects angular ambiguity** — members within 0.05 radians (~2.9°) of each other are rejected as angularly indeterminate
4. **Rejects center proximity** — members within 1.0 units of the center have undefined angles and are rejected
5. **Computes forgetful fingerprints** — `arc_graph_fingerprint()` hashes the abstract graph topology (node kinds, edges, regions) while stripping all geometric embedding (positions, circles, cyclic orders), enabling the U(A)=U(B) precondition check

A graph helper (`build_sensor_graph`) constructs the canonical test fixture: 4 sensor nodes in a ring region, each feeding a fusion node via directed edges. This is the minimal structure needed to test cyclic order effects.

## What Was Proven (the 13 tests)

### Phase 2B.0: Manual ρ (scaffolding — E0 evidence)

| Test | What it validates |
|------|-------------------|
| `test_cyclic_next_prev` | Wrap-around: next(last)=first, prev(first)=last. Missing node returns INVALID. |
| `test_cyclic_adjacent` | S0,S1 adjacent; S3,S0 adjacent (wrap); S0,S2 NOT adjacent in 4-member ring. |
| `test_cyclic_minimal_pair` | **Formal G3 (2B.0 only).** Same abstract graph (verified by fingerprint), different ρ: next(S0)=S1 vs. next(S0)=S2. Adjacency also differs. |

**Epistemic status**: These tests prove the plumbing works. They do NOT prove geometry adds value — manually setting ρ is equivalent to any ordering metadata. See Reporting Lock §3.

### Fingerprint tests: forgetful map infrastructure

| Test | What it validates |
|------|-------------------|
| `test_fingerprint_same_topology` | Two graphs built by the same procedure have identical FNV-1a fingerprints. |
| `test_fingerprint_different_topology` | Adding an extra edge changes the fingerprint. |
| `test_fingerprint_hidden_topology_control` | Pre-check: before any cyclic order is applied, minimal pair graphs must have matching fingerprints. Catches accidental topology differences. |

These are not experiment results — they validate the test infrastructure. The fingerprint enforces the U(A)=U(B) precondition that makes the minimal pair meaningful.

### Phase 2B.1: Geometry-derived ρ (the actual experiment)

| Test | What it validates | Evidence Level |
|------|-------------------|----------------|
| `test_cyclic_geo_derive_basic` | 4 nodes at cardinal positions around (100,100). atan2 produces order: S3(-π/2), S0(0), S1(π/2), S2(π). next(east)=north, next(west) wraps to south. | E3 |
| `test_cyclic_geo_g3` | **The key G3 test.** Same graph (fingerprint verified), S1 and S2 positions swapped. A: next(S0)=S1, B: next(S0)=S2. Same abstract graph, different geometry → different semantics. | **E2** |
| `test_cyclic_geo_g4_control` | Same angles at different distances (50 vs 80 from center). Same cyclic order → same next(). Geometric deformation that preserves angular order preserves semantics. | G4 |
| `test_cyclic_geo_angular_ambiguity` | S0 at (150,100), S1 at (150,100.5) — nearly identical angles. Derivation rejected as angularly indeterminate. | Rejection |
| `test_cyclic_geo_center_proximity` | S0 at (100.5,100) — 0.5 units from center. Derivation rejected: undefined angle. | Rejection |
| `test_cyclic_geo_reflection` | Positions reflected across x-axis (negate y). A: next(S0)=S1, B: next(S0)=S3. Reflection reverses successor order — orientation-sensitive semantics. | Orientation |
| `test_cyclic_sensor_ring_realistic` | **Naturalness test.** 4 sensors, fusion node, S1/S2 positions swapped. In A, S0 and S1 are adjacent (correlated readings expected). In B, S0 and S1 are not adjacent (correlation is suspicious). Same program, different physical arrangement, different semantic conclusion. | **E4** |

## `test_cyclic_geo_g3` Is the Most Important Test

This test directly answers the G3 question: does 2D geometric embedding produce semantic consequences that the abstract graph alone cannot?

1. Two graphs are constructed with identical nodes, edges, regions, and port roles
2. The forgetful fingerprint verifies U(A) = U(B) — the abstract topologies are identical
3. In layout A, S1 is at the north position (100, 150) and S2 is at the west position (50, 100)
4. In layout B, these positions are swapped — S2 at north, S1 at west
5. atan2 derivation produces different cyclic orders: ρ_A ≠ ρ_B
6. Consequently: `next(S0)` returns S1 in A but S2 in B
7. `adjacent(S0, S1)` is true in A but false in B

**The semantic difference comes entirely from the 2D positions.** The abstract graph has no mechanism to distinguish "S1 is clockwise of S0" from "S2 is clockwise of S0" — both are FUNC_CALL nodes in the same region with identical connectivity. Only the geometric embedding provides this distinction.

## What Cyclic Order Adds That Text Cannot

### Successor/adjacency from spatial arrangement

In a text-based language, the ordering `[S0, S1, S2, S3]` is a list with a first element. In Arcana, the cyclic order ρ = (S0, S1, S2, S3) has no first element — every member is equivalent under rotation. The `next_clockwise` relation wraps around: next(S3) = S0. This is a genuine cyclic structure derived from the continuous geometry of angular position.

Text can represent a list. Text cannot represent a ring without choosing an arbitrary start index. The atan2 derivation produces a ring directly from positions — no start index is ever chosen.

### Refusal semantics (angular analog of containment epsilon)

Two forms of rejection mirror Experiment 2A's boundary ambiguity:

1. **Angular ambiguity**: If two members are within 0.05 radians of each other, the cyclic order between them is indeterminate. The program is rejected. Text-based orderings have no concept of "too close in angle" — every pair of list elements has a definite order.

2. **Center proximity**: A member positioned at the ring center has no defined angle. The program is rejected. Text-based orderings don't have a center — every element has a position.

### Orientation sensitivity

Reflecting positions across an axis reverses the successor relationship. This is a purely geometric property — "clockwise" has meaning only in an embedding. In text, `[A, B, C, D]` and `[A, D, C, B]` are different lists, but neither is "the reflection of" the other. In Arcana, they are related by a geometric transformation (reflection), and that relationship is semantically meaningful.

### The sensor ring as a natural use case

The realistic fixture models 4 sensors physically arranged around a platform. The `adjacent_consistency` relation — "are these two sensors physically adjacent?" — is a real question in robotics, industrial monitoring, and distributed sensing. The answer depends on the physical arrangement, which is exactly what cyclic order encodes.

If sensors are repositioned (S1 and S2 swap physical locations), the adjacency graph changes. In layout A, S0-S1 are adjacent and expected to produce correlated readings. In layout B, S0-S1 are not adjacent, so correlation between them is anomalous. This is a genuine semantic difference produced by geometry.

## Classification: G1 / G2 / G3 / G4

### Cyclic order (`next_clockwise`)

| Dimension | Classification | Evidence |
|-----------|---------------|----------|
| G1 | **Yes** | `arc_geo_derive_cyclic_order` uses atan2 as the sole source of truth for member ordering. No fallback to declaration. |
| G2 | **Low** | One cyclic order derivation produces next/prev/adjacency queries (3 relations), but all are simple lookups on the same array. Leverage ~3×, lower than containment's 6×. |
| G3 | **Yes** | `test_cyclic_geo_g3`: same abstract graph (fingerprint verified), different positions → different ρ → different semantics. The abstract graph cannot distinguish the two programs. |
| G4 | **Yes** | `test_cyclic_geo_g4_control`: different positions but same angles → same ρ → same semantics. Deformation that preserves angular order preserves meaning. |

### Angular ambiguity rejection

| Dimension | Classification | Evidence |
|-----------|---------------|----------|
| G1 | **Yes** | Rejection requires knowing the angular distance between two members — a geometric quantity. |
| G2 | **No** | Binary accept/reject. No amplification. |
| G3 | **Yes** | Text orderings have no angular distance. "Too close in angle" is a property exclusive to geometric embeddings. |

### Orientation sensitivity (reflection reversal)

| Dimension | Classification | Evidence |
|-----------|---------------|----------|
| G1 | **Yes** | `test_cyclic_geo_reflection`: reflection of positions reverses successor order. |
| G2 | **No** | Binary same/reversed. No amplification. |
| G3 | **Yes** | "Clockwise" is an embedding property. Text sequences have no handedness. |

## Honest Assessment

### What Experiment 2B proves

1. **Cyclic order is a genuine G3 property.** The same abstract graph produces different semantics under different geometric embeddings. This is the first property in Arcana where geometry provides information that the abstract representation provably lacks — the forgetful fingerprint confirms the abstract graphs are identical.

2. **The derivation pipeline works end-to-end.** 2D positions → atan2 angles → sorted cyclic order → successor/adjacency queries. No manual metadata injection. The geometry is the sole source of truth.

3. **Angular refusal semantics are real and analogous to containment epsilon.** Programs with ambiguous angular arrangements or center-proximate members are rejected, preventing fragile cyclic orders that small perturbations could change.

4. **Orientation is semantically meaningful.** Reflection reverses successor order. This is a property that exists only in geometric embeddings — text has no concept of handedness.

5. **The sensor ring is a plausible (not contrived) use case.** Physical sensor arrangement determining adjacency relationships is a real-world pattern. The cyclic order encoding is natural for this domain.

### What Experiment 2B does NOT prove

1. **Leverage is low.** Containment's fan-out is 6× (one test derives membership, scope, effect ownership, capability context, dependency ownership, visibility). Cyclic order's leverage is ~3× (next, prev, adjacent — all simple array lookups). The cyclic order is less "productive" than containment.

2. **No downstream consumer yet.** The architecture verifier, scope resolver, and effect collector don't use cyclic order. It produces a query-able data structure, but no existing system reads from it. The consumed fan-out is effectively zero. Until a real compiler pass uses `next_clockwise`, the cyclic order is a capability without a consumer.

3. **The sensor ring is domain-specific.** Not all programs have physically-arranged components in a ring. The use case is natural for robotics/sensing but may not generalize to typical software. Whether Arcana targets this domain is a language design question, not an experiment result.

4. **Manual vs. geometric ρ produce the same queries.** `arc_cyclic_set()` and `arc_geo_derive_cyclic_order()` both populate the same `ArcCyclicOrder` struct. A system that accepts manually-set ρ gets the same queries as one that derives from geometry. The geometry adds source-of-truth discipline and refusal semantics, but the query semantics are identical.

5. **The forgetful fingerprint is a hash, not a proof.** The FNV-1a fingerprint catches *most* topology differences but is not collision-free. Two truly different graphs could theoretically produce the same hash. For the current test suite (small graphs, known topology), this is a negligible risk. For larger programs, a proper graph isomorphism check would be needed.

## What Changes in the Falsification Balance

After Experiment 2A:

> Could explicit `in R` declarations replicate this? Yes for membership. No for boundary ambiguity and deformation invariance.

After Experiment 2B:

> Could a text-based ordering replicate this? **Yes for the ordering itself (any metadata can carry a sequence). No for the source discipline (atan2 derivation with angular ambiguity rejection), the orientation sensitivity (reflection reversal), or the ring semantics (no distinguished first element).**

The delta is cleaner than 2A's. Containment's G3 case was "partial" — the core membership derivation was replicable by text. Cyclic order's G3 case is **unambiguous**: the forgetful fingerprint proves the abstract graphs are identical, and different embeddings produce different semantics. There is no text-based mechanism that can distinguish the two programs without introducing new metadata — and if you introduce that metadata, you're reinventing the geometry.

However, the practical value depends entirely on whether downstream systems consume cyclic order. Formal G3 is necessary but low-weight (Reporting Lock §8). The question "should Arcana include cyclic order as a language feature?" remains open until a real compiler pass uses it.

## Recommended Next Steps

### Immediate: Connect cyclic order to a consumer

Build a scope resolver or code generator that uses `next_clockwise` to determine evaluation order, routing, or resource allocation. Without a consumer, the cyclic order is a G3-valid property with zero consumed leverage.

### Experiment 2C: Edge embedding and planarity

The strongest remaining G3 candidate. Edges embedded as curves in 2D; planarity (no crossings) as a coupling bound. Text cannot express planarity because text has no 2D embedding. This is the property most likely to produce high G2 leverage (many coupling relationships bounded by one geometric constraint).

### Evaluate: Is cyclic order a language feature or an implementation detail?

The adversarial hardening document asks whether cyclic order is **authoritative** or **decorative**. If real robotics systems already encode sensor arrangement in URDF or calibration files, Arcana's geometric encoding duplicates existing information. The value proposition must be that the geometry is the **single source of truth** — not a mirror of something declared elsewhere.

## Code Pointers

- Cyclic order API: `src/geometry/cyclic_order.h` — 78 lines
- Derivation + fingerprint: `src/geometry/cyclic_order.c` — 223 lines
- Test suite: `tests/test_cyclic_order.c` — 475 lines, 13 tests
- Containment (Experiment 2A): `src/geometry/containment.h/.c`
- Design locks: `docs/design/experiment-2b-research-lock.md`, `experiment-2b-test-harness-lock.md`, `experiment-2b-adversarial-hardening.md`, `experiment-2b-reporting-lock.md`

## Implementation Details

- `src/geometry/cyclic_order.h` — 78 lines. Types (`ArcCyclicOrder`, `ArcCyclicResult`), query API, derivation API, fingerprint API.
- `src/geometry/cyclic_order.c` — 223 lines. atan2 derivation with angular ambiguity/center rejection, FNV-1a forgetful fingerprint.
- `tests/test_cyclic_order.c` — 475 lines. 13 tests (3 scaffolding + 3 fingerprint + 7 geometry-derived).

Total: ~776 new LOC, 13 new tests, bringing the project to 236 tests.

## Pre-Registered Verdict (from Reporting Lock §11)

| Criterion | Result |
|-----------|--------|
| G3 formal pass | **PASS** — `test_cyclic_geo_g3`: U(A)=U(B) ∧ ρ(A)≠ρ(B) ∧ semantics(A)≠semantics(B) |
| G4 stability | **PASS** — `test_cyclic_geo_g4_control`: same angles, different distances → same ρ |
| Angular ambiguity rejection | **PASS** — `test_cyclic_geo_angular_ambiguity` |
| Orientation sensitivity | **PASS** — `test_cyclic_geo_reflection` |
| Naturalness (sensor ring) | **QUALIFIED PASS** — fixture is plausible for robotics/sensing; generality to other domains unproven |
| Consumed leverage | **NOT YET MEASURED** — no downstream consumer exists |
| Feature recommendation | **HOLD** — G3 proven, but practical value depends on consumed leverage |
