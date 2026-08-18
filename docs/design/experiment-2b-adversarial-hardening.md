# Arcana Experiment 2B: Pre-Implementation Adversarial Hardening
## How Experiment 2B can still cheat, why G3 is almost tautological once semantics depend on ρ, and what the real experiment must prove

**Status:** Adversarial test-protocol review  
**Date:** 2026-08-18  
**Purpose:** Improve the locked Experiment 2B protocol before implementation. This document does not reopen the research order. It attacks remaining ways the cyclic-order experiment could produce a technically correct but scientifically weak result.

---

# 1. Bottom line

The Experiment 2B protocol is now strong enough to prevent the most obvious cheats:

- named ports are forbidden,
- the forgetful map is explicit,
- the embedding-stripped graph must match programmatically,
- harmless deformation is a negative control,
- realistic naturalness is required in addition to formal G3 success.

However, several deeper risks remain.

The most important are:

1. **G3 can become tautological once semantics are defined directly from an explicitly stored \(\rho\).**
2. **A manually supplied cyclic-order field does not yet prove that geometry produced the semantics.**
3. **The proposed sensor-ring use case may not actually be a graph-theoretic rotation system at a vertex.**
4. **The equivalence classes of rotation, reflection, and genuine reordering need to be mechanically distinguished.**
5. **Cyclic-order changes need their own ambiguity zone, analogous to containment's boundary epsilon.**
6. **Physical sensor geometry may be configuration data rather than program architecture; the realistic fixture must test that distinction rather than assume it away.**

These do not invalidate Experiment 2B.

They make the test sharper.

---

# 2. The biggest remaining trap: G3 can be true by definition

Suppose Arcana introduces:

\[
\rho_R=(A,B,C,D)
\]

and defines:

\[
next_\rho(A)=B
\]

Then fixture B changes:

\[
\rho_R=(A,C,B,D)
\]

and now:

\[
next_\rho(A)=C
\]

If the forgetful map \(U\) explicitly removes \(\rho\), then of course:

\[
U(A)=U(B)
\]

while:

\[
Sem(A)\ne Sem(B)
\]

That theorem is almost guaranteed **by construction**.

This is not worthless.

It validates the implementation and confirms that the embedded representation is strictly richer than the ordinary graph.

But it is not yet strong evidence that Arcana has discovered a useful programming primitive.

---

# 3. Separate the mechanical G3 theorem from the scientific result

Experiment 2B should explicitly report two outcomes.

## 2B-F — Formal embedding result

Question:

> Can a cyclic embedding carry semantic information that disappears when the embedding is forgotten?

Success:

\[
U(A)=U(B)
\]

\[
\rho_A\ne\rho_B
\]

\[
Sem(A)\ne Sem(B)
\]

This is a representation theorem.

It is expected to pass if the implementation is correct.

---

## 2B-N — Naturalness / language-value result

Question:

> Does a realistic program naturally need the cyclic relation, and is geometry a better authoritative representation of it than explicit successor metadata?

This is the difficult experiment.

This result must not be assumed merely because 2B-F passes.

---

# 4. The real falsification burden has shifted

The dangerous outcome is not:

```text
G3 fails
```

The more likely dangerous outcome is:

```text
G3 passes trivially
but the feature is pointless
```

Therefore the strongest falsification criteria now belong to 2B-N:

- no real computation needs the relation,
- explicit `next=` declarations are clearer,
- geometry duplicates physical configuration,
- the ordering changes too easily during editing,
- the cyclic structure secretly needs a permanent root,
- the feature only survives in toy examples.

A formal G3 pass should be treated as **necessary but low-weight evidence**.

---

# 5. Another major trap: manually storing ρ is not yet geometric derivation

Suppose the `.graph` fixture contains:

```text
cyclic_order ring S0 S1 S2 S3
```

and the compiler stores:

\[
\rho=(S0,S1,S2,S3)
\]

Then Experiment 2B proves:

> explicit cyclic-order metadata can affect semantics.

It does **not** prove:

> the source geometry intrinsically determines cyclic order.

That would merely repeat the Experiment 1 problem at a different layer.

---

# 6. Experiment 2B therefore needs a geometry-extraction phase

The strongest pipeline is:

```text
2D positions / ring geometry
        ↓
derive cyclic order ρ
        ↓
Geometric Semantic IR
        ↓
derive successor / neighbor semantics
        ↓
ordinary graph / runtime
```

The programmer should not author \(\rho\) directly in the final geometric test.

The fixture may expose \(\rho\) for debugging or expected-value assertions, but the authoritative source must be geometric position.

---

# 7. Split Experiment 2B implementation into two subtests

## 2B.0 — IR mechanism test

It is acceptable to manually set:

\[
\rho
\]

to validate the semantic machinery.

This is an implementation unit test.

It proves:

```text
rotation-system semantics work
```

but not:

```text
geometry derives them
```

---

## 2B.1 — Geometric extraction test

Use actual positions.

For a ring center \(c\) and members \(x_i\), derive:

\[
\theta_i=atan2(y_i-c_y,x_i-c_x)
\]

and sort by angle in the chosen orientation.

The resulting oriented cyclic order is:

\[
\rho_R
\]

No explicit user-authored ordering field is allowed.

This is the actual Arcana experiment.

---

# 8. A conceptual correction: sensor-ring order may not be a vertex rotation system

Graph-embedding theory uses **rotation system** for the cyclic order of incident half-edges around each graph vertex.

That is a precise concept.

But the realistic Arcana proposal is:

> four separate sensor nodes arranged around a circular region/platform.

That is not necessarily the same mathematical object.

There may be no central graph vertex whose incident half-edges define the sensor order.

Instead, Arcana may have:

\[
CyclicMembers(R,\{S_0,S_1,S_2,S_3\},\rho_R)
\]

where the cyclic order belongs to a **region/ring**, not a computational node.

These ideas are related but should not be conflated.

---

# 9. Do not let graph-theory terminology force the wrong source primitive

There are two candidate primitives.

## Primitive V — Vertex rotation

For node \(v\):

\[
\rho_v
\]

orders incident half-edges around \(v\).

This is the standard graph-embedding rotation-system concept.

Potential Arcana uses:

- anonymous ports,
- local fan-in/fan-out arrangement,
- cyclic interface ordering.

---

## Primitive R — Region/ring member order

For circular region/ring \(R\):

\[
\rho_R
\]

orders a set of member nodes by angular position around \(R\).

Potential Arcana uses:

- sensor arrays,
- worker rings,
- phase positions,
- radial interfaces,
- repeated circle motifs.

The sensor fixture appears more naturally to test **Primitive R**.

That may be more faithful to the magic-circle source model.

---

# 10. Recommendation: use a cyclically ordered ring relation for the first naturalness fixture

The clean semantic object could be:

\[
Ring(R,\{S_0,S_1,S_2,S_3\},\rho_R)
\]

The ordinary abstract representation retains:

```text
ring identity R
member set {S0,S1,S2,S3}
region/context
ordinary dataflow edges
```

The forgetful map removes only:

\[
\rho_R
\]

Then:

\[
U(A)=U(B)
\]

can hold while successor semantics differ.

This avoids inventing a fake central computation node merely to borrow vertex-rotation terminology.

---

# 11. The forgetful map must be stage-specific

There is another subtle failure mode.

Suppose cyclic order is lowered into explicit edges:

```text
next(S0) -> S1
next(S1) -> S2
...
```

After that lowering, fixtures A and B have different ordinary graphs.

Then:

\[
U(A)\ne U(B)
\]

at the lower IR.

This does not invalidate G3.

It means \(U\) was applied at the wrong stage.

---

# 12. Freeze the stage where U is evaluated

Experiment 2B should define:

\[
U :
GeometricSemanticIR
\rightarrow
BaseSemanticGraph
\]

**before** cyclic-order semantics are expanded into lower-level successor edges or runtime operations.

Pipeline:

```text
GEOMETRIC SOURCE
      ↓
GEOMETRIC SEMANTIC IR
  members + ρ
      │
      ├── U forgets ρ
      │
      ▼
BASE SEMANTIC GRAPH
      │
      ▼
cyclic semantic lowering
      ↓
COMPUTATIONAL GRAPH
```

The equality:

\[
U(A)=U(B)
\]

must be asserted at this precise boundary.

Do not compare the post-lowering computational graphs.

They are expected to differ if the cyclic relation matters.

---

