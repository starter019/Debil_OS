# DEBIL OS

A small 32-bit x86 terminal operating system written in **NASM** and **C**.
It boots from a custom 512-byte boot sector (no GRUB), enters protected mode,
enables paging, and drops into an interactive shell with a RAM filesystem,
programs, a snake game, a live task manager, and intentional crash tests.

The same kernel ships as **two boot options**: a raw hard-disk image
(`debil.img`) and a bootable El Torito CD image (`debil.iso`).

```
  ____  _____ ____ ___ _     ___  ____
 |  _ \| ____| __ )_ _| |   / _ \/ ___|
 | | | |  _| |  _ \| || |  | | | \___ \
 | |_| | |___| |_) | || |__| |_| |___) |
 |____/|_____|____/___|_____\___/|____/
```

## Features

- Custom 512-byte boot sector: E820 map, EDD disk reads, A20, protected mode.
- Dual boot media: raw MBR disk image and an El Torito no-emulation CD ISO.
- Flat 32-bit kernel loaded at `0x100000`, built as a raw binary.
- IDT + 8259 PIC + PIT (100 Hz) + PS/2 keyboard + COM1 serial console.
- VGA text mode (80x25) with an ANSI mirror on the serial port.
- 24-file x 4 KiB in-memory filesystem with create, write, append, save,
  copy, rename, delete, stat and usage reporting.
- 25+ shell commands (see below).
- `.debil` program registry: `start <name>` and a program list.
- Snake game with keyboard controls.
- Live task manager showing CPU, RAM, GPU and filesystem information.
- `test_crash cpu|ram|gpu` injects a real fault, prints a register dump,
  and resets the machine to exercise the reboot path.

## Requirements

| Tool | Purpose |
| --- | --- |
| NASM | Assembles the boot sectors and kernel entry/stubs |
| MinGW-w64 `gcc` (`-m32`) | Compiles the C kernel |
| MinGW-w64 `ld`, `objcopy` | Links a PE image and converts it to a flat binary |
| Python 3 | Builds the El Torito ISO (`tools/mkiso.py`, standard library only) |
| QEMU (`qemu-system-i386`) | Runs the disk image or ISO |

The build was verified with the MinGW-w64 toolchain and QEMU 9.2 on Windows.
The scripts look for QEMU in `PATH`, `C:\Program Files\qemu`, and
`%TEMP%\qemu-2024-extract` (the portable extraction layout).

## Build

```powershell
.\build.ps1
```

or double-click `build.bat`.

Output:

- `build\debil.img` - 8 MB bootable raw disk image
- `build\debil.iso` - bootable El Torito CD image (~98 KB)
- `build\kernel.bin` - flat kernel binary (~47 KB)
- `build\isoboot.bin` - 512-byte CD boot stage
- `build\kernel.exe`, `build\*.o` - intermediate link artifacts

The raw disk boot sector loads up to 448 sectors (224 KiB) for the kernel. The
CD boot stage reads the kernel into a 64 KiB staging buffer, so the ISO build
requires a kernel below 64 KiB. The build fails with a clear message if either
limit is exceeded.

The ISO is assembled by `tools/mkiso.py`, which writes a minimal ISO 9660
filesystem with an El Torito no-emulation entry. It uses only the Python
standard library, so no external ISO tool (`mkisofs`, `xorriso`, `oscdimg`) is
needed.

## Run

```powershell
.\run.ps1          # boot the raw disk image (QEMU window + serial mirror)
.\run.ps1 -Iso     # boot the ISO instead
.\run.ps1 -NoGui   # no QEMU window; serial console in the terminal
```

`-Iso` and `-NoGui` can be combined. Double-clicking `run.bat` boots the raw
disk image, and double-clicking `run-iso.bat` boots the ISO.

Manual QEMU commands:

```powershell
# Raw disk image
qemu-system-i386 -m 256 -boot c -drive file=build\debil.img,format=raw,if=ide -vga std -serial stdio

# Bootable ISO (CD only, no hard disk)
qemu-system-i386 -m 256 -boot d -cdrom build\debil.iso -vga std -serial stdio
```

The serial console mirrors the VGA screen, so the OS can be driven headlessly:

```powershell
qemu-system-i386 -m 256 -boot d -cdrom build\debil.iso `
  -vga std -display none -serial stdio -monitor none
