.section .text
.altmacro

# Macro to save gp registers
.macro savegp i
	sd	x\i, (8 * \i)(t6)
.endm

# Macro to load gp registers
.macro loadgp i
	ld	x\i, (8 * \i)(t6)
.endm

# Here is our sbi trap vector that we reference in main
.align 4
.global sbi_trap_vector
sbi_trap_vector:
	csrrw	t6, mscratch, t6
		
	# Save Context
	.set i, 1
	.rept 30
		savegp %i
		.set i, i + 1
	.endr
	
	mv		t5, t6
	csrrw	t6, mscratch, t6
	sd 		t6, 8*31(t5)
	
.option push
.option norelax
    la      sp, _stack_end
    la      gp, __global_pointer$
.option pop
    # Now that we have multiple HARTs, we need multiple stacks
    csrr    t0, mhartid
    slli    t0, t0, 12
    sub     sp, sp, t0

    call    c_trap_handler # Delegate trap in C code
    csrr    t6, mscratch

	# Load context
    .set i, 1
    .rept 31
        loadgp %i
        .set i, i + 1
    .endr
 
    mret