# 13. "Different rotation system" needs tighter terminology

Three different transformations must not be conflated.

Let:

\[
\rho_A=(S0,S1,S2,S3)
\]

## Same oriented cyclic order under rotation

\[
(S1,S2,S3,S0)
\]

\[
(S2,S3,S0,S1)
\]

\[
(S3,S0,S1,S2)
\]

These are the **same cyclic order**.

They differ only in where the notation starts.

---

## Reflected / reversed cyclic order

\[
(S0,S3,S2,S1)
\]

This reverses orientation.

For orientation-sensitive semantics such as `next_clockwise`, this should be semantically distinct.

But it is a very specific distinction:

```text
same adjacency
opposite orientation
```

---

## Genuinely reordered cyclic structure

Example:

\[
\rho_B=(S0,S2,S1,S3)
\]

This is not a cyclic shift of \(\rho_A\).

It is also not merely the reversal of \(\rho_A\).

This changes adjacency structure.

That is the strongest B fixture.

---

# 14. Use four members, not three, for the strongest reorder test

With three members, permutations collapse too aggressively under rotation/reflection.

A four-member ring gives a clean fixture where:

\[
(S0,S2,S1,S3)
\]

is neither:

- a cyclic rotation of the baseline,
- nor its simple reflection.

This allows Experiment 2B to separately test:

```text
rotation equivalence
reflection reversal
genuine cyclic reorder
```

Use \(n=4\) as the minimum controlled fixture.

---

# 15. Canonicalize cyclic orders in the test harness

The harness should have explicit predicates:

```text
same_oriented_cycle(rhoA, rhoB)
is_reflection(rhoA, rhoB)
is_genuine_reorder(rhoA, rhoB)
```

For orientation-sensitive cyclic order:

\[
\rho\sim_C\rho'
\]

