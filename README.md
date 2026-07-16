# Canopy

Canopy is a Linux desktop file manager that represents the filesystem as an
interactive botanical tree. Directories become branches, files become leaves or
other plant forms, symlinks become vines, mounted drives become neighboring
trees, and trash becomes compost.

The metaphor is meant to improve spatial understanding, not replace dependable
file-management conventions. Canopy will always retain practical navigation,
keyboard support, clear labels, and a conventional list fallback.

## Product position

Canopy is a casual, visual file manager for browsing a home directory, opening
and previewing files, everyday organization, search, favorites, recent places,
and removable drives.

It is not intended to replace tools such as Krusader for privileged operations,
advanced bulk work, synchronization, remote administration, or complex archive
management.

## Current status

The project is in Phase 1: a fake-data interaction prototype. No real filesystem
mutation will be implemented until the visual navigation model has proved useful.

The first runnable milestone is deliberately narrow:

- one desktop window
- a fake home-directory tree with several levels
- deterministic botanical branch layout
- pan and cursor-centered zoom
- hover and selection
- animated directory focus
- breadcrumbs and parent context
- a practical metadata inspector
- one crowded fake directory to expose layout problems

Garden View, real filesystem scanning, thumbnails, file operations, packaging,
and broad theme customization are outside this milestone.

## Technology direction

- Qt 6
- QML / Qt Quick for the application shell and controls
- modern C++ for models, layout, rendering support, and later filesystem work
- CMake
- a custom `QQuickItem` rendering path for the tree scene
- KDE and freedesktop integration where it materially improves behavior

See [Architecture](docs/architecture.md), [Design Principles](docs/design-principles.md),
and [Roadmap](docs/roadmap.md).

The original detailed project brief is preserved in
[`startingPrompt.md`](startingPrompt.md).

## Safety boundary

Canopy is not yet connected to real user data. Later destructive features must
default to trash, require explicit confirmation for permanent deletion, report
partial failures honestly, and never silently overwrite files.

## License

No license has been selected yet. Until one is added, all rights are reserved.
