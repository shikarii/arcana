# Arcana: Experiment 2A Corrections and Experiment 2B Lock
## Accepted corrections, updated research tracker, and immediate implementation actions

**Status:** Decision memo / project state update  
**Date:** 2026-08-18  
**Purpose:** Record the conclusions now considered settled after Experiment 2A and its adversarial review, and freeze the next implementation target.

---

# 1. What is now settled

The following conclusions should be treated as project state, not open discussion.

## Experiment 1

Experiment 1 proved:

- transitive architectural effect analysis works,
- hidden effects can be discovered through call chains,
- sealed-region capability checks are practical,
- cross-region dependency checks are practical.

It also established an important negative result:

> Most of the Experiment 1 verifier could be replicated with explicit metadata plus static analysis.

Therefore Experiment 1 validated the verifier infrastructure, but not yet the necessity or special value of geometry.

---

# 2. Experiment 2A changed the balance

Experiment 2A established the first real pipeline:

\[
\boxed{
\text{geometry}
\rightarrow
\text{derived semantic ownership}
\rightarrow
\text{existing architecture verifier}
\rightarrow
\text{compiler rejection}
}
\]

The L4_04 fixture remains the most important result.

Geometry reassigns nodes into Planner, the existing transitive effect walk discovers the hidden Logger effect, and the sealed Planner region is rejected because Logger is undeclared.

The verifier itself did not need to change.

That demonstrates good compiler layering:

```text
geometric source facts
        ↓
semantic graph
        ↓
source-agnostic verifier
```

---

# 3. Correction: "6× measured leverage" is not established

The current findings describe:

```text
membership
scope
effect ownership
capability context
dependency ownership
visibility
```

as six consequences of:

\[
Inside(x,R)
\]

That is still the right **semantic fan-out hypothesis**.

However, the current implementation increments all six counters unconditionally during derivation.

Therefore the correct current label is:

```text
declared fan-out = 6
```

or:

```text
potential fan-out = 6
```

not:

```text
empirically measured leverage = 6×
```

The exact leverage factor remains unproven until downstream consumers are instrumented.

---

# 4. Fan-out should now be measured by actual consumption

Define:

\[
Fanout_{used}(g)
=
|\{
c :
c\text{ actually consumes a semantic fact derived from }g
\}|
\]

Optionally define:

\[
Fanout_{effective}(g)
=
|\{
c :
c\text{ consumes the fact and can alter compiler behavior}
\}|
\]

For containment:

\[
g=Inside(x,R)
\]

Instrument actual consumers such as:

```text
region membership
scope resolver
effect collector
capability checker
boundary verifier
visibility resolver
```

If a subsystem does not exist or does not yet consume derived membership, mark it:

```text
NOT YET MEASURED
```

Do not count it.

---

# 5. Semantic fan-out remains the primary containment metric

The conceptual insight survives the correction.

The important question is still:

> **How much useful compiler semantics follows from one authoritative geometric fact?**

If:

\[
Inside(x,R)
\]

only produces:

```text
membership
```

then containment is largely a geometric replacement for:

```text
in R
```

If it drives:

```text
membership
scope
effect ownership
capability requirements
dependency accounting
visibility
```

through real downstream consumers, then the circle is a high-leverage programming primitive.

The experiment should measure this rather than assume it.

---

# 6. Correction: deformation invariance is not G3

The prior classification mixed two opposite ideas.

## G3

G3 asks:

\[
U(A)=U(B)
\]

but:

\[
Sem(A)\ne Sem(B)
\]

because embedding information changes semantics.

This is a **semantic distinction created by embedding**.

## Deformation invariance

Deformation invariance instead says:

\[
A\ne B
\]

geometrically, but:

\[
Sem(A)=Sem(B)
\]

because the relevant structural relation remains unchanged.

These are different properties.

---

# 7. G4 is now adopted

Use:

## G1 — Intrinsic encoding

Geometry directly determines a semantic fact.

Example:

\[
Inside(x,R)
\]

---