if \(\rho'\) is a cyclic shift of \(\rho\).

Reflection is **not** equivalent.

Then test cases become mechanically clear.

---

# 16. Required cyclic-order fixture matrix

## A — baseline

\[
(S0,S1,S2,S3)
\]

---

## A-rotated-notation — same semantic cycle

\[
(S2,S3,S0,S1)
\]

Expected:

```text
same cycle
same semantics
```

This may only exist as an IR/canonicalization test because geometric global rotation does not actually change the extracted member sequence modulo cyclic shift.

---

## A-deformed — different coordinates, same ρ

Expected:

```text
G4 PASS
same semantics
```

---

## A-reflected

\[
(S0,S3,S2,S1)
\]

Expected:

```text
orientation reversed
next_clockwise becomes prior-counterclockwise
semantics reverse coherently
```

---

## B-reordered

\[
(S0,S2,S1,S3)
\]

Expected:

```text
not rotation
not reflection
genuine different cyclic structure
different neighbor semantics
```

This matrix is stronger than a simple A/B pair.

---

# 17. Orientation and G4 must be mathematically compatible

If Arcana says:

> reflection reverses `next_clockwise`

then reflection cannot simultaneously be considered harmless aesthetic deformation.

Therefore G4 transformations for oriented cyclic semantics should be restricted to **orientation-preserving** deformations.

Allowed G4 transformations:

```text
translation
rotation
uniform/nonuniform scale that preserves orientation
smooth movement preserving cyclic order
curve deformation
```

Not automatically G4-equivalent:

```text
mirror reflection
```

Reflection changes chirality.

That is acceptable and should be explicit.

---

# 18. Add an angular ambiguity rule

Containment already discovered that geometric semantics need an ambiguity zone.

Cyclic order has the same problem.

Suppose two nodes approach the same angular position around the ring center.

At the instant their order swaps:

\[
\theta_i=\theta_j
\]

the cyclic order is ambiguous.

Without a guard band, tiny coordinate noise could silently change semantics.

That would violate the usability goal.

---

# 19. Define angular separation robustness

For adjacent members in angular order, require:

\[
\Delta\theta_{ij}>\epsilon_\theta
\]

for some chosen semantic guard threshold.

If:

\[
\Delta\theta_{ij}\le\epsilon_\theta
\]

then return:

```text
AMBIGUOUS_CYCLIC_ORDER
```

and reject or require snapping.

This is the cyclic-order analogue of Experiment 2A's boundary epsilon.

It may become an important G4 robustness mechanism.

---

# 20. Also reject undefined angular position

If cyclic order is derived around center \(c\), then a member too close to \(c\) has unstable/undefined angle.

Require:

\[
\|x_i-c\|>\epsilon_r
\]

or define an explicit ring/annulus membership condition.

The first experiment can use a simple minimum-radius rule.

Do not let `atan2(0,0)` or near-center noise determine semantics.

---

# 21. The geometric source should derive ρ, then quantize it

This follows the existing Semantic Quantization Principle.

Continuous source:

```text
(x_i, y_i)
```

is used to derive:

\[
\rho_R
\]

Then the semantic system works with discrete:

```text
cyclic successor
cyclic predecessor
adjacency
```

not exact angles.

Exact degrees should normally disappear after extraction.

Pipeline:

```text
coordinates
    ↓
robust cyclic-order extraction
    ↓
ρ
    ↓
semantic lowering
```

---

# 22. The sensor-ring naturalness test has a new adversarial problem: configuration duplication

A physical sensor ring sounds extremely natural.

But ask:

> Is the source geometry actually the authoritative description of physical sensor placement?

Real robotics systems may already have:

```text
calibration
URDF-like transforms
hardware configuration
sensor extrinsics
```

If Arcana separately draws the sensor ordering, the geometry may duplicate configuration rather than eliminate metadata.

That weakens the product result.

---

# 23. The realistic fixture must choose what the geometry means

There are at least three possibilities.

## A — Source geometry models physical arrangement

Then:

```text
clockwise in Arcana
=
clockwise on the robot
```

Strong naturalness if Arcana becomes authoritative for that structural relation.

Risk:

```text
duplication with hardware calibration
```

---

## B — Source geometry models logical communication topology

Then:

```text
clockwise
=
next logical peer
```

independent of physical sensor angle.

This avoids physical-calibration duplication.

Risk:

```text
why should logical successor be represented spatially?
```

---

## C — Source geometry derives from imported hardware metadata

Then Arcana visualizes an existing physical arrangement.

Risk:

```text
geometry is no longer the authored source of truth
```

but it may still be useful as verified architecture.

Experiment 2B should be explicit which claim it is testing.

---

# 24. A better naturalness comparison may include two candidate domains

Do not build two large fixtures.

But after the minimal G3 mechanism succeeds, compare the cyclic primitive conceptually or with tiny integrations in:

1. **physical sensor adjacency**, and
2. **logical peer-ring topology**.

Whichever has less duplicated metadata and more natural semantic use should become the realistic integration.

This is a small domain-selection step, not a reopening of the experiment.

---

# 25. A physical sensor neighbor check is plausible, but do not assume it is the winner

Possible real behavior:

\[
check(S_i,next(S_i))
\]

for adjacent field-of-view consistency.

That is credible.

But if the actual robot already stores sensor azimuths numerically, then:

```text
sort sensors by azimuth
```

may be a more authoritative source than code layout.

Arcana's geometry only wins if:

- it is authoritative,
- it eliminates duplicated ordering metadata,
- or it provides enough other semantic leverage to justify representing the same structure geometrically.

This should be measured honestly.

---

# 26. The formal test should distinguish derived relation from observable behavior

In the minimal fixture:

```text
next_clockwise(S0)
```

changing is enough to prove the relation changed.

But `Sem(A) != Sem(B)` is stronger if the difference is observable.

The realistic fixture should ensure the cyclic relation affects:

- computed output,
- verification outcome,
- generated lower graph,
- or another defined observable.

Otherwise the feature may remain unused metadata.

---

# 27. Define two semantic observations

For Experiment 2B:

## Structural observation

\[
Obs_{struct}(P)=\rho_P
\]

or derived successor map.

This is enough for the formal mechanism test.

## Program observation

\[
Obs_{prog}(P)
\]

is some actual computation/verifier behavior that consumes the successor relation.

The strongest result is:

\[
U(A)=U(B)
\]

while:

\[
Obs_{prog}(A)\ne Obs_{prog}(B)
\]

in the realistic fixture.

---

# 28. The embedding-stripped fingerprint should preserve multigraph structure correctly

If the new cyclic relation ever permits:

- parallel incidences,
- self-loops,
- repeated endpoints,

then comparing only neighbor sets is insufficient.

The fingerprint must preserve:

```text
edge identity or multiplicity
direction
endpoint pair
semantic edge payload
```

Graph-embedding rotation systems are formally orders of **incident half-edges**, not merely unique neighboring vertices.

For Experiment 2B, the easiest path is to restrict the test relation to:

```text
distinct member nodes
one membership/incidence each
no self-loops
no parallel incidences
```

Then generalize only if later needed.

---

# 29. Keep the fingerprint simpler than the theorem

The first pair should intentionally use identical IDs and structure.

Fingerprint comparison can therefore be deterministic and straightforward.

Do not implement a general graph-isomorphism solver.

Required:

```text
same IDs
same kinds
same payloads
same directed edges
same region membership
same cyclic member set
```

Ignored:

```text
coordinates
angles
ρ
curves
other embedding state
```

This is enough.

---

# 30. Pre-register the feature-kill criteria before seeing the result

Naturalness can otherwise become post-hoc rationalization.

Before realistic integration, lock these questions.

Cyclic order is **not promoted** if:

1. explicit successor metadata is clearer,
2. geometry duplicates another authoritative configuration source,
3. angular-order ambiguity occurs frequently during normal editing,
4. meaningful programs require a permanent source/root that dominates the cycle,
5. the cyclic relation rarely affects actual computation,
6. programmers need textual labels to understand the order anyway,
7. geometric rearrangement causes surprising semantic changes,
8. the feature only works in one contrived fixture.

Formal G3 success does not override these.

---

# 31. Add positive promotion criteria

Cyclic order is a strong candidate for promotion if:

1. \(\rho\) is derived from geometry with no explicit ordering metadata,
2. the embedding-stripped graph equality is mechanically proven,
3. cyclic semantics affect real program behavior,
4. harmless deformation preserves \(\rho\),
5. ambiguous reorderings are rejected before semantics flip,
6. geometry eliminates multiple explicit successor/neighbor declarations,
7. the relation is visually obvious,
8. the realistic domain genuinely contains persistent cyclic structure,
9. the source geometry is authoritative rather than duplicated documentation.

---

# 32. Revised Experiment 2B protocol

## Phase 0 — fan-out cleanup

Complete empirical Experiment 2A consumer instrumentation.

Do not expand scope.

---

## Phase 1 — fingerprint infrastructure

Implement:

```text
embedding_stripped_fingerprint()
```

with exact preserved/ignored attributes.

---

## Phase 2 — cyclic semantic IR unit test

Introduce the new anonymous cyclic relation.

Manually inject \(\rho\) only for low-level semantic machinery tests.

Do not count this as the geometry result.

---

## Phase 3 — geometric cyclic-order extraction

Derive \(\rho\) from 2D member positions around a ring/region.

Add:

```text
minimum radius
angular ambiguity epsilon
orientation
canonical cyclic-order representation
```

---

## Phase 4 — full equivalence matrix

Test:

```text
baseline
global rotation / deformation
reflected layout
genuine reordered layout
```

and assert the expected G3/G4 behavior.

---

## Phase 5 — minimal formal G3 result

Require:

\[
U(A)=U(B)
\]

programmatically.

Require:

\[
\rho_A\ne\rho_B
\]

for genuine reorder.

Require:

\[
Obs_{struct}(A)\ne Obs_{struct}(B)
\]

Report this as:

```text
2B-F PASS/FAIL
```

---

## Phase 6 — domain selection sanity check

Compare:

```text
physical sensor adjacency
logical peer-ring topology
```

against the naturalness and duplication criteria.

Pick one.

Do not add a third domain unless both fail.

---

## Phase 7 — realistic integration

Extend the existing 18-node fixture.

Require cyclic order to affect:

\[
Obs_{prog}
\]

not merely expose `next_clockwise()` as unused metadata.

---

## Phase 8 — product verdict

Report independently:

```text
G3 formal result: PASS/FAIL
G4 stability: PASS/FAIL
Naturalness: PASS/FAIL
Metadata elimination: PASS/FAIL
Feature promotion: YES/NO
```

Do not collapse these into one result.

---

# 33. Updated test matrix

| Test | Geometry change | Expected \(\rho\) relation | Expected semantics |
|---|---|---|---|
| Baseline A | None | baseline | baseline |
| A translated | translation | same oriented cycle | same |
| A globally rotated | rotation | same oriented cycle | same |
| A mildly deformed | coordinate perturbation | same oriented cycle | same |
| A near-order collision | two angular positions within \(\epsilon_\theta\) | ambiguous | reject |
| A reflected | mirror | reversed cycle | reversed clockwise semantics |
| B reordered | swap creating non-dihedral order | genuinely different cycle | different |
| Hidden-topology control | edge set changed | fingerprint differs | test invalid before semantics |

This should be implemented directly.

---

# 34. Updated living falsification tracker

| Hypothesis | Status before 2B | Required evidence |
|---|---|---|
| Embedding-stripped equality can be enforced mechanically | **OPEN** | fingerprint tests |
| Anonymous cyclic relation can avoid named-port ordering | **OPEN** | new node/relation kind |
| Geometry can derive \(\rho\) without explicit order metadata | **OPEN** | coordinate extraction |
| Same oriented cycle survives deformation | **OPEN** | G4 controls |
| Reflection reverses semantics coherently | **OPEN** | mirror test |
| Genuine reorder changes semantics | **OPEN** | B-reordered fixture |
| Formal G3 exists | **OPEN** | \(U(A)=U(B)\), different structural observation |
| Cyclic relation affects actual program behavior | **OPEN** | realistic fixture |
| Geometry eliminates metadata rather than duplicating config | **OPEN** | domain comparison |
| Cyclic order deserves core-language status | **OPEN** | formal + naturalness pass |
| Boundary ports should be designed now | **NO** | still frozen |
| Winding should be implemented now | **NO** | still deferred |
| Planarity should be implemented now | **NO** | still deferred |

---

# 35. One terminology rule should be locked

Use these terms precisely:

## Global rotation

Move the entire drawing by an orientation-preserving rotation.

Expected:

```text
same oriented cyclic order
same semantics
```

## Cyclic shift

Write the same cycle starting from another member.

Example:

\[
(A,B,C,D)
\sim
(C,D,A,B)
\]

Expected:

```text
same cyclic order
```

## Reflection / reversal

Reverse orientation.

Example:

\[
(A,B,C,D)
\rightarrow
(A,D,C,B)
\]

Expected for `clockwise` semantics:

```text
different oriented cycle
reversed semantics
```

## Genuine reorder

A permutation not equivalent by cyclic shift and not merely reversal.

Example:

\[
(A,B,C,D)
\rightarrow
(A,C,B,D)
\]

Expected:

```text
different adjacency structure
different semantics
```

Avoid using the phrase:

> "different rotation"

for all four cases.

It is too ambiguous.

---

# 36. The deepest adversarial conclusion

The fingerprint infrastructure is load-bearing.

But it is not the hardest intellectual part anymore.

Once semantics explicitly depend on \(\rho\), the formal G3 pair is close to a consistency check.

The hard question is:

\[
\boxed{
\text{Did the geometry naturally create a relation that a real program actually wanted?}
}
\]

That requires all of:

```text
geometry-derived ρ
no hidden ordering metadata
robust ambiguity handling
real observable use
metadata elimination
authoritative source
natural visual comprehension
```

That is the bar that prevents Experiment 2B from becoming a beautiful tautology.

---

# 37. Final recommendation

Keep the locked sequence, but harden Experiment 2B with four additions:

1. **derive \(\rho\) from actual geometry rather than authoring it as fixture metadata,**
2. **distinguish region/ring cyclic order from graph-vertex rotation systems,**
3. **add angular ambiguity rejection and orientation-preserving G4 rules,**
4. **treat formal G3 as the mechanism result and naturalness/authority as the real feature verdict.**

The most important new warning is:

> **Defining `next_clockwise` from \(\rho\) and then observing that different \(\rho\) changes `next_clockwise` is not, by itself, an interesting research result.**

The real result would be:

> **A programmer expresses an already-useful cyclic relation simply by arranging elements in the geometric source; the compiler derives that relation robustly, proves the underlying abstract program is otherwise unchanged, uses the relation in real behavior, and eliminates explicit ordering metadata.**

If Arcana can do that, cyclic order has earned its place.

If not, G3 can pass and the feature should still die.
