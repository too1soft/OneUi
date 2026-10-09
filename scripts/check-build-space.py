#!/usr/bin/env python3
"""Check output volumes, including the Windows volume backing a WSL distro."""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

GIB = 1024 ** 3


def existing_parent(path):
    path = Path(path).resolve()
    while not path.exists():
        path = path.parent
    return path


def wsl_backing_volume():
    powershell = shutil.which("powershell.exe")
    if not powershell:
        # Build scripts may deliberately exclude Windows tools from PATH.
        converted = subprocess.run(
            ["wslpath", "-u", r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe"],
            check=True, capture_output=True, text=True, timeout=10,
        )
        powershell = converted.stdout.strip()
    distro = os.environ.get("WSL_DISTRO_NAME")
    if not distro:
        raise RuntimeError("WSL_DISTRO_NAME is unavailable; cannot identify the backing volume")
    # Quote a PowerShell literal, not executable shell text from an environment variable.
    distro_literal = "'" + distro.replace("'", "''") + "'"
    command = r"""
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new()
$entries = @(Get-ChildItem 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Lxss' |
    Where-Object { $_.GetValue('DistributionName') -eq DISTRO_LITERAL })
if ($entries.Count -ne 1) { throw 'Cannot identify WSL distribution storage' }
$base = $entries[0].GetValue('BasePath')
if ($base.StartsWith('\\?\')) { $base = $base.Substring(4) }
$root = [IO.Path]::GetPathRoot($base)
$drive = [IO.DriveInfo]::new($root)
[pscustomobject]@{path=$base; free=$drive.AvailableFreeSpace} | ConvertTo-Json -Compress
""".replace("DISTRO_LITERAL", distro_literal)
    completed = subprocess.run(
        [powershell, "-NoProfile", "-NonInteractive", "-Command", command],
        check=True, capture_output=True, text=True, encoding="utf-8", timeout=20,
    )
    result = json.loads(completed.stdout.lstrip("\ufeff"))
    free = result["free"]
    if not isinstance(free, int) or free < 0 or not isinstance(result["path"], str):
        raise RuntimeError("Invalid Windows disk-space response")
    return result["path"], free


def check_space(paths, minimum_gib, host_query=wsl_backing_volume):
    volumes = {}
    for target in paths:
        parent = existing_parent(target)
        volumes[str(parent)] = shutil.disk_usage(parent).free
    if platform.system() == "Linux" and "microsoft" in platform.release().lower():
        path, free = host_query()
        volumes["WSL backing volume: " + path] = free
    low = False
    for path, free in volumes.items():
        print(f"Build space: {free / GIB:.2f} GiB available at {path}")
        if free < minimum_gib * GIB:
            print(f"Insufficient space: at least {minimum_gib:g} GiB required at {path}", file=sys.stderr)
            low = True
    return not low


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--minimum-gib", type=float, default=4)
    parser.add_argument("paths", nargs="+")
    args = parser.parse_args()
    if not 0 < args.minimum_gib < float("inf"):
        parser.error("--minimum-gib must be finite and positive")
    try:
        return 0 if check_space(args.paths, args.minimum_gib) else 1
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print(f"Cannot verify build storage: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
