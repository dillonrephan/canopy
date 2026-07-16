# Roadmap

The roadmap is gated by demonstrated usability. Later phases do not begin merely
because earlier code exists.

## Phase 1A — Smallest runnable prototype

Goal: prove that one fake directory tree is readable and pleasant to navigate.

Deliverables:

- CMake-based Qt 6 application shell
- deterministic fake home-directory model with 3–4 levels
- one intentionally wide directory
- custom tree scene with intentional botanical geometry
- pan and cursor-centered zoom
- hover and single selection
- click-to-focus directory transition
- visible parent context
- breadcrumbs
- inspector with fake metadata
- reduced-motion toggle
- unit tests for deterministic model and basic layout invariants

Explicitly excluded:

- real filesystem access
- Garden View
- file mutation
- thumbnails and previews
- drag and drop
- search
- broad theme selection
- packaging

Exit criteria:

- builds and launches on the development workstation
- hierarchy remains understandable at three tested zoom levels
- navigating away and back preserves stable branch placement
- the wide fixture compresses without severe overlap
- all primary interactions work with reduced motion enabled
- no critical Qt warnings during a normal smoke test

## Phase 1B — Interaction validation

Goal: make the visual concept robust enough to justify real filesystem work.

- expand fake fixtures to deep, wide, sparse, crowded, and Unicode-heavy cases
- refine semantic zoom and aggregation
- add keyboard navigation and visible focus
- add navigation history
- add search highlighting using fake data
- add the Garden View only if Tree View navigation is already coherent
- test label strategy and crowded hit targets
- conduct an accessibility pass
- record interaction findings and rejected approaches

Gate: do not proceed until the visual tree is useful rather than merely novel.

## Phase 2 — Read-only filesystem

- asynchronous home-directory scanning
- incremental loading and cancellation
- MIME classification
- hidden-file toggle
- directory navigation and default-application opening
- symlink and inaccessible-path states
- thumbnails and previews
- favorites, recent places, and removable drives
- structured logging and visible errors

No destructive operations.

## Phase 3 — Safe file operations

- operation queue and explicit state machine
- create folder and rename
- copy, move, cut, and paste
- trash and carefully confirmed permanent deletion
- conflict handling
- progress, cancellation, retry, and partial-failure reporting
- drag and drop
- temporary-directory integration tests

## Phase 4 — Performance and polish

- viewport culling and batched geometry
- spatial indexing
- asynchronous layout where measurements justify it
- large-directory stress fixtures up to 10,000 entries
- thumbnail caching
- animation tuning
- crash recovery and persistent navigation state
- comprehensive keyboard and accessibility audit

## Phase 5 — Desktop integration and packaging

- application identity and icon
- `.desktop` entry
- folder and MIME integration where appropriate
- Flatpak manifest
- release builds and distro-independent documentation
- diagnostic mode, uninstall instructions, and release checklist

## Immediate implementation sequence

1. Confirm Qt 6 development prerequisites without installing broad dependency
   sets blindly.
2. Add the minimal CMake application and QML shell.
3. Add semantic fake-node types and deterministic fixtures.
4. Implement and test a basic stable layout.
5. Render the first botanical scene.
6. Add pan, cursor-centered zoom, hover, and selection.
7. Add focus navigation, breadcrumbs, and inspector.
8. Add reduced-motion behavior.
9. Run the crowded-fixture smoke test and document observed layout failures.

Each step should leave the project buildable and should be committed separately
when it represents a coherent change.

## Known risks

- wide directories may overwhelm literal branching
- unstable organic layouts may destroy spatial memory
- decorative file forms may reduce precise identification
- labels may dominate or clutter the scene at medium zoom
- focus animations may feel slower than conventional navigation
- a custom scene-graph renderer raises the initial technical cost
- Qt/KDE dependency choices can complicate packaging if introduced too early
- visual polish may expand scope without improving navigation

These risks should be tested with fake data before real filesystem integration.
