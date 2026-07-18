# Bounded Directory Presentation

A directory with an extreme number of entries is valid input, not an error.
Canopy must remain responsive without pretending that every entry can be useful
as an individual botanical form.

## Current prototype contract

- Semantic directory nodes keep total child count separate from materialized
  child IDs.
- Directory state is explicit: ready, loading, partial, inaccessible, or error.
- Tree layout considers at most 24 direct children of the focused directory.
- The layout reports how many children remain outside the visual presentation.
- Directories that are incomplete or exceed the visual budget request the
  conventional Contents fallback.
- The fake Large Dataset fixture reports 1,000,000 entries while materializing
  only 24 samples.

The value 24 is an initial interaction budget, not a filesystem limit. It should
change only after visual testing, and increasing it must not remove the hard
bound.

## Contents fallback

The inspector provides separate Details and Contents views. Selecting a bounded
or incomplete directory opens Contents automatically, reports the real state and
counts, and shows only the currently materialized sample.

The Phase 1 list is backed by fake data. The read-only filesystem phase must
replace it with an incremental model that virtualizes delegates, loads bounded
pages, supports cancellation, and does not copy an entire large directory into
QML.

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
