# DEBIL OS build script (Windows, MinGW-w64 + NASM)
# Usage:  powershell -ExecutionPolicy Bypass -File .\build.ps1
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $root 'build'
$src = Join-Path $root 'src'
New-Item -ItemType Directory -Force $build | Out-Null

function Need($name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if (-not $cmd) { throw "Required tool '$name' was not found in PATH." }
    return $cmd.Source
}

$nasm    = Need 'nasm'
$gcc     = Need 'gcc'
$ld      = Need 'ld'
$objcopy = Need 'objcopy'
$python  = Need 'python'

Write-Host "[1/7] Assembling boot sector..."
& $nasm -f bin (Join-Path $src 'boot\boot.asm') -o (Join-Path $build 'boot.bin')
if ($LASTEXITCODE -ne 0) { throw 'nasm failed for boot.asm' }

$bootSize = (Get-Item (Join-Path $build 'boot.bin')).Length
if ($bootSize -ne 512) { throw "boot.bin must be 512 bytes, got $bootSize" }
Write-Host "      boot.bin: $bootSize bytes"

Write-Host "[2/7] Assembling kernel entry and interrupt stubs..."
& $nasm -f win32 (Join-Path $src 'kernel\entry.asm') -o (Join-Path $build 'entry.o')
if ($LASTEXITCODE -ne 0) { throw 'nasm failed for entry.asm' }
& $nasm -f win32 (Join-Path $src 'kernel\isr.asm') -o (Join-Path $build 'isr.o')
if ($LASTEXITCODE -ne 0) { throw 'nasm failed for isr.asm' }

Write-Host "[3/7] Compiling C sources..."
$cflags = @(
    '-m32', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Wno-unused-parameter',
    '-ffreestanding', '-fno-builtin', '-fno-pic', '-fno-stack-protector',
    '-fno-asynchronous-unwind-tables', '-fno-ident', '-mgeneral-regs-only',
    '-fno-strict-aliasing'
)

$objects = @()
foreach ($cfile in (Get-ChildItem (Join-Path $src 'kernel') -Filter '*.c' | Sort-Object Name)) {
    $obj = Join-Path $build ($cfile.BaseName + '.o')
    Write-Host "      cc $($cfile.Name)"
    & $gcc @cflags -c $cfile.FullName -o $obj
    if ($LASTEXITCODE -ne 0) { throw "gcc failed for $($cfile.Name)" }
    $objects += $obj
}

Write-Host "[4/7] Linking flat kernel..."
$ldArgs = @(
    '-m', 'i386pe',
    '--image-base', '0',
    '--file-alignment', '0x200',
    '--section-alignment', '0x1000',
    '-T', (Join-Path $src 'kernel\linker.ld'),
    '-o', (Join-Path $build 'kernel.exe'),
    (Join-Path $build 'entry.o'),
    (Join-Path $build 'isr.o')
) + $objects
& $ld @ldArgs
if ($LASTEXITCODE -ne 0) { throw 'ld failed' }

& $objcopy -O binary (Join-Path $build 'kernel.exe') (Join-Path $build 'kernel.bin')
if ($LASTEXITCODE -ne 0) { throw 'objcopy failed' }

$kernelSize = (Get-Item (Join-Path $build 'kernel.bin')).Length
$maxSize = 448 * 512
Write-Host "      kernel.bin: $kernelSize bytes (limit $maxSize)"
if ($kernelSize -gt $maxSize) {
    throw "kernel.bin is too large ($kernelSize > $maxSize). Raise KERNEL_SECTORS in boot.asm and the copy size."
}

Write-Host "[5/7] Building bootable disk image..."
$imageSize = 8MB
$img = New-Object byte[] $imageSize
$bootBytes = [System.IO.File]::ReadAllBytes((Join-Path $build 'boot.bin'))
$kernelBytes = [System.IO.File]::ReadAllBytes((Join-Path $build 'kernel.bin'))
[Array]::Copy($bootBytes, 0, $img, 0, $bootBytes.Length)
[Array]::Copy($kernelBytes, 0, $img, 512, $kernelBytes.Length)
[System.IO.File]::WriteAllBytes((Join-Path $build 'debil.img'), $img)

Write-Host "[6/7] Building CD boot stage..."
if ($kernelSize -ge 65536) {
    throw "The ISO boot stage currently copies less than 64 KiB. Kernel is $kernelSize bytes."
}
$kernelLba = 24
$isoBootPath = Join-Path $build 'isoboot.bin'
& $nasm -f bin "-DKERNEL_BYTES=$kernelSize" "-DKERNEL_LBA=$kernelLba" `
    (Join-Path $src 'boot\isoboot.asm') -o $isoBootPath
if ($LASTEXITCODE -ne 0) { throw 'nasm failed for isoboot.asm' }
$isoBootSize = (Get-Item $isoBootPath).Length
if ($isoBootSize -ne 512) { throw "isoboot.bin must be 512 bytes, got $isoBootSize" }

Write-Host "[7/7] Building El Torito ISO..."
& $python (Join-Path $root 'tools\mkiso.py') `
    --boot $isoBootPath `
    --kernel (Join-Path $build 'kernel.bin') `
    --kernel-lba $kernelLba `
    --output (Join-Path $build 'debil.iso') `
    --label DEBIL_OS
if ($LASTEXITCODE -ne 0) { throw 'mkiso.py failed' }

$isoSize = (Get-Item (Join-Path $build 'debil.iso')).Length

Write-Host ""
Write-Host "Build complete:" -ForegroundColor Green
Write-Host "  build\debil.img    ($imageSize bytes, bootable raw disk)"
Write-Host "  build\debil.iso    ($isoSize bytes, bootable El Torito CD)"
Write-Host "  build\kernel.bin   ($kernelSize bytes)"
Write-Host ""
Write-Host "Run:  .\run.ps1          (raw disk image)"
Write-Host "      .\run.ps1 -Iso     (bootable ISO)"
