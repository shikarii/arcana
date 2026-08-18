# Arcana Experiments 1 + 2A: Consolidated Adversarial Assessment
## What is now proven, what remains overstated, and what Experiment 2B should actually test

**Status:** Research synthesis / decision memo  
**Date:** 2026-08-18  
**Purpose:** Consolidate the findings from Experiment 1 and Experiment 2A, preserve the strongest positive results, correct overclaims, and freeze the next research target.

---

# 1. Executive summary

The first two experiments have moved Arcana from:

> "a compelling geometric-language idea"

toward:

> "a language with at least one implemented path from geometry to compiler-enforced architectural semantics."

That is real progress.

The strongest combined story is:

```text
Experiment 1
    ↓
proved architecture verification works

Experiment 2A
    ↓
proved geometry can become the authoritative source
for semantic region membership

that derived membership
    ↓
feeds the existing verifier unchanged

which can then reject a hidden transitive effect
```

This is the first important Arcana result where:

\[
\text{geometry}
\rightarrow
\text{semantic ownership}
\rightarrow
\text{static analysis}
\rightarrow
\text{compiler refusal}
\]

The project has therefore established that geometry can participate meaningfully in compilation.

However, several claims in the current findings should be weakened or reclassified before they become design dogma.

The most important corrections are:

1. **6× fan-out is not yet empirically measured downstream consumption.**
2. **Boundary ambiguity and deformation invariance are not evidence that text is fundamentally incapable of representing the same facts.**
3. **Deformation invariance is not naturally a G3 property; it is a semantic-stability/equivalence property.**
4. **Experiment 2B should not jump to planarity. The locked research plan still favors cyclic order as the clean G3 test.**
5. **Boundary crossing must not automatically imply authorization. Crossing and permission are different semantic facts.**

---

# 2. Experiment 1: what is settled

Experiment 1 established that the architecture verifier works over Arcana's semantic graph.

It proved two main properties for sealed regions:

## Effect containment

\[
Effects(R)\subseteq Capabilities(R)
\]

with effects collected transitively through:

- direct nodes,
- intrinsics,
- function calls,
- spawned threads,
- nested regions.

## Boundary integrity

A dependency crossing between separate sealed regions requires an explicitly declared architectural path/channel.

The most important Experiment 1 result was the transitive effect case:

```text
sealed region
    ↓
calls helper
    ↓
helper eventually calls print
    ↓
Logger effect discovered
    ↓
region rejected if Logger undeclared
```

That demonstrated that effects cannot simply be hidden behind ordinary function abstraction.

---

# 3. Experiment 1 also produced an important negative result

The experiment explicitly asked:

> Could a YAML capability declaration plus ordinary static analysis replicate this?

The answer was:

> **Mostly yes.**

That matters.

It established that:

```text
effect containment
+
module dependency checking
```

are not sufficient to justify Arcana's geometric programming model.

Experiment 1 therefore produced necessary infrastructure, not yet a uniquely geometric product result.

This was a successful falsification outcome.

---

# 4. Experiment 2A changes the causal story

Experiment 2A introduced actual geometric containment.

The implementation:

1. computes point-in-circle containment,
2. distinguishes Inside / Outside / Ambiguous,
3. chooses the innermost containing circle,
4. derives region membership from geometry,
5. writes that derived membership into the semantic graph,
6. feeds the resulting graph into the existing architecture verifier.

The important architectural fact is:

> **The verifier did not need to be modified.**

This means the semantic graph provides a clean boundary:

```text
geometry
    ↓
semantic graph
    ↓
existing verifier
```

That is a good compiler architecture result.

---

# 5. L4_04 is the strongest combined experiment

The 18-node pipeline is the most important fixture across Experiments 1 and 2A.

The critical sequence is:

```text
nodes initially belong to root r0

geometry positions helper/route inside Planner

arc_geo_derive_regions()
    ↓
Planner becomes their semantic owner

helper transitively reaches print
    ↓
Logger effect

Planner is sealed
Planner lacks Logger

verifier rejects
```

If those nodes remain in the unsealed root, the program passes.

Therefore:

\[
\boxed{
\text{geometry caused the semantic ownership relation
that made the architectural violation observable}
}
\]

This is much stronger than:

> "we successfully drew a circle."

It demonstrates actual semantic composition between geometry and verification.

---

# 6. A precision note: "sole source of truth" is true with a qualification

The findings say geometry is the sole source of truth for region membership.

For positioned nodes participating in geometric regions, that is essentially correct:

```text
geometry
    ↓
derived membership
```

and the prior region field is overwritten.

However, the fixture also begins with nodes in root `r0`, and only a subset are explicitly positioned into the three geometric regions.

So the most precise statement is:

> **For geometrically positioned nodes, containment is authoritative for explicit region membership; uncontained/unpositioned nodes remain in the ambient root context.**

This is not a weakness.

A root environment is a reasonable default.

But it is worth keeping the implementation claim exact.

---

# 7. G1 containment is proven

Containment now satisfies the first geometric criterion cleanly.

\[
Inside(x,R)
\Rightarrow
Region(x)=R
\]

No separate author declaration such as:

```text
node x ... in R
```

is required for the derived membership.

Therefore:

```text
Containment G1: YES
```

This does not need further theoretical debate.

---

# 8. Semantic fan-out is the right idea

The most important conceptual result from the previous design work remains:

> **The value of containment is not that it can encode membership. The value is how many useful semantic consequences follow from one authoritative geometric relation.**

Candidate downstream consequences include:

- membership,
- lexical scope,
- effect ownership,
- capability context,
- dependency ownership,
- visibility.

This is the right lens.

If:

\[
Inside(x,R)
\]

controls all of these naturally, then a single geometric fact becomes a high-leverage semantic primitive.

---

# 9. But "6× measured leverage" is not yet proven

This is the biggest overstatement in the findings.

The document itself admits that the six fan-out counters are incremented unconditionally in the same derivation loop.

That means the current implementation measures:

> **six intended semantic interpretations of membership**

not:

> **six independently observed downstream consumers of the derived fact.**

Those are not equivalent.

The project should not call the current value:

```text
measured 6× leverage
```

yet.

A more accurate label is:

```text
declared fan-out = 6
```

or:

```text
potential semantic fan-out = 6
```

---

# 10. What is already empirically proven about fan-out

Experiment 2A does demonstrate real G2 behavior.

At minimum:

\[
Inside(x,R)
\rightarrow
RegionMembership(x,R)
\]

which then affects:

\[
EffectOwnership(x,R)
\]

and:

\[
CapabilityCheck(R)
\]

because L4_04 changes compiler acceptance.

Boundary verification also uses sealed-region ancestry, so dependency ownership is plausibly already an actual consumer as well.

Therefore:

```text
G2 containment: YES
```

is defensible.

But:

```text
G2 leverage = exactly 6×
```

is not yet experimentally established.

---

# 11. Strengthen fan-out measurement before moving far beyond 2A

This should be a small cleanup task, not a new research phase.

Instrument actual consumers.

For each geometrically derived membership, record whether it is consumed by:

```text
scope resolver
effect collector
capability checker
boundary verifier
visibility resolver
other implemented analysis
```

Example:

```text
node helper:

membership            DERIVED
scope                  NOT EXERCISED
effect ownership       CONSUMED
capability context     CONSUMED
dependency ownership   CONSUMED
visibility             NOT EXERCISED
```

Now the leverage claim becomes empirical.

---

# 12. Better fan-out metrics

Define:

\[
Fanout_{used}(g)
=
|\{
c :
c\text{ actually consumes a fact derived from }g
\}|
\]

and optionally:

\[
Fanout_{effective}(g)
=
|\{
c :
c\text{ consumes }g
\land
c\text{ can alter compiler behavior}
\}|
\]

This prevents the metric from being inflated by simply inventing additional conceptual interpretations.

---

# 13. Fan-out is semantic reuse, not information multiplication

The six consequences are not six independent bits magically created by one point-in-circle test.

A more precise interpretation is:

> **one authoritative structural fact is reused by multiple semantic subsystems.**

That is valuable.

In fact, that may be the product.

The circle prevents several systems from maintaining separate, potentially inconsistent declarations.

So the better phrase is:

```text
semantic leverage
```

or:

```text
single-source semantic reuse
```

rather than literal information amplification.

---

# 14. Boundary ambiguity rejection is good engineering

The three-valued containment relation:

```text
Inside
Outside
Ambiguous
```

is a good design.

The epsilon zone ensures that a program cannot depend on being microscopically close to a semantic boundary.

This gives Arcana a robustness rule:

\[
NearBoundary(x,R)
\Rightarrow
Reject
\]

That is exactly the kind of well-formedness constraint a geometric source language needs.

---

# 15. But "text fundamentally cannot do this" is too strong

A textual file can encode:

```text
coordinates
distance-to-boundary
containment constraints
guard bands
```

and a compiler can reject an ambiguous geometric state.

So the strong statement:

> text cannot express boundary ambiguity

is false in the literal sense.

The better claim is:

> **Boundary ambiguity exists intrinsically in a geometric source representation and requires additional geometric metadata or a secondary model in a non-geometric representation.**

That is still meaningful.

Arcana's advantage is **intrinsicness**, not the impossibility of serialization.

---

# 16. Deformation invariance is important, but it is not G3

The findings currently classify deformation invariance as G3.

That conflicts with the earlier G3 definition.

G3 asks for:

\[
U(A)=U(B)
\]

but:

\[
Sem(A)\ne Sem(B)
\]

because embedding differs.

Deformation invariance instead says:

\[
A\ne B
\]

geometrically, while:

\[
Sem(A)=Sem(B)
\]

because the relevant topological relation is unchanged.

That is nearly the opposite pattern.

Deformation invariance should therefore be moved into a separate category.

---

# 17. Add G4: geometric semantic stability

A cleaner framework is:

## G1 — Intrinsic encoding

Geometry directly determines a semantic fact.

Example:

\[
Inside(x,R)
\]

---

## G2 — Semantic leverage

One geometric fact drives multiple useful semantic consequences.

Example:

\[
Inside(x,R)
\rightarrow
membership + effects + capabilities
\]

---

## G3 — Embedding-exclusive distinction

Same abstract graph, different embedding-sensitive semantics:

\[
U(A)=U(B)
\]

but:

\[
Sem(A)\ne Sem(B)
\]

Cyclic order is the clean next candidate.

---

## G4 — Geometric semantic stability

Different geometric representatives preserve semantics under allowed deformation:

\[
A\sim_g B
\Rightarrow
Sem(A)=Sem(B)
\]

This captures:

- resize,
- translation,
- curve deformation,
- small node movement,
- other aesthetic changes,

provided semantic relations remain unchanged.

Containment has now demonstrated an early G4 result.

---

# 18. G4 may be essential for Arcana's artistic goal

G4 is not an incidental property.

Arcana wants programs to be visually expressive and beautiful.

That requires many distinct drawings to represent the same program.

Otherwise programming becomes pixel-sensitive.

So the language needs a large equivalence class:

\[
[A]_{\sim_g}
\]

of visually different but semantically identical drawings.

Experiment 2A's deformation test is therefore important evidence for the artistic thesis:

> **beautification can occur without behavior changes as long as semantic topology is preserved.**

That deserves its own category.

---

# 19. Spatial architecture is useful, but it is currently presentation + semantic structure, not proven G3

The findings say the left-to-right arrangement:

```text
Perception -> Planner -> Controller
```

