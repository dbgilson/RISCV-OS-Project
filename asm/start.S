.section .text.init
.global _start
_start:
.option push
.option norelax
	la		sp, _stack_end
	la		gp, __global_pointer$
.option pop
	la		t0, main
	csrw	sepc, t0 				# Set sepc to go to OS main
	la		t0, asm_trap_vector
	csrw	stvec, t0				# Set stvec to our asm_trap_vector address
	li		t0, 1 << 9
	csrw	sie, t0					# Set a Supervisor level External Interrupt Pending
	li		t0, (1 << 9) | (1 << 5) | (1 << 1)
	csrw    sip, t0					# Set supervisor level external, timer, and software interrupts
	li		t0, (1 << 13) | (1 << 8) | (1 << 5)
	csrw	sstatus, t0 			# Enable floating point, Set to supervisor mode, and set SPIE
	la		ra, park
	csrr	a0, sscratch
	sret

# Simple wait for interupt function for harts
.global park
park:
	wfi
	j	park
park_end:

# void *idleproc(void);
# uint64_t idleprocsize;
.section .rodata
.global idleproc
.global idleprocsize
idleproc: .dword park
idleprocsize: .dword park_end - park

.global test_hart
test_hart:
.option push
.option norelax
	la		sp, _stack_end
	la		gp, __global_pointer$
.option pop
	csrr	a0, sscratch
	slli	t0, a0, 13
	sub		sp, sp, t0
	tail	c_test_hart