```

Type commands followed by Enter. On Windows, use a real terminal (Windows
Terminal, PowerShell console) for serial input.

## Shell commands

| Command | Description |
| --- | --- |
| `help` | List commands |
| `clear` | Clear the screen |
| `echo <text>` | Print text |
| `ls` | List files |
| `mkfile <name>` | Create an empty file (`touch`/`create` also work) |
| `edit <name>` | Line editor (`:w`, `:q`, `:q!`, `:wq`) |
| `cat <name>` | Print a file (`type` also works) |
| `write <name> <text>` | Append text, creating the file if needed |
| `rm <name>` | Delete a file (`del` also works) |
| `cp <src> <dst>` | Copy a file |
| `mv <src> <dst>` | Rename a file |
| `df` | Filesystem capacity/usage |
| `stat <name>` | File details |
| `programs` | List installed `.debil` programs |
| `start <name>` | Run a program; `.debil` is optional |
| `tasks` | Open the live task manager |
| `sysinfo` | CPU / RAM / GPU summary |
| `meminfo` | E820 memory map |
| `pci` | PCI display devices and BARs |
| `uptime` | Time since boot |
| `date` | Read the CMOS clock |
| `stress [ms]` | Burn CPU for a while |
| `history` | Last commands |
| `test_crash <mode>` | Fault the CPU/RAM/GPU and reboot |
| `reboot` | Reset the machine |
| `shutdown` | ACPI power-off |

## Programs

Run with `start <program>` or `start <program>.debil`.

| Program | Description |
| --- | --- |
| `hello.debil` | Welcome demo; press Enter to return |
| `snake.debil` | Snake: arrows/WASD, `P` pause, `ESC` or `Q` quit |
| `tasks.debil` | Live task manager; `Q` or `ESC` to exit |
| `crash.debil` | Interactive crash menu (`1` CPU, `2` RAM, `3` GPU, `Q` cancel) |
| `calc.debil` | Integer calculator; `12 + 7`, `100 / 4`, `q` to quit |

User-created `.debil` files are text scripts run by a small interpreter:

```text
# demo.debil
print Hello from my script
echo This uses echo
sleep 500
pause
```

Supported script commands: `print`, `echo`, `clear`, `sleep <ms>`, `pause`,
`crash <cpu|ram|gpu>`, `reboot`, `shutdown`.

## Crash tests

`test_crash` deliberately faults the kernel, catches the exception, prints a
register/CR dump, waits three seconds, and resets the machine. After the reset
the boot banner and shell reappear, which demonstrates the full reboot path.

| Mode | Injected fault | Expected dump |
| --- | --- | --- |
| `test_crash cpu` | `ud2` invalid opcode | Exception 6 |
| `test_crash ram` | Write to unmapped `0x40000000` | Exception 14, CR2=`0x40000000` |
| `test_crash gpu` | Write to unmapped GPU range `0xE0000000` | Exception 14, CR2=`0xe0000000` |

> **Warning:** these commands really reset the machine. The RAM filesystem is
> volatile, so any unsaved files are lost.

All three modes were tested end-to-end and return to the shell after reboot.

## Filesystem

The filesystem lives entirely in kernel BSS:

- 24 file slots, 4095 usable data bytes per file (~95.9 KiB total).
- Names are case-insensitive, up to 31 characters; path separators and
  Windows-reserved characters are rejected.
- Six sample files are seeded at boot (`readme.txt` plus one `.debil` text
  description per built-in program).
- Changes are lost on reset/reboot by design - there is no disk driver yet.

## Architecture

```
BIOS
  -> raw disk boot (debil.img)
       MBR at 0x7C00
         - E820 map at 0x8000, boot info at 0x7F00
         - EDD reads kernel to 0x10000
         - A20, GDT, CR0.PE
  -> CD boot (debil.iso)
       El Torito no-emulation entry -> 512-byte stage at 0x7C00
         - E820 map at 0x8000, boot info at 0x7F00
         - INT 13h EDD reads kernel from CD LBA 24 to 0x10000
         - A20, GDT, CR0.PE
  -> 32-bit protected mode
       - copy kernel to 0x100000
       - jump to 0x100000
  -> kernel entry (entry.asm)
       - reload GDT, stack at 0x90000, zero BSS
       - page directory at 0x3000: identity map 0-256 MB with 4 MB PSE pages
       - CR4.PSE, CR0.PG, call kernel_main()
  -> kernel_main()
       - console, IDT/PIC, PIT, keyboard/serial, E820, PCI, filesystem, CPUID
       - shell_run()
