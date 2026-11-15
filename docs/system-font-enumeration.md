# Installed system font enumeration

OneUI provides a platform-neutral installed-font snapshot for font pickers and
settings. Applications must not scan font folders or duplicate OS-specific code.

- C++: `oneui::enumerateSystemFonts()` in `<oneui/fonts.h>`.
- C ABI: `oneui_enumerate_system_fonts(callback, user_data)`.
- Rust: `oneui::enumerate_system_fonts() -> Result<Vec<FontFamilyInfo>, Error>`.

Each record has a UTF-8 `family` usable by OneUI font setters and `monospaced`,
the normal face's fixed-pitch metadata. This is not a guarantee that every script
or fallback glyph is monospaced. The snapshot excludes fonts registered only
in OneUI's process-local font provider; callers can merge their own embedded
font catalog if appropriate. Results are sorted by UTF-8 family name and deduplicated.

The implementation reuses OneUI's existing Skia platform font manager:
Windows DirectWrite (GDI fallback), macOS CoreText, Linux Fontconfig. There are
no product-specific OS branches. A window and UI thread are not required.
Enumerate on a worker and cache the result in the consumer; call again on an
explicit refresh. OS font-cache visibility governs recently installed fonts.
An empty installation returns an empty snapshot. Failure to create the platform
manager raises a C++ exception / returns -1 through C / FontEnumerationFailed in Rust.

C callbacks run synchronously on the calling thread. Copy the borrowed string
before returning. They must not throw or unwind. A null callback returns the
count. The safe Rust binding owns its strings and catches callback panics.
This is an additive C ABI extension: use headers, Rust bindings and binary from
the same SDK build. No existing ABI struct or calling convention changed.

Validation: `oneui_system_fonts_tests` and `oneui_system_fonts_static_tests` compare the public C++ and C ABI results
on a worker without creating a window in dynamic and static builds. The C/Rust
ABI manifest includes the new entry point and passes `oneui_abi_sync`. `cargo test -p oneui --test system_fonts`
checks the safe Rust boundary. Windows executed locally on 2026-09-30. On
2026-10-05, Linux x86_64 WSLg dynamic/static C++ and C ABI tests passed after
fixing archive symbol visibility; the Rust suite was not rerun in that review.
macOS remains unbuilt and untested. See the [platform validation record](37-native-desktop-backends.md).

## Embedded font storage

`oneui_font_register_memory` / Rust `register_font_from_memory` copy their input;
temporary decoded font buffers can be dropped after the call. ABI 47 adds
`oneui_font_register_static_memory` / Rust `register_font_from_static_memory` for
immutable bytes that remain valid until process exit. The Rust API requires
`&'static [u8]`, suitable for executable `include_bytes!` assets. It retains those
bytes without another heap copy; it does not transfer ownership or install a
system font. Dynamic/decompressed buffers must continue using the copying API.
Both paths share alias validation, deduplication and variable-weight handling.

The Rust registration and bundled-font helpers now live with enumeration and
measurement in `fonts.rs`, with their original crate-root exports preserved.
Windows Release validation covers the C/Rust ABI (1377 checks and 649 typed
function signatures), rejection of a temporary Rust font buffer at compile time,
and the product's normal/bold Latin/CJK rendering. This review did not run the
new static-registration path on Linux or macOS.
