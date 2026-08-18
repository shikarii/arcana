# Arcana Experiment 2B: Reporting and Evidence Lock
## 2B.0 is scaffolding; only geometry-derived ρ counts as experimental evidence

**Status:** Research reporting lock  
**Date:** 2026-08-18  
**Purpose:** Prevent Experiment 2B from accidentally reporting implementation scaffolding as evidence for geometric semantics.

---

# 1. Core correction

Experiment 2B is now split into two implementation layers:

## 2B.0 — Manual-ρ scaffolding

The test harness may manually inject:

\[
\rho
\]

to verify that:

- the cyclic semantic IR can store an oriented cyclic order,
- `next_clockwise` / adjacency queries work,
- lowering from cyclic semantic IR behaves correctly,
- canonicalization of cyclic shifts works,
- reflection/reversal behavior is implemented correctly.

This is useful engineering work.

It is **not an experiment result**.

---

## 2B.1 — Geometry-derived cyclic order

The authoritative source is 2D geometry.

Pipeline:

```text
2D member positions
    ↓
angular extraction
    ↓
robust cyclic-order derivation
    ↓
ρ
    ↓
cyclic semantic IR
    ↓
program semantics
```

Only this phase counts as evidence that Arcana's geometry contributes semantic structure.

---

# 2. Reporting rule

The findings document must never say:

> "Experiment 2B proved G3"

based solely on manually authored or manually injected \(\rho\).

The strongest statement permitted after 2B.0 alone is:

> **The cyclic semantic machinery is implemented and internally consistent.**

Not:

> geometry is semantic.

Not:

> G3 is proven.

Not:

> Arcana derives cyclic structure from embedding.

---

# 3. Why 2B.0 cannot count

If the fixture contains:

```text
cyclic_order ring S0 S1 S2 S3
```

and the compiler derives:

```text
next_clockwise(S0) = S1
```

then the experiment has shown only:

> **explicit ordering metadata affects semantics.**

That is tautologically true.

Changing:

```text
cyclic_order ring S0 S2 S1 S3
```

and observing changed semantics does not establish a geometric result.

It is the same class of mistake Experiment 1 already exposed:

```text
metadata
    ↓
semantic consequence
```

with geometry absent.

---

# 4. Geometry must be authoritative in 2B.1

The final experimental fixture must not contain an authoritative user-authored cyclic order.

It may contain:

```text
ring R
position S0 x0 y0
position S1 x1 y1
position S2 x2 y2
position S3 x3 y3
```

The compiler derives:

\[
\rho_R
\]

from those positions.

Any stored \(\rho\) in the semantic IR is therefore a **derived fact**.

The user must not be able to independently declare a contradictory order and have both remain sources of truth.

---

# 5. 2B.1 formal success conditions

A valid geometry-derived G3 result requires all of the following:

## A. Same embedding-stripped semantic graph

\[
U(A)=U(B)
\]

asserted programmatically.

The preserved graph must include:

- node identities,
- node kinds,
- semantic payloads,
- directed edges,
- edge payloads,
- region membership,
- ring/member set.

The forgotten data includes:

- coordinates,
- angular positions,
- derived \(\rho\),
- curve geometry,
- crossings,
- winding.

---

## B. Different geometry-derived cyclic order

\[
\rho_A \ne \rho_B
\]

where the difference is a genuine reorder, not merely a cyclic shift.

---

## C. No explicit ordering metadata

No:

```text
arg0
arg1
lhs
rhs
next=
cyclic_order=
```

may independently determine the cyclic semantics.

---

## D. Semantic difference follows from derived ρ

\[
Obs(A)\ne Obs(B)
\]

because the geometry-derived cyclic relation differs.

---

## E. G4 control passes

Two geometrically different layouts with the same derived cyclic order must satisfy:

\[
Sem(A)=Sem(A')
\]

---

# 6. 2B.0 findings must be labeled as scaffolding

Recommended findings format:

```text
## 2B.0 — Semantic machinery validation

Status: SCAFFOLDING PASS

Validated:
- cyclic-order storage
- successor queries
- reflection semantics
- lowering behavior

Research significance:
- none by itself
- does not count toward G3
```

This prevents future summaries from quietly inflating infrastructure tests into research evidence.

---

# 7. 2B.1 findings are the actual formal experiment

Recommended findings format:

```text
## 2B.1 — Geometry-derived cyclic-order experiment

Status: PASS / FAIL

Precondition:
embedding-stripped fingerprints equal

Geometry:
ρ derived solely from positions

Result:
same abstract semantic graph
different derived cyclic order
different defined observation

G3:
PASS / FAIL
```

Only this section may contribute to the statement:

> Arcana has demonstrated an embedding-exclusive semantic distinction.

---

# 8. Formal G3 remains low-weight evidence

Even a correct 2B.1 G3 pass is not enough to promote cyclic order into the language.

Once:

\[
Semantics=f(\rho)
\]

and:

\[
\rho=f(\text{geometry})
\]

then changing geometry so that \(\rho\) changes is expected to alter semantics.

Therefore:

```text
2B.1 G3 PASS
```

means:

> **the geometric pipeline is real and non-redundant.**

It does not yet mean:

> **this is a good programming construct.**

The feature verdict still depends on naturalness.

---

# 9. 2B-N remains the real product test

The realistic fixture must answer:

> Does a real program naturally want this cyclic relation?

and:

> Is geometry the natural authoritative place for that relation?

This requires:

- real observable use,
- metadata elimination,
- visual obviousness,
- editing stability,
- no hidden root requirement,
- no duplicated external configuration,
- low surprise.

Formal G3 is a prerequisite.

Naturalness is the promotion test.

---

# 10. Evidence hierarchy

Experiment 2B should explicitly rank evidence.

## E0 — Plumbing

Manual \(\rho\) affects semantics.

```text
Expected
Low research value
```

---

## E1 — Geometric extraction

2D geometry robustly derives \(\rho\).

```text
Necessary geometric evidence
```

---

## E2 — G3

Same abstract semantic graph, different geometry-derived \(\rho\), different semantics.

```text
Formal embedding evidence
```

---

## E3 — G4

Harmless deformation preserves \(\rho\) and semantics.

```text
Robustness evidence
```

---

## E4 — Natural use

A realistic program consumes the relation.

```text
Language-value evidence
```

---

## E5 — Metadata elimination / authority

Geometry replaces rather than duplicates another authoritative ordering source.

```text
Product-level evidence
```

The project should not summarize all of these merely as:

```text
Experiment 2B passed
```

Report each separately.

---

# 11. Pre-registered verdict format

The final Experiment 2B findings should end with something like:

| Dimension | Result |
|---|---|
| 2B.0 semantic scaffolding | PASS / FAIL |
| Geometry derives ρ | PASS / FAIL |
| Embedding-stripped equality | PASS / FAIL |
| Formal G3 | PASS / FAIL |
| G4 stability | PASS / FAIL |
| Angular ambiguity handling | PASS / FAIL |
| Realistic observable use | PASS / FAIL |
| Metadata elimination | PASS / FAIL |
| Naturalness | PASS / FAIL |
| Promote cyclic order to core semantics | YES / NO |

This makes post-hoc rationalization difficult.

---

# 12. Terminology lock

Use these terms consistently.

## Manual cyclic order

\[
\rho
\]

supplied directly by test code or fixture metadata.

Purpose:

```text
scaffolding only
```

---

## Derived cyclic order

\[
\rho
\]

computed from 2D geometry.

Purpose:

```text
actual experiment
```

---

## Cyclic shift

Same oriented cycle, different written starting point.

Example:

\[
(A,B,C,D)\sim(C,D,A,B)
\]

---

## Reflection / reversal

Opposite orientation.

Example:

\[
(A,B,C,D)\to(A,D,C,B)
\]

---

## Genuine reorder

Neither cyclic shift nor simple reversal.

Example:

\[
(A,B,C,D)\to(A,C,B,D)
\]

Only a genuine reorder should serve as the primary G3 A/B distinction.

---

# 13. The research lock remains unchanged

Sequence:

```text
fan-out instrumentation
    ↓
fingerprint infrastructure
    ↓
2B.0 scaffolding
    ↓
2B.1 geometry-derived cyclic order
    ↓
G3 + G4 matrix
    ↓
realistic naturalness fixture
    ↓
feature promotion / rejection
```

Still deferred:

```text
boundary-port design
winding
planarity
richer edge embedding
```

---

# 14. Final rule

The project should treat this as a hard reporting invariant:

\[
\boxed{
\text{Manually supplied }\rho\text{ can validate code, but cannot validate geometry.}
}
\]

Only:

\[
\boxed{
\text{geometry}
\rightarrow
\rho
\rightarrow
\text{semantic consequence}
}
\]

counts toward Experiment 2B's formal result.

And even then:

\[
\boxed{
G3\ PASS \ne Feature\ Accepted
}
\]

The final decision still belongs to the realistic naturalness test.

That distinction should be preserved in code, tests, findings, and every later summary of the experiment.
