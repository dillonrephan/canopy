You are building a Linux desktop file manager called **Canopy**.

Canopy is not a conventional file manager with a plant-themed skin. Its primary navigation model is a literal, interactive botanical visualization of the filesystem.

The filesystem is represented as a living directory tree:

* directories are branches
* subdirectories are branch forks
* files are leaves, flowers, fruit, seed pods, mushrooms, or other botanical forms
* mounted drives are neighboring trees or grafted branches
* symlinks are vines
* hidden files are visually subdued growth, moss, roots, or shaded leaves
* inaccessible paths may use thorns or other restrained warning indicators
* deleted files go to a compost-themed trash destination

The application must remain a real, dependable file manager. The visual metaphor must improve navigation and comprehension rather than interfere with basic file operations.

## Product role

Canopy is the user’s casual, visual file manager.

It is not intended to replace advanced tools such as Krusader for:

* root operations
* complicated bulk file work
* dual-pane workflows
* remote administration
* synchronization
* advanced archive management

Canopy should excel at:

* browsing the home directory
* visually understanding directory structure
* opening files
* previewing files
* copying and moving files
* renaming files
* creating folders
* sending files to trash
* restoring files where practical
* searching
* navigating favorites and recent locations
* browsing removable drives
* casual drag-and-drop workflows

## High-level design

The main view is a spatial directory tree centered on a selected directory.

The default starting location should be the user’s home directory, not `/`.

At a high zoom level, a user sees major branches such as:

* Documents
* Downloads
* Pictures
* Music
* Videos
* Projects

Selecting a branch should smoothly focus and zoom into that directory.

The parent directories should remain visually represented enough to preserve context and allow backward navigation.

A conventional breadcrumb path must always be available as a fallback.

Example:

```text
/home/user/Projects/Canopy/src
```

The visual tree should represent that same hierarchy spatially.

## Critical product principle

Do not begin by implementing every possible file operation.

The first milestone is a polished, convincing interface prototype using fake filesystem data.

The visual concept must prove itself before deep filesystem integration begins.

Do not accidentally spend weeks building conflict dialogs, permissions handling, archive support, or remote filesystems before the core interaction feels good.

## Recommended technology

Use:

* Qt 6
* QML / Qt Quick for the interface
* C++ for the application and filesystem backend
* CMake
* Qt model/view architecture
* Linux freedesktop standards
* KIO where it materially improves file operations or desktop integration

Prefer KDE/Qt-native components when practical, but do not make the interface visually resemble Dolphin or standard KDE applications.

Do not use:

* Electron
* web technologies wrapped as a desktop app
* GTK/libadwaita
* a naive recursive pile of QML Repeaters for the complete filesystem tree
* direct arbitrary filesystem mutation from QML

Filesystem operations must be implemented in a backend layer.

QML should request actions and display state. It should not contain critical filesystem logic.

## Architectural requirements

Use a clean separation between:

1. filesystem data
2. directory scanning
3. tree layout
4. rendering
5. file operations
6. application state
7. theme data
8. UI controls

A suggested initial structure:

```text
canopy/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── docs/
│   ├── architecture.md
│   ├── design-principles.md
│   └── roadmap.md
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── ApplicationController.cpp
│   │   ├── ApplicationController.h
│   │   ├── NavigationController.cpp
│   │   └── NavigationController.h
│   ├── filesystem/
│   │   ├── FileNode.h
│   │   ├── DirectoryModel.cpp
│   │   ├── DirectoryModel.h
│   │   ├── DirectoryScanner.cpp
│   │   ├── DirectoryScanner.h
│   │   ├── FileOperations.cpp
│   │   ├── FileOperations.h
│   │   ├── MimeClassifier.cpp
│   │   └── MimeClassifier.h
│   ├── layout/
│   │   ├── TreeLayoutEngine.cpp
│   │   ├── TreeLayoutEngine.h
│   │   ├── SpatialIndex.cpp
│   │   └── SpatialIndex.h
│   ├── rendering/
│   │   ├── TreeSceneItem.cpp
│   │   ├── TreeSceneItem.h
│   │   ├── TreeGeometry.cpp
│   │   └── TreeGeometry.h
│   ├── previews/
│   │   ├── ThumbnailProvider.cpp
│   │   ├── ThumbnailProvider.h
│   │   ├── PreviewController.cpp
│   │   └── PreviewController.h
│   └── themes/
│       ├── ThemeManager.cpp
│       ├── ThemeManager.h
│       └── ThemeDefinition.h
├── qml/
│   ├── Main.qml
│   ├── views/
│   │   ├── TreeView.qml
│   │   ├── GardenView.qml
│   │   └── ConventionalListView.qml
│   ├── components/
│   │   ├── BreadcrumbBar.qml
│   │   ├── InspectorPanel.qml
│   │   ├── SearchBar.qml
│   │   ├── FilePreview.qml
│   │   ├── ContextMenu.qml
│   │   ├── OperationToast.qml
│   │   └── NavigationControls.qml
│   ├── dialogs/
│   │   ├── RenameDialog.qml
│   │   ├── ConflictDialog.qml
│   │   └── ErrorDialog.qml
│   └── animations/
│       ├── GrowthAnimation.qml
│       ├── FocusTransition.qml
│       └── ReducedMotion.qml
├── assets/
│   ├── icons/
│   ├── illustrations/
│   └── themes/
├── tests/
│   ├── unit/
│   ├── integration/
│   └── fixtures/
└── packaging/
    ├── linux/
    └── flatpak/
```

This structure is guidance, not an immutable requirement. Improve it where necessary, but preserve strong subsystem boundaries.

## Core data model

Represent filesystem entries independently from their visual form.

A possible starting point:

```cpp
using NodeId = quint64;

enum class NodeType {
    Directory,
    File,
    Symlink,
    MountPoint,
    Unknown
};

struct FileNode {
    NodeId id;
    NodeId parentId;

    QUrl url;
    QString name;
    QString displayName;
    QString mimeType;

    NodeType type;

    quint64 sizeBytes;
    QDateTime modifiedTime;

    bool hidden;
    bool readable;
    bool writable;
    bool executable;
    bool expanded;
    bool loaded;
    bool loading;
    bool hasChildren;

    QVector<NodeId> children;
};
```

Do not bind visual theme concepts directly into the filesystem model.

For example, `FileNode` should not contain “pink petal,” “pine needle,” or “mushroom.”

The visual layer or classifier should translate semantic file information into themed visual forms.

## File classification

Create a semantic classification layer that maps files to general visual categories.

Suggested categories:

* directory
* image
* audio
* video
* document
* code
* archive
* executable
* shortcut or symlink
* unknown
* hidden
* inaccessible

Themes may render those categories differently.

For example:

* image files may appear as flowers
* audio files may appear as berries
* videos may appear as fruit
* documents may appear as broad leaves
* code may appear as fern fronds
* archives may appear as seed pods
* executables may appear as buds
* unknown files may appear as mushrooms or stones

The mapping must be theme-driven, not hardcoded throughout the application.

## Visual scaling and semantic compression

A literal full rendering of every filesystem object will become unusable in large directories.

The design must support level of detail.

At distant zoom levels:

* display directory canopies
* aggregate file groups into clusters
* use branch thickness to imply total size
* use foliage density to imply item count
* show only major directories
* suppress individual files

At medium zoom:

* display important subdirectories
* display grouped file clusters
* show selected or recently interacted-with files

At close zoom:

* display individual files
* display filenames
* display thumbnails
* allow direct manipulation

Do not attempt to render tens of thousands of independent QML objects.

Use an efficient custom rendering path such as:

* a custom `QQuickItem`
* scene graph nodes
* batched geometry
* a spatial index for hit testing
* cached layout results
* viewport culling
* asynchronous layout calculations where appropriate

A naive recursive `Repeater` implementation is acceptable only for an early fake-data prototype and must not become the long-term renderer.

## Layout engine

The tree layout engine should:

* accept a focused root node
* produce branch geometry
* preserve parent context
* support expanded and collapsed directories
* calculate visible nodes based on zoom
* provide stable positions where possible
* avoid dramatic random repositioning after scans
* support animation between layout states
* expose hit-testable bounds for directories and files

The layout should feel organic but remain predictable.

Do not make it physically accurate at the cost of usability.

Directories with many children must not become visually explosive.

Use controlled branching angles, compression, aggregation, and focus-based emphasis.

## Main views

### Tree View

This is the primary filesystem experience.

Requirements:

* spatial tree representation
* pan and zoom
* click to select
* double-click or Enter to open
* smooth focus transition into directories
* visible parent context
* keyboard navigation
* context menu
* drag-and-drop
* selection state
* search highlighting
* reduced-motion mode

### Garden View

This is a friendly landing screen.

It should display common places as individual plants, pots, plots, or small trees.

Suggested entries:

* Home
* Documents
* Downloads
* Pictures
* Music
* Videos
* Projects
* Recent
* Favorites
* Trash
* Removable drives

This view should be customizable later.

### Conventional List View

Canopy must provide a practical escape hatch.

A selected directory can be displayed in a standard list or table containing:

* filename
* type
* size
* modified date
* permissions where relevant

This view may appear:

* in a side inspector
* as a toggleable full view
* as a bottom drawer
* as a compact panel

Do not force the user to perform precise administrative tasks entirely through botanical objects.

## Navigation

Support:

* back
* forward
* up
* home
* breadcrumbs
* direct path entry
* keyboard focus
* history
* favorites
* recent locations

Suggested shortcuts:

```text
Alt+Left        Back
Alt+Right       Forward
Alt+Up          Parent directory
Ctrl+L          Edit location
Ctrl+F          Search
Ctrl+H          Toggle hidden files
Ctrl+N          New folder
F2              Rename
Delete          Move to trash
Shift+Delete    Permanent delete, with explicit confirmation
Ctrl+C          Copy
Ctrl+X          Cut
Ctrl+V          Paste
Space           Preview or open inspector
Enter           Open selected item
Escape          Cancel current action or clear selection
```

Do not invent unusual shortcut behavior where desktop conventions already exist.

## Initial file operations

Implement these before advanced features:

1. open file with default application
2. enter directory
3. back, forward, and parent navigation
4. create folder
5. rename
6. copy
7. cut
8. paste
9. move
10. move to trash
11. permanent delete with confirmation
12. drag-and-drop
13. reveal basic properties
14. open terminal here, if easy to support cleanly
15. removable drive browsing

Operations must include:

* errors
* cancellation where practical
* progress reporting
* name conflicts
* permission failures
* unavailable destinations
* partial-failure reporting

Do not silently discard failures.

## Safety requirements

This is a file manager. Treat destructive operations as high risk.

Rules:

* default deletion means move to trash
* permanent deletion requires explicit confirmation
* never overwrite silently
* show clear conflict options
* do not follow dangerous symlink chains during recursive operations
* preserve file metadata where practical
* handle partial copy and move failures honestly
* do not claim success until the operation has completed
* do not freeze the interface during large operations
* do not perform privileged root operations in the initial version
* do not embed a general-purpose privileged helper in the MVP

Maintain a clear operation queue.

Each operation should expose:

```text
pending
running
paused
completed
failed
cancelled
partially completed
```

## Search

Initial search should support:

* current directory
* recursive search below current directory
* filename matching
* basic MIME or file-type filtering

Search results in Tree View should:

* bloom or glow subtly
* dim unrelated branches
* preserve structural context where possible
* allow jumping directly to a result

Do not make search depend on a heavy permanent indexing daemon in the first version.

An optional indexing backend may be added later.

## Previews and thumbnails

Support initial previews for:

* images
* plain text
* common documents where thumbnailers exist
* audio metadata
* video thumbnails

Use freedesktop thumbnail standards or KDE thumbnail infrastructure where practical.

Do not decode huge media files synchronously on the UI thread.

Use caching.

Provide placeholder visuals when no thumbnail exists.

## Accessibility

Accessibility is not optional.

