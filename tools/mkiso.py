#!/usr/bin/env python3
"""Build a small bootable ISO 9660 image with an El Torito no-emulation entry.

The boot image is expected to be the 512-byte CD boot stage followed by the
flat DEBIL kernel. The BIOS loads it at 0x7C00; the stage copies the kernel
out of the loaded image and jumps to it. No CD filesystem driver is needed.

Only the Python standard library is used.
"""

import argparse
import datetime
import os
import struct
import sys

SECTOR = 2048
BOOT_CODE_SIZE = 512


def both16(value: int) -> bytes:
    return struct.pack("<H", value) + struct.pack(">H", value)


def both32(value: int) -> bytes:
    return struct.pack("<I", value) + struct.pack(">I", value)


def dir_date(when: datetime.datetime) -> bytes:
    # 7-byte ECMA-119 directory record date: years since 1900, month, day,
    # hour, minute, second, GMT offset in 15-minute units.
    return bytes([
        max(0, min(255, when.year - 1900)),
        when.month,
        when.day,
        when.hour,
        when.minute,
        when.second,
        0,
    ])


def volume_date(when: datetime.datetime) -> bytes:
    # 17-byte volume descriptor date: YYYYMMDDHHMMSS + hundredths + offset.
    return when.strftime("%Y%m%d%H%M%S").encode("ascii") + b"00" + b"\x00"


def dir_record(name: bytes, extent: int, size: int, flags: int,
               when: datetime.datetime) -> bytes:
    length = 33 + len(name)
    if length & 1:
        length += 1
    rec = bytearray(length)
    rec[0] = length
    rec[1] = 0
    rec[2:10] = both32(extent)
    rec[10:18] = both32(size)
    rec[18:25] = dir_date(when)
    rec[25] = flags
    rec[26] = 0
    rec[27] = 0
    rec[28:32] = both16(1)
    rec[32] = len(name)
    rec[33:33 + len(name)] = name
    return bytes(rec)


def path_table(root_extent: int, big_endian: bool) -> bytes:
    rec = bytearray()
    rec.append(1)              # directory identifier length
    rec.append(0)              # extended attribute record length
    rec += struct.pack(">I" if big_endian else "<I", root_extent)
    rec += struct.pack(">H" if big_endian else "<H", 1)  # parent directory
    rec.append(0)              # root directory identifier
    if len(rec) & 1:
        rec.append(0)          # pad to even length
    return bytes(rec)


def build_readme(label: str, boot_size: int, kernel_size: int) -> bytes:
    text = (
        f"{label.replace('_', ' ')} - bootable ISO\r\n"
        "\r\n"
        "This CD boots with an El Torito no-emulation entry. The BIOS loads\r\n"
        "the 512-byte DEBIL OS boot stage, which reads the flat kernel from\r\n"
        "the CD with INT 13h and jumps to it.\r\n"
        "\r\n"
        f"Boot stage size: {boot_size} bytes\r\n"
        f"Kernel size    : {kernel_size} bytes\r\n"
        "\r\n"
        "The same OS is also available as a raw disk image (debil.img) in\r\n"
        "the build directory. Use whichever boot option your VM or machine\r\n"
        "supports.\r\n"
        "\r\n"
        "Type 'help' at the DEBIL OS prompt for the command list.\r\n"
    )
    return text.encode("ascii")


