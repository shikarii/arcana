# Arcana: Experiment 2B Research Lock
## Final decisions before cyclic-order implementation

**Status:** Project state / implementation lock  
**Date:** 2026-08-18  
**Purpose:** Record the conclusions now considered settled after Experiment 2A, its corrections, and the final review of the Experiment 2B plan.

---

# 1. No substantive design disagreements remain

The current research direction is now sufficiently converged.

The important corrections from the previous documents are accepted:

- containment is G1 and meaningful G2,
- deformation invariance belongs in G4,
- exact 6× fan-out remains unproven until downstream consumers are instrumented,
- boundary crossing is not authorization,
- spatial layout currently aids comprehension but does not yet define semantics,
- cyclic order remains the cleanest next G3 experiment,
- planarity remains deferred.

No further broad review is needed before implementation.

---

# 2. Governing representational question

The project should stop using claims of the form:

> "text fundamentally cannot represent X."

Text can serialize essentially any finite structure if enough metadata is added.

The governing question is now:

\[
\boxed{
\text{Does the fact arise directly from the representation the programmer already uses?}
}
\]

This standard should be applied to every future Arcana feature.

Examples:

## Containment

Arcana:

```text
draw x inside R
```

and the containment fact exists intrinsically.

A textual language may encode the same fact:

```text
x in R
```

but the programmer must explicitly declare it.

## Boundary ambiguity

Arcana naturally has:

```text
near boundary
```

because the source has spatial extent.

A textual system can reproduce the check only by adding a secondary geometric model.

## Cyclic order

Arcana may obtain:

\[
\rho_v
\]

directly from the local embedding around a node.

A textual graph can encode the same rotation system, but it must add explicit ordering metadata.

The relevant distinction is therefore:

```text
intrinsic relation
```

versus:

```text
secondary declaration
```

not:

```text
possible
```

versus:

```text
impossible
```

---

# 3. Research order is locked

The project sequence is:

```text
1. fan-out instrumentation cleanup

2. Experiment 2B:
   cyclic order

3. immediate realistic-fixture integration

4. winding only if a real feedback use case demands it

5. planarity / crossing-number / richer edge embedding later
```

No reordering should occur without new experimental evidence.

This is now a research lock, not a recommendation.

---

# 4. Immediate action 1 — make containment fan-out empirical

The current containment implementation has:

```text
declared/potential fan-out = 6
```

Candidate consumers:

```text
membership
scope
effect ownership
capability context
dependency ownership
visibility
```

Before making further leverage claims, instrument actual downstream use.

For each geometrically derived membership:

```text
membership            CONSUMED / NOT CONSUMED
scope                 CONSUMED / NOT CONSUMED
effect ownership      CONSUMED / NOT CONSUMED
capability context    CONSUMED / NOT CONSUMED
dependency ownership  CONSUMED / NOT CONSUMED
visibility            CONSUMED / NOT CONSUMED
```

Then report:

\[
Fanout_{used}
\]

rather than the unconditional count.

This should remain a small cleanup task.

Do not let it become a new effect-system project.

---

# 5. Immediate action 2 — cyclic order is the Experiment 2B target

Experiment 2B should directly target G3:

\[
\boxed{
U(A)=U(B)
\quad\text{but}\quad
Sem(A)\ne Sem(B)
}
\]

where the only semantic difference is the local rotation system.

The ordinary abstract connectivity must remain identical.

---

# 6. The named-port trap remains the critical implementation hazard

The current semantic graph contains named ports such as:

```text
arg0
arg1
lhs
rhs
value
```

These already encode positional meaning.

Therefore a cyclic-order experiment using those ports would be invalid.

If:

```text
A.out -> F.arg0
B.out -> F.arg1
C.out -> F.arg2
```

then the ordering already exists independently of geometry.

Changing clockwise order would prove nothing.

---

# 7. Do not retrofit anonymous ordering into ordinary call nodes first

Two implementation options exist:

## Option A — parallel anonymous-edge mode

Extend existing nodes so they can operate either with:

```text
named ports
```

or:

```text
anonymous incident edges + rotation system
```

This preserves reuse but risks complexity and ambiguity.

## Option B — new node/relation kind with no intrinsic port ordering

Introduce a construct whose semantics are genuinely cyclic.

