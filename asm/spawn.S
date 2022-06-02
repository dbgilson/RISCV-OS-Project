.section .text
.altmacro

#define GP_BASE 0
#define FP_BASE (32*8)

# Macro to save GP registers
.macro savegp i, m=x31
	sd	x\i, (GP_BASE + 8 * \i)(\m)
.endm

# Macro to load GP registers
.macro loadgp i, m=x31
	ld	x\i, (GP_BASE + 8 * \i)(m)
.endm

# Macro to save FP registers
.macro savefp i, m=x31
	fsd f\i, (FP_BASE + 8 * \i)(m)
.endm

# Macro to load FP registers
.macro loadfp i, m=x31
	fld f\i, (FP_BASE + 8 * \i)(m)
.endm


.global asm_trap_vector
.align 4
asm_trap_vector:
	csrrw	t6, sscratch, t6

	# Save context (GP registers)
	.set i, 1
	.rept 30
		savegp %i
		.set i, i+1
	.endr

	mv		t5, t6
	csrrw	t6, sscratch, t6
	savegp	31, t5

	# See if we need to save FP registers
	csrr	t0, sstatus
	srli	t0, t0, 13
	andi	t0, t0, 3
	beqz	t0, 1f
	.set i, 0
	.rept 32
		savefp %i, t5
		.set i, i+1
	.endr
1:

	call	c_trap_handler # Delegate to our trap handler in C code
	csrr	t6, sscratch
	# See if we need to load back FP registers
	csrr	t0, sstatus
	srli	t0, t0, 13
	andi	t0, t0, 3
	li		t1, 2
	blt		t0, t1, 1f

	.set i, 0
	.rept 32
		loadfp %i
		.set i, i+1
	.endr

1:
	# Load Context (GP Registers)
	 .set i, 1
	.rept 31
		loadgp %i
		.set i, i+1
	.endr


	sret

# #################################################
# The code below is for spawning processes

# Spawn Table Fields Reference
	# 0    - gpregs
    # 256  - fpregs
    # 512  - sepc
    # 520  - sstatus
    # 528  - sie
    # 536  - satp
    # 544  - sscratch
    # 552  - stvec
    # 560  - trap_satp
    # 568  - trap_stack

_spawn_kthread:
	# Load Frame Data from sscratch
    csrr        t6, sscratch

	ld		t0, 512(t6) # sepc
	csrw	sepc, t0

	ld		t4, 520(t6) # sstatus
	csrw	sstatus, t4

	ld		t0, 528(t6) # sie
	csrw	sie, t0

	ld		t0, 552(t6) # stvec
	csrw	stvec, t0

	ld		t1, 536(t6) # satp

	ld		t6, 544(t6) # sscratch
	csrw	sscratch, t6

    # Turn on MMU
	csrw	satp, t1

    # Check for FP Regs
    srli    t4, t4, 13
    andi    t4, t4, 3
    beqz    t4, 1f

    .set i, 0
    .rept 32
        loadfp %i
        .set i, i+1
    .endr

1:
	.set i, 1
	.rept 31
		loadgp  %i
		.set i, i + 1
	.endr

	sret
.align 4
_spawn_kthread_end:

# Pretty much the same as my asm_trap_handler above, but for processes
_spawn_trap:
    csrrw   t6, sscratch, t6

    .set i, 1
    .rept 30
        savegp  %i
    .set i, i + 1
    .endr

    mv      t5, t6
    csrrw   t6, sscratch, t6
    savegp  31, t5

    csrr    t6, sstatus
    srli    t6, t6, 13
    beqz    t6, 1f

    csrr    t6, sscratch
    .set i, 0
    .rept 32
        savefp %i
        .set i, i + 1
    .endr

    ld      t5, 560(t6)
    ld      sp, 568(t6)
    csrw    satp, t5

    call    c_trap_handler

    # After return from c_trap_handler, we're in kernel space
    csrr    t6, sscratch
    ld      t5, 536(t6)
    csrw    satp, t5

    csrr    t5, sstatus
    srli    t5, t5, 13
    andi    t5, t5, 3
    beqz    t5, 1f
    .set i, 0
    .rept 32
        loadfp  %i
    .set i, i + 1
    .endr
1:
    .set i, 1
    .rept 31
        loadgp %i
    .set i, i + 1
    .endr
1:
    sret
_spawn_trap_end:

.section .rodata
.global spawn_thread_start
.global spawn_thread_end
.global spawn_thread_size
spawn_thread_start: .dword _spawn_kthread
spawn_thread_end: .dword _spawn_kthread_end
spawn_thread_size: .dword _spawn_kthread_end - _spawn_kthread

.global spawn_trap_start
.global spawn_trap_end
.global spawn_trap_size
spawn_trap_start: .dword _spawn_trap
spawn_trap_end: .dword _spawn_trap_end
spawn_trap_size: .dword _spawn_trap_end - _spawn_trap
