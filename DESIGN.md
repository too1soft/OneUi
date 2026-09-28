---
name: OneUI
description: Native desktop defaults for C++ declarative pages.
colors:
  light-canvas: "#f5f7f4"
  light-surface: "#ffffff"
  light-ink: "#1c2b23"
  light-muted: "#54675b"
  light-line: "#bac8bf"
  light-control: "#e7eee8"
  light-hover: "#d5e2d7"
  light-accent: "#28613d"
  light-accent-ink: "#ffffff"
  light-selection: "#d5e9d7"
  light-error: "#a02b24"
  light-warning: "#785315"
  light-success: "#28613d"
  dark-canvas: "#101517"
  dark-surface: "#171e21"
  dark-ink: "#e5ece8"
  dark-muted: "#a5b5af"
  dark-line: "#52645d"
  dark-control: "#232e29"
  dark-hover: "#34473c"
  dark-accent: "#c4ed87"
  dark-accent-ink: "#16230f"
  dark-selection: "#36503e"
  dark-error: "#ffb4a8"
  dark-warning: "#efce88"
  dark-success: "#b2dca4"
typography:
  heading:
    fontSize: "28px"
    fontWeight: 600
  page-title:
    fontSize: "22px"
    fontWeight: 600
  section-title:
    fontSize: "18px"
    fontWeight: 600
  body:
    fontSize: "14px"
    fontWeight: 400
  field-label:
    fontSize: "14px"
    fontWeight: 600
  read-only-label:
    fontSize: "13px"
    fontWeight: 400
  field-value:
    fontSize: "15px"
    fontWeight: 500
  muted:
    fontSize: "13px"
    fontWeight: 400
  button:
    fontSize: "14px"
    fontWeight: 500
rounded:
  control: "6px"
  section: "12px"
spacing:
  space-sm: "8px"
  space-md: "16px"
  space-lg: "24px"
  copy-gap: "4px"
  label-control-gap: "8px"
  page-pad: "28px"
  page-pad-compact: "20px"
  section-pad: "24px"
  section-pad-compact: "16px"
  section-gap: "24px"
  section-gap-compact: "16px"
  field-gap: "20px"
  field-gap-compact: "12px"
  control-pad: "10px"
  control-pad-compact: "6px"
components:
  button-default:
    backgroundColor: "{colors.light-control}"
    textColor: "{colors.light-ink}"
    typography: "{typography.button}"
    rounded: "{rounded.control}"
    padding: "10px 18px"
  button-primary:
    backgroundColor: "{colors.light-accent}"
    textColor: "{colors.light-accent-ink}"
    typography: "{typography.button}"
    rounded: "{rounded.control}"
    padding: "10px 18px"
  button-danger:
    backgroundColor: "{colors.light-surface}"
    textColor: "{colors.light-error}"
    typography: "{typography.button}"
    rounded: "{rounded.control}"
    padding: "10px 18px"
  input:
    backgroundColor: "{colors.light-surface}"
    textColor: "{colors.light-ink}"
    rounded: "{rounded.control}"
    padding: "10px 12px"
  section:
    backgroundColor: "{colors.light-surface}"
    rounded: "{rounded.section}"
    padding: "{spacing.section-pad}"
  read-only-value:
    textColor: "{colors.light-ink}"
    typography: "{typography.field-value}"
---

# Design System: OneUI

## Overview

**Creative North Star: "Native desktop clarity"**

OneUI's existing identity is a restrained green desktop interface with light and dark surfaces, readable forms, and compact or comfortable density. This descriptive north star records the incumbent system; it does not introduce a new visual identity. Content, field relationships, and actions establish the hierarchy.

The product context comes from [README.md](README.md). The authoritative implementation is [ui_theme.h](include/oneui/ui_theme.h); this document captures its public defaults. Tokens above are the machine-readable snapshot, with theme and density suffixes exposing the existing alternatives. Component entries show light, comfortable defaults; dark mode substitutes the corresponding dark color roles and compact mode substitutes compact spacing. These names do not add new runtime CSS variables.

**Key Characteristics:**

- Green emphasis on neutral surfaces.
- Separate spacing for sections, fields, and related copy.
- Native controls and Yoga layout shared by C++ and template authoring.
- Read-only values carry more emphasis than their labels.

## Colors

The light palette combines a pale green canvas, white surfaces, dark green ink, and a deep green accent. The dark palette uses charcoal green surfaces, pale ink, and a light green accent.

### Primary

`light-accent` and `dark-accent` identify primary actions, selected switches, pending status, carets, and focus outlines. Their paired `accent-ink` roles supply foreground color on accent fills. A primary button's hover state uses ink as its fill and canvas as its foreground.

### Neutral