This is cleaner.

The preferred choice for Experiment 2B is:

\[
\boxed{
\text{Option B}
}
\]

Do not force the entire compiler to support anonymous operands merely to prove one theorem.

---

# 8. Use a genuinely cyclic relation

The best Experiment 2B construct should not secretly reconstruct:

```text
arg0
arg1
arg2
```

Instead, use a relation where no participant is intrinsically first.

Candidate:

```text
sensor ring
worker ring
clockwise successor
counterclockwise neighbor
protocol phase cycle
```

For node or region \(v\), define:

\[
\rho_v=(e_1,e_2,\dots,e_n)
\]

Then derive:

\[
next_{\rho}(e_i)=e_{i+1}
\]

with wraparound.

No linear anchor is required.

Only orientation matters.

This is a purer cyclic-order semantic.

---

# 9. Keep a minimal proof-of-mechanism fixture

A tiny controlled test should still exist.

Example:

```text
same nodes
same edges
same node kinds
different cyclic order
```

Fixture A:

\[
\rho=(A,B,C,D)
\]

Fixture B:

\[
\rho=(A,C,B,D)
\]

The ordinary graph remains the same.

The cyclic successor relation changes.

Expected:

\[
U(A)=U(B)
\]

but:

\[
Sem(A)\ne Sem(B)
\]

This is the formal G3 result.

---

# 10. Immediately ground the result in the persistent 18-node fixture

Do not leave cyclic order as a micro-theorem.

Extend the existing Experiment 2A pipeline.

Inside Perception, introduce something like:

```text
four symmetric sensor-processing workers
```

arranged in a ring.

Use cyclic order to define:

```text
clockwise successor
```

or another truly cyclic relation.

Then ask:

- is the relation visually obvious?
- did geometry eliminate metadata?
- is the program easier to inspect?
- does the rotation system remain stable under ordinary editing?
- would explicit `next=` declarations actually be clearer?

The realistic fixture is the naturalness test.

---

# 11. Mathematical validity is not enough

A cyclic-order feature can succeed formally and still fail as language design.

Possible result:

\[
U(A)=U(B)
\]

and:

\[
Sem(A)\ne Sem(B)
\]

but programmers prefer:

```text
next(worker0) = worker1
next(worker1) = worker2
```

because it is clearer.

Then:

```text
G3 theorem: PASS
core language feature: REJECT
```

That is an acceptable outcome.

The realistic fixture is allowed to kill a mathematically valid feature.

---

# 12. Boundary-port ideas are preserved but frozen

A future geometric boundary port may plausibly drive:

```text
authorization
interface membership
directionality
capability type
channel identity
visibility rule
```

This is potentially high G2 leverage.

However:

\[
\boxed{
\text{Do not design or implement boundary ports during Experiment 2B.}
}
\]

Reason:

- cyclic order may change how boundary interfaces should be arranged,
- port ordering may become cyclic,
- orientation may matter,
- later geometry may change the natural interface representation.

The idea should remain recorded, not acted on.

---

# 13. Crossing != authorization remains a permanent invariant

Future geometry may derive:

\[
CrossesBoundary(e,R)
\]

but a separate construct must establish:

\[
Authorized(e,R)
\]

Therefore:

\[
\boxed{
CrossesBoundary(e,R)
\not\Rightarrow
Authorized(e,R)
}
\]

Any future boundary-port design must preserve this.

A bare crossing is observation.

A port/sigil/interface object is permission.

---

# 14. Winding remains intentionally unscheduled

Do not implement winding because:

```text
it is the next interesting geometric property
```

Wait until the persistent fixture develops a real distinction involving:

- feedback,
- repeated re-entry,
- path class,
- enclosure,
- cyclic interaction.

Then ask:

> Would winding express something this program already needs?

If the answer is no, winding remains nonsemantic.

This is deliberate.

---

# 15. Planarity remains deferred

Planarity should not displace cyclic order.

It introduces too many concerns simultaneously:

```text
edge geometry
crossing detection
routing
embedding
layout
global constraints
```

Cyclic order isolates one embedding-sensitive relation much more cleanly.

Planarity remains interesting after G3 has been tested with a simpler construct.

---

# 16. Updated G-framework

## G1 — Intrinsic encoding

Geometry directly determines a semantic fact.

