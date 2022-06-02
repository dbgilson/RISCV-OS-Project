#include <csr.h>
#include <printf.h>
#include <plic.h>
#include <hart.h>
#include <stdbool.h>
#include <svccall.h>
#include <clint.h>

//This function handles trap calls caused either by asyncronous actions (hardware)
//or by syncronous interrupts (OS/Software)
void c_trap_handler(void){
	//We'll need mcause, mhartid, mscratch, and mepc values to determine
	//and handle trap.
	long mcause;
	long mhartid;
	long *mscratch;
	unsigned long mepc;
	CSR_READ(mcause, "mcause");
	CSR_READ(mhartid, "mhartid");
	CSR_READ(mscratch, "mscratch");
	CSR_READ(mepc, "mepc");

	int is_async = (mcause >> 63) & 1;
	mcause &= 0xff;
	
	//If asyncronous, then it is machine level trap (SBI)
	//Else, it is syncronous (OS/Supervisor)
	if (is_async) {
		switch (mcause) {
			case 3: // MSIP
				hart_handle_msip(mhartid);
			break;
			case 7: ;// MTIP
				//Delegate SIP
				unsigned long sip;
				CSR_READ(sip, "mip");
				CSR_WRITE("mip", sip | SIP_STIP);
				clint_set_mtimecmp(mhartid, CLINT_MTIMECMP_INFINITE);
			break;
			case 11: // MEI (Only one we have right now though is UART)
				plic_handle_irq(mhartid);
			break;
			default:
				printf("Unhandled Asynchronous interrupt in SBI: %ld\n", mcause);
				break;
		}
	}
	else {
		switch (mcause) {
			case 9:
				//Pass to OS trap handler
				svccall_handle(mhartid);
				CSR_WRITE("mepc", mepc + 4);
			break;
			default:
				printf("Unhandled Synchronous interrupt %ld. Hanging hart %d\n", mcause, mhartid);
				while (1) { WFI(); }
			break;
		}
		
	}
}


