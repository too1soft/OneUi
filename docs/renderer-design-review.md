# Renderer status: scoped design review

This records a local extension to the existing native Windows Performance Lab: renderer status replaces the overview subtitle and appears in a 34 px strip above the retained editor/gallery. It documents the incumbent visual treatment and the evidence below; it is not a validation of the full application or a new app-wide design system. No new visual identity is introduced.

## Evidence

| Capture | Observed result |
| --- | --- |
| [Incumbent overview](images/performance-lab/overview.png), 1320 × 900 | Establishes the dark dashboard, sidebar, metric row, chart cards, typography and accent usage. |
| [GPU overview](images/renderers/gpu-wide.png), 1320 × 900 | Subtitle distinguishes GPU preference from the actual OpenGL / Skia Ganesh backend and names the NVIDIA device. The status fits on one line; the established dashboard composition remains visually consistent with the incumbent capture. |
| [CPU overview](images/renderers/cpu-wide.png), 1320 × 900 | Subtitle identifies explicit CPU selection, actual CPU / Skia Raster rendering and the software-selection reason. It occupies the same subtitle position. |
| [Automatic CPU, narrow gallery](images/renderers/auto-cpu-narrow.png), 640 × 800 | A single status line remains readable in the dark strip above the light gallery. The form uses its existing single-column layout and vertical scrolling; the visible status does not overlap the gallery heading or controls. |

Source checks covered the palette in `examples/performance_lab/plots.hpp` and the label helper, renderer status update, layout and window setup in `examples/performance_lab/main.cpp`.

## Colors

The extension reuses the existing background (`BG`, `#101517`) and muted text (`MUTED`, `#96AAA4`). The surrounding overview retains its panel (`#171E21`), divider (`#2B3539`), primary text (`#E5ECE8`), lime (`#C4ED87`), teal (`#61C9BB`) and orange (`#ECAD72`) palette. Backend status is supporting information and does not introduce a new success/error color or decorative badge.

## Typography

The window retains Microsoft YaHei UI. The subtitle uses the existing label defaults: 12 px, weight 400, muted text. Renderer/device details share that treatment; they do not compete with the page heading or metric values.

## Layout

The overview reuses the subtitle frame at a 68 px vertical offset with a 22 px height. Its sidebar, content offsets, cards and controls retain their established placement. Editor/gallery mode places the label 20 px from each horizontal edge and 7 px from the top, in a 20 px text frame; the retained page starts 34 px below the window top and receives the remaining height.

The configured minimum client sizes are 1050 × 720 for the overview and 640 × 560 for editor mode. The 640 × 800 capture supports the narrow gallery observation, not a claim that the overview supports that width. The reviewed images do not establish behavior for every device name, long fallback reason, display scale, viewport or interactive state.

## Components

The status presents **startup selection** separately from **actual backend**. `gpu` means GPU preference; only reported OpenGL operation is labeled OpenGL / Skia Ganesh. `cpu` selects software rendering. `auto` preserves the `ONEUI_ENABLE_GPU` environment setting, so automatic selection can correctly report CPU / Skia Raster with an explicitly selected software reason.

Software status distinguishes an environment-disabled GPU from a GPU fallback reason. Before backend initialization it reports that the first paint is pending. The tooltip repeats the complete status and explains that changing modes requires a restart and that automatic selection follows the environment. These semantics were checked in source; fallback, pending and tooltip interaction were not demonstrated by the three reviewed captures.

The reviewed extension preserves the incumbent hierarchy and makes the requested-versus-actual distinction visible at the captured sizes. Screenshot metric values are incidental observations, not benchmark conclusions.
