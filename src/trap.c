#include <csr.h>
#include <printf.h>
#include <sbi.h>
#include <plic.h>
#include <process.h>
#include <syscall.h>
#include <p_scheduling.h>

void c_trap_handler(void) {
	//printf("Got into c trap handler\n");
    unsigned long cause;
    long *sscratch;
    unsigned long sepc;
    CSR_READ(cause, "scause");
	CSR_READ(sscratch, "sscratch");
	CSR_READ(sepc, "sepc");

    int hart = sbi_whoami();
    int is_async = (cause >> 63) & 1;
    cause &= 0xff;
	//printf("Cause is %ld\n", cause);
    if (is_async) {
		switch (cause) {
			case 1: // SSIP
                printf("SSIP %d\n", hart);
				CSR_WRITE("sip", (0 << 1));
                break;
			case 5: // STIP
				sbi_ack_timer();
				schedule_hart_spawn(hart);
			case 9: //SEIP
                plic_handle_irq(hart);
				break;
			default:
				printf("OS: Unhandled Asynchronous interrupt %ld\n", cause);
				break;
		}
	}
	else {
		switch (cause) {
			case 8: //ECALL U-Mode
				syscall_handle(hart, sepc, sscratch);
				break;
			default:
				printf("OS: Unhandled Synchronous interrupt %ld. Hanging hart %d\n", cause, hart);
				while (1) { WFI(); }
			break;
		}
		
	}
}
