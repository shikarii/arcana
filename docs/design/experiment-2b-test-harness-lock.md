# Arcana Experiment 2B: Test-Harness Lock and Naturalness Criteria
## Anonymous edges, exact forgetful-map equivalence, and how to avoid forcing a fake cyclic domain

**Status:** Implementation/test-design lock  
**Date:** 2026-08-18  
**Purpose:** Freeze the exact validity conditions for Experiment 2B before implementation, especially the named-port trap, the \(U(A)=U(B)\) test harness, and the realistic-fixture naturalness test.

---

# 1. The named-port constraint is make-or-break

The existing semantic graph contains nodes with ports such as:

```text
lhs
rhs
arg0
arg1
value
```

Those names already encode role and/or order.

Therefore:

> **Experiment 2B must not use any existing node kind whose semantics depend on named positional ports.**

If it does, cyclic order becomes a redundant second encoding and the G3 result is invalid.

The preferred implementation remains:

\[
\boxed{
\text{new node/relation kind with genuinely anonymous incident edges}
}
\]

Do not retrofit the first experiment onto ordinary calls, arithmetic nodes, or anything else whose ports already carry positional meaning.

---

# 2. Option B is now the locked implementation strategy

Do not begin by adding a general "anonymous operand mode" to the entire graph IR.

That would create unnecessary compiler surface area.

Instead:

1. add one new cyclic relation/node kind,
2. give it anonymous incident edges,
3. store a local rotation system,
4. define one minimal semantic consequence of that rotation system,
5. test G3 in isolation.

Only generalize anonymous-port infrastructure if the experiment succeeds and another feature needs it.

---

# 3. The forgetful map must erase only embedding information

A correction is needed to the phrase:

> "compare the underlying undirected multigraph."

That forgets too much.

Arcana's ordinary semantic graph already contains meaningful non-geometric information such as:

- edge direction,
- node kind,
- node labels/constants,
- region membership,
- possibly edge type.

The G3 test should preserve all of that.

Define:

\[
U :
EmbeddedGraph
\rightarrow
AbstractSemanticGraph
\]

where \(U\) removes only:

```text
rotation system
2D coordinates
curve geometry
crossing information
winding
other embedding-specific facts
```

while preserving:

```text
node identities/kinds
edge identities
edge direction
ordinary connectivity
non-geometric labels
region membership
non-geometric types
```

Then require:

\[
\boxed{
U(A)=U(B)
}
\]

or graph isomorphism under exactly those retained attributes.

---

# 4. Prefer identity over isomorphism for the first pair test

For the first controlled fixture, there is no need to make graph-isomorphism complexity part of the experiment.

Use the exact same:

```text
node IDs
edge IDs
node kinds
edge endpoints
edge directions
regions
ordinary attributes
```

in fixtures A and B.

Change only:

\[
\rho
\]

the local rotation system.

Then the test can assert:

```text
strip_embedding(A) == strip_embedding(B)
```

byte-for-byte or structurally.

This is stronger and simpler than merely checking graph isomorphism.

Graph isomorphism becomes useful later when testing equivalent source rewrites.

---

# 5. Add a canonical embedding-stripped fingerprint

The test harness should compute something conceptually like:

```text
semantic_graph_fingerprint(graph, IGNORE_EMBEDDING)
```

The fingerprint should include:

```text
node IDs / canonical node identities
node kinds
node semantic payloads
directed edges
edge semantic payloads
region membership
```

and exclude:

```text
positions
angles
cyclic order
curve control points
winding
crossing data
```

Experiment 2B should assert:

```text
fingerprint(A) == fingerprint(B)
```

before comparing semantics.

If this assertion fails, the G3 test is invalid.

---

# 6. The pair test should fail loudly if hidden topology changes

A common accidental cheat would be:

Fixture A:

```text
A -> B
B -> C
C -> D
D -> A
```

Fixture B:

```text
A -> C
C -> B
B -> D
D -> A
```

These may look like "different cyclic arrangements," but they are different abstract graphs.

That is not a G3 result.

The test harness must reject the experiment setup before evaluating semantic output.

---

# 7. The hardest design question is not implementation — it is finding a truly cyclic relation

The candidate phrases:

```text
sensor ring
worker ring
clockwise successor
protocol phase
```

sound plausible.

But many real systems that look cyclic still contain a distinguished element.

Examples:

```text
ring buffer -> head/tail
pipeline feedback loop -> source stage
token ring -> token owner at a given moment
round-robin scheduler -> current cursor
state cycle -> initial state
```