## G2 — Semantic leverage

One geometric fact drives several useful semantic consequences.

---

## G3 — Embedding-exclusive distinction

Same abstract graph, different semantics because geometric embedding differs:

\[
U(A)=U(B)
\]

but:

\[
Sem(A)\ne Sem(B)
\]

---

## G4 — Geometric semantic stability

Different visual/geometric representatives preserve semantics under allowed deformation:

\[
A\sim_gB
\Rightarrow
Sem(A)=Sem(B)
\]

This is the correct category for:

- resizing circles,
- translating diagrams,
- moving nodes within a region,
- harmless curve deformation,
- aesthetic layout changes.

---

# 8. G4 is product-critical

G4 is not merely a formal cleanup.

Arcana's artistic premise requires a large equivalence class of visually different but semantically identical programs.

Without G4:

```text
beautification
=
risk of behavioral change
```

That would make the language unusably fragile.

The desired property is:

> **A programmer can beautify or reorganize a drawing while preserving its semantic topology.**

Experiment 2A produced early evidence for this through deformation-stability tests.

---

# 9. Correction: boundary crossing is not authorization

This is now a locked design invariant.

Derived geometric fact:

\[
CrossesBoundary(e,R)
\]

does **not** imply:

\[
Authorized(e,R)
\]

Automatically authorizing every crossing would destroy the sealed-region guarantee.

Therefore:

\[
\boxed{
CrossesBoundary(e,R)
\not\Rightarrow
Authorized(e,R)
}
\]

---

# 10. Crossing and permission are different semantic roles

A crossing is:

```text
an observed geometric relation
```

Authorization is:

```text
a semantic grant
```

The verifier should check something like:

\[
CrossesBoundary(e,R)
\Rightarrow
Authorized(e,R)
\]

where authorization must come from a distinct construct.

Possible future constructs:

```text
boundary port
capability sigil
typed interface glyph
explicit channel object
```

A bare edge crossing is never enough.

---

# 11. Future geometric authorization should use a distinct boundary object

A plausible future rule:

\[
Cross(e,\partial R)
\land
Intersects(e,Port_p)
\land
PortAllows(p,e)
\Rightarrow
Authorized(e,R)
\]

This preserves both:

- geometric intrinsicness,
- architectural refusal semantics.

It also creates a future semantic fan-out candidate:

```text
boundary port
    ->
authorization
interface membership
directionality
capability type
channel identity
visibility rule
```

But this is not Experiment 2B.

---

# 12. Correction: spatial architecture is currently presentation, not formal semantics

The left-to-right layout of:

```text
Perception -> Planner -> Controller
```

is useful for human comprehension.

But the dataflow semantics currently come from the directed edges.

Therefore the spatial layout is presently:

```text
presentation
+
human architectural readability
```

not:

```text
formal semantic derivation
```

This distinction should remain explicit.

Spatial position may become semantic later, but it is not yet.

---

# 13. Boundary ambiguity remains significant

The correction that "text can serialize geometry" should not be interpreted as diminishing the architectural value of boundary ambiguity.

The important distinction is not:

> text is logically incapable of representing a boundary distance.

A textual system could add:

```text
coordinates
boundary geometry
distance checks
```

The important distinction is:

> **In Arcana, the ambiguity condition falls out naturally from the primary source representation. In a conventional textual system, it requires adding a secondary geometric model.**

This is an intrinsicness advantage.

That is architecturally meaningful even though it is not an impossibility theorem.

---

# 14. Intrinsicness is a legitimate language-design advantage

The standard should not be:

\[
\text{text cannot serialize this information}
\]

because text can serialize essentially any finite structure.

The stronger useful distinction is:

\[
\boxed{
\text{Does the fact arise directly from the representation the programmer already uses?}
}
\]

For containment:

```text
draw inside
    ↓
membership exists
```

For a conventional text/module system:

```text
write declaration
    ↓
membership exists
```

If geometry additionally provides:

- ambiguity rejection,
- deformation stability,
- visual architecture,
- semantic fan-out,

