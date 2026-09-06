#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="${1:?Usage: package-sdk-posix.sh BUILD_DIRECTORY [OUTPUT_DIRECTORY]}"
build="$(cd "$build" && pwd)"
output="${2:-$root/dist}"
mkdir -p "$output"
output="$(cd "$output" && pwd)"
case "$(uname -s)" in
    Linux)
        . /etc/os-release
        baseline="$ID-$VERSION_ID"
        session="${XDG_SESSION_TYPE:-unknown}"
        if [[ "$(uname -r)" == *[Mm]icrosoft* ]]; then session="wslg-development-only"; fi
        ;;
    Darwin) baseline="macos-$(sw_vers -productVersion)"; session="cocoa" ;;
    *) echo "This packager requires Linux or macOS" >&2; exit 1 ;;
esac
arch="$(uname -m)"
name="OneUI-SDK-$baseline-$arch"
archive="$output/$name.tar.gz"
if [[ -e "$archive" ]]; then echo "Refusing to overwrite $archive" >&2; exit 1; fi
stage="$(mktemp -d "$output/sdk-stage.XXXXXXXX")"
sdk="$stage/$name"
cmake --install "$build" --prefix "$sdk" --strip
{
    printf 'baseline=%s\narchitecture=%s\nsession=%s\n' "$baseline" "$arch" "$session"
    printf 'source_revision=%s\nsource_dirty=%s\n' "$(git -C "$root" rev-parse HEAD)" "$(test -z "$(git -C "$root" status --porcelain)" && echo false || echo true)"
    printf 'native_acceptance=pending\nskia_revision=1f26101197bff9fcd939a791beb3094297436d59\n'
    printf 'compiler_cache_begin\n'
    sed -n '/^CMAKE_\(CXX_COMPILER\|C_COMPILER\|OSX_ARCHITECTURES\|OSX_DEPLOYMENT_TARGET\|SYSTEM_PROCESSOR\):/p' "$build/CMakeCache.txt"
} > "$sdk/BUILD-BASELINE.txt"
bash "$root/scripts/audit-runtime-posix.sh" "$sdk"
if find "$sdk" -type f \( -name '*.a' -o -name '*.lib' \) -print -quit | grep -q .; then
    echo 'Static archives must not enter the ordinary SDK' >&2; exit 1
fi
tar -czf "$archive" -C "$stage" "$name"
du -h "$archive"
printf 'SDK staging directory retained for consumer checks: %s\n' "$sdk"
