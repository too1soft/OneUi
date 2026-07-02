# Third-Party Source Policy

OneUI product builds must not depend on SDK users or end users installing third-party
graphics/text packages.

This directory is reserved for vendored third-party source and build recipes.

Planned contents:

```text
third_party/
  skia/
  harfbuzz/
  freetype/
  fonts/
```

The first major dependency target is a static Skia build that includes the text and image
features OneUI needs. MSYS2 dynamic Skia remains acceptable only for development demos.

See `../docs/05-static-skia.md` and `../scripts/build-skia-static.ps1`.
