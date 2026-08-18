# Geometry-Native Semantics: Beyond Node-Edge Graphs

**Status:** Design proposal
**Date:** 2026-08-17
**Prerequisite:** Arcana v0 prototype (66 opcodes, 195 tests, regions/edges/ports)

---

## 1. The Problem

Arcana v0 uses three topological facts to encode program meaning:

| Fact | Semantic | Representation |
|------|----------|---------------|
| Region containment | Lexical scope | `ArcRegion` with parent ID |
| Directed edges | Data flow | `ArcSemEdge` from output port to input port |
| Cyclic port order | Operand ordering | `cyclic_order` array on `ArcNode` |

These are sufficient for a conventional programming language. But Arcana's canonical
source is a **drawn magic circle**, not text. A magic circle is a planar diagram with
geometric structure that a simple directed graph cannot express. This document catalogs
those structures and proposes how each maps to language semantics.

The goal: when a user draws a magic circle, every geometric choice they make — nesting
depth, symmetry, crossings, winding, angular position, inscribed figures, boundary
annotations, tangent circles — should have deterministic semantic meaning that the
compiler can extract from topology alone.

---

## 2. What a Magic Circle Is (Topologically)

A magic circle is not a graph with visual decoration. It is a **planar diagram**
composed of:

- **Closed curves** (circles, rings) that partition the plane into bounded regions
- **Nodes** placed at specific positions within or on those regions
- **Connections** (curves between nodes) that may cross other connections
- **Inscribed figures** (polygons, stars) that create structured connectivity
- **Annotations** (text, sigils) on curves and within regions

A directed graph captures connectivity (who connects to whom). A magic circle captures
connectivity **plus** topology (how connections are embedded in the plane) **plus**
geometry (where things are positioned, what symmetries exist, how curves wind).

Each of these three layers adds information that a bare graph discards:

```
Layer 0: Connectivity    — nodes + edges (what v0 has)
Layer 1: Topology        — crossings, winding, knot type, region boundaries
Layer 2: Geometry         — symmetry, angular position, size, inscribed figures
```

---

## 3. Catalog of Geometric Properties

### 3.1 Concentric Containment (Depth)

**What it is:** Circles nested within circles. The number of rings you cross to
reach a node from the outside is its *depth*.

**How it differs from regions:** v0 regions have parent pointers forming a tree.
Concentricity adds a metric: depth is the distance from the root region. Two regions
at the same depth are *peers*, even if they have different parents.

**Semantic mapping:**
- Depth 0 (outermost ring): module-level declarations, public API
- Depth 1: function bodies, private implementation
- Depth 2+: nested scopes, closures, inner functions
- Crossing from depth N to depth N+1 requires capability/permission

**Representation:**
```
Derived property: region.depth = 0 if root, else parent.depth + 1
New validation: edges crossing multiple depth levels emit warnings
```

**What this enables:** Access control by depth. A node at depth 2 can reference
bindings at depth 0 and 1 (outward visibility), but depth 0 cannot reach into
depth 2 without an explicit export edge. Closures naturally capture across depth
boundaries — the depth delta determines the upvalue chain length.

---

### 3.2 Rotational Symmetry

**What it is:** A pattern that is invariant under rotation by 360/N degrees.
A triangle inscribed in a circle has 3-fold symmetry. A pentagram has 5-fold.

**How it differs from a graph:** A graph has no concept of symmetry. Three nodes
with identical edge structure are just three nodes. In a magic circle, three nodes
arranged at 120° intervals with identical local structure form a **symmetric group**
— the compiler can prove they are structurally identical.

**Semantic mapping:**
- N-fold symmetry = N parallel, structurally identical execution paths
- The compiler verifies structural identity: same node types, same edge patterns,
  same sub-region structure in each symmetric segment
- Breaking symmetry (e.g., one segment has an extra edge) is a type error unless
  explicitly annotated as intentional asymmetry

