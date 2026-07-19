# Bounded Directory Presentation

A directory with an extreme number of entries is valid input, not an error.
Canopy must remain responsive without pretending that every entry can be useful
as an individual botanical form.

## Current read-only contract

- Semantic directory nodes keep total child count separate from materialized
  child IDs.
- Directory state is explicit: ready, loading, partial, inaccessible, or error.
- Tree layout considers at most 16 direct children of the focused directory.
- The first eight children occupy full-opacity front corridors. Additional
  materialized children use progressively faded rear corridors, and faint rear
  labels are suppressed unless selected or hovered.
- Zoom progressively clears rear-corridor fading and makes eligible labels
  available again; faded growth is never a navigation dead end.
- The layout reports how many children remain outside the visual presentation.
- The scene renders that remainder as a trunk-side “more in Contents” handoff.
- Directories that are incomplete or exceed the visual budget request the
  conventional Contents fallback.
- Production tree scans count visible entries while retaining at most 16
  directory-first records for the current directory.
- Retained tree entries arrive in batches of four while counting continues.
- The Contents model retains one deterministic page of at most 64 entries.
- The fake Large Dataset test fixture reports 1,000,000 entries while
  materializing only 16 samples.

The value 16 is an initial materialization budget, reduced after the first
real-home scan showed that 24 simultaneous branches overwhelmed the scene. The
front-corridor budget is 8; the remaining materialized entries provide spatial
context without equal visual weight. Neither value is a filesystem limit. They
should change only after visual testing, and increasing them must not remove the
hard bound.

## Contents fallback

The inspector provides separate Details and Contents views. Selecting a bounded
or incomplete directory opens Contents automatically, reports the real state and
counts, and shows one 64-entry page. Previous and Next replace the page rather
than accumulating names in QML, and ListView virtualizes its delegates.

Page queries use the last entry's deterministic ordering key as their cursor.
This removes a fixed directory-size ceiling while keeping memory proportional to
one page. Each page currently performs a fresh background enumeration; indexing
or durable scan snapshots can optimize that cost later.

Hidden visibility is one shared scan option. Toggling it restarts both the active
tree scan and current page under cancellation, while an explicitly typed hidden
directory remains directly reachable regardless of discovery visibility.

## Error semantics

Size alone is never an error. Loading, partial results, permission denial, scan
failure, disconnected mounts, vanished entries, and symlink cycles remain
distinct states with literal messages and recovery actions where possible.

## Non-negotiable guarantees

- Focusing a directory never creates scene geometry proportional to an
  unbounded entry count.
- Total counts never imply that all entries are loaded.
- Partial results are identified as partial.
- The user always has a conventional path to precise contents.
- Scanners and page queries must remain asynchronous, cancellable, and
  memory-bounded.
