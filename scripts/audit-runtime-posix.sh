#!/usr/bin/env bash
set -euo pipefail
sdk="${1:?Usage: audit-runtime-posix.sh SDK_DIRECTORY}"
case "$(uname -s)" in
    Linux)
        for binary in "$sdk/lib/liboneui.so" "$sdk/bin/oneui_gallery"; do
            file "$binary"
            dependencies="$(ldd "$binary")"
            printf '%s\n' "$dependencies"
            if [[ "$dependencies" == *'not found'* ]]; then exit 1; fi
            if readelf -d "$binary" | grep -E 'RPATH|RUNPATH' | grep -qE '/(home|mnt|tmp|Users)/'; then
                echo "Non-relocatable runtime path in $binary" >&2; exit 1
            fi
        done
        ;;
    Darwin)
        for binary in "$sdk/lib/liboneui.dylib" "$sdk/examples/gallery/oneui_gallery.app/Contents/MacOS/oneui_gallery"; do
            file "$binary"
            dependencies="$(otool -L "$binary")"
            printf '%s\n' "$dependencies"
            if printf '%s\n' "$dependencies" | tail -n +2 | grep -qE '/(Users|opt/homebrew|usr/local|private/tmp)/'; then
                echo "Unexpected non-system dependency in $binary" >&2; exit 1
            fi
        done
        ;;
    *) exit 1 ;;
esac
