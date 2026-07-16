# Design Principles

## The metaphor serves navigation

Canopy is a file manager first. Botanical representation should make hierarchy,
density, type, and location easier to understand. When the metaphor conflicts
with precision or safety, conventional controls win.

## Spatial memory must be earned

Branches should remain stable across ordinary interaction. Organic variation is
welcome only when it does not cause directories to jump unpredictably. Layout
must be deterministic and focus transitions must preserve visible context.

## Botanical, not ornamental

The initial art direction is an illustrated field guide:

- warm, dark, low-noise background
- restrained curved branches
- softly geometric botanical forms
- crisp labels and obvious focus states
- subtle depth rather than simulated realism
- little or no ambient motion

The first prototype must look intentional, but decoration must not obscure
names, hit targets, hierarchy, or state.

## Semantic compression is part of the product

Canopy cannot show every item at every zoom level. Distant views emphasize
directory structure and density; medium views show meaningful groups; close
views reveal individual files and labels. Compression should be understandable,
consistent, and reversible through zoom or focus.

## Conventional escape hatches are permanent

Breadcrumbs, keyboard navigation, direct paths, clear text labels, and an
eventual list view are core features—not admissions that the visual design
failed. Users should never be trapped into precise work through decorative
objects alone.

## Motion communicates change

Animation explains focus, creation, movement, loading, and search state. Routine
motion should generally finish in 120–250 ms, remain interruptible, and never
block input. Reduced-motion mode replaces spatial animation with immediate state
changes or short fades.

## Accessibility shapes the design

Every interactive item needs keyboard reachability, visible focus, readable
text, and a semantic label. Color cannot be the only indicator of type, state,
selection, errors, or permissions. The interface must support scalable text,
high contrast, reduced motion, and eventually a conventional list fallback.

## Safety remains literal

Whimsical language may accompany an operation, but its consequence must be
plain. “Compost” must also say “Trash.” Permanent deletion must be explicit.
Errors and partial failures must name the real operation and affected items.

## Themes change expression, not behavior

Themes may alter palette, branch geometry, botanical forms, motion character,
background, typography, and decoration. They do not alter shortcuts, navigation,
file-operation semantics, safety rules, or accessibility guarantees.

## Prototype evaluation questions

The Phase 1 prototype succeeds only if hands-on use answers yes to most of these:

- Can a user identify the focused directory and its parent context immediately?
- Can they predict where a known directory will be after returning to a view?
- Do wide directories remain legible instead of exploding into visual noise?
- Does zoom reveal useful detail rather than merely enlarge decoration?
- Are hover, selection, and click targets unambiguous?
- Can the user navigate comfortably without waiting for animations?
- Does the scene feel unmistakably botanical without resembling a themed list?
- Does the inspector supply precision without competing with the tree?

If those answers are weak, the next task is design correction—not filesystem
integration.