These are cyclic structures with runtime state that chooses an origin.

That does not invalidate cyclic order.

But it means the source relation may be:

```text
cyclic neighborhood
```

while execution separately has:

```text
current element
```

The experiment must not confuse those.

---

# 8. A genuinely cyclic source relation can coexist with a runtime cursor

For example, a ring buffer has:

\[
\rho=(A,B,C,D)
\]

which defines structural adjacency:

\[
next(A)=B,\quad
next(B)=C,\quad
next(C)=D,\quad
next(D)=A
\]

Separately, runtime state may contain:

\[
head=C
\]

The existence of `head` does not make the structural relation linear.

It merely selects a current point in a cyclic structure.

This is a potentially useful distinction for Arcana:

```text
geometry defines cyclic topology
runtime state defines current position
```

That is more honest than pretending a real program can never have a distinguished element.

---

# 9. The naturalness test should look for structural cyclicity, not "no start anywhere"

The bar should not be:

> absolutely no distinguished element exists at runtime.

That is unnecessarily strict.

The better question is:

> **Is the persistent structural relationship cyclic even if runtime execution temporarily singles out one element?**

Examples that may qualify:

- ring-buffer neighborhood,
- token-ring topology,
- circular work-stealing peers,
- rotary sensor array,
- phased actuator/sensor array,
- cyclic protocol peers,
- round-robin membership order.

The structural source can remain cyclic even when runtime state has a cursor, leader, token, or phase.

---

# 10. This gives a better candidate than a contrived sensor-processing pipeline

The existing 18-node Perception pipeline is directional:

```text
raw
    ↓
filtered
    ↓
fused
    ↓
classified
```

Trying to turn these processing stages into a cycle would be artificial.

Do not do that.

Instead, add a **cyclic substructure inside one stage**.

For example:

```text
four equivalent directional sensors
arranged physically around a platform
```

The cyclic relation describes:

```text
clockwise neighbor
counterclockwise neighbor
```

while each sensor still feeds the ordinary Perception pipeline.

Then the overall program remains directional, but one local substructure is genuinely cyclic.

This is a much more natural integration.

---

# 11. A robotics fixture can supply a truly cyclic relation without forcing the pipeline

Example:

```text
             sensor0
          /           \
     sensor3           sensor1
          \           /
             sensor2
```

All four feed Perception.

The abstract dataflow can remain:

```text
sensor0 -> fusion
sensor1 -> fusion
sensor2 -> fusion
sensor3 -> fusion
```

The rotation system additionally provides:

```text
next_clockwise(sensor0) = sensor1
next_clockwise(sensor1) = sensor2
next_clockwise(sensor2) = sensor3
next_clockwise(sensor3) = sensor0
```

Now fixtures A and B can retain exactly the same dataflow edges but differ in the geometric neighbor relation.

That is a legitimate G3 candidate.

---

# 12. The semantic use must need neighbor relations

Simply computing `next_clockwise()` and never using it is not enough.

The realistic fixture should contain a behavior that genuinely depends on local cyclic adjacency.

Possible examples:

## Neighbor consistency check

Each sensor compares its reading against the two adjacent sensors.

## Circular smoothing

Each sensor's derived value is:

\[
f(s_{prev},s_i,s_{next})
\]

## Failure substitution

If sensor \(i\) fails, use its clockwise neighbor.

## Sector stitching

Adjacent sensor fields overlap, so processing occurs between clockwise-neighbor pairs.

These are much more natural than inventing a cycle among sequential processing stages.

---

# 13. This creates a stronger G3 fixture

Keep the ordinary dataflow graph identical:

```text
S0 -> Fusion
S1 -> Fusion
S2 -> Fusion
S3 -> Fusion
```

and identical node kinds.

Then add a cyclic relation used by a separate operation:

```text
neighbor_check(S_i, next_clockwise(S_i))
```

The rotation system determines which pair is selected.

Fixture A:

\[
\rho_A=(S0,S1,S2,S3)
\]

Fixture B:

\[
\rho_B=(S0,S2,S1,S3)
\]

The embedding-stripped graph remains identical.

The derived neighbor semantics differ.

That is a stronger and more natural test than positional arguments.

---

# 14. The first G3 test should still be minimal

Before the 18-node integration, keep a tiny pair.

Example:

```text
nodes:
    A
    B
    C
    D
    ring R

anonymous incidences:
    A -- R
    B -- R
    C -- R
    D -- R
```

Fixture A:

\[
\rho_R=(A,B,C,D)
\]

Fixture B:

\[
\rho_R=(A,C,B,D)
\]

Semantic query:

```text
next_clockwise(A)
```

Results:

```text
A -> B   in fixture A
A -> C   in fixture B
```

Before accepting the result:

```text
assert strip_embedding(A) == strip_embedding(B)
```

This is the controlled proof.

---

# 15. But "query next_clockwise" alone is still a weak product result

It proves G3 mechanically.

It does not establish that a programmer benefits.

Therefore the realistic fixture should consume the relation in actual computation or verification.

For example:

```text
adjacent_consistency(sensor, next_clockwise(sensor))
```

or:

```text
fallback(sensor) = next_clockwise(sensor)
```

The semantic result must affect:

- computed output,
- verifier behavior,
- or another observable program property.

---

# 16. Naturalness criteria should be explicit

After integration, evaluate:

## N1 — Visual obviousness

Can a programmer infer the cyclic relation by looking at the source?

## N2 — Metadata elimination

Did geometry remove explicit declarations such as:

```text
next(S0)=S1
next(S1)=S2
...
```

## N3 — Editing stability

Can sensors move while preserving cyclic order without changing semantics?

## N4 — Locality

Are neighbor relations easier to understand geometrically than through distant declarations?

## N5 — Runtime usefulness

Does the relation participate in a real computation or check?

## N6 — Low surprise

Would a programmer reasonably predict the semantics from the geometry?

## N7 — No hidden anchor

Does the relation remain truly cyclic at the structural level?

If several of these fail, kill the feature even if G3 passes.

---

# 17. Add a "feature-kill" rule

The project should explicitly permit:

```text
G3 formal result = PASS
Naturalness = FAIL
```

leading to:

```text
cyclic order not promoted to core Arcana semantics
```

This is important enough to encode in the tracker.

A theorem is evidence about representation.

It is not automatically evidence about product design.

---

# 18. G3 success must be accompanied by G4 testing

Once cyclic order is semantic, test deformation invariance.

Allowed changes:

```text
rotate whole ring
scale ring
move nodes slightly
bend incident edges
make spacing nonuniform
```

while preserving the same cyclic order.

Require:

\[
Sem(A)=Sem(A')
\]

Then swap two neighboring elements.

Require:

\[
Sem(A)\ne Sem(B)
\]

This gives:

```text
G3:
semantic distinction from rotation system

G4:
stability under geometry that preserves rotation system
```

The pair is stronger than either alone.

---

# 19. Orientation must be explicit

For an oriented cyclic relation:

\[
\rho=(A,B,C,D)
\]

reflection produces:

\[
(A,D,C,B)
\]

The language must decide whether reflection changes semantics.

For `next_clockwise`, it obviously should.

Therefore the minimal cyclic construct should likely be orientation-sensitive.

But absolute angle should still not matter.

Desired:

```text
rotation of entire diagram -> same semantics
reflection -> reversed successor semantics
```

This is clean and testable.

---

# 20. No anchor is needed for the first cyclic relation

This is another advantage of `next_clockwise`.

Linear argument ordering requires:

```text
cyclic order
+
orientation
+
anchor
```

But neighbor/successor semantics require only:

```text
cyclic order
+
orientation
```

No privileged first element is needed.

This removes one major source of accidental linearization.

---

# 21. The test harness should assert all preconditions programmatically

Before comparing semantics:

```text
assert same_node_set(A, B)
assert same_node_kinds(A, B)
assert same_semantic_payloads(A, B)
assert same_directed_edge_set(A, B)
assert same_regions(A, B)
assert different_rotation_system(A, B)
assert equal_embedding_stripped_fingerprint(A, B)
```

Only then:

```text
assert Sem(A) != Sem(B)
```

This prevents a human-authored fixture mistake from masquerading as G3 evidence.

---

# 22. Add a negative control

Also include two drawings with visibly different coordinates but the **same** cyclic order.

Require:

```text
fingerprint_without_embedding differs only in ignored geometry
rotation system equal
semantics equal
```

This demonstrates that:

- arbitrary layout differences do not matter,
- the specific discrete rotation-system difference does.

It is a strong G4 control.

---

# 23. Boundary-port work remains frozen

The future idea remains recorded:

```text
boundary port
    ->
authorization
interface membership
directionality
capability type
channel identity
visibility
```

But no design work should begin.

Why?

Cyclic order may affect:

- ordering of multiple ports,
- orientation of interfaces,
- radial vs tangential relationships,
- how boundary annotations are interpreted.

Designing ports now would prematurely freeze assumptions.

Status:

```text
PRESERVED
DEFERRED
NO IMPLEMENTATION
NO API DESIGN
```

until Experiment 2B concludes.

---

# 24. Winding remains deferred

No change.

Do not invent winding semantics to maintain geometric momentum.

Use the persistent fixture.

If a real future behavior needs:

```text
path class
feedback enclosure
re-entry
```

then evaluate winding.

Otherwise leave it nonsemantic.

---

# 25. Planarity remains deferred

No change.

Planarity is still a noisier experiment than cyclic order because it introduces:

```text
edge embedding
crossing detection
routing
global graph constraints
```

Do not touch it until the simpler G3 question is resolved.

---

# 26. Updated Experiment 2B tracker

| Hypothesis | Test | Success | Failure consequence |
|---|---|---|---|
| Rotation system adds semantic information | Minimal anonymous-edge pair | Same stripped graph, different semantics | G3 fails for cyclic order |
| No hidden port ordering exists | Harness audits node/edge attributes | No positional metadata survives | Test invalid if violated |
| Geometry is deformation-stable | Same \(\rho\), different coordinates | Same semantics | G4 failure if unstable |
| Orientation matters coherently | Mirror ring | Successor reverses predictably | Semantics underspecified |
| Cyclic relation is useful | Sensor-neighbor computation | Relation affects real behavior | Feature remains toy-only |
| Geometry eliminates metadata | Compare to explicit `next=` declarations | Fewer independent declarations | Product advantage weak if not |
| Realistic fixture remains readable | 18-node pipeline extension | Cyclic substructure obvious | Kill feature if forced |
| Runtime cursor can coexist with structural cycle | Optional later control | Cursor does not redefine \(\rho\) | Design collapses to list if entangled |

This table governs Experiment 2B.

---

# 27. Immediate implementation order

## Step 1

Finish empirical fan-out instrumentation from Experiment 2A.

## Step 2

Add an embedding-stripped graph fingerprint / equality helper.

## Step 3

Add one new cyclic relation/node kind with anonymous incident edges.

## Step 4

Store an oriented cyclic order:

\[
\rho
\]

without any positional port labels.

## Step 5

Build fixture pair A/B with identical abstract semantic graphs.

## Step 6

Assert:

\[
U(A)=U(B)
\]

programmatically.

## Step 7

Assert differing successor semantics.

## Step 8

Add G4 deformation controls.

## Step 9

Extend the persistent 18-node fixture with a natural cyclic sensor/worker substructure.

## Step 10

Make the relation affect real computation or verification.

## Step 11

Evaluate naturalness.

## Step 12

Promote or kill cyclic order based on both formal and product results.

---

# 28. The key implementation insight

The first cyclic-order experiment does **not** need to answer:

> Is all programming fundamentally cyclic?

It needs to answer:

> **Can one real structural relationship that is naturally cyclic be represented more directly by geometric rotation than by conventional metadata?**

That is a much smaller and more defensible claim.

A programming language can contain:

```text
linear structures
trees
DAGs
cycles
regions
```

simultaneously.

Cyclic order only needs to earn its place where cyclic structure genuinely exists.

---

# 29. The strongest realistic candidate

At the moment, a circular physical sensor arrangement appears stronger than an artificial worker-processing cycle.

Why?

Because the problem already has spatial adjacency.

The geometry can represent:

```text
which sensor is physically next clockwise
```

without inventing a source ordering unrelated to the domain.

That is exactly the kind of domain-language alignment Arcana is searching for.

It should be the default realistic candidate unless implementation reveals a better one.

---

# 30. Final lock

The next phase should not debate whether cyclic order is theoretically interesting.

It is.

The experiment must determine whether it is **honestly semantic and naturally useful**.

The validity conditions are now:

\[
\boxed{
\text{No named-port ordering}
}
\]

\[
\boxed{
U(A)=U(B)\text{ asserted programmatically}
}
\]

\[
\boxed{
\rho_A\ne\rho_B
}
\]

\[
\boxed{
Sem(A)\ne Sem(B)
}
\]

\[
\boxed{
\text{Same }\rho\text{ under harmless deformation } \Rightarrow \text{ same semantics}
}
\]

and finally:

\[
\boxed{
\text{the realistic fixture must make the relation feel natural}
}
\]

If the formal test passes and the realistic fixture feels forced, kill the feature.

If both pass, Arcana will have its first clean G3 result.

That is the standard.
