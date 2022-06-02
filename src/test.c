#include <printf.h>
#include <sbi.h>
#include <csr.h>
#include <strhelp.h>
#include <pci.h>
#include <cpage.h>
#include <mmu.h>
#include <symbols.h>
#include <test.h>

//I'm moving a bunch of functionality testing functions here to help keep main cleaner


//This function tests the sbi time comparisons using the clint
static void test_sleep(unsigned long hart, unsigned long secs) {
	unsigned long tm = sbi_get_time();
	sbi_set_timecmp(hart, tm + secs * 10000000);
	WFI();
}

//This function helps testing our sbi timer functions using the clint as well as starting/stopping the harts.
ATTR_NAKED_NORET
void c_test_hart(void) {
	unsigned int hart;
	CSR_READ(hart, "sscratch");
	printf("[HART %d]: Started at %ld, going to sleep\n", hart, sbi_rtc_get_time() / 1000000000UL);
	test_sleep(hart, 1);
	printf("[HART %d]: Going to die now....bye\n", hart);
	sbi_hart_stop();
	printf("[HART %d]: Uh oh, I couldn't stop my heart :(\n", hart);
	while (1) { WFI(); }
}