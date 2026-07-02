# OneUI DPI And GDI Scaling Contract

Date: 2026-05-28

This document defines the DPI and presentation contract for OneUI platform backends. It exists to prevent product-level DPI hacks and to make the Win32/GDI path reusable for Remote, Wangyunchuan, iShell, and future products.

## Core Rule

OneUI widgets, layout, style, input hit-testing, animation, and invalidation use logical device-independent pixels.

Platform backends own all conversion between logical coordinates and physical pixels:

- window creation size
- backing surface size
- dirty rectangle conversion
- mouse coordinate conversion
- resize/fullscreen geometry
- per-monitor DPI changes
- final presentation to the native window

Remote and other downstream products must not calculate DPI scale, GDI scale, monitor scale, or physical backing size.

## Win32/GDI Contract

The Win32 backend uses a physical-pixel Skia raster backing surface and presents that surface through GDI.

The correct pipeline is:

1. The app requests a logical window size, for example `980 x 720`.
2. Win32 resolves the current DPI scale, for example `1.25`.
3. The backend creates a native client area of `1225 x 900` physical pixels.
4. The backend creates or reuses a Skia raster surface large enough for physical pixels.
5. Before painting widgets, the backend applies `canvas.scale(dpiScale, dpiScale)`.
6. Widgets still receive logical `Rect`, `Size`, and `Point` values.
7. Dirty rects are converted from logical pixels to physical pixels before `InvalidateRect`.
8. Mouse coordinates are converted from physical client pixels to logical points before dispatch.
9. GDI blits the physical surface to the physical client area without low-resolution stretching.

This means GDI is not allowed to stretch a low-DPI frame to a high-DPI window. The backing surface itself must be high-DPI.

## Public Window Metrics

`oneui::Window` exposes three distinct concepts:

- `clientSize()`: logical client size in OneUI device-independent pixels.
- `clientPixelSize()`: physical backing/presentation size in platform pixels.
- `dpiScale()`: physical pixels per OneUI logical pixel.

The C ABI mirrors these values:

- `oneui_window_client_width`
- `oneui_window_client_height`
- `oneui_window_client_pixel_width`
- `oneui_window_client_pixel_height`
- `oneui_window_dpi_scale`

Downstream products may read these values for diagnostics or native integration, but layout must remain in logical pixels.

## DPI Awareness

The Win32 backend must enable DPI awareness dynamically and remain Win7-compatible:

- Try `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` when available.
- Fall back to `SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE)` when available.
- Fall back to `SetProcessDPIAware()` on older systems.
- Resolve modern APIs dynamically with `GetProcAddress`; do not statically require APIs missing on Win7.

`WM_DPICHANGED` must update backend scale, recreate or invalidate physical backing resources, apply the suggested window rect when appropriate, and schedule a repaint through the standard window pipeline.

## Cross-Platform Mapping

The abstraction is not Win32-specific.

macOS backend should map:

- logical coordinates: Cocoa points
- physical pixels: backing pixels
- scale source: `backingScaleFactor`
- DPI changes: screen/backing property changes

Linux backend should map:

- logical coordinates: toolkit-independent OneUI units
- physical pixels: X11/Wayland buffer pixels
- scale source: Wayland output scale, fractional scale protocol, Xft DPI, or compositor-provided scale
- DPI changes: output enter/leave and scale-change events

Each backend must keep the same public contract. Widgets and products must not know which platform supplied the scale.

## Red Lines

Forbidden:

- Product-specific DPI code in Remote.
- Component-specific scale branches such as `if TextField then scale differently`.
- Fixed monitor assumptions such as `scale = 1.25`.
- GDI low-resolution stretching as a final quality solution.
- Physical mouse coordinates leaking into widget hit-testing.
- Physical dirty rectangles leaking into widget invalidation APIs.
- Static linkage to modern Windows DPI APIs that breaks Win7 launch.

Required:

- Logical layout and physical backing must be separate.
- Input conversion must happen before widget dispatch.
- Dirty rect conversion must happen at the platform boundary.
- Full repaint after DPI change must go through the standard scheduler.
- Backend contract tests must verify logical-to-physical size relation.

## Verification

Minimum checks for this contract:

- `oneui_backend_contract_tests` verifies `clientSize`, `clientPixelSize`, and `dpiScale`.
- `ctest --test-dir build/msvc-bundled-static --output-on-failure` must pass.
- Remote shell screenshot smoke must still render correctly with the bundled OneUI DLL.
- Manual high-DPI checks should include 100%, 125%, 150%, fullscreen, restore, and resize.

