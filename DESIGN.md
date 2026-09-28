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
  workspace-light-canvas: "#edf0f3"
  workspace-light-surface: "#fafbfc"
  workspace-light-chrome: "#f0f3f6"
  workspace-light-ink: "#202732"
  workspace-light-muted: "#596574"
  workspace-light-line: "#d1d8e0"
  workspace-light-control: "#e5ebf1"
  workspace-light-hover: "#e4ebf2"
  workspace-light-accent: "#0067c7"
  workspace-light-accent-ink: "#ffffff"
  workspace-light-selection: "#dceafb"
  workspace-light-success: "#12825d"
  workspace-dark-canvas: "#1c2126"
  workspace-dark-surface: "#1c2228"
  workspace-dark-chrome: "#282e34"
  workspace-dark-ink: "#e3e8ed"
  workspace-dark-muted: "#aebac6"
  workspace-dark-line: "#3b444e"
  workspace-dark-control: "#2b343d"
  workspace-dark-hover: "#333f49"
  workspace-dark-accent: "#3699ff"
  workspace-dark-accent-ink: "#ffffff"
  workspace-dark-selection: "#243e55"
  workspace-dark-success: "#36d69b"
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
  workspace-tool: "4px"
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
  workspace-tool-dark:
    backgroundColor: "transparent"
    textColor: "{colors.workspace-dark-ink}"
    rounded: "{rounded.workspace-tool}"
    padding: "6px"
    height: "32px"
    width: "32px"
  workspace-panel-header-dark:
    backgroundColor: "{colors.workspace-dark-chrome}"
    textColor: "{colors.workspace-dark-ink}"
    padding: "0px 12px"
    height: "40px"
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

### Optional workspace palette

`workspaceTheme(dark)` explicitly selects compact density with slate surfaces and blue emphasis. Only `workspace-*` tokens describe that optional palette; the green defaults above remain the authority for ordinary pages. Chrome groups the title, navigation rail, sessions, panel headers, and status bar; surface carries the working content. Selection and accent identify the active area, while success keeps its semantic meaning. Error and warning inherit the corresponding light or dark default roles.

**The Opt-In Workspace Rule.** Apply workspace colors as a complete optional theme; do not promote its blue accent or compact composition into the default green system.

## Typography

Use the heading, page-title, section-title, body, and supporting roles in the frontmatter. `Page` and `Header` use heading text; the page-pattern components use page-title text; section and empty/loading titles use section-title text. Default field labels are semibold. Read-only labels use muted regular text, while a `Text` field value uses the stronger field-value role. A `Status` field keeps its own status styling.

The theme does not declare a font family, line-height, or tracking token. Preserve the native typography implementation and fallback behavior; no web font or invented font stack is specified here.

The terminal workbench locally uses semibold section labels (13px) and metric values (23px). These belong to that example; they do not redefine the SDK section-title role. Terminal text retains its native terminal font and rendering.

**The Read-Only Hierarchy Rule.** In a read-only field, the label describes the value; the value receives the stronger text role.

## Layout

Use the existing Yoga-backed `Mount` components. `Compose` is a thin typed authoring layer over that same mount, and templates share the component schema. None of these authoring choices establishes another layout or rendering system.

The section gap separates major groups; the field gap separates rows and grid cells. Copy gap groups a label with its hint and a control with its validation message. Grid fields use label-control gap between those two groups. Comfortable and compact density each have explicit page, section, field, and control spacing; copy and label-control spacing stay stable in both densities.

`SettingsPage` constrains its scrolling content to (880px); `DetailPage` uses (1040px). Titles remain outside the body scroll. A single direct child `ActionBar` is placed below the body; its contents wrap. Nested action bars remain ordinary wrapping layouts. `ListPage` lets the native table consume remaining space and scroll itself.

`FormGrid` accepts `FormRow` children and places labels above controls. Its default cell basis is (320px), with wrapping and geometry owned by Yoga. `min-column-width` adjusts that layout input; it is not an OS or device breakpoint. Standalone form rows use wrapping label and control groups with bases of (260px) and (300px). Do not infer fixed screen breakpoints from these values. Optional empty titles, subtitles, hints, and errors collapse.