then the representational difference can become substantial.

---

# 15. Current classification after Experiment 2A

## Containment

```text
G1: YES
```

Geometry authoritatively determines membership.

```text
G2: YES qualitatively
```

Derived membership demonstrably affects real downstream architecture verification.

Exact leverage factor remains pending.

```text
G3: NOT CLEANLY PROVEN
```

Containment membership can be encoded explicitly in a non-geometric representation.

```text
G4: EARLY YES
```

Harmless deformation preserves semantics as long as containment relations remain unchanged.

---

# 16. The clean G3 test remains open

The project still lacks the ideal result:

\[
U(A)=U(B)
\]

but:

\[
Sem(A)\ne Sem(B)
\]

where the semantic difference comes from embedding information that the ordinary abstract graph forgets.

Cyclic order remains the best next candidate.

---

# 17. Planarity is deferred

Do not move next to planarity.

Reasons:

- it introduces edge geometry and crossing detection simultaneously,
- it mixes layout, routing, graph embedding, and semantics,
- it is a noisier controlled experiment,
- previous planarity reasoning already produced one incorrect coupling claim,
- cyclic order is the cleaner G3 isolation test.

Planarity remains interesting.

It is simply not the next experiment.

---

# 18. Experiment 2B target: cyclic order

The goal is:

\[
\boxed{
U(A)=U(B)
\quad\text{and}\quad
Sem(A)\ne Sem(B)
}
\]

where the only meaningful difference is the local rotation system.

The experiment must avoid the known named-port trap.

---

# 19. Anonymous-edge discipline remains mandatory

Do not use:

```text
arg0
arg1
lhs
rhs
```

as independent ordering metadata.

Use anonymous incident edges:

```text
A -> F
B -> F
C -> F
```

Then derive ordering solely from:

\[
\rho_F
\]

the cyclic order around the node.

If positional information survives elsewhere in the abstract graph, the experiment is invalid.

---

# 20. Prefer genuinely cyclic semantics in the realistic fixture

A minimal noncommutative operator remains useful as a proof-of-mechanism test.

But the realistic fixture should use something genuinely cyclic.

Candidates:

```text
sensor ring
worker ring
clockwise successor
counterclockwise neighbor
protocol phase cycle
```

These preserve the meaning of cyclic order as cyclic order rather than forcing it into a conventional linear argument list.

---

# 21. Reuse the existing realistic fixture

Do not replace L4_04 with a separate demo architecture.

Extend the same 18-node reactive pipeline.

Possible addition:

```text
sensor/preprocessing ring
```

inside Perception.

Then cyclic order can determine:

```text
next-clockwise worker
```

or another real ring relation.

This preserves continuity across experiments.

---

# 22. Immediate action 1: instrument fan-out consumers

Before or at the beginning of Experiment 2B:

```text
declared fan-out = 6
```

must become:

```text
actual consumed fan-out = N
```

Instrument the existing consumers.

This should be a small empirical cleanup, not a new research branch.

---

# 23. Immediate action 2: supersede the Experiment 2A overclaims

The Experiment 2A findings should either be updated or explicitly superseded by this corrected state.

Corrections:

```text
"Measured leverage = 6×"
    ->
"Declared/potential fan-out = 6; consumed fan-out pending"

"Deformation invariance = G3"
    ->
"Deformation invariance = G4"

"Spatial architecture = formal semantic delta"
    ->
"Spatial architecture currently aids human comprehension"

"Boundary ambiguity cannot exist in text"
    ->
"Boundary ambiguity is intrinsic to geometric source;
text requires an added geometric model"
```

---

# 24. Immediate action 3: lock crossing != authorization

Record as a permanent design invariant:

\[
\boxed{
ObservedBoundaryCrossing
\ne
AuthorizedBoundaryCrossing
}
\]

Any future implementation that violates this should be considered a regression against Experiment 1's core guarantee.

---

# 25. Immediate action 4: implement cyclic-order Experiment 2B

Controlled fixture:

```text
same nodes
same anonymous edges
same operations
different local cyclic order
```

Expected:

\[
U(A)=U(B)
\]

and:

\[
Sem(A)\ne Sem(B)
\]

only because the rotation system differs.

---

# 26. Immediate action 5: test cyclic naturalness in the persistent fixture

After the controlled pair succeeds or fails, immediately place the construct into the 18-node fixture.

Ask:

- is the relation obvious visually?
- does geometry eliminate ordering metadata?
- is editing stable?
- would explicit textual successor declarations be clearer?
- does the semantic distinction still feel natural at realistic scale?

A mathematically valid G3 result is not enough.

The feature must also survive the naturalness test.

---

# 27. Updated living falsification matrix

| Hypothesis | Current status | Evidence | Next action |
|---|---|---|---|
| Architecture verifier catches hidden transitive effects | **PASS** | Experiment 1 | Preserve |
| Geometry can authoritatively determine region membership | **PASS** | Experiment 2A | Preserve |
| Geometry composes with source-agnostic verifier | **PASS** | L4_04 | Preserve |
| Containment has real semantic leverage | **PASS qualitatively** | Effect/capability behavior changes | Instrument consumers |
| Containment leverage = exactly 6× | **UNPROVEN** | Counters unconditional | Measure actual consumers |
| Harmless deformation preserves semantics | **PASS, early** | Experiment 2A tests | Classify as G4 |
| Boundary ambiguity is intrinsic to geometric source | **PASS** | Three-valued containment | Keep claim precise |
| Spatial layout formally determines dataflow | **NOT PROVEN** | Directed edges still determine flow | Presentation only for now |
| Bare boundary crossing authorizes communication | **REJECT AS STATED** | Violates sealing model | Require distinct authorization construct |
| Clean G3 embedding-exclusive semantic distinction exists | **OPEN** | Not yet shown | Cyclic order 2B |
| Planarity should be next | **REJECT FOR NOW** | Too many confounds | Defer |
| Winding has useful semantics | **DEFERRED** | No natural use case yet | Wait |

This matrix is the governing project tracker.

---

# 28. The research plan is still locked

The order remains:

```text
Experiment 2A
containment
DONE

    ↓

fan-out instrumentation cleanup
SMALL

    ↓

Experiment 2B
cyclic order

    ↓

realistic cyclic-order integration
same 18-node fixture

    ↓

winding
ONLY if a real feedback distinction demands it

    ↓

planarity / crossings / richer edge geometry
later
```

No further reordering is warranted from the current evidence.

---

# 29. Strongest combined conclusion after Experiments 1 and 2A

The most defensible statement is now:

> **Arcana has demonstrated that a geometric relation can become the authoritative source of semantic architecture, and that existing compiler analyses can consume that geometry-derived structure to reject invalid programs.**

That is stronger than Experiment 1.

It does not yet establish that geometry is necessary.

It does establish that geometry can be:

- authoritative,
- compiler-relevant,
- compositional with existing verification,
- stable under harmless deformation,
- and potentially high-leverage.

---

# 30. The remaining question is sharper now

Containment has established:

\[
G1
\]

and meaningful:

\[
G2
\]

with early:

\[
G4
\]

The next experiment must target:

\[
G3
\]

directly.

The research question is:

\[
\boxed{
\text{Can the same ordinary graph carry different useful semantics
solely because its cyclic embedding differs?}
}
\]

That is Experiment 2B.

---

# 31. Final decision

No more broad review is needed before implementation.

The immediate work is:

1. instrument real fan-out consumers,
2. correct/supersede the Experiment 2A overclaims,
3. lock `crossing != authorization`,
4. implement the anonymous-edge cyclic-order pair,
5. extend the existing 18-node fixture with a genuinely cyclic relation,
6. evaluate both theorem success and language naturalness.

The project should continue using the falsification matrix rather than a feature checklist.

The standard remains:

> **Do not ask merely whether geometry can carry a fact. Ask whether geometry is the natural, authoritative, and high-leverage place for that fact.**
