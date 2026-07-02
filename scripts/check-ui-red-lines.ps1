[CmdletBinding()]
param(
    [string]$OneUiRoot,
    [string]$RemoteRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($OneUiRoot)) {
    $OneUiRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
}
if ([string]::IsNullOrWhiteSpace($RemoteRoot)) {
    $RemoteRoot = "E:\project\byname\remote"
}

$scanRoots = @(
    (Join-Path $OneUiRoot "src"),
    (Join-Path $OneUiRoot "include"),
    (Join-Path $RemoteRoot "clients\rust-oneui-shell")
) | Where-Object { Test-Path -LiteralPath $_ }

$rules = @(
    @{
        Id = "NO_DIRTY_RECT_SIDEBAR_X_MAGIC";
        Pattern = "(requestRedrawRect|requestRedraw|invalidate|dirty|repaint).*(rect\.x|sidebar)|(rect\.x|sidebar).*(requestRedrawRect|requestRedraw|invalidate|dirty|repaint)";
        Scope = "all";
        Message = "Do not special-case sidebar or rect.x in dirty rect, repaint, or invalidate paths. Fix generic invalidation, clipping, and painting instead.";
    },
    @{
        Id = "NO_REMOTE_PRODUCT_BRANCH_IN_ONEUI_CORE";
        Pattern = "isRemoteHome|RemoteHome|remote-home|device\s*code|temporary\s*code|temporaryCode";
        Scope = "oneui";
        Message = "Do not put Remote product branches or business terms into OneUI core.";
    }
)

$violations = New-Object System.Collections.Generic.List[string]

foreach ($root in $scanRoots) {
    $files = Get-ChildItem -LiteralPath $root -Recurse -File |
        Where-Object { $_.Extension -in @(".cpp", ".cc", ".cxx", ".h", ".hpp", ".rs") }

    foreach ($file in $files) {
        $isOneUiFile = $file.FullName.StartsWith((Join-Path $OneUiRoot ""), [System.StringComparison]::OrdinalIgnoreCase)
        $lines = Get-Content -LiteralPath $file.FullName
        for ($index = 0; $index -lt $lines.Count; $index++) {
            $line = $lines[$index]
            foreach ($rule in $rules) {
                if ($rule["Scope"] -eq "oneui" -and -not $isOneUiFile) {
                    continue
                }
                if ($line -match $rule["Pattern"]) {
                    $lineNumber = $index + 1
                    $message = "[{0}] {1}:{2}: {3}`n    {4}" -f @(
                        $rule["Id"],
                        $file.FullName,
                        $lineNumber,
                        $rule["Message"],
                        $line.Trim()
                    )
                    $violations.Add($message)
                }
            }
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Host "OneUI UI red-line scan failed:" -ForegroundColor Red
    $violations | ForEach-Object { Write-Host $_ -ForegroundColor Red }
    exit 1
}

Write-Host "OneUI UI red-line scan passed."
