#pragma once

#define SBI_SVCCALL_HART_STATUS  (1)
#define SBI_SVCCALL_HART_START   (2)
#define SBI_SVCCALL_HART_STOP    (3)
#define SBI_SVCCALL_GET_TIME     (5)
#define SBI_SVCCALL_SET_TIMECMP  (6)
#define SBI_SVCCALL_ADD_TIMECMP  (7)
#define SBI_SVCCALL_RTC_GET_TIME (8)
#define SBI_SVCCALL_WHOAMI       (9)

#define SBI_SVCCALL_PUTCHAR  (10)
#define SBI_SVCCALL_GETCHAR  (11)

#define SBI_SVCCALL_ACK_TIMER (12)