```

Only the first 256 MB are identity-mapped. That is why the RAM and GPU crash
tests produce real page faults instead of silently touching mapped memory.

## Project layout

```
build.ps1 / build.bat        Build script and wrapper
run.ps1 / run.bat            QEMU launcher and wrapper (raw image)
run-iso.bat                  QEMU launcher wrapper (ISO)
src/boot/boot.asm            512-byte MBR boot sector (debil.img)
src/boot/isoboot.asm         512-byte El Torito CD boot stage (debil.iso)
tools/mkiso.py               Pure-Python ISO 9660 / El Torito builder
src/kernel/entry.asm         Protected-mode entry, BSS, paging
src/kernel/isr.asm           Exception/IRQ stubs and stub table
src/kernel/linker.ld         Flat PE layout, kernel_end/BSS symbols
src/kernel/kernel.c          Boot sequence and subsystem init
src/kernel/console.c/.h      VGA text console + ANSI serial mirror + printf
src/kernel/serial.c/.h       COM1 driver
src/kernel/input.c/.h        PS/2 keyboard + serial ANSI key parser
src/kernel/idt.c/.h          IDT, 8259 PIC, IRQ dispatch
src/kernel/timer.c/.h        PIT, uptime, CPU busy/idle accounting
src/kernel/mem.c/.h          E820 parsing and reporting
src/kernel/cpu.c/.h          CPUID brand/vendor/features/TSC clock
src/kernel/pci.c/.h          PCI bus scan, display controller + BAR sizes
src/kernel/fs.c/.h           In-memory filesystem
src/kernel/shell.c/.h        Line editor, command parser, shell commands
src/kernel/programs.c/.h     Program registry, snake, tasks, crash menu, calc
src/kernel/panic.c/.h        Panic dump, fault injection, reboot/shutdown
src/kernel/string.c/.h       Freestanding string/atoi/utoa helpers
src/kernel/io.h              Port I/O and descriptor helpers
src/kernel/types.h           Fixed-width types and packing macros
```

## Oracle VM VirtualBox

Both boot options work in VirtualBox. The ISO was verified with VirtualBox
7.2.22: it reaches the shell and detects the VirtualBox display adapter
(`80ee:beef`).

GUI steps:

1. **New** -> Type **Other**, Version **Other/Unknown** (32-bit).
2. Memory **256 MB** or more; no hard disk is required for the ISO.
3. **Settings -> System -> Motherboard**: enable **Optical** and move it to the
   top of the boot order if needed.
4. **Settings -> Storage**: attach `build\debil.iso` to the IDE optical drive.
5. Start the VM and wait for the `debil>` prompt.

For the raw image, attach `build\debil.img` as a hard disk instead (or use the
ISO; the result is the same).

Headless test with `VBoxManage`:

```powershell
$vbox = 'C:\Program Files\Oracle\VirtualBox\VBoxManage.exe'
& $vbox createvm --name DEBIL-OS --ostype Other --register
& $vbox modifyvm DEBIL-OS --memory 256 --vram 16 --boot1 dvd `
    --boot2 none --boot3 none --boot4 none --firmware bios --nic1 none
& $vbox storagectl DEBIL-OS --name IDE --add ide --controller PIIX4
& $vbox storageattach DEBIL-OS --storagectl IDE --port 1 --device 0 `
    --type dvddrive --medium build\debil.iso
& $vbox startvm DEBIL-OS

# later
& $vbox controlvm DEBIL-OS poweroff
& $vbox unregistervm DEBIL-OS --delete
```

Add `--uart1 0x3F8 4 --uartmode1 file <log>` in `modifyvm` to capture the
serial console to a file.

## Limitations

- Single-core, cooperative (no scheduler or preemption).
- RAM-only filesystem; no persistent disk, no ELF loader.
- Monochrome-style 16-color VGA text mode only.
- GPU usage in the task manager is a VGA text-activity proxy, not 3D load.
- Identity mapping is limited to 256 MB; machines with more RAM still boot,
  but only the first 256 MB are mapped.
- The ISO CD boot stage stages the kernel below 64 KiB. The raw disk image
  supports up to 224 KiB.

## Troubleshooting

- **QEMU not found:** install QEMU and add it to `PATH`, or place
  `qemu-system-i386.exe` in `%TEMP%\qemu-2024-extract`.
- **Python not found:** install Python 3 and make sure `python` is on `PATH`;
  the ISO step (`tools\mkiso.py`) needs it.
- **No serial input:** use a real console window; some IDE terminals do not
  forward keystrokes to QEMU's stdio serial.
- **Kernel too large:** raise `KERNEL_SECTORS` in `src/boot/boot.asm` and the
  copy length in `protected_mode` for the disk image. For the ISO, raise the
  staging address/`KERNEL_BYTES` limit in `src/boot/isoboot.asm` and
  `build.ps1` together, or reduce code size.
- **Build tools missing:** the build script reports the missing command name.
