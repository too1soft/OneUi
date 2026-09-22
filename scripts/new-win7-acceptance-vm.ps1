param(
    [Parameter(Mandatory = $true)][string]$Iso,
    [Parameter(Mandatory = $true)][string]$VmRoot,
    [ValidateSet('x86', 'x64')][string]$Arch = 'x86',
    [string]$VBoxManage = 'D:\VitualBox\VBoxManage.exe',
    [string]$VmName = '',
    [int]$ImageIndex = 5
)
$ErrorActionPreference = 'Stop'
if (!$VmName) { $VmName = "OneUI-Win7SP1-$Arch-" + (Get-Date -Format 'yyyyMMdd-HHmmss') }
if ($VmName -notmatch '^OneUI-Win7SP1-[A-Za-z0-9-]+$') { throw 'Only isolated OneUI acceptance VM names are allowed' }
$vmRootPath = [IO.Path]::GetFullPath($VmRoot)
if ($vmRootPath -eq [IO.Path]::GetPathRoot($vmRootPath)) { throw 'A dedicated VM subdirectory is required' }
if (!(Test-Path -LiteralPath $Iso) -or !(Test-Path -LiteralPath $VBoxManage)) { throw 'ISO or VBoxManage is missing' }
$existing = & $VBoxManage list vms
if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate VirtualBox machines' }
if ($existing -match ('^"' + [regex]::Escape($VmName) + '"')) { throw 'VM already exists; refusing to modify it' }
$vmDirectory = Join-Path $vmRootPath $VmName
if (Test-Path -LiteralPath $vmDirectory) { throw 'VM directory already exists; refusing to overwrite it' }

function Invoke-VMCommand {
    param([string[]]$Arguments)
    $result = & $VBoxManage @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        # Unattended output can contain generated credentials. Never print it.
        if ($Arguments[0] -ne 'unattended') { $result | Write-Host }
        throw "VirtualBox $($Arguments[0]) failed (exit $LASTEXITCODE)"
    }
    if ($Arguments[0] -ne 'unattended') { $result | Write-Host }
}

$osType = if ($Arch -eq 'x64') { 'Windows7_64' } else { 'Windows7' }
Invoke-VMCommand @('createvm', '--name', $VmName, '--ostype', $osType, '--basefolder', $vmRootPath, '--register')
Invoke-VMCommand @('modifyvm', $VmName, '--memory', '2048', '--cpus', '2', '--vram', '128', '--graphicscontroller', 'vboxsvga', '--accelerate3d', 'off', '--nic1', 'none', '--boot1', 'dvd', '--boot2', 'disk')
$disk = Join-Path $vmDirectory 'acceptance.vdi'
Invoke-VMCommand @('createmedium', 'disk', '--filename', $disk, '--size', '24576', '--format', 'VDI')
Invoke-VMCommand @('storagectl', $VmName, '--name', 'SATA', '--add', 'sata', '--controller', 'IntelAhci', '--portcount', '2')
Invoke-VMCommand @('storageattach', $VmName, '--storagectl', 'SATA', '--port', '0', '--device', '0', '--type', 'hdd', '--medium', $disk)

# Generated test-only credential; no existing VM account or password is used.
$passwordFile = Join-Path $vmDirectory 'acceptance-password.private'
[IO.File]::WriteAllText($passwordFile, ([guid]::NewGuid().ToString('N') + 'Q7!'), [Text.Encoding]::ASCII)
Invoke-VMCommand @('unattended', 'install', $VmName, "--iso=$([IO.Path]::GetFullPath($Iso))", '--user=oneuiqa', "--user-password-file=$passwordFile", '--full-user-name=OneUI Acceptance', '--install-additions', "--image-index=$ImageIndex", '--locale=zh_CN', '--country=CN', '--time-zone=Asia/Shanghai', "--hostname=oneui-$Arch.test", '--start-vm=headless')
Write-Host "Created isolated, network-disconnected VM: $VmName"
Write-Host "Existing VMs and installed software were not changed. VM files: $vmDirectory"
