.section .text.init
.global _start
_start:
	csrr	a0, mhartid
.option push
.option norelax
	la	sp, _stack_end
	la	gp, __global_pointer$

# We're going to put much of the functionality in C for better readability,
# so we're wanting to go to main as fast as possible.
.option pop
	li	t0, 4096
	mul	t0, t0, a0
	sub	sp, sp, t0

	tail main

# Simple park loop that waits for an interrupt before waking up (WFI)
.section .text
.global park
park:
  wfi
  j     park