Canvas separates the application background from section surfaces. Ink carries content; muted carries supporting copy and read-only labels. Line defines input edges and unselected switches. Control, hover, and selection distinguish interaction states. The palette also has explicit success, warning, and error roles; success shares the light accent but has its own dark value.

**The Semantic Feedback Rule.** Status text must communicate the state in words; color and the native status dot support that meaning.

## Typography

Use the heading, page-title, section-title, body, and supporting roles in the frontmatter. `Page` and `Header` use heading text; the page-pattern components use page-title text; section and empty/loading titles use section-title text. Default field labels are semibold. Read-only labels use muted regular text, while a `Text` field value uses the stronger field-value role. A `Status` field keeps its own status styling.

The theme does not declare a font family, line-height, or tracking token. Preserve the native typography implementation and fallback behavior; no web font or invented font stack is specified here.

**The Read-Only Hierarchy Rule.** In a read-only field, the label describes the value; the value receives the stronger text role.

## Layout

Use the existing Yoga-backed `Mount` components. `Compose` is a thin typed authoring layer over that same mount, and templates share the component schema. None of these authoring choices establishes another layout or rendering system.

The section gap separates major groups; the field gap separates rows and grid cells. Copy gap groups a label with its hint and a control with its validation message. Grid fields use label-control gap between those two groups. Comfortable and compact density each have explicit page, section, field, and control spacing; copy and label-control spacing stay stable in both densities.

`SettingsPage` constrains its scrolling content to (880px); `DetailPage` uses (1040px). Titles remain outside the body scroll. A single direct child `ActionBar` is placed below the body; its contents wrap. Nested action bars remain ordinary wrapping layouts. `ListPage` lets the native table consume remaining space and scroll itself.

`FormGrid` accepts `FormRow` children and places labels above controls. Its default cell basis is (320px), with wrapping and geometry owned by Yoga. `min-column-width` adjusts that layout input; it is not an OS or device breakpoint. Standalone form rows use wrapping label and control groups with bases of (260px) and (300px). Do not infer fixed screen breakpoints from these values. Optional empty titles, subtitles, hints, and errors collapse.

## Elevation & Depth

The public declarative theme defines no shadow tokens. Canvas, surface, control, borders, and selection convey separation. Keep these default surfaces flat. Focus outlines are interaction feedback rather than surface elevation: controls use an accent outline (2px) with offset (2px); invalid focused input uses the error role.

## Shapes

Controls use the control radius and sections use the section radius. Empty and loading surfaces share the section radius. Inputs and selects have a line border (1px); default and primary buttons have no border, while danger buttons use a line border (1px). Data tables omit column dividers and use native rows (44px) in the current declarative mount defaults.

## Components

### Buttons

Primary actions use the accent fill; secondary actions use the control fill; destructive actions use an error foreground on a surface with a line border. Default hover uses the hover role, pressed feedback uses selection, and danger hover changes its border to error. Disabled buttons use control and muted roles. The theme provides a button transition duration (120ms), without declaring an easing token.

### Inputs and fields

`Input`, `SearchInput`, and `Select` share surface fill, ink, line borders, and accent focus feedback. Inputs use muted placeholders, accent carets, and selection fill. Invalid input uses error border and focus outline; disabled input uses control fill and muted ink. `FormRow` groups label, optional hint, one native control or read-only value, and optional validation text. Updating hint or error preserves the input control.

### Sections and state surfaces

`Section` provides the default grouping surface and density-aware padding. `EmptyState` and `LoadingState` use the same surface with padding (32px) and gap (12px). Loading feedback is static by default. State text explains what happened and actions are supplied by the caller.

### Read-only details

Compose detail content from `DetailPage`, `Section`, `FormGrid`, `FormRow`, and `Text` or `Status`. A read-only label/value pair gets its hierarchy from the theme automatically. This is a shared component behavior, not page-specific styling.

### Tables and status

`DataTable` uses a surface body, control-colored content background, hover and selection roles, and native scrolling. Status uses muted text by default and semantic tones when requested. The component schema has no standalone navigation or chip primitive; document application-specific navigation separately if introduced.

The [sidecar](.impeccable/design.json) contains self-contained HTML illustrations of representative native primitives for documentation panels. They are token translations, not native rendering or input-behavior evidence. The [native review record](docs/typed-authoring-design-review.md) links the actual screenshots and states the validation limits.

## Do's and Don'ts

### Do:

- **Do** apply the shared theme and density roles when composing public pages.
- **Do** let Yoga measure, wrap, and position fields and action groups.
- **Do** use muted read-only labels and the field-value role for Text values.
- **Do** retain semantic status text alongside color feedback.

### Don't:

- **Don't** introduce a second widget tree or layout engine for typed authoring.
- **Don't** replace shared default tokens with page-specific coordinates to reproduce a screenshot.
- **Don't** treat documentation HTML previews as proof of native input, focus, DPI, or IME behavior.
