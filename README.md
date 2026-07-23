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
- live create, rename, and delete updates for active/inspected directories
- one-directory-at-a-time navigation with no recursive crawl
- editable direct paths, including explicitly entered hidden directories
- a shared hidden-entry toggle for tree discovery and Contents pages
- directory activation and default-application file opening
- 16-entry tree materialization and 64-entry paged Contents
- explicit loading, partial, inaccessible, and error states
- extension-based MIME classification without reading file contents
- deterministic 2.5D branch corridors with stable incremental placement
- depth-faded busy-directory growth with an explicit Contents handoff
- grow/fade transitions for live filesystem changes and directory focus
- bounded perspective depth travel by wheel, touchpad, keyboard, controls, and
  open-space double-click
- planar pan and cursor-centered zoom retained as the rollback interaction
- hover and selection
- animated directory focus
- breadcrumbs and parent context
- a practical metadata inspector
- deterministic fake fixtures retained for layout and stress tests

Thumbnails, filesystem mutation, Garden View, packaging, and broad theme
customization remain outside this checkpoint. Contents pages are cursor-based
and memory-bounded, though each alphabetically sorted page currently requires a
fresh background enumeration.

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

When returning after a break, start with the project
[`CHECKLIST.md`](CHECKLIST.md) to verify Git state, environment health, the
active architectural checkpoint, and the next bounded task.

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

Pass an optional directory to open it directly:

```bash
toolbox run --container canopy-dev ./build-toolbox/canopy ~/.config
```

Capture a frame on an isolated virtual KWin display without opening or focusing
a window on the real desktop:

```bash
./scripts/capture-virtual.sh /tmp/canopy.png ~/.config
```

The output path and directory are optional. The host-side capture harness uses
`kwin_wayland`, `spectacle`, and `dbus-run-session`; it runs the existing host-
visible build inside a private D-Bus and Wayland session, then tears the session
down automatically. Override the virtual display with `CANOPY_CAPTURE_WIDTH`,
`CANOPY_CAPTURE_HEIGHT`, or `CANOPY_CAPTURE_DELAY` when needed.

Static-check the QML:

```bash
toolbox run --container canopy-dev \
    cmake --build build-toolbox --target canopy_qmllint
```

## Safety boundary

Canopy reads real directory metadata but has no code path for changing files.
Opening a file delegates it to the system's default application, which may offer
its own editing behavior. Later destructive features inside Canopy must default
to trash, require explicit confirmation for permanent deletion, report partial
failures honestly, and never silently overwrite files.

## License

Canopy is free software licensed under the
[GNU General Public License v3.0](LICENSE).
Copyright © 2026 iceBurgThic.
