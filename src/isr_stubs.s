/*
Low-level interrupt entry points.

The CPU jumps to one of these stubs through the IDT. Each stub pushes the
vector number (and a dummy error code when the CPU does not push one), so
every interrupt reaches isr_common with the same stack layout. isr_common
saves the registers, builds a `struct regs` on the stack and hands a
pointer to it to interrupt_dispatch() in idt.c.

Stack layout seen by interrupt_dispatch (low -> high address):
    ds | edi esi ebp esp ebx edx ecx eax | int_no err_code | eip cs eflags
*/

.section .text

/* Exceptions without an error code: push a fake one to keep the layout. */
.macro ISR_NOERR num
.global isr\num
.type isr\num, @function
isr\num:
	pushl $0
	pushl $\num
	jmp isr_common
.endm

/* Exceptions where the CPU already pushed an error code. */
.macro ISR_ERR num
.global isr\num
.type isr\num, @function
isr\num:
	pushl $\num
	jmp isr_common
.endm

/* CPU exceptions 0x00-0x1F */
.irp n, 0,1,2,3,4,5,6,7,9,15,16,18,19,20,22,23,24,25,26,27,28,31
	ISR_NOERR \n
.endr
.irp n, 8,10,11,12,13,14,17,21,29,30
	ISR_ERR \n
.endr

/*
Vectors 32-255: hardware IRQs 0-15 (remapped by the PIC to 0x20-0x2F),
then software interrupts, int 0x80 included. None of them has an error
code. .altmacro lets %i expand the counter into the stub name.
*/
.altmacro
.set i, 32
.rept 224
	ISR_NOERR %i
	.set i, i + 1
.endr

.global isr_common
.type isr_common, @function
isr_common:
	pusha                   /* save general purpose registers */
	xorl %eax, %eax
	mov %ds, %ax
	pushl %eax              /* save the data segment */

	mov $0x10, %ax          /* kernel data selector */
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	pushl %esp              /* argument: struct regs * */
	cld                     /* C code expects DF = 0 */
	call interrupt_dispatch
	addl $4, %esp

	popl %eax               /* restore the data segment */
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	popa                    /* restore registers (eax may hold a syscall result) */
	addl $8, %esp           /* drop int_no and err_code */
	iret
.size isr_common, . - isr_common

/*
panic(const char *msg): fatal error from C code.

Written in assembly so the registers are captured before any C prologue
touches them. It builds the same struct regs as isr_common, with the
return address as EIP and int_no = 0xFFFFFFFF (not an interrupt), then
calls panic_regs(msg, regs), which never returns.

Stack on entry: [esp] = return address, [esp+4] = msg.
*/
.global panic
.type panic, @function
panic:
	pushfl                  /* eflags of the caller, saved before cli */
	cli
	pushl %cs               /* cs */
	pushl 8(%esp)           /* eip = return address (2 pushes above it) */
	pushl $0                /* err_code */
	pushl $0xFFFFFFFF       /* int_no: not from an interrupt */
	pusha                   /* registers of the caller, untouched */
	xorl %eax, %eax
	mov %ds, %ax
	pushl %eax              /* ds: struct regs is complete (56 bytes) */
	movl %esp, %eax
	pushl %eax              /* argument 2: struct regs * */
	pushl 64(%esp)          /* argument 1: msg (56 + 4 + return address) */
	call panic_regs
.size panic, . - panic

/*
Last step of a panic or halt: disable interrupts, reset the stack and
zero every general purpose register and EFLAGS, so nothing is left
behind, then halt the CPU forever.
*/
.global cpu_halt_clean
.type cpu_halt_clean, @function
cpu_halt_clean:
	cli
	mov $stack_top, %esp
	xorl %eax, %eax
	xorl %ebx, %ebx
	xorl %ecx, %ecx
	xorl %edx, %edx
	xorl %esi, %esi
	xorl %edi, %edi
	xorl %ebp, %ebp
	/* Last, since xor sets ZF/PF: EFLAGS = 0x2 (bit 1 is reserved and
	   always reads 1, IF stays cleared). The pushed 0 lands on
	   stack_top - 4 and is popped right away, so ESP ends at stack_top. */
	pushl $0
	popfl
1:	hlt
	jmp 1b
.size cpu_halt_clean, . - cpu_halt_clean

/*
int_trigger + 4*n: "int $n; ret" for n = 0..255. The vector of the int
instruction is an immediate, so the shell's "int N" command calls into
this table instead of building code at runtime. Each entry is padded to
4 bytes so its address is easy to compute.
*/
.global int_trigger
.type int_trigger, @function
.balign 4
int_trigger:
.set i, 0
.rept 256
	.balign 4
	int $i
	ret
	.set i, i + 1
.endr
.size int_trigger, . - int_trigger

/* Addresses of stubs 0-255, used by idt_init() to fill the table. */
.section .rodata
.global isr_stub_table
isr_stub_table:
.macro STUB_ADDR n
	.long isr\n
.endm
.set i, 0
.rept 256
	STUB_ADDR %i
	.set i, i + 1
.endr
