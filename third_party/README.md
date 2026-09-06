# Third-Party Source Policy

OneUI product builds must not depend on SDK users or end users installing third-party
graphics/text packages.

This directory is reserved for vendored third-party source and build recipes.

Dependency layout (Skia's DEPS owns text/image dependencies; do not add a parallel segmentation stack):

```text
third_party/
  skia/
    third_party/externals/  HarfBuzz, ICU, FreeType, image libraries
```

Full text modules are mandatory: SkParagraph, SkShaper/HarfBuzz and SkUnicode/ICU.
The normal SDK remains dynamic but embeds its text dependencies. There is no 5 MB hard cap
or separate reduced-text edition. Static archives remain separate development artifacts.
The installed MSYS2 milestone-143 package lacks required internal headers and does not match
the pinned milestone-150 source; its old demo baseline is not a passing full-text build.

See `../docs/05-static-skia.md` and `../scripts/build-skia-static.ps1`.

## Native desktop build pin

Linux/macOS development uses Skia revision
`1f26101197bff9fcd939a791beb3094297436d59` and its DEPS / fetch-gn tool versions.
Fetch into the ignored checkout, then build with `scripts/build-skia-posix.sh`:

```sh
git clone https://skia.googlesource.com/skia.git third_party/skia
git -C third_party/skia checkout 1f26101197bff9fcd939a791beb3094297436d59
(cd third_party/skia && python3 tools/git-sync-deps && python3 bin/fetch-gn)
bash scripts/build-skia-posix.sh
```

Do not run the clone over an existing checkout or discard local third-party
changes. Linux is currently tested with Clang/LLVM 18; macOS toolchain/build
verification is pending. See `../docs/37-native-desktop-backends.md`.

The pinned DEPS provides ICU 74.2 / Unicode 15.1 and HarfBuzz commit
`9cb1fee51069b206effb4736e443b038d230789d`. Windows embeds ICU data; deployment must
not require a neighboring `icudtl.dat`. The base revision remains fixed; OneUI's
[reviewed Bidi patch](patches/README.md) fixes the 30 Unicode 15.1 conformance failures.
Build scripts apply it idempotently before rebuilding all affected archives, and
configuration verifies its source hashes. Never suppress failed fixtures or
substitute system ICU silently. SDK license bundling and host ICU/HarfBuzz symbol
coexistence still require release audit. Skia uses BSD-3-Clause, HarfBuzz its bundled
MIT-style notices, and ICU its bundled Unicode/third-party notices; distribute the
actual pinned license files, not just this summary.

Acceptance fonts and Unicode datasets are pinned with SHA-256 in `../tests/text-assets.cmake`.
Fetch with `cmake -P scripts/fetch-text-test-assets.cmake`. Fonts are OFL-1.1 and Unicode
fixtures Unicode-3.0; downloaded licenses accompany the fixtures in ignored `out/text-assets`.
They are test inputs, not automatically part of the shipped SDK.
