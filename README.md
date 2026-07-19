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

The project has entered Phase 2 with a deliberately narrow read-only filesystem
slice:

- one desktop window
- asynchronous, cancellable scanning with incremental branch growth
- one-directory-at-a-time navigation with no recursive crawl
- 16-entry tree materialization and 64-entry paged Contents
- explicit loading, partial, inaccessible, and error states
- extension-based MIME classification without reading file contents
- deterministic botanical branch layout
- pan and cursor-centered zoom
- hover and selection
- animated directory focus
- breadcrumbs and parent context
- a practical metadata inspector
- deterministic fake fixtures retained for layout and stress tests

Hidden files, thumbnails, opening files, filesystem mutation, Garden View,
packaging, and broad theme customization remain outside this checkpoint.
Contents pages are cursor-based and memory-bounded, though each alphabetically
sorted page currently requires a fresh background enumeration.

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

## Development environment

Development uses a Fedora Toolbx named `canopy-dev` so Qt and compiler packages
remain isolated from the host workstation. The source tree stays on the host and
is automatically visible inside Toolbx.

Create or update the container, build the project, and run its tests:

```bash
./scripts/bootstrap-test-env.sh
```

The script is safe to run again. It reads Fedora packages from
[`requirements-dev.txt`](requirements-dev.txt), reuses the `canopy-dev`
container when it exists, and keeps build output in `build-toolbox/`. Set
`CANOPY_TOOLBOX_NAME` or `CANOPY_BUILD_DIR` to override those defaults.

After the environment is provisioned, run the complete configure, build, QML
lint, and unit-test loop without reinstalling packages:

```bash
./scripts/check.sh
```

To configure and build manually after provisioning:

```bash
toolbox run --container canopy-dev \
    cmake -S . -B build-toolbox -G Ninja -DCMAKE_BUILD_TYPE=Debug
toolbox run --container canopy-dev cmake --build build-toolbox
```

Run from a graphical desktop session:

```bash
toolbox run --container canopy-dev ./build-toolbox/canopy
```

Static-check the QML:

```bash
toolbox run --container canopy-dev \
    cmake --build build-toolbox --target canopy_qmllint
```

## Safety boundary

Canopy reads real directory metadata but has no code path for changing files.
Later destructive features must default to trash, require explicit confirmation
for permanent deletion, report partial failures honestly, and never silently
overwrite files.

## License

No license has been selected yet. Until one is added, all rights are reserved.
