# DEBIL OS launcher
# Usage:  .\run.ps1            (QEMU window + serial mirror in this console)
#         .\run.ps1 -NoGui     (serial console only, good for SSH / automation)
param(
    [switch]$NoGui,
    [switch]$Iso,
    [int]$MemoryMB = 256
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
if ($Iso) {
    $image = Join-Path $root 'build\debil.iso'
    $media = 'ISO'
    if (-not (Test-Path $image)) {
        throw "build\debil.iso not found. Run .\build.ps1 first."
    }
} else {
    $image = Join-Path $root 'build\debil.img'
    $media = 'raw disk image'
    if (-not (Test-Path $image)) {
        throw "build\debil.img not found. Run .\build.ps1 first."
    }
}

$candidates = @(
    (Get-Command qemu-system-i386 -ErrorAction SilentlyContinue).Source,
    'C:\Program Files\qemu\qemu-system-i386.exe',
    'C:\Program Files (x86)\qemu\qemu-system-i386.exe',
    (Join-Path $env:TEMP 'qemu-2024-extract\qemu-system-i386.exe')
) | Where-Object { $_ -and (Test-Path $_) }

$qemu = $candidates | Select-Object -First 1
if (-not $qemu) {
    Write-Host "QEMU (qemu-system-i386) was not found." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Install QEMU for Windows from:"
    Write-Host "  https://qemu.weilnetz.de/w64/"
    Write-Host ""
    Write-Host "After installing, either add its folder to PATH or copy"
    Write-Host "qemu-system-i386.exe next to this script's build folder."
    Write-Host ""
    Write-Host "The bootable image is ready at:"
    Write-Host "  $image"
    exit 1
}

$qemuArgs = @(
    '-name', 'DEBIL-OS',
    '-m', "$MemoryMB"
)

if ($Iso) {
    $qemuArgs += @('-boot', 'd', '-cdrom', $image)
} else {
    $qemuArgs += @('-boot', 'c', '-drive', "file=$image,format=raw,if=ide")
}

$qemuArgs += @(
    '-vga', 'std',
    '-rtc', 'base=localtime',
    '-serial', 'stdio'
)

if ($NoGui) {
    $qemuArgs += @('-display', 'none', '-monitor', 'none')
}

Write-Host "Booting DEBIL OS from $media with $qemu" -ForegroundColor Cyan
Write-Host "Serial console is connected to this terminal."
if (-not $NoGui) {
    Write-Host "Use the QEMU window (or type into this terminal) to interact."
}
Write-Host ""

& $qemu @qemuArgs