### Optional workspace composition

`Workspace` arranges a content title bar (38px), left navigation rail (56px), session bar (42px), flexible body, and status bar (38px). `DockPanel` arranges its header (40px), flexible body, and optional footer (40px). These are reusable SDK defaults, with narrow line-colored separation (1px), rather than application-calculated child coordinates. The content title bar does not replace operating-system window chrome.

The terminal-workbench example starts with a terminal and SFTP stack taking 70% of the body and monitoring taking 30%. Below its application-specific width (1050px), a segmented terminal/files/monitor switch selects the visible retained region. Split ratios, terminal objects, and input survive region changes. The SDK does not force this breakpoint or ratio onto other workspaces. Application code selects regions and binds state; Yoga and the shared split components allocate geometry.

## Elevation & Depth

Canvas, surface, control, borders, and selection convey separation. Keep default surfaces flat. The shared theme additionally offers opt-in button feedback: `motion-lift` uses `0px 2px 6px #00000018` at rest and `0px 5px 12px #00000030` on hover, while `motion-press` changes `0px 2px 4px #00000020` to `inset 0px 2px 5px #00000038` on press. Both remove shadows when disabled. These classes communicate interaction without moving the hit target; they do not establish a raised-card default. Focus outlines are interaction feedback rather than surface elevation: controls use an accent outline (2px) with offset (2px); invalid focused input uses the error role.

The effects gallery demonstrates multi-stop linear/radial fills and real inner shadows. Its light green gradient uses a dark foreground in both themes. These are optional demonstrations, not replacement palette tokens; retain readable contrast when applying gradients.

The optional workspace uses flat, connected panes with chrome and line separation. Do not introduce card shadows between its terminal, files, and monitoring regions.

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

### Content transitions

`Reveal` retains one native subtree. Use expand for a content disclosure that should move adjacent layout, and fade when content should keep its slot until disappearing. The shared default is 220ms with ease-out; closing uses 75% of the duration. `motion-lift` and `motion-press` use 140ms and 110ms respectively. Transitions finish rather than loop. Respect the local reduced-motion setting and Windows client-area animation preference; do not add continuous decorative animation to the default forms. Closing content releases its focus and does not steal focus back on reopening.

### Tables and status

`DataTable` uses a surface body, control-colored content background, hover and selection roles, and native scrolling. Status uses muted text by default and semantic tones when requested. `Sidebar` with `SidebarLayout` is the retained native navigation pattern; it becomes a wrapping top row below its 960px container breakpoint. `MasterDetail` switches from two panes to a state-selected single pane below 800px. There is no generic routing/history or chip primitive.

### Workspace regions and tools

Compose workspace areas from `Workspace`, its named regions, `DockPanel`, and `PanelHeader` / `PanelBody` / `PanelFooter` through either `.one` templates or C++ Compose. Both authoring paths retain the same native widgets. `ToolButton` provides a transparent compact control with hover and pressed fills, a small radius, and an accent focus outline (1px). Icon-only tools default to (32px) square; a text tool may grow horizontally. Set `name` to provide its accessible name and tooltip. Use `Spacer` for alignment rather than local coordinates.

`Tabs` supports session documents and equal-width segmented views. `Progress` uses a bound value from 0 to 1. Native tables remain scroll owners. Coordinate `DockPanel.collapsed` with `SplitView.second-collapsed` to retain a header-only region (40px in this example) and restore the original split ratio on expansion. This is an immediate retained layout change; it does not imply animation, floating windows, or arbitrary drag docking.

The [workspace layout guide and native captures](docs/57-workspace-layout-components.md) record the optional palette and wide/narrow composition. Terminal inset color and compact table styling in the example are local choices, not replacements for the default form and table metrics.

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

### Page recipes and surface presets

Standard keeps the existing 6px control / 12px surface radii. Soft uses 8px / 14px and a restrained default Surface shadow. Explicit flat, outlined, raised and tinted Surface appearances share theme roles. Settings actions remain outside the scrolling body. Use the [native recipe captures and constraints](docs/52-layout-recipes-and-presets.md); the two authoring entries share widgets and state.
