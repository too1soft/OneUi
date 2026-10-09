#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
skia="${ONEUI_BUNDLED_SKIA_ROOT:-$root/third_party/skia}"
revision=1f26101197bff9fcd939a791beb3094297436d59
if [[ "$(git -C "$skia" rev-parse HEAD)" != "$revision" ]]; then
    echo "Expected Skia $revision. Fetch the pinned source and its DEPS before building." >&2
    exit 1
fi
os="$(uname -s | tr '[:upper:]' '[:lower:]')"
arch="${ONEUI_TARGET_ARCH:-$(uname -m)}"
case "$arch" in x86_64) cpu=x64 ;; aarch64|arm64) cpu=arm64 ;; *) echo "Unsupported architecture: $arch" >&2; exit 1 ;; esac
out="${ONEUI_BUNDLED_SKIA_OUT:-$skia/out/oneui-$os-$arch-release}"
python3 "$root/scripts/check-build-space.py" "$out" "${TMPDIR:-/tmp}"
cmake "-DONEUI_ICU_ROOT=$skia/third_party/externals/icu" -P "$root/scripts/apply-skia-patches.cmake"
gpu="${ONEUI_SKIA_GPU:-1}"
case "$gpu" in 0) ganesh=false ;; 1) ganesh=true ;; *) echo 'ONEUI_SKIA_GPU must be 0 or 1' >&2; exit 1 ;; esac
gn="${ONEUI_GN:-$skia/bin/gn}"
if [[ ! -x "$gn" ]]; then
    if [[ -n "${ONEUI_GN:-}" ]]; then echo "ONEUI_GN is not executable: $gn" >&2; exit 1; fi
    python3 "$skia/bin/fetch-gn"
fi
archiver="${ONEUI_AR:-llvm-ar}"
if [[ "$os" == darwin && -z "${ONEUI_AR:-}" ]]; then archiver="$(xcrun --find ar)"; fi
args="is_debug=false is_official_build=true target_cpu=\"$cpu\" cc=\"clang\" cxx=\"clang++\" ar=\"$archiver\"
skia_enable_ganesh=$ganesh skia_enable_graphite=false skia_enable_pdf=false skia_enable_skottie=false
skia_enable_svg=false skia_enable_tools=false skia_use_dng_sdk=false skia_use_wuffs=false
skia_use_system_expat=false skia_use_system_harfbuzz=false skia_use_system_icu=false
skia_use_icu=true skia_use_harfbuzz=true skia_enable_skparagraph=true skia_enable_skshaper=true skia_enable_skunicode=true
skia_use_system_libjpeg_turbo=false skia_use_system_libpng=false skia_use_system_libwebp=false
skia_use_system_zlib=false skia_use_system_freetype2=false extra_cflags=[\"-fPIC\"]"
if [[ "$os" == darwin ]]; then
    args="$args skia_use_metal=$ganesh skia_use_gl=false
skia_use_freetype=true skia_enable_fontmgr_custom_empty=true"
else
    args="$args skia_use_gl=$ganesh skia_use_egl=$ganesh skia_use_vulkan=false"
fi
"$gn" gen "$out" --root="$skia" --args="$args"
ninja -C "$out" -j "${ONEUI_BUILD_JOBS:-6}" skia skparagraph skunicode_icu