**Representation:**
```c
/* New region attribute */
typedef struct {
    uint8_t fold;         /* N-fold symmetry (0 = no symmetry) */
    uint8_t segments;     /* number of segments (should equal fold) */
    ArcRegionId* segment_regions;  /* one sub-region per segment */
} ArcSymmetry;
```

**Concrete example — Dining Philosophers:**
A 3-fold symmetric circle with 3 philosopher nodes and 3 fork nodes at the
intersection points between segments. The symmetry tells the compiler: "these
three segments are structurally identical, compile once and instantiate three
times." Symmetry-breaking (ordered fork pickup) becomes a visible annotation
that deforms one segment.

---

### 3.3 Crossings

**What it is:** Two edges (curves) that intersect in the plane. The crossing
point is not a node — it is a topological event where two independent data
paths meet.

**How it differs from a graph:** In a directed graph, edges either share an
endpoint or they don't. There is no concept of two edges *crossing* without
sharing a node. In a planar diagram, crossings are a first-class topological
feature.

**Semantic mapping:**
- **Synchronization point:** Two independent execution paths must both reach
  the crossing before either can proceed past it. This is a barrier/rendezvous.
- **Interaction:** Values flowing along the two edges are exchanged or compared
  at the crossing point. The crossing *is* the operation.
- **Over/under:** If crossings have over/under information (as in knot diagrams),
  the "over" path has priority — it reads first, or its value takes precedence
  in a conflict.

**Representation:**
```c
typedef struct {
    ArcEdgeId edge_a;     /* first crossing edge */
    ArcEdgeId edge_b;     /* second crossing edge */
    bool a_over_b;        /* true if edge_a passes over edge_b */
    ArcElementId source;  /* source element for debug provenance */
} ArcCrossing;

/* Added to ArcGraph */
ArcCrossing* crossings;
uint32_t crossing_count;
uint32_t crossing_cap;
```

**Compilation:** A crossing compiles to a synchronization barrier. Both edges
must have produced their values before execution continues past the crossing
point. In a concurrent context, this maps to a channel rendezvous. In a
sequential context, it constrains evaluation order.

---

### 3.4 Winding Number

**What it is:** How many times a curve wraps around a point or region. A
straight edge has winding number 0. A curve that loops once around a region
before connecting to its target has winding number 1.

**How it differs from a graph:** A graph edge connects A to B. Period. It has
no concept of *how* the edge reaches B. In a planar diagram, the path an edge
takes through the plane matters — winding around regions encodes information.

**Semantic mapping:**
- Winding number 0: direct connection (normal data flow)
- Winding number 1: one iteration (execute the enclosed region once)
- Winding number N: N iterations (execute the enclosed region N times)
- Negative winding: reverse iteration (countdown)
- Fractional winding (partial arc): partial application or lazy evaluation

**Representation:**
```c
/* Added to ArcSemEdge */
int8_t winding;  /* winding number around enclosed regions */
```

**Relationship to CYCLE:** v0's `CYCLE` node is a degenerate case of winding.
A cycle region with a `BREAK_IF` exit condition is equivalent to an edge with
unbounded winding that terminates on a predicate. Fixed winding (N iterations)
is a `CYCLE` with an implicit counter — the compiler unrolls or emits a
counted loop.

---

### 3.5 Knot Type

**What it is:** A closed curve that crosses itself in a specific pattern. The
simplest non-trivial knot is the trefoil (three crossings). Knot type is a
topological invariant — it cannot be changed without cutting the curve.

**How it differs from crossings:** A crossing is a local event (two curves
meet at a point). A knot is a *global* invariant of a single closed curve.
The same three crossings can form a trefoil or an unknot depending on the
over/under pattern. Two knots are equivalent if one can be deformed into the
other without cutting.

**Semantic mapping:**
- **Unknot (0 crossings):** Simple closed scope — a function or module boundary
- **Trefoil (3 crossings):** Three-way entangled computation — three operations
  that are mutually dependent and cannot be separated. Like a 3-way deadlock
  that is *intentional* — a fixed point of three mutually recursive definitions
- **Figure-eight (4 crossings):** Two interlocked loops — coroutine pair, or
  two mutually recursive functions