encodes data flow visually.

That is true as a human-factors statement.

But if the actual semantic graph still contains explicit directed edges, then the left-to-right layout itself is not yet creating that dataflow.

The dataflow exists independently.

So:

```text
spatial architecture reasoning
```

is currently primarily:

- presentation,
- human comprehension,
- architectural visualization.

That can be extremely valuable.

But it is not yet evidence that spatial position changes formal semantics.

Do not overclassify it.

---

# 20. The "three G3-exclusive properties" conclusion should therefore be revised

Current proposed three:

1. boundary ambiguity rejection,
2. deformation invariance,
3. spatial architecture.

A more rigorous classification is:

## Boundary ambiguity

```text
geometric well-formedness / intrinsicness
```

Potentially geometry-specific in the source model, but serializable.

## Deformation invariance

```text
G4 semantic stability
```

not G3.

## Spatial architecture

```text
human-readable structural presentation
```

unless future semantics actually derive meaning from relative spatial arrangement.

Therefore Experiment 2A has **not yet produced the clean G3 theorem**.

That is fine.

G3 was always expected to be cyclic order's job.

---

# 21. This makes Experiment 2A cleaner, not weaker

The strongest honest classification is:

```text
G1: YES
geometry is authoritative for containment-derived membership

G2: YES
containment has demonstrated real downstream semantic leverage,
exact factor still to be instrumented

G3: NOT YET
no clean same-graph/different-semantic embedding pair yet

G4: YES, initial evidence
harmless geometric deformation preserves semantics
```

That is a strong result.

There is no need to force containment into G3.

---

# 22. Do not jump to planarity as Experiment 2B

The findings recommend:

> Experiment 2B: Edge embedding and planarity.

I disagree.

The research order was already settled for good reasons:

1. containment,
2. cyclic order,
3. winding only when demanded.

The planarity proposal appears to be motivated by wanting a stronger G3 result.

But cyclic order is still the cleaner controlled test.

---

# 23. Why planarity is a poor immediate next experiment

Planarity introduces several confounds at once:

- edge geometry,
- crossing detection,
- rerouting,
- graph embedding,
- layout constraints,
- potentially automatic edge routing,
- interaction between visual and semantic crossings.

It also risks generating a result that is mathematically interesting but product-weak.

And planarity has already caused one overclaim in Experiment 1:

> planarity does not bound maximum component degree.

It bounds global edge density for simple planar graphs.

So planarity deserves careful treatment later.

It should not displace the cleaner cyclic-order experiment.

---

# 24. Cyclic order remains the best G3 test

The next clean theorem is still:

\[
U(A)=U(B)
\]

but:

\[
Sem(A)\ne Sem(B)
\]

where the only difference is the rotation system around a node or region.

The test must preserve the previously established discipline:

```text
anonymous incident edges
no arg0/arg1/lhs/rhs
no hidden ordering metadata
```

Then:

\[
\rho_v^A \ne \rho_v^B
\]

while ordinary abstract connectivity stays identical.

That directly isolates embedding information.

---

# 25. But do not force cyclic order into ordinary positional arguments

The minimal noncommutative operator remains useful as a mechanism test.

Example:

```text
A = 1
B = 2
C = 3
```

and geometry orders them differently.

But that should not automatically become the final language design.

A better realistic test may use a genuinely cyclic relation:

```text
sensor ring
worker ring
protocol successor
clockwise neighbor
phase cycle
```

This avoids rebuilding a linear argument list from a circle.

---

# 26. The existing 18-node fixture should become the cyclic-order grounding environment

Do not create a new architecture.

Extend L4_04 or its successor.

Possible addition:

```text
four sensor/preprocessing nodes
arranged in a ring
```

Use cyclic order to determine:

```text
clockwise successor
```

or:

```text
neighbor relation
```

Then test:

```text
same nodes
same edges
same region membership
different cyclic order
```

