[CmdletBinding()]
param(
    [string]$Exe,
    [string]$BuildDir,
    [string]$Out,
    [int]$TimeoutSeconds = 8,
    [int]$SettleMilliseconds = 700,
    [switch]$ExerciseHover
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $root "build\msvc-bundled-static"
}
if ([string]::IsNullOrWhiteSpace($Exe)) {
    $Exe = Join-Path $BuildDir "examples\remote_component_gallery\oneui_remote_component_gallery.exe"
}
if ([string]::IsNullOrWhiteSpace($Out)) {
    $Out = Join-Path $BuildDir "oneui-remote-component-gallery-smoke.png"
}

$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$Exe = [System.IO.Path]::GetFullPath($Exe)
$Out = [System.IO.Path]::GetFullPath($Out)

if (-not (Test-Path -LiteralPath $BuildDir -PathType Container)) {
    throw "Build directory was not found: $BuildDir"
}
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
    throw "OneUI Remote component gallery executable was not found: $Exe"
}
if (-not (Test-Path -LiteralPath (Join-Path $BuildDir "oneui.dll") -PathType Leaf)) {
    throw "oneui.dll was not found in build directory: $BuildDir"
}

$outDir = Split-Path -Parent $Out
if (-not (Test-Path -LiteralPath $outDir -PathType Container)) {
    New-Item -ItemType Directory -Path $outDir | Out-Null
}

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class OneUiGallerySmokeWin32 {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdcBlt, uint nFlags);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int X, int Y);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    public static IntPtr FindGalleryWindow(uint pid) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((hWnd, lParam) => {
            if (!IsWindowVisible(hWnd)) {
                return true;
            }
            uint windowPid;
            GetWindowThreadProcessId(hWnd, out windowPid);
            if (windowPid != pid) {
                return true;
            }
            var title = new StringBuilder(256);
            GetWindowText(hWnd, title, title.Capacity);
            if (title.ToString().StartsWith("OneUI Remote Component Gallery", StringComparison.Ordinal)) {
                found = hWnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@

$oldPath = $env:PATH
$env:PATH = "$BuildDir;$oldPath"
$process = $null

try {
    Write-Host "==> Launch OneUI Remote component gallery"
    Write-Host "    exe: $Exe"
    Write-Host "    build: $BuildDir"
    $process = Start-Process -FilePath $Exe -WorkingDirectory $BuildDir -PassThru

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $hwnd = [IntPtr]::Zero
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($process.HasExited) {
            throw "OneUI gallery exited during screenshot smoke with code $($process.ExitCode)"
        }
        $hwnd = [OneUiGallerySmokeWin32]::FindGalleryWindow([uint32]$process.Id)
        if ($hwnd -ne [IntPtr]::Zero) {
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if ($hwnd -eq [IntPtr]::Zero) {
        throw "OneUI Remote component gallery window was not found for pid $($process.Id)"
    }

    $rect = New-Object OneUiGallerySmokeWin32+RECT
    if (-not [OneUiGallerySmokeWin32]::GetWindowRect($hwnd, [ref]$rect)) {
        throw "GetWindowRect failed"
    }

    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -lt 640 -or $height -lt 480) {
        throw "OneUI gallery window is too small: ${width}x${height}"
    }

    if ($SettleMilliseconds -gt 0) {
        Start-Sleep -Milliseconds $SettleMilliseconds
    }

    if ($ExerciseHover) {
        [void][OneUiGallerySmokeWin32]::SetForegroundWindow($hwnd)
        $points = @(
            @(0.04, 0.04),
            @(0.14, 0.18),
            @(0.46, 0.08),
            @(0.86, 0.08),
            @(0.34, 0.39),
            @(0.74, 0.39),
            @(0.36, 0.60),
            @(0.56, 0.60),
            @(0.74, 0.60),
            @(0.88, 0.90)
        )
        foreach ($point in $points) {
            $screenX = [int]($rect.Left + ($width * [double]$point[0]))
            $screenY = [int]($rect.Top + ($height * [double]$point[1]))
            [void][OneUiGallerySmokeWin32]::SetCursorPos($screenX, $screenY)
            Start-Sleep -Milliseconds 80
            if ($process.HasExited) {
                throw "OneUI gallery exited during hover exercise with code $($process.ExitCode)"
            }
        }
        Start-Sleep -Milliseconds 250
    }

    $file = $null
    for ($attempt = 1; $attempt -le 5; $attempt++) {
        $bitmap = New-Object System.Drawing.Bitmap $width, $height
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $hdc = $graphics.GetHdc()
        try {
            [void][OneUiGallerySmokeWin32]::PrintWindow($hwnd, $hdc, 2)
        }
        finally {
            $graphics.ReleaseHdc($hdc)
            $graphics.Dispose()
        }

        $bitmap.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()

        $file = Get-Item -LiteralPath $Out
        if ($file.Length -ge 10000) {
            break
        }
        Start-Sleep -Milliseconds 250
    }

    if ($null -eq $file -or $file.Length -lt 10000) {
        throw "Screenshot is unexpectedly small: $($file.Length) bytes"
    }

    $sample = [System.Drawing.Bitmap]::FromFile($Out)
    try {
        $total = 0
        $dark = 0
        $bright = 0
        $accent = 0
        for ($y = 0; $y -lt $sample.Height; $y += 8) {
            for ($x = 0; $x -lt $sample.Width; $x += 8) {
                $pixel = $sample.GetPixel($x, $y)
                $total += 1
                if ($pixel.R -lt 48 -and $pixel.G -lt 48 -and $pixel.B -lt 56) {
                    $dark += 1
                }
                if ($pixel.R -gt 220 -and $pixel.G -gt 220 -and $pixel.B -gt 220) {
                    $bright += 1
                }
                if (($pixel.B -gt 145 -and $pixel.R -gt 35 -and $pixel.R -lt 140) -or
                    ($pixel.B -gt 170 -and $pixel.G -gt 70 -and $pixel.R -lt 110)) {
                    $accent += 1
                }
            }
        }

        if ($total -le 0) {
            throw "Screenshot pixel sampling failed"
        }
        $darkRatio = $dark / $total
        $brightRatio = $bright / $total
        if ($darkRatio -lt 0.45) {
            throw ("Screenshot does not look like the expected dark component gallery. darkRatio={0:N3}" -f $darkRatio)
        }
        if ($brightRatio -gt 0.30) {
            throw ("Screenshot looks too bright or blank. brightRatio={0:N3}" -f $brightRatio)
        }
        if ($accent -lt 12) {
            throw "Screenshot is missing expected blue/purple accent surfaces"
        }
        Write-Host ("    visual smoke: dark={0:N3} bright={1:N3} accentSamples={2}" -f $darkRatio, $brightRatio, $accent)
    }
    finally {
        $sample.Dispose()
    }

    Write-Host "OneUI Remote component gallery screenshot smoke passed"
    Write-Host "    $Out"
}
finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit()
    }
    $env:PATH = $oldPath
}