def main() -> int:
    ap = argparse.ArgumentParser(description="Build a DEBIL OS El Torito ISO")
    ap.add_argument("--boot", required=True,
                    help="512-byte El Torito boot stage")
    ap.add_argument("--kernel", required=True,
                    help="flat kernel binary placed at --kernel-lba")
    ap.add_argument("--kernel-lba", type=int, default=24,
                    help="CD LBA (2048-byte sectors) of the kernel")
    ap.add_argument("--output", required=True, help="output .iso path")
    ap.add_argument("--label", default="DEBIL_OS", help="ISO volume label")
    args = ap.parse_args()

    with open(args.boot, "rb") as fh:
        boot_data = fh.read()
    with open(args.kernel, "rb") as fh:
        kernel_data = fh.read()
    if len(boot_data) < 1:
        raise SystemExit("boot stage is empty")
    if len(boot_data) > 0xFFFF * 512:
        raise SystemExit("boot stage is too large for a 16-bit El Torito count")
    if len(kernel_data) < 1:
        raise SystemExit("kernel binary is empty")
    if len(kernel_data) > 0xFFFF * 2048:
        raise SystemExit("kernel is too large for the CD read")

    label = args.label.encode("ascii", "replace")[:32]
    label = label.ljust(32, b" ")

    boot_512 = (len(boot_data) + 511) // 512
    boot_iso = (len(boot_data) + SECTOR - 1) // SECTOR
    kernel_iso = (len(kernel_data) + SECTOR - 1) // SECTOR

    now = datetime.datetime.now()

    pvd_lba = 16
    brd_lba = 17
    term_lba = 18
    ptl_lba = 19
    ptm_lba = 20
    root_lba = 21
    cat_lba = 22
    boot_lba = 23
    kernel_lba = args.kernel_lba
    if kernel_lba < boot_lba + boot_iso:
        raise SystemExit(
            f"--kernel-lba {kernel_lba} overlaps the boot stage "
            f"(ends at sector {boot_lba + boot_iso})"
        )
    readme_lba = kernel_lba + kernel_iso

    readme = build_readme(args.label, len(boot_data), len(kernel_data))
    readme_iso = max(1, (len(readme) + SECTOR - 1) // SECTOR)
    total = readme_lba + readme_iso

    root_record = dir_record(b"\x00", root_lba, SECTOR, 0x02, now)

    # Primary Volume Descriptor -------------------------------------------------
    pvd = bytearray(SECTOR)
    pvd[0] = 1
    pvd[1:6] = b"CD001"
    pvd[6] = 1
    pvd[7] = 0
    pvd[8:40] = b" " * 32                    # system identifier
    pvd[40:72] = label                       # volume identifier
    pvd[72:80] = b"\x00" * 8                 # unused
    pvd[80:88] = both32(total)               # volume space size
    pvd[88:120] = b"\x00" * 32               # escape sequences
    pvd[120:124] = both16(1)                 # volume set size
    pvd[124:128] = both16(1)                 # volume sequence number
    pvd[128:132] = both16(SECTOR)            # logical block size
    pvd[132:140] = both32(10)                # path table size
    pvd[140:144] = struct.pack("<I", ptl_lba)
    pvd[144:148] = b"\x00" * 4
    pvd[148:152] = struct.pack(">I", ptm_lba)
    pvd[152:156] = b"\x00" * 4
    pvd[156:190] = root_record
    pvd[190:318] = b" " * 128                # volume set identifier
    pvd[318:446] = b" " * 128                # publisher identifier
    pvd[446:574] = b" " * 128                # data preparer identifier
    pvd[574:702] = b" " * 128                # application identifier
    pvd[702:739] = b" " * 37                 # copyright file identifier
    pvd[739:776] = b" " * 37                 # abstract file identifier
    pvd[776:813] = b" " * 37                 # bibliographic file identifier
    pvd[813:830] = volume_date(now)          # creation
    pvd[830:847] = volume_date(now)          # modification
    pvd[847:864] = b"0" * 16 + b"\x00"       # expiration (unspecified)
    pvd[864:881] = volume_date(now)          # effective
    pvd[881] = 1                             # file structure version
    pvd[882] = 0
    pvd[883:1395] = b"\x00" * 512            # application use
    pvd[1395:2048] = b"\x00" * 653

    # Boot Record Volume Descriptor --------------------------------------------
    brd = bytearray(SECTOR)
    brd[0] = 0
    brd[1:6] = b"CD001"
    brd[6] = 1
    brd[7:39] = b"EL TORITO SPECIFICATION".ljust(32, b"\x00")
    brd[39:71] = b"DEBIL OS".ljust(32, b"\x00")
    brd[71:75] = struct.pack("<I", cat_lba)

    # Volume Descriptor Set Terminator -----------------------------------------
    term = bytearray(SECTOR)
    term[0] = 255
    term[1:6] = b"CD001"
    term[6] = 1

    # Path tables ---------------------------------------------------------------
    pt_l = path_table(root_lba, False)
    pt_m = path_table(root_lba, True)

    # Root directory ------------------------------------------------------------
    root = bytearray(SECTOR)
    dot = dir_record(b"\x00", root_lba, SECTOR, 0x02, now)
    dotdot = dir_record(b"\x01", root_lba, SECTOR, 0x02, now)
    kernel_rec = dir_record(b"DEBIL.BIN;1", kernel_lba, len(kernel_data), 0, now)
    readme_rec = dir_record(b"README.TXT;1", readme_lba, len(readme), 0, now)
    offset = 0
    for rec in (dot, dotdot, kernel_rec, readme_rec):
        root[offset:offset + len(rec)] = rec
        offset += len(rec)

    # El Torito boot catalog ----------------------------------------------------
    cat = bytearray(SECTOR)
    val = bytearray(32)
    val[0] = 0x01                 # validation entry
    val[1] = 0x00                 # 80x86 platform
    val[4:28] = b"DEBIL OS".ljust(24, b"\x00")
    val[30] = 0x55
    val[31] = 0xAA
    checksum = (-sum(struct.unpack("<16H", bytes(val)))) & 0xFFFF
    val[28:30] = struct.pack("<H", checksum)

    entry = bytearray(32)
    entry[0] = 0x88               # bootable
    entry[1] = 0x00               # no emulation
    entry[2:4] = struct.pack("<H", 0)          # load segment 0 => 0x7C00
    entry[4] = 0                  # system type
    entry[5] = 0
    entry[6:8] = struct.pack("<H", boot_512)   # 512-byte sectors to load
    entry[8:12] = struct.pack("<I", boot_lba)  # load RBA (2048-byte blocks)

    cat[0:32] = val
    cat[32:64] = entry

    # Assemble ------------------------------------------------------------------
    iso = bytearray(total * SECTOR)

    def put(lba: int, data: bytes) -> None:
        off = lba * SECTOR
        iso[off:off + len(data)] = data

    put(pvd_lba, pvd)
    put(brd_lba, brd)
    put(term_lba, term)
    put(ptl_lba, pt_l)
    put(ptm_lba, pt_m)
    put(root_lba, root)
    put(cat_lba, cat)
    put(boot_lba, boot_data)
    put(kernel_lba, kernel_data)
    put(readme_lba, readme)

    out_dir = os.path.dirname(os.path.abspath(args.output))
    os.makedirs(out_dir, exist_ok=True)
    with open(args.output, "wb") as fh:
        fh.write(iso)

    print(f"  ISO       : {args.output}")
    print(f"  Size      : {len(iso)} bytes ({total} sectors)")
    print(f"  Boot stage: {len(boot_data)} bytes, {boot_512} x 512-byte sectors")
    print(f"  Boot LBA  : {boot_lba}  catalog LBA: {cat_lba}")
    print(f"  Kernel LBA: {kernel_lba}  ({len(kernel_data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