This keeps Experiment 2B grounded.

---

# 27. Boundary-crossing inference needs a major semantic correction

The findings propose:

> infer channels from edges that geometrically cross circle boundaries.

This is dangerous if interpreted literally.

A boundary crossing is an **observed geometric fact**.

A channel is an **authorization claim**.

Those are not the same thing.

If every crossing edge automatically becomes a valid channel, then this invalid architecture:

```text
sealed A
sealed B

hidden edge A -> B
```

would become legal merely because the edge visibly crosses the boundaries.

That destroys the key Experiment 1 guarantee.

---

# 28. Crossing and authorization must remain distinct

The correct structure is:

\[
CrossesBoundary(e,R)
\]

is derived geometrically.

But:

\[
Authorized(e,R)
\]

must come from an independent semantic declaration or recognized port/capability construct.

Then the verifier checks:

\[
CrossesBoundary(e,R)
\Rightarrow
Authorized(e,R)
\]

not:

\[
CrossesBoundary(e,R)
\Rightarrow
Authorized(e,R)\text{ automatically}
\]

This distinction is fundamental.

---

# 29. Geometry can still eliminate explicit channel metadata — but only with a real port object

A stronger geometric design could use:

```text
boundary port glyph
```

or:

```text
capability sigil located on the boundary
```

Then:

```text
edge intersects recognized port
```

may derive authorization.

Conceptually:

\[
Cross(e,\partial R)
\land
Intersects(e,Port_p)
\land
PortAllows(p,e)
\Rightarrow
Authorized(e,R)
\]

Now the geometry carries the authorization structure.

But a bare crossing should never be enough.

---

# 30. This is another semantic fan-out opportunity

A boundary port could potentially derive:

```text
cross-boundary authorization
interface membership
directionality
capability type
channel identity
visibility boundary
```

That could become another G2 feature later.

But it should be tested only after cyclic order unless required by the current fixture.

---

# 31. The next immediate cleanup: make fan-out empirical

Before Experiment 2B, make the current containment leverage measurement honest.

This should be small.

Goal:

```text
potential fan-out
    ↓
actual consumer fan-out
```

Instrument:

- effect collector,
- capability checker,
- boundary verifier,
- scope resolver if implemented,
- visibility resolver if implemented.

Do not delay cyclic order for large missing subsystems.

If scope/visibility are not implemented, simply mark them:

```text
NOT YET MEASURED
```

---

# 32. The consolidated falsification matrix

This should remain the project tracker.

| Hypothesis | Current status | Evidence | Next falsification step |
|---|---|---|---|
| Architecture verifier catches hidden effects | **PASS** | Experiment 1 transitive Logger fixture | Realistic/higher-order later |
| Geometry can authoritatively determine region membership | **PASS** | Experiment 2A containment derivation | Scale beyond circles later |
| Geometry composes with existing verifier | **PASS** | L4_04 hidden Logger rejection | Continue using persistent fixture |
| Containment has semantic fan-out | **PASS, factor uncertain** | Effects/capabilities definitely consume membership | Instrument actual consumers |
| 6× fan-out specifically | **UNPROVEN** | Counters currently unconditional | Consumption instrumentation |
| Containment survives harmless deformation | **PASS, early** | deformation tests | Broader edits later |
| Geometry has a clean G3 semantic distinction | **NOT YET** | containment does not cleanly establish it | Cyclic-order pair |
| Spatial layout formally changes semantics | **NOT YET** | flow already encoded by edges | Future spatial semantics if useful |
| Boundary crossing can imply authorization | **REJECT AS STATED** | would weaken sealing theorem | Require explicit/recognized port |
| Winding is useful | **DEFERRED** | no natural use case yet | Wait for real feedback distinction |

---

# 33. Revised score after two experiments

A good honest summary is:

## After Experiment 1

```text
Verifier:
YES

Unique geometry value:
NO

Could YAML + analysis replicate most of it?
YES
```