**Representation:**
```c
typedef struct {
    ArcCrossing* crossings;  /* ordered list of crossings */
    uint32_t count;
    uint8_t knot_invariant;  /* computed from crossing signs: unknot, trefoil, etc. */
} ArcKnot;
```

**Design note:** Full knot invariant computation (Jones polynomial, etc.) is
complex. For v1, support only: unknot, trefoil, figure-eight. These cover the
most common magic circle patterns. More exotic knots can be added later.

---

### 3.6 Angular Position

**What it is:** The angle at which a node sits on a ring, measured from a
reference direction (typically "up" = 0°, clockwise).

**How it differs from cyclic order:** v0's cyclic port order says "port A comes
before port B in the cycle." Angular position says "port A is at 90° and port B
is at 180°." The gap between A and B (90°) is as meaningful as their order.

**Semantic mapping:**
- **Phase encoding:** Nodes at 0°/90°/180°/270° represent four execution phases
  (init/compute/validate/output). The angular sector determines when in the
  pipeline a node executes
- **Priority:** Smaller angles = higher priority (top of circle = first to execute)
- **Equal spacing:** N nodes equally spaced at 360/N degrees = symmetric parallel
  paths (connects to §3.2)
- **Unequal spacing:** Clustered nodes = tightly coupled operations; isolated
  nodes = independent operations

**Representation:**
```c
/* Added to ArcNode (optional, 0.0 = unspecified) */
float angle;  /* angular position in degrees [0, 360) on parent ring */
```

---

### 3.7 Inscribed Figures

**What it is:** Regular geometric figures (triangle, square, pentagram, hexagram)
drawn inside a circle, connecting nodes at the vertices. The figure creates a
specific connectivity pattern that is more constrained than arbitrary edges.

**How it differs from edges:** Three edges connecting three nodes in a triangle
are just three edges. An inscribed triangle is a *named pattern* — it says "these
three nodes are connected by the triangle relationship, not by three independent
relationships." The figure type (triangle vs square vs pentagram) determines the
connectivity semantics.

**Semantic mapping:**

| Figure | Vertices | Connectivity | Semantic |
|--------|----------|-------------|----------|
| Triangle | 3 | Each pair connected | 3-way mutual dependency (join/merge) |
| Square | 4 | Adjacent pairs | 4-phase pipeline (sequential stages) |
| Pentagram | 5 | Skip-one connections | 5-element protocol with indirect coupling |
| Hexagram | 6 | Two interlocked triangles | Dual 3-way merge (compare-and-select) |

**Representation:**
```c
typedef enum {
    ARC_FIGURE_NONE,
    ARC_FIGURE_TRIANGLE,
    ARC_FIGURE_SQUARE,
    ARC_FIGURE_PENTAGRAM,
    ARC_FIGURE_HEXAGRAM,
    ARC_FIGURE_STAR_N,     /* generic N-pointed star */
} ArcFigureKind;

typedef struct {
    ArcFigureKind kind;
    ArcNodeId* vertices;   /* nodes at figure vertices */
    uint8_t vertex_count;
    ArcRegionId region;    /* enclosing circle */
} ArcInscribedFigure;
```

---

### 3.8 Boundary Annotations (Inscription Bands)

**What it is:** Text, symbols, or sigils placed along the arc of a circle's
boundary — not inside the circle, not outside, but *on the boundary itself*.

**How it differs from nodes:** A node lives inside a region and participates in
data flow. A boundary annotation decorates the region's *membrane* — it describes
properties of the boundary, not of any specific computation inside.

**Semantic mapping:**
- **Type constraints:** "Everything entering this circle must be an integer"
- **Invariants:** "The sum of values inside this circle is always positive"
- **Contracts:** "This circle promises to produce a value of type T"
- **Effect annotations:** "This circle may perform I/O" or "This circle is pure"

**Representation:**
```c
typedef struct {
    ArcRegionId region;       /* which region's boundary */
    const char* inscription;  /* text content */
    float start_angle;        /* arc start (degrees) */
    float end_angle;          /* arc end (degrees) */
    ArcElementId source;      /* debug provenance */
} ArcBoundaryAnnotation;
```

