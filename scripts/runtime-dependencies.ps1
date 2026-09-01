$script:OneUISystemDlls = @(
    "advapi32.dll",
    "comctl32.dll",
    "comdlg32.dll",
    "dwmapi.dll",
    "dwrite.dll",
    "gdi32.dll",
    "imm32.dll",
    "kernel32.dll",
    "msvcrt.dll",
    "ole32.dll",
    "oleaut32.dll",
    "opengl32.dll",
    "rpcrt4.dll",
    "shell32.dll",
    "shlwapi.dll",
    "user32.dll",
    "usp10.dll",
    "uuid.dll",
    "winmm.dll",
    "winspool.drv",
    "ws2_32.dll"
)

function Get-OneUIImportedDlls {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Binary,

        [Parameter(Mandatory = $true)]
        [string]$Objdump
    )

    $output = & $Objdump -p $Binary
    if ($LASTEXITCODE -ne 0) {
        throw "objdump failed for $Binary with exit code $LASTEXITCODE"
    }

    $output |
        Select-String "DLL Name:\s*(.+)$" |
        ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
}

function Test-OneUISystemDll {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $lower = $Name.ToLowerInvariant()
    return $script:OneUISystemDlls -contains $lower -or
        $lower.StartsWith("api-ms-win-") -or
        $lower.StartsWith("ext-ms-win-")
}

function Copy-OneUIRuntimeDependencies {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$SeedBinaries,

        [Parameter(Mandatory = $true)]
        [string]$DestinationDirectory,

        [Parameter(Mandatory = $true)]
        [string]$SearchDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Objdump,

        [string[]]$IgnoredDlls = @()
    )

    $ignored = @{}
    foreach ($dll in $IgnoredDlls) {
        $ignored[$dll.ToLowerInvariant()] = $true
    }

    $seen = @{}
    $missing = New-Object System.Collections.Generic.List[string]
    $missingDetails = New-Object System.Collections.Generic.List[string]
    $queue = New-Object System.Collections.Generic.Queue[string]
    foreach ($binary in $SeedBinaries) {
        $queue.Enqueue($binary)
    }

    while ($queue.Count -gt 0) {
        $current = $queue.Dequeue()
        foreach ($dll in Get-OneUIImportedDlls -Binary $current -Objdump $Objdump) {
            $key = $dll.ToLowerInvariant()
            if ($seen.ContainsKey($key) -or $ignored.ContainsKey($key) -or (Test-OneUISystemDll -Name $dll)) {
                continue
            }

            $seen[$key] = $true
            $target = Join-Path $DestinationDirectory $dll
            if (Test-Path $target) {
                $queue.Enqueue($target)
                continue
            }

            $source = Join-Path $SearchDirectory $dll
            if (!(Test-Path $source)) {
                $missing.Add($dll)
                $missingDetails.Add("$dll imported by $current")
                continue
            }

            Copy-Item -LiteralPath $source -Destination $target
            $queue.Enqueue($target)
        }
    }

    [PSCustomObject]@{
        MissingDlls = @($missing | Sort-Object -Unique)
        MissingDetails = @($missingDetails | Sort-Object -Unique)
    }
}
