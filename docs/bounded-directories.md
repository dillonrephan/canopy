# Bounded Directory Presentation

A directory with an extreme number of entries is valid input, not an error.
Canopy must remain responsive without pretending that every entry can be useful
as an individual botanical form.

## Current read-only contract

- Semantic directory nodes keep total child count separate from materialized
  child IDs.
- Directory state is explicit: ready, loading, partial, inaccessible, or error.
- Tree layout considers at most 16 direct children of the focused directory.
- The layout reports how many children remain outside the visual presentation.
- Directories that are incomplete or exceed the visual budget request the
  conventional Contents fallback.
- Production scans count visible entries while retaining at most 256 sorted
  records for the current directory.
- The fake Large Dataset test fixture reports 1,000,000 entries while
  materializing only 16 samples.

The value 16 is an initial interaction budget, reduced after the first real-home
scan showed that 24 simultaneous branches overwhelmed the scene. It is not a
filesystem limit. It should change only after visual testing, and increasing it
must not remove the hard bound.

## Contents fallback

The inspector provides separate Details and Contents views. Selecting a bounded
or incomplete directory opens Contents automatically, reports the real state and
counts, and shows only the currently materialized sample. QML virtualizes the
delegates, while the scanner limits the current list to 256 entry names.

The next iteration must replace the copied name list with a paged model that can
deliver partial results incrementally without copying an entire large directory
into QML.

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
- Future scanners must be asynchronous, cancellable, and memory-bounded.
