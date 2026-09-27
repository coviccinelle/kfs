# KFS_2 — GDT & Stack

## Build

Needs `gcc` with 32-bit support and `binutils` (or an `i386-elf` cross
toolchain, used automatically when installed), `grub-mkrescue` or
`grub2-mkrescue`, `xorriso`, `mtools` and `qemu-system-i386`.

    make                                  # myos.bin + kfs.iso (< 1 MB)
    make TARGET=i686-elf                  # other cross toolchain prefix
    make run                              # boot the ISO, QEMU monitor on stdio

## Check the GDT

In the QEMU monitor:

    info registers        # GDT= 00000800 00000037, CS=0008 DS=0010 SS=0018
    xp /14wx 0x800        # the 7 raw descriptors

| Sel. | Entry        | Access | Flags |
|------|--------------|--------|-------|
| 0x00 | null         | 00     | 0     |
| 0x08 | kernel code  | 9A     | C     |
| 0x10 | kernel data  | 92     | C     |
| 0x18 | kernel stack | 92     | C     |
| 0x23 | user code    | FA     | C     |
| 0x2B | user data    | F2     | C     |
| 0x33 | user stack   | F2     | C     |

All segments are flat (base 0, limit 0xFFFFF x 4 KiB = 4 GiB).

## Shell

`help`, `stack` (kernel stack dump + call trace), `gdt` (decoded GDT),
`clear`, `reboot`, `halt`.
Return addresses of the trace: `addr2line -f -e myos.bin 0x...`
