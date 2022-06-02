#include <csr.h>
#include <uart.h>
#include <plic.h>
#include <printf.h>
#include <lock.h>
#include <hart.h>

// Function to initialize PMP of a Hart
static void pmp_init(){
	// Top of range address is 0xffff_ffff. Since this is pmpaddr0
	// then the bottom of the range is 0x0
	CSR_WRITE("pmpaddr0", 0xffffffff >> 2);
	
	//Permissions for RWX
	CSR_WRITE("pmpcfg0", (0b01 << 3) | (1 << 2) | (1 << 1) | (1 << 0));
}

// Function to initialize PLIC and set priorities (Only done by Hart 0)
void plic_init(){
	plic_enable(0, 10); // Enable uart
	plic_set_priority(10, 7); // Set Uart Priority
	plic_set_threshold(0, 0); // Hart 0 hears everything
}

//Below are a few assembly functions just to make main more
//readable in C code.

// in sbi/asm/clearbss.S
void clear_bss();

// in sbi/asm/start.S
void park();

// in sbi/asm/trap.S
void sbi_trap_vector();

long int SBI_GPREGS[8][32];
Mutex hart0lock = MUTEX_LOCKED;

ATTR_NAKED_NORET
void main(int hart) {
	if (hart != 0) {
	    // If we get here, then the running hart is not hart 0, so we want to
		// wait for hart 0 to initialize before allowing other harts to run
		mutex_spinlock(&hart0lock);
		mutex_unlock(&hart0lock);
		pmp_init();

		// Hart 0 is the only one that should be started initially, so we'll
		// stop the rest in order to delegate tasks to them later.
		sbi_hart_data[hart].status = HS_STOPPED;
		sbi_hart_data[hart].target_address = 0;
		sbi_hart_data[hart].priv_mode = HPM_MACHINE;

		CSR_WRITE("mscratch", &SBI_GPREGS[hart][0]);
		CSR_WRITE("sscratch", hart);
		CSR_WRITE("mepc", park); // Have the Harts park
		CSR_WRITE("mideleg", 0); 
		CSR_WRITE("medeleg", 0);
		CSR_WRITE("mie", MIE_MSIE); // Only enable software interrupts
		CSR_WRITE("mstatus", MSTATUS_FS_INITIAL | MSTATUS_MPP_MACHINE | MSTATUS_MPIE);
		CSR_WRITE("mtvec", sbi_trap_vector);
		MRET();
	}

	// If we get here, we are Hart 0
	clear_bss();
	uart_init();
	// Now we're good to let the other Harts run
	mutex_unlock(&hart0lock);
	plic_init();
	pmp_init();
	// We park the rest of the Harts, so Hart 0 is the only one that is
	// started (running the OS)
	sbi_hart_data[0].status = HS_STARTED;
	sbi_hart_data[0].target_address = 0;
	sbi_hart_data[0].priv_mode = HPM_MACHINE;

	CSR_WRITE("mscratch", &SBI_GPREGS[hart][0]);// Store trap frame into mscratch register
	CSR_WRITE("sscratch", hart); // Store hart # in sscratch so the OS can hear it.
	CSR_WRITE("mepc", OS_LOAD_ADDR); // Give PC the OS address 
	CSR_WRITE("mtvec", sbi_trap_vector); // Give address of the trap vector
	CSR_WRITE("mie", MIE_MEIE | MIE_SEIE | MIE_MTIE | MIE_STIE | MIE_MSIE | MIE_SSIE); // Enable machine/software mode external, timer, and software interrupts
	CSR_WRITE("mideleg", SIP_SEIP | SIP_STIP | SIP_SSIP); // Delegate S-level external, timer, and software interrupts
	CSR_WRITE("medeleg", MEDELEG_ALL); // Delegate all S-level exceptions
	CSR_WRITE("mstatus", MSTATUS_FS_INITIAL | MSTATUS_MPP_SUPERVISOR | MSTATUS_MPIE);  // Set up floating point, supervisor mode, and enable interrupts
	MRET(); // Jump to OS
}