**Compilation:** Boundary annotations compile to pre/post-condition checks at
region entry and exit points. A type constraint on a function region boundary
becomes parameter type validation. An effect annotation becomes an effect tag
on the function's type signature.

---

### 3.9 Tangent and Overlapping Circles

**What it is:** Two circles can have four spatial relationships:
1. **Disjoint** — no shared boundary or interior
2. **Tangent** — touching at exactly one point
3. **Overlapping** — sharing a lens-shaped region
4. **Nested** — one entirely inside the other (v0's containment)

**How it differs from regions:** v0 regions are either nested (parent/child) or
siblings (separate children of the same parent). There is no concept of two
regions *partially* overlapping or touching at a single point.

**Semantic mapping:**
- **Disjoint:** No relationship. Separate modules with no shared interface
- **Tangent (1 contact point):** Minimal coupling — exactly one shared symbol,
  one shared type, or one shared channel between two modules
- **Overlapping (shared lens):** Shared state — both modules can read/write
  symbols defined in the overlap region. The lens region belongs to both parents
- **Nested:** Containment — inner has access to outer's scope (current behavior)

**Representation:**
```c
typedef enum {
    ARC_CIRCLE_REL_DISJOINT,
    ARC_CIRCLE_REL_TANGENT,
    ARC_CIRCLE_REL_OVERLAP,
    ARC_CIRCLE_REL_NESTED,     /* existing parent/child */
} ArcCircleRelation;

typedef struct {
    ArcRegionId region_a;
    ArcRegionId region_b;
    ArcCircleRelation relation;
    ArcRegionId shared_region;  /* for OVERLAP: the lens region */
    ArcPortId contact_port;     /* for TANGENT: the single shared port */
} ArcCircleContact;
```

---

### 3.10 Curve Morphology

**What it is:** The *shape* of a connection between two nodes. A straight line,
a flowing curve, a zigzag, a spiral — the visual form of the edge carries meaning
beyond its endpoints.

**How it differs from edges:** A graph edge is abstract — it has a source, a
target, and optionally a label. It has no shape. In a magic circle, the curve
connecting two nodes is drawn with a specific visual character.

**Semantic mapping:**
- **Straight line:** Direct, unconditional data flow (standard edge)
- **Smooth curve:** Buffered or delayed flow (the curve's arc length suggests latency)
- **Zigzag/angular:** Conditional flow (each angle is a decision point)
- **Spiral:** Iterative refinement (value is progressively transformed)
- **Branching/forking:** The curve splits — multicast (one value to many targets)
- **Tapered:** Priority decay — the value weakens along the connection

**Representation:**
```c
typedef enum {
    ARC_CURVE_DIRECT,     /* straight / default */
    ARC_CURVE_SMOOTH,     /* buffered channel */
    ARC_CURVE_ANGULAR,    /* conditional pipeline */
    ARC_CURVE_SPIRAL,     /* iterative refinement */
} ArcCurveKind;

/* Added to ArcSemEdge */
ArcCurveKind curve;
```

**Design note:** Curve morphology is the most speculative property in this catalog.
It may be better to treat curve shape as a visual hint for the IDE rather than a
semantic property. The other properties in this document have clear topological
definitions; curve shape is more aesthetic. Mark as experimental.

---

### 3.11 Size as Weight

**What it is:** Circles in a magic circle are not all the same size. A large
circle dominates; a small circle is minor. The relative size of circles encodes
importance, complexity, or resource allocation.

**How it differs from regions:** v0 regions have no notion of size. A function
region and a loop region are structurally identical. In a magic circle, a large
prominent circle might represent the main computation while small satellite
circles are helper functions.

**Semantic mapping:**
- **Stack allocation:** Larger circle = more stack space reserved
- **Priority scheduling:** Larger circle = higher priority in concurrent execution
- **Inlining hints:** Small circle = inline candidate; large circle = keep as function
- **Visual weight in IDE:** Larger circles render with more detail at far zoom

**Representation:**
```c
/* Added to ArcRegion (optional, 0.0 = unspecified) */
float radius;  /* relative size [0.0, 1.0] within parent */
```

---

### 3.12 Sigils as Type Identity

**What it is:** Each circle in a magic circle may contain a distinct visual
symbol — a spiral, crescent, sun, pentagram, hexagram, rune. The sigil is not
decoration; it identifies what *kind* of thing the circle represents.

**How it differs from node kinds:** v0 has `ArcNodeKind` (FUNC_DEF, CONST_INT,
ADD, etc.) — these are compiler-defined categories. A sigil is a *user-defined*
type identity. Two circles with different sigils have different types, even if
they have the same internal structure.

**Semantic mapping:**
- Each sigil is a type tag in the type system
- Sigil compatibility determines which connections are valid
- Sigil inheritance: a circle with sigil A inside a circle with sigil B means
  "type A is a subtype of type B" (containment = subtyping)

**Representation:**
```c
/* Added to ArcRegion */
const char* sigil;  /* type identity string, NULL = untyped */
```

**Connection to visual IDE:** The drawing editor will have a sigil palette. When
a user draws a circle and stamps a sigil into it, the compiler records that sigil
as the circle's type identity. Connections between circles with incompatible
sigils are rejected at compile time.

---

### 3.13 Boundary Ports

**What it is:** Small circles placed *on* a ring's boundary at specific angular
positions. Not inside the circle (nodes), not outside (external), not text along
the arc (annotations) — these are discrete **interface points** embedded in the
membrane itself.

**Observed in:** Zodiac circle — 12 small circles on the outer ring, each bearing
a distinct symbol (zodiac signs). These are the circle's external API: the
fixed set of typed connection points through which the outside world interacts
with the circle's interior.

**How it differs from ports:** v0 ports belong to *nodes*. A node has input and
output ports for data flow. Boundary ports belong to *regions*. They define what
can enter or leave a scope, independent of any specific node inside.

**Semantic mapping:**
- **Function signature:** Boundary ports on a function region = the function's
  parameters and return type, visible from outside
- **Module exports:** Boundary ports on a module region = the module's public API
- **Protocol channels:** In a concurrent circle, boundary ports are the channels
  through which the circle communicates with the outside
- **Cardinality matters:** 12 boundary ports means exactly 12 connection points.
  Connecting to a 13th is a compile error

**Representation:**
```c
typedef struct {
    ArcRegionId region;     /* which region's boundary */
    float angle;            /* angular position on boundary (degrees) */
    ArcPortDir dir;         /* input, output, or bidirectional */
    const char* sigil;      /* type identity of this port */
    ArcElementId source;    /* debug provenance */
} ArcBoundaryPort;

/* Added to ArcGraph */
ArcBoundaryPort* boundary_ports;
uint32_t boundary_port_count;
uint32_t boundary_port_cap;
```

**Key insight:** Boundary ports make a circle into a **component** with a fixed
interface. Two circles are composable if their boundary ports are type-compatible.
The angular position of each port determines its role in the interface — port at
0° (top) might be the primary input, port at 180° (bottom) the primary output.

---

### 3.14 Interstitial Regions

**What it is:** The spaces *between* satellite circles within a ring are not
empty. In the zodiac circle, dark crescent-shaped regions between the 8
satellites contain ornamental patterns. In a node-edge graph, the space between
nodes is meaningless void. In a magic circle, every region of the plane —
including the gaps — is potentially semantic.

**How it differs from regions:** v0 regions are explicitly declared with
parent-child relationships. Interstitial regions are *emergent* — they arise
from the spatial arrangement of other circles. When you place 8 circles around
a ring, you implicitly create 8 wedge-shaped regions between them.

**Semantic mapping:**
- **Transition logic:** The interstitial region between satellite A and
  satellite B contains the transformation applied when data flows from A to B
  along the ring (tangential flow)
- **Guard conditions:** The interstitial content acts as a gate — data only
  passes from A to B if the guard in the gap is satisfied
- **Ambient context:** Interstitial regions define the "background" computation
  that runs between active satellites — like interrupt handlers or idle loops
- **Decorative (no semantic):** Some interstitial content is purely visual.
  The compiler ignores interstitial regions unless they contain explicit nodes

**Representation:**
```c
typedef struct {
    ArcRegionId parent_ring;   /* the ring these gaps belong to */
    ArcRegionId neighbor_a;    /* satellite on one side */
    ArcRegionId neighbor_b;    /* satellite on the other side */
    ArcRegionId gap_region;    /* the interstitial region itself */
} ArcInterstitialRegion;
```

**Design note:** Interstitial regions are the most context-dependent property
in this catalog. Whether a gap is semantic or decorative depends on whether it
contains nodes. The compiler should detect nodes in interstitial positions and
compile them; empty interstices are ignored.

---

### 3.15 Multi-Cardinality Rings

**What it is:** Different concentric rings within the same circle have different
numbers of elements. The zodiac circle has 12 ports on the outer ring, 8
satellites on the middle ring, and 1 star at the center. The cardinalities
12, 8, and 1 are all semantically significant and intentionally different.

**How it differs from symmetry:** §3.2 describes N-fold symmetry within a single
ring. Multi-cardinality is about the *relationship between* rings. When an outer
ring has 12 ports and an inner ring has 8 satellites, the circle encodes a
many-to-many mapping: each satellite serves some subset of the 12 outer ports,
and the central star aggregates all 8 satellites.

**Semantic mapping:**
- **Fan-in / fan-out:** 12 outer → 8 inner = some inputs are merged (fan-in
  ratio 12:8 = 3:2). 8 inner → 1 center = full aggregation
- **Dispatch table:** 12 outer ports map to 8 handlers. The mapping pattern
  (which ports go to which satellites) encodes a dispatch or routing table
- **Dimensional reduction:** Each ring inward reduces cardinality. The circle
  compresses 12 inputs down to 1 output through progressive aggregation
- **Protocol adaptation:** Outer ring speaks a 12-element protocol; inner ring
  speaks an 8-element protocol. The ring boundary performs protocol translation

**Representation:**
```c
/* Derived from existing region structure:
   - Count boundary_ports per ring → outer cardinality
   - Count child regions per ring → satellite cardinality
   - Radial edges between rings encode the mapping */
```

**Concrete example — Zodiac circle:**
The 12 zodiac ports represent 12 input categories (types, seasons, phases).
The 8 satellites represent 8 processing elements (each with a distinct sigil).
The mapping is not 1:1 — some satellites handle multiple zodiac inputs, and the
central star synthesizes all 8 satellite outputs into a single result. The
compiler generates a dispatch table from the radial edge pattern connecting
the 12 ports to the 8 satellites.

---

## 4. Coverage Matrix

Which properties does each magic circle pattern exercise?

| # | Property | v0 | v1 Proposed | Symbol |
|---|----------|----|----|--------|
| 3.1 | Region containment (nesting) | Yes | — | `[v0]` |
| — | Directed edges (data flow) | Yes | — | `[v0]` |
| — | Cyclic port order | Yes | — | `[v0]` |
| 3.1 | Concentric depth metric | Derived | Validate | `[v1]` |
| 3.2 | Rotational symmetry | No | Yes | `[v1]` |
| 3.3 | Edge crossings | No | Yes | `[v1]` |
| 3.4 | Winding number | No | Yes | `[v1]` |
| 3.5 | Knot type | No | Yes (3 types) | `[v1]` |
| 3.6 | Angular position | No | Yes | `[v1]` |
| 3.7 | Inscribed figures | No | Yes | `[v1]` |
| 3.8 | Boundary annotations | No | Yes | `[v1]` |
| 3.9 | Tangent/overlapping circles | No | Yes | `[v1]` |
| 3.10 | Curve morphology | No | Experimental | `[exp]` |
| 3.11 | Size as weight | No | Yes | `[v1]` |
| 3.12 | Sigils as type identity | No | Yes | `[v1]` |
| 3.13 | Boundary ports | No | Yes | `[v1]` |
| 3.14 | Interstitial regions | No | Experimental | `[exp]` |
| 3.15 | Multi-cardinality rings | No | Yes | `[v1]` |

---

## 5. Implementation Priority

Ordered by semantic impact and implementation complexity:

### Tier 1 — Core topology (extend ArcGraph)

These add new topological facts to the graph that the compiler can extract
mechanically. Low risk, high value.

1. **Crossings** (§3.3) — new `ArcCrossing` array on graph; compiler emits
   sync barriers
2. **Winding** (§3.4) — `int8_t winding` on edges; compiler emits counted
   loops or delays
3. **Concentric depth** (§3.1) — derived from existing parent chain; add
   validation rules
4. **Boundary ports** (§3.13) — typed interface points on region boundaries;
   defines the circle's API and composability

### Tier 2 — Structural patterns (compiler analysis)

These require the compiler to recognize patterns in the graph and exploit them.
Medium complexity.

5. **Symmetry detection** (§3.2) — compiler identifies N-fold symmetric regions
   and verifies structural identity; enables parallel instantiation
6. **Multi-cardinality rings** (§3.15) — different N-fold counts on different
   rings; compiler generates dispatch tables and fan-in/fan-out mappings
7. **Inscribed figures** (§3.7) — named connectivity patterns; compiler matches
   against known figures and emits specialized code
8. **Tangent/overlap** (§3.9) — new circle relationship types in region model;
   compiler generates shared-state or minimal-interface code

### Tier 3 — Annotations and hints (type system and IDE)

These primarily affect the type system and visual editor, with lighter compiler
impact.

9. **Boundary annotations** (§3.8) — type constraints on region boundaries
10. **Sigils** (§3.12) — user-defined type identity on regions
11. **Angular position** (§3.6) — execution phase and priority encoding
12. **Size** (§3.11) — resource allocation hints

### Tier 4 — Experimental

13. **Curve morphology** (§3.10) — may be better as IDE-only visual hint
14. **Knot type** (§3.5) — requires knot invariant computation; defer until
    crossings are proven useful
15. **Interstitial regions** (§3.14) — context-dependent; semantic only when
    they contain explicit nodes

---

## 6. Relationship to Existing Roadmap

The ROADMAP.md "Future" section lists:

> Geometry-native semantics beyond v0 (crossings, winding, symmetry)

This document expands that line item into 12 concrete properties with proposed
representations, semantic mappings, and implementation priorities. The three
properties named in the roadmap (crossings, winding, symmetry) are all in Tier 1
or Tier 2 — they should be implemented first.

The development-direction.md Phase 4 items map to this document:

| Phase 4 Item | This Document |
|------|------|
| §4.1 Edge Crossing Semantics | §3.3 Crossings |
| §4.2 Region Boundary Crossing | §3.8 Boundary Annotations + §3.9 Tangent/Overlap |
| §4.3 Directed Cycles as Iteration | Done (CYCLE nodes) — generalized by §3.4 Winding |
| §4.4 Concentric Regions as Effect Domains | §3.1 Concentric Depth + §3.8 Boundary Annotations |

---

## 7. Design Principles

1. **Every geometric choice is semantic.** If a user can draw it, it must mean
   something. Decoration is handled by a separate visual layer in the IDE.

2. **Topology before geometry.** Topological properties (crossings, winding, knot
   type) are invariant under continuous deformation — they survive layout changes.
   Geometric properties (angles, sizes) are hints that may be adjusted by the
   layout engine.

3. **Graceful degradation.** Programs that use only v0 features (regions, edges,
   ports) continue to work unchanged. New geometric properties are additive.

4. **Deterministic extraction.** The compiler extracts geometric properties from
   the graph representation deterministically. No layout-dependent behavior.
   The canonical representation stores topology and geometry explicitly, not as
   pixel coordinates.

5. **Verify structural claims.** If a region claims N-fold symmetry, the compiler
   verifies that the N segments are structurally identical. False claims are
   compile errors, not silent miscompilation.
