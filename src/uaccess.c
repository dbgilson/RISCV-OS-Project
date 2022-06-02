#include <errno.h>
#include <mmu.h>
#include <printf.h>
#include <process.h>
#include <uaccess.h>
#include <memhelp.h>
#include <cpage.h>

int copy_from_user(void *kernel_addr, Process *p, const void *user_addr, int bytes){
    int i = 0;
    while (i < bytes) {
        int till_next_page = ALIGN_UP_POT((uint64_t)user_addr + i, PAGE_SIZE) - (uint64_t)user_addr - i;
        uint64_t phys = mmu_translate(p->ptable, (uint64_t)user_addr + i);
        if (phys == -1UL) {
            break;
        }
        memcpy(kernel_addr + i, (void *)phys, till_next_page > (bytes - i) ? (bytes - i) : till_next_page);
        i += till_next_page;
    }

    return i;
}

int copy_to_user(Process *p, void *user_addr, const void *kernel_addr, int bytes){
    int i = 0;
    while (i < bytes) {
        int till_next_page = ALIGN_UP_POT((uint64_t)user_addr + i, PAGE_SIZE) - (uint64_t)user_addr - i;
        uint64_t phys = mmu_translate(p->ptable, (uint64_t)user_addr + i);
        if (phys == -1UL) {
            break;
        }
        memcpy((void *)phys, kernel_addr + i, till_next_page > (bytes - i) ? (bytes - i) : till_next_page);
        i += till_next_page;
    }
    return i;
}