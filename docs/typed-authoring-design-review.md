# Typed authoring: native design review

This record captures the approved refinement of OneUI's existing public defaults. It preserves the green identity, light/dark themes, native controls, `Mount`, and Yoga. It is not a redesign of the product identity. [README.md](../README.md) remains the product reference; [ui_theme.h](../include/oneui/ui_theme.h) remains the runtime token authority. [DESIGN.md](../DESIGN.md) and its [sidecar](../.impeccable/design.json) document that implementation.

## Implemented system changes

- Comfortable/compact section spacing and padding: 24/16px.
- Comfortable/compact field spacing: 20/12px.
- Related label/hint and control/error copy spacing: 4px.
- Grid label-to-control spacing: 8px.
- Read-only labels: muted, 13px, weight 400; read-only `Text` values: 15px, weight 500. `Status` keeps its semantic styling.
- Typed `Compose` handles delegate to the existing mount and shared component schema; they do not own another widget tree, subscription layer, or layout engine.

Page behavior follows [page patterns](36-declarative-page-patterns.md): constrained scrolling bodies, fixed page headings and direct action bars, and Yoga-managed wrapping fields. The examples use the same public defaults as consumer pages.

## Native evidence

These six native captures cover the principal surfaces and narrow/wide examples. They are image evidence of the captured states, not a guarantee for every platform configuration.

| Capture | Observed coverage |
|---|---|
| [Detail, light and wide](images/typed-authoring/detail-light-wide.png) | Read-only hierarchy, two-column content, section grouping, bottom actions |
| [Detail, dark and narrow](images/typed-authoring/detail-dark-narrow.png) | Dark palette, narrow detail layout |
| [Editor, light and wide](images/typed-authoring/editor-light-wide.png) | Editable fields and wide form layout |
| [Editor error, narrow](images/typed-authoring/editor-error-narrow.png) | Validation feedback and narrow form layout |
| [List, light and narrow](images/typed-authoring/list-light-narrow.png) | Narrow list and toolbar arrangement |
| [States, dark and narrow](images/typed-authoring/states-dark-narrow.png) | Disabled input, semantic statuses, and empty state |

Earlier native examples remain under [connections](images/connections/). The fresh independent review of source and screenshots reported **ship**, with no material findings in the reviewed scope. The record does not claim that this documentation pass reran all behavior or build checks.

## Limits and follow-up evidence

OS DPI scaling, moving a window across displays, and a real IME candidate window remain unproven in this review. A native web-style detector was not available or used. Layout diagnostics and source inspection cannot establish native input behavior or replace screenshot review; conversely, static screenshots cannot establish focus order or text-editing correctness.

The sidecar's HTML/CSS snippets only illustrate extracted component styles in a documentation panel. Native screenshots remain the visual evidence for the shipped C++ rendering path. Font family, easing curves, device breakpoints, shadows, and synthesized color ramps are omitted because this extraction did not find public theme tokens defining them.