Support:

* full keyboard navigation
* visible focus indicators
* screen-reader labels for interactive items
* text alternatives for purely visual file forms
* high-contrast mode
* scalable UI
* reduced motion
* disable decorative animation
* conventional list fallback
* color-independent state indicators

Do not use color alone to communicate file type, selection, errors, or permissions.

## Animation principles

Animations should be quick, tactile, and informative.

Potential interactions:

* opening a directory grows or reveals a branch
* creating a file produces a bud
* copying produces a second growth at the destination
* moving retracts and regrows an object
* deleting detaches an object and moves it toward compost
* loading sends a subtle pulse along a branch
* search results bloom
* drag targets swell slightly
* branches bend subtly during drag hover

Constraints:

* avoid long decorative transitions
* avoid blocking input during animation
* target roughly 120–250 ms for routine interactions
* allow longer scene transitions only when they remain interruptible
* provide reduced-motion behavior
* never make routine navigation feel slower for the sake of animation

## Theme system

Themes are a later feature, but the architecture must anticipate them from the beginning.

Do not hardcode visual assumptions specific to one type of plant.

The default MVP theme can be neutral and botanical.

Future themes may include:

### Sakura

* soft anime-inspired cherry blossom aesthetic
* rounded branch forms
* pale pink, cream, lavender, and warm brown palette
* petals, blossoms, ribbons, and soft ambient motion
* rounded typography and gentle transitions

### Yosemite

* evergreen and mountain-forest aesthetic
* sharper branch angles
* pine needles and cones
* deep greens, slate, bark brown, and cold sky tones
* firmer movement and less softness
* stronger geometric structure

These themes should alter:

* palette
* branch shape
* leaf and file glyphs
* animation characteristics
* background
* particle effects
* typography choices where appropriate
* spacing and corner geometry
* decorative elements

Themes must not alter:

* file operation semantics
* keyboard shortcuts
* accessibility behavior
* core navigation
* safety rules

A possible theme schema:

```json
{
  "id": "default",
  "name": "Default",
  "palette": {
    "background": "#...",
    "surface": "#...",
    "branch": "#...",
    "selection": "#...",
    "warning": "#...",
    "text": "#..."
  },
  "geometry": {
    "branchStyle": "soft",
    "branchAngleBias": 0.5,
    "cornerRadius": 18,
    "leafRoundness": 0.8
  },
  "motion": {
    "navigationDurationMs": 180,
    "growthDurationMs": 220,
    "springStrength": 0.4,
    "ambientMotion": true
  },
  "forms": {
    "directory": "branch",
    "image": "flower",
    "audio": "berry",
    "video": "fruit",
    "document": "leaf",
    "code": "fern",
    "archive": "seedPod",
    "executable": "bud",
    "unknown": "mushroom"
  }
}
```

This is illustrative. Design a schema that is maintainable and versionable.

## MVP phases

### Phase 0: repository inspection

Before writing code:

* inspect the current repository
* identify the current build system
* identify any existing implementation
* summarize what exists
* document major architectural issues
* do not delete working code without reason
* preserve useful work
* create a clear implementation plan

### Phase 1: fake-data visual prototype

Build:

* application window
* Garden View
* Tree View with fake filesystem data
* zoom and pan
* branch focus transitions
* selection
* hover
* inspector panel
* breadcrumbs
* reduced-motion toggle
* placeholder theme system

No real file mutation yet.

The purpose of this phase is to prove:

* the tree is readable
* navigation makes sense
* the interface feels good
* large branch groups can compress
* the visual metaphor is not annoying

### Phase 2: read-only real filesystem

Add:

* home directory scanning
* asynchronous directory loading
* real MIME types
* hidden-file toggle
* file opening
* directory navigation
* thumbnails
* favorites
* recent places
* removable drive detection
* error states
* symlink representation

Still no destructive file operations.

### Phase 3: safe file operations

Add:

* create folder
* rename
* copy
* move
* trash
* conflict handling
* progress
* cancellation
* operation history
* drag-and-drop

### Phase 4: polish and performance

Add:

* semantic compression
* viewport culling
* batched rendering
* spatial hit testing
* large-directory stress tests
* animation tuning
* keyboard navigation
* accessibility auditing
* crash recovery
* persistent navigation state

### Phase 5: packaging

Add:

* `.desktop` entry
* application icon
* MIME and folder handling integration
* Flatpak manifest
* distro-independent documentation
* release build instructions
* diagnostics and logging
* uninstall instructions

## Performance targets

The application should:

* launch quickly
* show the initial shell before deep scanning completes
* never synchronously recurse through the entire home directory at startup
* scan directories incrementally
* avoid blocking the UI thread
* remain usable in directories with thousands of entries
* avoid one QML object per filesystem entry at large scales
* cancel obsolete scans when navigation changes
* cache thumbnails
* handle unavailable removable drives gracefully

Create stress-test fixtures for:

* 10 files
* 100 files
* 1,000 files
* 10,000 files
* deeply nested directories
* very wide directories
* symlink loops
* unreadable directories
* disappearing removable media
* duplicate filenames
* long filenames
* unusual Unicode filenames

## Testing requirements

Use automated tests for:

* path normalization
* MIME classification
* node insertion and removal
* directory scan cancellation
* symlink handling
* copy and move conflict resolution
* trash behavior
* operation-state transitions
* navigation history
* theme loading
* invalid theme fallback
* large-directory aggregation
* hidden-file filtering

Use temporary directories for filesystem integration tests.

Never test destructive operations against real user data.

## Error handling

Errors must be visible, useful, and non-catastrophic.

Examples:

* permission denied
* file disappeared during operation
* destination unavailable
* removable drive disconnected
* filename conflict
* invalid path
* unsupported file
* thumbnail failure
* partial move
* insufficient disk space
* read-only filesystem

Provide:

* concise user-facing error
* detailed log entry
* retry where sensible
* cancel
* reveal affected path
* no silent data loss

## Logging

Implement structured logging.

Suggested levels:

* debug
* info
* warning
* error
* critical

Do not log private file contents.

Avoid logging full user paths by default in normal release mode unless necessary for diagnostics.

Provide a diagnostic mode.

## Coding standards

* use modern C++
* use RAII
* avoid raw owning pointers
* use clear ownership semantics
* avoid giant controller classes
* keep UI logic out of the filesystem backend
* keep filesystem mutation out of QML
* document public interfaces
* write small focused commits
* preserve buildability throughout development
* prefer readable code over clever code
* do not introduce dependencies without documenting why
* do not use global mutable state
* do not hide errors behind catch-all exception handling
* do not leave major TODOs without recording them in the roadmap

## Working process

You have direct access to the repository and filesystem.

Follow this workflow:

1. inspect the repository
2. read all existing project documentation
3. identify the current state
4. build the current project before modifying it
5. record current build failures
6. create or update `docs/architecture.md`
7. create or update `docs/roadmap.md`
8. propose the smallest coherent milestone
9. implement that milestone
10. build
11. run tests
12. fix regressions
13. summarize exactly what changed

Do not rewrite the entire project merely because you prefer another structure.

Do not delete files without first understanding their purpose.

Do not modify unrelated system files.

Do not run privileged commands unless absolutely required, and explain why before doing so.

Do not install large system-wide dependency sets blindly.

Prefer project-local or clearly documented dependencies.

## First task

Begin by inspecting the repository.

Then produce:

1. a concise repository assessment
2. a proposed architecture based on what already exists
3. a concrete Phase 1 implementation plan
4. a list of files you intend to create or modify
5. any technical risks you see
6. the smallest runnable prototype milestone

After that, begin implementing the Phase 1 fake-data visual prototype unless the repository is missing critical prerequisites.

The first prototype should prioritize:

* a visually convincing tree
* smooth focus navigation
* readable directory hierarchy
* pan and zoom
* selection and hover
* a practical inspector panel
* fake data large enough to expose layout problems
* no destructive filesystem operations

The result should already feel like the beginning of Canopy, not a generic Qt demo with green rectangles.
