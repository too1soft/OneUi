# OneUI And iShellPro Native Readiness Baseline

Status: active

This document is the engineering baseline for moving iShellPro from a
Tauri/WebView desktop shell to a native OneUI desktop shell. It is deliberately
written before implementation so that OneUI gains reusable primitives instead
of accumulating iShellPro-specific exceptions.

## Product Objective

iShellPro must be able to ship a Windows and Kylin Linux desktop application
without a browser engine or WebView runtime. Its existing Rust business logic
remains the authority for sessions, SSH/SFTP/RDP/VNC protocols, persistence,
licensing, CLI/deep-link handling, configuration, and enterprise policy.

OneUI owns native windows, rendering, UI input, layout, accessibility, and
desktop integration. Product code consumes OneUI only through its stable C ABI
and a safe Rust wrapper; it must not link OneUI C++ classes directly or add
platform-specific GUI code outside OneUI.

## Non-Negotiable Architecture

```text
ishell-core (Rust, platform independent)
  sessions, database, config, licensing, CLI, sync, protocol services

ishell-oneui (Rust desktop adapter)
  safe OneUI bindings, view models, UI event routing, app state subscriptions

OneUI (C++ framework)
  window lifecycle, renderer, widgets, layouts, input, DPI, accessibility
```

The current Tauri application remains a supported delivery path while the
native client reaches feature and quality parity. It is a compatibility
baseline, not a second source of truth for future native UI behavior.

## iShellPro Desktop Inventory

### Application Shell

- Custom title bar: drag region, minimize, maximize, close, close confirmation,
  system tray, native window state, and enterprise splash screen.
- Collapsible navigation rail and optional expanded sidebar.
- Global command palette / quick open, application shortcuts, toast,
  confirmation, startup error, and migration dialogs.
- Split panes, resizable right tools panel, persistent widths, immersive mode,
  background media, theme, density, font, and DPI-sensitive layout.
- Chinese and English localization, including CJK fallback font handling.

### Hosts And Credentials

- Host grid/list views, filtering, sorting, grouping, tags, status/ping state,
  multi-selection, batch move, quick connection, and cloud-sync actions.
- Host editor for SSH, Telnet, RDP, VNC, serial, proxy/jump host, authentication,
  key files, environment variables, terminal options, and notes.
- Credential manager, import dialogs for XShell, FinalShell, SecureCRT,
  MobaXterm, PuTTY, Xterminal, SSH config, and CSV.
- Context menus, copy-to-clipboard actions, validation, destructive-action
  confirmation, and long-list performance.

### Terminal Workspace

- Multi-session tabs, close/reorder/restore, split panes, terminal toolbar,
  context menu, session state overlays, host-key confirmation, password/MFA and
  keyboard-interactive authentication.
- VT terminal rendering, scrollback, selection, search, hyperlinks, resize,
  copy/paste, drag/drop, font selection, themes, local PTY, SSH/Telnet/serial,
  WSL, command history, command folding, highlight rules, recording, ZMODEM,
  X11, VNC, tunnels, and session diagnostics.
- Autocomplete popup with keyboard navigation, command history and optional AI
  suggestions. This is a performance- and correctness-critical surface.

### Files, Monitoring, And Operations

- SFTP file navigator with directory tree, file grid/list, drag/drop,
  multi-file transfer, transfer progress, conflict resolution, permissions,
  archive actions, remote clipboard, create/rename/delete/copy/move, favorites,
  multi-tab code editor, and editor scroll-state restoration.
- Remote monitor dashboard: CPU, memory, disk, network, process and port lists,
  time-series charts, filters, refresh scheduling, and a docked terminal tools
  panel.
- SQL manager object tree and query/editor workflow; Docker resource manager
  and container file browser.

### Knowledge And Assistance

- Notes with Markdown and rich-text editing, links, text wrapping, search, and
  persistence.
- Snippet groups, snippet editor, parameter form, host selector, execution
  confirmation, and output/error feedback.
- AI assistant and terminal AI: conversation list, streaming content,
  Markdown/code rendering, prompt input, session selector, auto-scroll,
  settings, and IME-safe input.

### Settings, Enterprise, And Integration

- General, terminal, SSH, SFTP, recording, security, shortcut, cloud sync,
  diagnostics, update, import/export, edition, and license settings.