## After Experiment 2A

```text
Geometry as authoritative semantic source:
YES

Geometry -> verifier composition:
YES

Containment G1:
YES

Containment G2:
YES, exact leverage factor not yet measured

Containment/geometry G4 stability:
EARLY YES

Clean G3 same-graph/different-semantics result:
NOT YET
```

That is genuine forward movement.

---

# 34. What Experiment 2A actually changed in the product thesis

Before:

> Arcana can statically verify architecture.

After:

> **Arcana can derive architecture from geometry and statically verify consequences of that derived architecture.**

That is the real delta.

The strongest product sentence is now:

> **The circle is not merely drawn around code. The compiler uses the fact that code is inside that circle to decide which architectural rules apply to it.**

That is much stronger than the Experiment 1 story.

---

# 35. But geometry is still not proven necessary

The findings are right to preserve this negative result.

Explicit:

```text
in Planner
```

can reproduce the membership relation.

So containment does not establish:

> only geometry can support these semantics.

Instead it establishes:

> **geometry can make the semantic relation intrinsic, visible, and structurally reusable.**

That is a defensible and useful result.

The project should not chase an impossible standard where text must be incapable of serializing the same information.

---

# 36. The real next question remains G3

Experiment 2B should now ask:

\[
\boxed{
\text{Can two programs with the same ordinary abstract graph have different useful semantics solely because their geometric rotation systems differ?}
}
\]

That is the cleanest remaining geometric theorem.

Cyclic order is still the best candidate.

---

# 37. Recommended immediate engineering sequence

## Step 1 — tighten Experiment 2A reporting

Replace:

```text
measured fan-out = 6×
```

with:

```text
declared fan-out = 6
empirically consumed fan-out = N
```

after instrumentation.

---

## Step 2 — lock crossing vs authorization semantics

Document:

```text
boundary crossing != authorization
```

A recognized port/capability relation may authorize a crossing later.

---

## Step 3 — preserve the 18-node fixture

Use it as the persistent Experiment 2 environment.

---

## Step 4 — implement cyclic-order micro-pair

Use:

- same nodes,
- same anonymous incident edges,
- same operations,
- different rotation systems.

No named-port cheating.

---

## Step 5 — test deformation/equivalence for cyclic order

Move/rotate/bend while preserving the same combinatorial order.

Semantics must remain stable.

---

## Step 6 — embed cyclic order in the persistent fixture

Prefer a genuinely cyclic relationship:

```text
worker/sensor ring
clockwise successor
protocol phase
```

rather than merely reimplementing `arg0`.

---

## Step 7 — evaluate naturalness

If the pair theorem works but the realistic fixture becomes less understandable, reject cyclic order as a core semantic primitive.

The theorem can be true while the feature is bad.

---

# 38. Final assessment

Experiment 2A succeeded.

It produced the first meaningful Arcana pipeline of:

\[
\boxed{
\text{geometry}
\rightarrow
\text{semantic architecture}
\rightarrow
\text{compiler-enforced consequence}
}
\]

That is the result to celebrate.

But the findings should not inflate it into claims that have not been demonstrated.

The cleanest current classification is:

\[
\boxed{
G1\ \text{proven}
}
\]

\[
\boxed{
G2\ \text{proven qualitatively, exact leverage pending}
}
\]

\[
\boxed{
G3\ \text{still open}
}
\]

\[
\boxed{
G4\ \text{deformation stability has early evidence}
}
\]

The project is now exactly where it should be:

> containment has shown that geometry can be authoritative and useful; cyclic order must now test whether embedding itself can carry a useful semantic distinction that the ordinary graph forgets.

Do not jump to planarity merely because it sounds more geometric.

Do not let a boundary crossing automatically become permission.

Do not claim 6× leverage until six real consumers have been observed.

Keep the adversarial standard that made the first two experiments informative.
