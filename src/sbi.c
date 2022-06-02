#include <sbi.h>

//Calls the SBI_SVCCALL_PUTCHAR function to attempt to put a char in the ring buffer.
void sbi_putchar(char c) {
	asm volatile ("mv a7, %0\nmv a0, %1\necall" :: "r"(SBI_SVCCALL_PUTCHAR), "r"(c) : "a7", "a0");
}

//Calls the SBI_SVCCALL_GETCHAR function to attempt to get a char from the ring buffer
char sbi_getchar(void) {
	char c;
	asm volatile ("mv a7, %1\necall\nmv %0, a0\n" : "=r"(c) : "r"(SBI_SVCCALL_GETCHAR) : "a7", "a0");
	return c;
}

//Calls the SBI_SVCCALL_HART_STATUS to return the status of a given Hart.
int sbi_hart_get_status(unsigned int hart) {
	int stat;
	asm volatile ("mv a7, %1\nmv a0, %2\necall\nmv %0, a0\n" : "=r"(stat) : "r"(SBI_SVCCALL_HART_STATUS), "r"(hart) : "a0", "a7");
	return stat;
}

//Calls the SBI_SVCCALL_HART_START function to attempt to start a hart with a given frame (given by scratch register)
int sbi_hart_start(unsigned int hart, unsigned long target, unsigned long scratch){
    int stat;
    asm volatile(
        "mv a7, %1\nmv a0, %2\nmv a1, %3\nmv a2, %4\necall\nmv %0, a0\n"
        : "=r"(stat)
        : "r"(SBI_SVCCALL_HART_START), "r"(hart), "r"(target), "r"(scratch)
        : "a0", "a1", "a2", "a7");
    return stat;
}

//Calls the SBI_SVCCALL_HART_STOP function to attempt to stop a given hart
int sbi_hart_stop(void) {
	int stat;
	asm volatile ("mv a7, %1\necall\nmv a0, %0" : "=r"(stat) :
				  "r"(SBI_SVCCALL_HART_STOP) : "a0", "a7");
	return stat;
}

//Calls the SBI_SVCCALL_GET_TIME function to attempt to get the current machine time from the clint
unsigned long sbi_get_time(void) {
	unsigned long ret;
	asm volatile("mv a7, %1\necall\nmv %0, a0" : "=r"(ret) : "r"(SBI_SVCCALL_GET_TIME) : "a0", "a7");
	return ret;
}

//Calls the SBI_SVCCALL_SET_TIMECMP function to attempt to set the timecmp register to get a timer interrupt at a specified time
void sbi_set_timecmp(unsigned int hart, unsigned long val) {
	asm volatile("mv a7, %0\nmv a0, %1\nmv a1, %2\necall" :: "r"(SBI_SVCCALL_SET_TIMECMP),
	             "r"(hart), "r"(val) : "a0", "a1", "a7"
	);
}

//Calls the SBI_SVCCALL_ADD_TIMECMP function to attempt to add to the timecmp register to increase the specified time that we'll get an interrupt
void sbi_add_timercmp(unsigned int hart, unsigned long val){
    asm volatile(
        "mv a7, %0\nmv a0, %1\nmv a1, %2\necall" ::"r"(SBI_SVCCALL_ADD_TIMECMP),
        "r"(hart), "r"(val)
        : "a0", "a1", "a7");
}

//Calls the SBI_SVCCALL_ACK_TIMER function to attempt to acknowledge the timer interrupt
void sbi_ack_timer(void){
    asm volatile("mv a7, %0\necall" ::"r"(SBI_SVCCALL_ACK_TIMER) : "a7");
}

//Calls the SBI_SVCCALL_RTC_GET_TIME function to attempt to get the time value from the rtc
unsigned long sbi_rtc_get_time(void) {
	unsigned long ret;
	asm volatile("mv a7, %1\necall\nmv %0, a0" : "=r"(ret) : "r"(SBI_SVCCALL_RTC_GET_TIME) : "a0","a7");
	return ret;
}

int sbi_whoami(void) {
	int ret;
	asm volatile("mv a7, %1\necall\nmv %0, a0" : "=r"(ret) : "r"(SBI_SVCCALL_WHOAMI) : "a0","a7");
	return ret;
}
