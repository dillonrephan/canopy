# Architecture

## Goals

Canopy needs to preserve a strict boundary between filesystem truth and its
botanical presentation. The visual metaphor may change radically between themes;
file semantics and operation safety must not.

The architecture should also let the Phase 1 prototype remain small without
creating a renderer or data model that must immediately be discarded.

The perspective-theme extraction and the boundary between stable world layout
and viewpoint projection are specified in
[Theme and Projection Architecture](theme-architecture.md).

## Initial subsystem boundaries

### Application shell

QML owns window composition and ordinary interface controls:

- navigation controls
- editable path/breadcrumb navigation
- inspector
- status and error presentation
- reduced-motion preference
- view composition

It does not perform filesystem mutation or own critical operation logic.

### Semantic node model

C++ model types represent entries independently of their appearance.
`FilesystemTreeModel` owns the production node graph, scan generations, watched
directories, and the paged Contents model. It is populated from bounded
asynchronous scans and emits one observable change boundary for both the
renderer and inspector. Tree changes identify their reason and the added and
removed node IDs so presentation layers can animate the delta without inferring
filesystem semantics. Deterministic fake data remains available only to layout
and stress tests.

Initial node information should include a stable ID, parent ID, display name,
semantic type, absolute path, metadata, child IDs, and expanded/selected state.
Botanical forms and colors do not belong in this model.

Directory nodes also keep total entry count separate from materialized child
IDs and report unloaded, ready, loading, partial, inaccessible, or error state.
Extreme counts never require an extreme in-memory child vector.

The model watches the active tree directory and the directory currently shown
in Contents. Watch notifications are debounced and trigger fresh cancellable
scans; external create, rename, and delete operations therefore update both
presentations without teaching the renderer filesystem semantics.

### Filesystem scanning

The read-only scanner enumerates one requested directory on a worker thread. It
makes a directory-first pass, emits retained tree entries in four-entry growth
batches, checks for cancellation between entries, and never recursively follows
the directory tree. The semantic tree retains at most 16 entries for a scanned
directory. A generation controller ensures that only the newest navigation
request can update the model.

The conventional Contents model holds one 64-entry page. Page cursors are based
on the last deterministic ordering key rather than an accumulating row offset,
so later pages do not require earlier pages to remain in memory. Each page query
currently enumerates the directory on a worker thread to find the next bounded
sorted page; a future indexed backend may optimize repeated enumeration without
changing the model boundary.

Hidden entries are excluded by default. Symbolic links are identified
literally and are never followed automatically by the scanner. The visibility
option is shared by incremental tree scans, Contents pages, and live refreshes.
An explicitly entered hidden directory remains resolvable even while discovery
of hidden entries is disabled.

### Layout engine

A deterministic C++ layout engine converts a focused semantic tree into scene
geometry. It must:

- preserve stable positions when state has not changed
- keep enough parent context for backward navigation
- represent directories as connected cubic branch paths, not folder-shaped nodes
- provide thinner attachment paths for files and other non-directory entries
- constrain branching angles and spacing
- compress or aggregate wide groups
- expose bounds for hit testing
- support interpolation between focus states

Layout output is plain scene data. It does not draw and does not access the
filesystem. Direct children occupy deterministic staggered corridor slots.
Existing IDs retain their assigned slot while incremental results arrive, the
eight front slots fill before any rear slot, and rear slots carry progressively
lower depth opacity.

The layout applies a hard visual-child budget before generating geometry. See
[Bounded Directory Presentation](bounded-directories.md) for the current
prototype contract and eventual incremental-list requirements.

### Tree renderer

The tree scene is a custom painted `QQuickItem`. It reads semantic nodes from the
shared model and reacts to its change signal, but does not own scanners,
watchers, node mutation, or Contents paging.

The renderer owns:

- directory limbs, file attachment stems, and botanical endpoint geometry
- labels appropriate to the current zoom level
- viewport culling
- whole-branch hover and selection hit testing
- scene-to-view coordinate conversion
- short-lived arrival and departure snapshots for typed model deltas

Focus changes advance the selected corridor while obsolete siblings recede and
newly revealed children grow from their attachment points. Live create, rename,
and delete changes use the same grow/fade vocabulary. Completed fades are
discarded and cannot participate in hit testing; reduced-motion mode skips all
transient geometry. Zoom progressively restores rear-corridor opacity, while a
blank-space double-click recenters and advances the camera into that region.
This reveals already materialized geometry without weakening the hard
filesystem presentation bound.

QML overlays remain appropriate for conventional controls and the inspector.

### Navigation state

A small controller owns the focused node, back/forward history, parent
navigation, current selection, pan/zoom state, and reduced-motion behavior. It
coordinates model, layout, and UI without absorbing their responsibilities.

Direct paths resolve to semantic directory nodes before focus changes, including
nodes outside the currently materialized sample. Directory rows and branches
use that same path. File activation bypasses the directory model and delegates a
local-file URL to the desktop's default application; Canopy itself performs no
file mutation.

### Theme data

The application uses a versioned set of tokens for palette, geometry, semantic
forms, typography, and motion timing. The first theme is Sakura Morning. C++
scene rendering and the QML shell consume the same token source so colors and
geometry do not drift between layers.

The theme interface must not change navigation semantics or accessibility.

## Intended source shape

The exact file list should grow only when working code needs it. The initial
shape is expected to resemble:

```text
canopy/
├── CMakeLists.txt
├── README.md
├── docs/
│   ├── architecture.md
│   ├── design-principles.md
│   └── roadmap.md
├── src/
│   ├── main.cpp
│   ├── filesystem/
│   ├── model/
│   ├── layout/
│   ├── rendering/
│   └── navigation/
├── qml/
│   ├── Main.qml
│   └── components/
├── assets/
│   └── themes/
└── tests/
    └── unit/
```

Avoid creating empty subsystem scaffolding before it has a concrete use.

## Data flow

```text
filesystem watcher -> shared semantic model -> navigation -> layout -> renderer
        |                     |                                     |
incremental scanner ----------+-> paged contents -> QML inspector <-+
                                                       |
                                                 user interaction
```

File operations will become a separate queued backend; they never pass through
the scanner or renderer.

## Read-only checkpoint constraints

- No filesystem mutation.
- No recursive scan.
- No dependency on KIO.
- No one-QML-object-per-file recursive tree.
- No randomized layout between runs or navigation states.
- No theme-specific concepts in semantic model types.
- No UI-thread work that scales with an unbounded filesystem.

## Early technical decisions to validate

The first code spike must answer these questions before the renderer expands:

1. Whether `QQuickItem::updatePaintNode()` provides the best initial path or a
   simpler Qt Quick-supported custom drawing approach is sufficient for Phase 1.
2. Whether text labels should be scene-graph nodes or a culled QML overlay.
3. What stable branch layout remains readable for both deep and wide fake trees.
4. Which camera transform produces natural cursor-centered zoom and interruptible
   focus transitions.

These are prototype decisions, not reasons to delay the runnable shell.
