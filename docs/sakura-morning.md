# Sakura Morning

Sakura Morning is Canopy's first reference theme. It exists to make the
application feel warm, personal, botanical, and clearly distinct from terminal,
dashboard, and stark productivity-tool aesthetics.

This document defines constraints and semantic roles rather than prescribing an
external visual reference.

## Direction

- bright, but never white or clinical
- warm brown wood
- pastel pink blossoms
- supporting spring foliage
- soft, organic, and welcoming
- precise enough for everyday file navigation
- restrained depth instead of glass, neon, or heavy effects
- no constant ambient motion

The application shell should feel like part of the same illustrated object as
the tree. Sentence case, comfortable spacing, and quiet controls are preferred
over command-bar styling, excessive uppercase, and dense chrome.

## Contrast

Every theme provides two base text colors:

- dark text for the canvas, paper-like surfaces, and other bright contexts
- light text for dark branches, strong selections, and locally dark surfaces

Muted text is derived from one of those two colors through opacity. Themes do
not accumulate unrelated gray text colors. Text placement must select the base
role that maintains contrast in its actual context.

## Semantic color roles

The theme owns roles for the scene canvas, shell surface, panel surface, raised
surface, border, wood, wood shadow, hover, selection, each file form, and image
flower centers. Semantic roles remain stable when their concrete colors change.

File types use both shape and color. The palette is never the sole indication of
type, selection, state, errors, or permissions.

## Theme boundary

Themes may change palette, branch weight, curvature character, typography,
botanical forms, spacing, and motion timing. They do not change navigation,
shortcuts, filesystem semantics, operation safety, loading states, or the
bounded-directory guarantee.

Sakura Morning is a reference implementation, not a permanent restriction on
future themes. Its tokens should reveal the smallest useful theme interface
before additional themes are attempted.