Containment:

```text
PASS
```

---

## G2 — Semantic leverage

One geometric fact is reused across multiple meaningful compiler semantics.

Containment:

```text
PASS qualitatively
exact consumed fan-out pending
```

---

## G3 — Embedding-exclusive distinction

Same abstract graph, different semantics because embedding differs.

Current status:

```text
OPEN
```

Experiment 2B target:

```text
cyclic order
```

---

## G4 — Geometric semantic stability

Different drawings preserve semantics under allowed deformation.

Containment:

```text
early PASS
```

This remains important for Arcana's artistic design.

---

# 17. Living falsification matrix

| Hypothesis | Status | Evidence | Next action |
|---|---|---|---|
| Architecture verifier catches hidden transitive effects | **PASS** | Experiment 1 | Preserve |
| Geometry determines region membership authoritatively | **PASS** | Experiment 2A | Preserve |
| Geometry composes with existing verifier | **PASS** | L4_04 | Preserve |
| Containment has real semantic leverage | **PASS qualitatively** | Verifier behavior changes | Instrument consumers |
| Containment leverage = exactly 6× | **UNPROVEN** | Counters were unconditional | Measure |
| Harmless deformation preserves semantics | **PASS, early** | Experiment 2A | G4 |
| Boundary ambiguity is intrinsic to geometric source | **PASS** | Experiment 2A | Preserve |
| Bare crossing authorizes communication | **REJECTED** | Violates sealing theorem | Never regress |
| Clean G3 result exists | **OPEN** | Not yet shown | Cyclic order |
| Cyclic order is natural at realistic scale | **OPEN** | Not yet tested | Extend 18-node fixture |
| Boundary-port design is ready | **DEFERRED** | Depends on later geometry | Do not implement |
| Winding has useful semantics | **DEFERRED** | No real use case yet | Wait |
| Planarity should be next | **REJECTED FOR NOW** | Too many confounds | Later |

This table remains the project tracker.

---

# 18. What should count as Experiment 2B success

Experiment 2B should require **both**:

## Formal success

\[
U(A)=U(B)
\]

but:

\[
Sem(A)\ne Sem(B)
\]

with no named-port or hidden metadata cheat.

## Naturalness success

The same geometric relation remains understandable and useful in the persistent realistic fixture.

Only then should cyclic order be promoted to a core semantic primitive.

---

# 19. What should count as partial success

Possible outcome:

```text
formal G3 result succeeds
realistic fixture is awkward
```

Conclusion:

> cyclic order proves that embedding can carry semantics, but this specific use should not become a central Arcana construct.

This would still answer the theoretical question.

---

# 20. What should count as failure

Experiment 2B fails if:

1. ordering still depends on named ports,
2. hidden index metadata is required,
3. global layout perturbations change semantics unpredictably,
4. a supposedly cyclic relation needs a textual anchor everywhere,
5. the realistic fixture is less understandable than explicit successor metadata,
6. the abstract graph secretly differs between the two pair fixtures.

Any of these invalidate or weaken the result.

---

# 21. Immediate implementation sequence

```text
1. instrument real fan-out consumers

2. update/supersede Experiment 2A leverage wording

3. freeze crossing != authorization in design docs/tests

4. introduce a genuinely cyclic node/relation kind

5. create anonymous-edge pair fixtures

6. verify same ordinary graph / different rotation system

7. prove semantic difference

8. add G4-style deformation tests for cyclic order

9. integrate the construct into the existing 18-node fixture

10. evaluate naturalness

11. only then decide whether cyclic order becomes core
```

---

# 22. Final research lock

The project has enough theory to proceed.

The next question is precise:

\[
\boxed{
\text{Can local cyclic embedding produce useful semantics
without relying on conventional positional metadata?}
}
\]

The existing architecture remains the grounding environment.

The project should not branch into:

- boundary-port design,
- winding semantics,
- planarity,
- richer effect systems,
- new visual-editor work,

until Experiment 2B produces new evidence.

The governing design standard remains:

> **Do not ask whether text could serialize the same fact. Ask whether the fact arises naturally and authoritatively from the representation the programmer is already using.**

And the governing product standard remains:

> **Do not keep a geometric feature merely because it proves a theorem. Keep it only if geometry is also the natural place for that meaning.**