- Enterprise license screen, online/offline activation, device identifier,
  policy gating, and first-run splash.
- CLI and deep-link receiver, including external bastion launch with dynamic
  connection parameters and password/key material.

## Current Web Dependencies To Retire

The native route must replace behavior, not only visual widgets.

| Current dependency family | Existing responsibility | Native replacement direction |
| --- | --- | --- |
| React, Tauri WebView, Tailwind | application composition and rendering | OneUI widget tree, Rust view models, OneUI style sheets |
| xterm.js and addons | terminal emulator, rendering, selection, links, fit, search | `oneui-terminal` surface plus a proven Rust terminal-emulation core |
| CodeMirror | code editing and syntax services | reusable `oneui-code-editor` extension or a Rust-backed editor integration |
| TipTap / ProseMirror | rich notes editor | reusable document editor extension; not a OneUI core widget initially |
| noVNC | VNC canvas, input, clipboard | Rust VNC protocol service plus OneUI realtime frame and raw-input surfaces |
| Recharts | monitor visualizations | `oneui-charts` extension with retained series and incremental rendering |
| react-markdown | AI and documentation content rendering | native Markdown document view extension |

## OneUI Capability Mapping

### Ready To Consume After Platform Parity

- Window shell, title bar primitives, tray, clipboard, logical-DPI contract.
- Widget tree, stack/grid/wrap/dock/split/panel/scroll layouts.
- Buttons, icon buttons, labels, cards, badges, tabs, switches, checkboxes,
  radio groups, sliders, basic text fields, select, lists, tables, popups,
  toast, and form validation.
- Style tokens, style sheets, focus states, basic overlays, and the C ABI.
- Realtime frame and raw remote-input primitives as a starting point for VNC
  and RDP integration.

### Must Be Hardened Before Product Use

- Full window lifecycle: multi-window ownership, close callbacks, fullscreen,
  restore/maximize state, and no process exit when a session window closes.
- UI-thread dispatcher, timers, cancellation, shutdown ordering, and callback
  lifetime guarantees across the Rust/C ABI boundary.
- CJK font fallback, text shaping, measurement, wrapping, truncation, line
  metrics, selection, IME composition, and clipboard behavior.
- Vertical and horizontal scrollbars, keyboard scrolling, thumb dragging,
  scroll anchoring, hidden-scrollbar policy, and virtualized data sources.
- Keyboard focus scopes, command routing, accessible semantics, and platform
  accessibility bridges.
- Windows and Linux build/test/package reproducibility with DPI checks at 100%,
  125%, and 150%.

### New Reusable Components Or Extensions

| Priority | Capability | Placement | Purpose |
| --- | --- | --- | --- |
| P0 | Linux X11 or Wayland backend | OneUI platform | Required for Kylin; choose the first backend from the supported deployment session type. |
| P0 | UTF-8 C ABI v1 | OneUI C ABI | Avoid `wchar_t` width differences between Windows and Linux; convert internally per platform. |
| P0 | Application dispatcher and lifetime API | OneUI core/C ABI | Safe Rust async-to-UI delivery and teardown. |
| P0 | Text and font subsystem | OneUI core | Correct Chinese, English, IME, scaling, and text geometry. |
| P0 | Virtual list/tree/data grid | OneUI core | Hosts, SFTP, processes, logs, Docker, SQL objects. |
| P0 | Command/menu/dialog/context-menu | OneUI core | Native product commands and modal workflows. |
| P0 | `oneui-terminal` | Extension | Grid rendering and input shell; terminal emulation remains in Rust. |
| P1 | File browser and transfer widgets | Extension | Reusable remote/local file management UI. |
| P1 | `oneui-charts` | Extension | Streaming monitor metrics without a browser chart stack. |
| P1 | `oneui-code-editor` | Extension | SQL and remote file editing. |
| P1 | Markdown document view | Extension | AI output, documentation, and notes rendering. |
| P2 | Rich document editor | Extension | Notes authoring after core product surfaces are stable. |

## Delivery Phases And Gates

## Progress Log

### 2026-07-31: P0 Baseline Established

- The bundled-static MSVC build is reproducible again. The Win32 renderer now
  uses the vendored Skia `SkGradient` / `SkShaders` API rather than the removed
  `SkGradientShader` header.
- The complete behavior suite passes: control, overlay, layout, remote frame,
  C ABI, monitor, and backend-contract tests.
