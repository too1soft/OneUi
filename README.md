# OneUI

OneUI is a small, self-drawn desktop UI framework planned for Windows, Linux, and macOS.

The first milestone is intentionally narrow: a reliable Win32 window, a tiny widget tree,
basic drawing, and a Button in a gallery app. The project favors a compact, testable core
over a broad component catalog.

## Product Shape

- One codebase for desktop UI components.
- Self-drawn controls for visual consistency.
- Thin platform layers for windows, input, clipboard, DPI, fonts, and system integration.
- Windows 7 is a first-class compatibility target.
- Skia is the main renderer from the start. The Win32 backend currently paints into a Skia
  raster surface and presents it to the native window.
- Product builds must have zero end-user-installed dependencies. MSYS2 is a development
  bootstrap only, not the final distribution model.

## Repository Layout

```text
docs/             Design and engineering decisions
include/oneui/    Public C++ headers
src/core/         Cross-platform UI core
src/platform/     Platform-specific adapters
examples/gallery/ Small executable that exercises controls
```

## Build

On Windows with MSYS2 MINGW64 installed, use the fast development build:

```powershell
C:\msys64\mingw64\bin\cmake.exe --preset mingw64
C:\msys64\mingw64\bin\cmake.exe --build --preset mingw64
.\scripts\run-gallery.ps1
```

Current Windows build outputs:

```text
build/mingw64/oneui.dll
build/mingw64/liboneui.dll.a
build/mingw64/examples/gallery/oneui_gallery.exe
```

`oneui_gallery.exe` is an SDK-user style example: it links against `oneui.dll` instead of
directly linking OneUI internals.

Package the current development SDK:

```powershell
.\scripts\package-sdk.ps1 -Toolchain mingw64
```

The current Windows product build uses MSVC plus vendored static Skia:

```powershell
.\scripts\build-skia-static.ps1 -Fetch -SyncDeps -Generate -Build -Proxy http://127.0.0.1:7890
.\scripts\build-oneui-msvc-bundled.ps1
.\scripts\audit-runtime.ps1 -Binary .\build\msvc-bundled-static\oneui.dll -Mode product
.\scripts\audit-runtime.ps1 -Binary .\build\msvc-bundled-static\examples\gallery\oneui_gallery.exe -Mode product -AllowOneUI
.\scripts\package-sdk.ps1 -Toolchain msvc-bundled-static
.\scripts\test-sdk-consumer.ps1
```

Product outputs:

```text
build/msvc-bundled-static/oneui.dll
build/msvc-bundled-static/oneui.lib
build/msvc-bundled-static/examples/gallery/oneui_gallery.exe
dist/OneUI-SDK-msvc-bundled-static.zip
```

Audit runtime dependencies in either mode:

```powershell
.\scripts\audit-runtime.ps1 -Binary .\build\mingw64\oneui.dll -Mode development
.\scripts\audit-runtime.ps1 -Binary .\build\mingw64\oneui.dll -Mode product
```

`development` mode allows the temporary MSYS2 Skia runtime. `product` mode is expected to
pass only when Skia and its text/image dependencies are vendored and statically linked.

Only the Win32 backend exists right now. Linux and macOS backends are planned after the core
model is stable. See `docs/04-dependency-policy.md` before changing renderer or packaging
choices.

See `docs/06-design-language.md` for the HTML/Vue3/MVVM-inspired component authoring model.
See `docs/07-component-inventory.md` for the full component catalog and known gaps.
See `docs/08-agent-iteration-process.md` for the PM -> Developer -> Tester iteration loop.
