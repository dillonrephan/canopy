# Theme and Projection Architecture

## Why this boundary exists

Canopy's first renderer proved the filesystem, navigation, bounded-loading, and
incremental-update foundations. It also exposed a limitation: fixed screen
positions and line-shaped branches can only produce a zoomable diagram. Sakura
Perspective should instead feel like moving through a tree, with nearby trunks
acting as broad paths and distant growth compressed by viewpoint.

That change must not tie filesystem truth to one visual metaphor. Themes may
radically alter projection and botanical form while preserving navigation,
accessibility, safety, and stable identity.

## Presentation pipeline

```text
filesystem truth
      |
semantic tree model
      |
bounded presentation policy
      |
stable world layout
      |
theme profile
  |-- projection
  |-- branch geometry
  |-- botanical forms
  |-- visual tokens
  `-- motion profile
      |
projected scene and hit geometry
      |
renderer
```

The semantic model owns what exists. The bounded presentation policy owns how
much may be considered. World layout gives each presented node stable spatial
meaning. Projection converts that meaning into viewpoint-dependent screen
geometry. The renderer paints projected output and uses matching projected hit
geometry.

## Stable world layout

World layout must not store final screen coordinates. A presented node should
instead receive stable attributes such as:

- parent connection
- deterministic sibling order
- side or lane assignment
- distance along the parent path
- depth relative to the focused directory
- semantic emphasis and parent-context state

These attributes remain stable while the camera moves and while incremental
siblings arrive. A node may move substantially on screen because projection has
changed, not because its world identity was randomized.

The current planar corridor layout remains the reference implementation during
the extraction. It should be expressible as an orthographic projection of the
same presentation data before Sakura Perspective replaces any behavior.

## Theme-owned modules

A statically registered theme profile may provide:

- palette and text contrast
- typography and label reveal thresholds
- branch surface and junction geometry
- botanical endpoint forms
- planar, perspective, or later projection behavior
- fog and atmospheric depth treatment
- animation durations and easing
- a preferred rendered-item budget at or below the core limit

Algorithmically different projections belong in separate modules. They must not
be represented as a growing set of conditionals inside one layout engine.

Dynamic plugin loading is intentionally deferred. Static registration gives the
code real theme boundaries and permits later runtime selection without adding a
plugin ABI, discovery rules, or untrusted-code concerns prematurely.

## Core-owned invariants

Themes must not change:

- filesystem scanning, identity, or metadata truth
- navigation meaning or path resolution
- error, permission, and partial-result semantics
- reduced-motion availability
- keyboard and accessibility requirements
- the global materialization ceiling
- the requirement that visible geometry and hit geometry agree
- the conventional Contents path to every entry

A theme may request fewer visible entries than the model retains. It may never
request an unbounded presentation.

## Sakura Perspective target

The first perspective prototype will retain the core 16-entry materialization
ceiling but project at most:

- six readable foreground corridors
- two distant fog hints
- one focused trunk and necessary parent context

The remaining entries stay precise and reachable through Contents. Passing a
depth threshold may replace expired projected growth with another bounded
window, but it must not accumulate hidden scene geometry.

The focused trunk should be a tapered filled surface rather than a thick line.
It begins broad near the lower viewport and narrows toward a horizon. Child
corridors join at staggered world distances. Camera travel changes their screen
position, scale, thickness, and opacity through deterministic projection.

Scrolling acts as a dolly through bounded world depth. Double-clicking open
space travels toward that depth; activating a directory still changes semantic
focus. Labels remain screen-aligned and appear as their junctions approach.

## Migration sequence

Each numbered stage should be a coherent, buildable commit:

1. **Complete:** Extract current palette, metrics, motion, and presentation values into a
   statically registered Sakura profile without changing behavior.
2. **Complete:** Introduce explicit camera/view state and stable world-layout
   output.
3. **Complete:** Reproduce the current renderer through a planar projection
   module.
4. **Complete:** Add a perspective projection beside the planar reference.
5. **Complete:** Add projected hit geometry and tests before switching
   interaction.
6. **Complete:** Render tapered trunk and corridor surfaces. Perspective output
   is viewport-space, paints far-to-near, and uses matching near-to-far hit
   geometry after a higher-priority visible-label/endpoint pass. Focus/live
   geometry switches atomically with opacity-only arrival at this stage so
   animated paint and hit endpoints cannot diverge.
7. **Complete:** Enforce a camera-ahead window of six foreground direct-child
   corridors plus two hittable fog hints. Perspective descendants are omitted,
   root/non-root scene cardinality is strictly bounded, reversal restores exact
   membership and geometry, and Contents reports every unprojected direct
   entry without changing planar/core presentation semantics.
8. Convert wheel and open-space travel from canvas scaling to camera movement.
9. Test live additions, removals, focus transitions, reduced motion, crowded
   directories, and backtracking under both projections.
10. Decide whether Sakura Perspective replaces Sakura Morning or remains a
    separate selectable profile.

The extraction stages must not mix behavior changes with module movement. The
planar reference stays available until perspective navigation meets the same
correctness and accessibility contracts.

## Validation questions

- Does viewpoint movement reveal structure rather than merely enlarge it?
- Can a user predict where a known directory will reappear when moving back?
- Are foreground corridors broad enough to read as traversable surfaces?
- Do distant hints invite travel without becoming dead hit targets?
- Does a busy directory remain bounded in model, layout, paint, and hit costs?
- Can reduced-motion users perform every navigation action immediately?
- Does Contents remain the honest route to entries outside projected growth?
