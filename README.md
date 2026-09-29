# kfs-4
IDT - interrupt descriptor table

![alt text](<Screenshot 2026-09-03 at 16.52.46.png>)

If the system GRUB BIOS modules are unavailable, download and extract them
locally before building an ISO:

```sh
mkdir -p .local/grub
url=$(dnf repoquery --location grub2-pc-modules | tail -1)
curl -L "$url" -o .local/grub/grub2-pc-modules.rpm
rpm2cpio .local/grub/grub2-pc-modules.rpm | cpio -idm --quiet -D .local/grub
```

Build and run:

```sh
make            # myos.bin + myos.iso (the delivered image)
make start      # qemu-system-i386 -kernel myos.bin
make start-iso  # boot the ISO through GRUB
make clean      # objects and myos.bin; myos.iso is kept
make fclean     # also removes myos.iso (make re = fclean all)
```


## KFS-4: interrupts

### Boot order (`src/kernel.c`)
`gdt_init` → `terminal_initialize` → `idt_init` (exceptions, PIC remap, `lidt`)
→ `signal_init` → `timer_init` (IRQ0, 100 Hz) → `keyboard_init` (IRQ1)
→ `syscall_init` (int 0x80) → `sti`. The main loop drains the keyboard
buffer, delivers pending signals, then sleeps with `sti; hlt`.

### How an interrupt travels
```
CPU ──IDT[n]──> isrN (isr_stubs.s)   push err_code(0 if the CPU did not) + n
            └─> isr_common           pusha, ds, load kernel segments
                └─> interrupt_dispatch(struct regs *)   (idt.c)
                     ├ 0x20-0x2F IRQ  -> registered handler, then PIC EOI
                     ├ handler registered -> call it (int3, int 0x80...)
                     ├ other exception    -> panic_regs()
                     └ other vector       -> "unhandled interrupt 0xNN"
            <── popa, iret
```

| File | Role |
|---|---|
| `src/isr_stubs.s` | 256 entry stubs, common save/restore, `panic` entry, `cpu_halt_clean`, `int_trigger` table |
| `src/idt.c` | 256-entry IDT (all present, 0x80 is DPL3), `idt_set_gate`, `register_interrupt_handler`, dispatcher, `idt` command |
| `src/pic.c` | 8259 remap (IRQ 0-15 → 0x20-0x2F), mask/unmask, EOI, spurious IRQ check |
| `src/timer.c` | PIT at 100 Hz, `timer_ticks`, `timer_sleep` |
| `src/keyboard.c` | IRQ1 pushes scancodes into a ring buffer; main loop processes them; Ctrl+C → SIGINT |
| `src/signal.c` | signal-callback API and scheduling |
| `src/panic.c` | `panic`, `panic_regs`, `system_halt` |
| `src/stack.c` | `stack_save` snapshot, call trace |
| `src/syscall.c` | bonus: `int 0x80` (write, ticks, kill) |

### Signals (`includes/signal.h`)
- `signal(sig, cb)` registers a callback (`SIG_DFL`, `SIG_IGN`; SIGKILL can't be caught)
- `signal_raise(sig)` delivers now; `signal_schedule(sig)` marks it pending;
  `signal_schedule_in(sig, ticks)` delivers after a delay (timer IRQ)
- Pending signals are delivered by `signal_process()` from the main loop, never
  inside an interrupt or exception handler (int3 only schedules SIGTRAP).
- Default actions: SIGTERM → graceful halt, SIGKILL/SIGSEGV/SIGFPE/... → panic,
  others → "ignored" message.

### Panic / halt
0. `panic()` is written in assembly: it saves EFLAGS, CS, EIP and `pusha`
   before any C code runs, so it reports the caller's real registers. It
   builds the same `struct regs` as an interrupt and calls `panic_regs()`.
1. `cli`  2. `stack_save()` copies ESP..stack_top into a static snapshot
3. prints reason, exception, registers, saved stack and call trace
   (starting with the function EIP points into)
4. `cpu_halt_clean()` zeroes EAX..EBP, resets ESP, sets EFLAGS to 0x2
   (bit 1 is reserved, IF cleared), then `hlt` forever.

### Shell commands to try
`idt`, `gdt`, `uptime`, `int3`, `syscall`, `kill 10`, `alarm 2`, Ctrl+C,
`int 0x30` / `int 255` (software interrupt, returns to the shell),
`kill 15` (graceful), `kill 9`, `div0`, `gpf`, `ud2`, `into`, `int 0`,
`panic`, `halt`.

`int N` refuses vectors 8, 10-14, 17, 21, 29 and 30: the CPU pushes an
error code for those exceptions, but a software `int` never does, so the
stub would read a shifted stack. Use `div0`, `gpf`, `ud2` or `into` to
raise real exceptions instead. `int 0x21` runs the keyboard handler
without a key press: it re-reads the last scancode from port 0x60.
