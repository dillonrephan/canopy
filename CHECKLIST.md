# Canopy Session Checklist

Use this page when returning to the project after a break. Keep it short,
literal, and current.

## Start of session

- Read `README.md` and this checklist.
- Run `git status --short --branch` before changing files.
- If the worktree is clean and time has passed, run `git pull --ff-only`.
- Confirm the intended branch and inspect the latest few commits.
- Read the documentation directly related to the active task.
- Run `./scripts/check.sh` before editing when environment or build state may
  have changed.
- Check `requirements-dev.txt` whenever dependencies are installed or changed.
- Before launching Canopy, close existing Canopy processes and start exactly
  one fresh instance.

If `canopy-dev` fails to start because `/run/avahi-daemon/socket` is missing,
check `avahi-daemon.socket`; the existing Toolbx binds that host socket.

## Current checkpoint

- Active branch: `visual/perspective-sakura`
- Stable planar rollback tag: `checkpoint/planar-fog-navigation`
- World/planar separation tag: `checkpoint/world-planar-separation`
- The modular Sakura profile, stable world placement, and planar projection are
  implemented and pushed.
- `PerspectiveProjection` exists beside the planar reference but is dormant.
- `TreeLayoutEngine` still selects `PlanarProjection`; the running application
  has no perspective behavior yet.
- The full build, QML lint, and test suite passed after adding the dormant
  projector.

## Active task: validate perspective math

Before activating the perspective projector:

- Reduce perspective lane spacing from 220 to approximately 160 units.
- Cull geometry after the camera passes it; never freeze it near the viewer.
- Remove culled geometry from both painting and hit-testing.
- Test that approaching growth moves lower, outward, larger, and clearer.
- Test that distant growth compresses toward the horizon and fogs.
- Test that reversing travel restores the exact prior projection.
- Test finite output at extreme travel depths and viewport sizes.
- Test that left/right lane ordering cannot flip.
- Keep projection bounded to six foreground corridors and two fog hints.
- Keep the planar projector available until perspective rendering and hit
  geometry satisfy the same correctness contracts.

See `docs/theme-architecture.md` for the full staged migration.

## Concept parking lot

### Recent-directory history

Start with the simplest implementation: a bounded vertical history list, not
Cover Flow.

- Record actual directory focus changes, not ordinary camera movement.
- Keep roughly 12 recent directory/viewpoint snapshots.
- Avoid adjacent duplicates.
- Restore directory identity, selected item, and saved view state.
- Support click, wheel while focused/hovered, Up/Down, Enter, Home/End, and
  visible keyboard focus.
- Provide literal names and paths for accessibility.
- Keep history semantics core-owned and its visual treatment theme-owned.
- A future Sakura presentation may replace the list with vertical iPod-style
  Cover Flow without changing the history model.

This is intentionally parked until the perspective projection and projected
hit geometry are trustworthy.

## End of session

- Run `git diff --check` and the relevant tests.
- Update this checklist if the active checkpoint or next task changed.
- Commit coherent work before changing direction.
- Push the active branch and any new rollback tag.
- Verify `git status --short` is clean before calling the session safe to close.