- Keyboard focus rules now distinguish non-modal overlays (which participate in
  Tab order) from focus-trapping modal overlays (which cycle inside their own
  scope). This is covered by control and overlay behavior tests.
- UTF-8 C ABI v1 is available for the native-shell primitives: window title and
  creation, label, button, text field, search box, clipboard, and text-change
  callback. Its strings use `OneUiUtf8String { data, length }`; input is copied
  during the call and callback buffers are valid only for that callback.
- The UTF-8 ABI test verifies Chinese and four-byte Unicode round trips. The
  legacy `wchar_t*` ABI remains available for existing Windows consumers but is
  not the interface for new Rust or Linux work.
- `bindings/rust` now contains `oneui-sys` for the raw ABI and `oneui` for safe
  window ownership. Its smoke test creates and destroys a hidden native window
  through the UTF-8 ABI, including Chinese and four-byte Unicode titles.

Still open before an iShellPro production page moves: Linux platform backend,
Rust dispatcher/lifetime binding, DPI/IME acceptance matrix, virtualized data
controls, native terminal surface, and the vertical-slice shell.

### Phase 0: Framework Release Gates

Purpose: make OneUI safe to adopt before any iShellPro page depends on it.

- Repair and stabilize all OneUI tests, including backend contract tests.
- Add CI for Windows MSVC bundled-static and Kylin Linux.
- Publish ABI ownership, threading, error, string encoding, and versioning
  contracts.
- Implement the first supported Linux desktop backend and run its full backend
  contract suite.
- Add screenshot and interaction checks for 100%, 125%, and 150% DPI.

Exit gate: all platform contract, control behavior, text input, clipboard,
window lifecycle, and package dependency tests pass without user-installed
runtime dependencies.

### Phase 1: Native iShellPro Vertical Slice

Purpose: prove that the architecture works end-to-end without a WebView.

- Extract platform-neutral Rust services into `ishell-core`.
- Create `ishell-oneui` and a safe `oneui-sys` binding layer.
- Implement native application shell, enterprise license gate, host list,
  host editor, settings shell, CLI/deep-link receiver, and one SSH connection.
- Implement a minimal `oneui-terminal` with the Rust terminal model, resize,
  scrollback, selection, copy/paste, and authentication overlays.

Exit gate: Windows and Kylin can launch a direct or bastion-provided SSH target,
activate an enterprise license, persist hosts, and complete a one-hour terminal
soak test with no browser process loaded.

### Phase 2: Operations Workspace

Purpose: replace daily operator workflows.

- Terminal tabs, split panes, search, command history, snippets, tunneling,
  recording, and diagnostics.
- SFTP browser and transfers, monitor dashboard, SQL manager, Docker manager,
  and native charts.

Exit gate: selected internal operators can perform their daily workflow solely
in the native client for two release cycles.

### Phase 3: Knowledge And Remote Protocol Parity

Purpose: close the remaining product gap.

- AI assistant, Markdown view, notes, code editing, VNC/RDP rendering and
  input, X11, serial, and advanced import flows.
- Performance, accessibility, localization, enterprise deployment, update, and
  full regression validation.

Exit gate: feature, CLI, enterprise licensing, and packaging parity are signed
off on Windows and Kylin before retiring the Tauri desktop build.

## Rules For Downstream Product Work

1. Do not implement a reusable visual primitive inside iShellPro. Add it to
   OneUI or a named OneUI extension with tests and a C ABI first.
2. Do not add product-specific DPI, scaling, font, IME, focus, or platform
   code outside OneUI.
3. Do not hand-roll a terminal emulator, text shaper, code editor, or chart
   engine when a proven core library can be contained behind a native API.
4. Keep OneUI core small. Terminal, editor, charts, file management, and rich
   text are extensions with their own dependency and performance budgets.
5. A feature is not portable until it passes the same behavioral contract on
   Windows and Kylin.

## Immediate Next Slice

The next implementation slice is Phase 0 only:

1. Make the OneUI test suite deterministic again.
2. Freeze the UTF-8 C ABI and Rust binding requirements.
3. Design and implement the Kylin Linux platform backend contract.
4. Build a native shell smoke application that exercises a real Rust callback,
   logical DPI, Chinese text, keyboard input, clipboard, and clean shutdown.

No iShellPro production page is migrated before these gates are met.
