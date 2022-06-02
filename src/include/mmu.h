#pragma once
#include <stdint.h>
#include <stdbool.h>

//Since a page table is 4KB at 512 entries (8B each), we need a struct
//that holds 512 unsigned long values (or uint64_t for guaranteed 64 bits)
typedef struct Page_Table{
    uint64_t pt_entry[512];
} Page_Table;

//Create MMU table for the OS
#define KERNEL_ASID 0xFFFFUL
extern Page_Table *kernel_mmu_table;

//Here are some provided definitions for a page
//table entry's permission and access/dirty bits
#define PB_NONE     0
#define PB_VALID    (1UL << 0)
#define PB_READ     (1UL << 1)
#define PB_WRITE    (1UL << 2)
#define PB_EXECUTE  (1UL << 3)
#define PB_USER     (1UL << 4)
#define PB_GLOBAL   (1UL << 5)
#define PB_ACCESS   (1UL << 6)
#define PB_DIRTY    (1UL << 7)

//Here are some definitions for easy access to SATP bits
//and to set the PPN or ASID of a Hart's SATP register
#define SATP_MODE_BIT    60
#define SATP_MODE_SV39   (8UL << SATP_MODE_BIT)
#define SATP_ASID_BIT    44
#define SATP_PPN_BIT     0
#define SATP_SET_PPN(x)  ((((uint64_t)x) >> 12) & 0xFFFFFFFFFFFUL)
#define SATP_SET_ASID(x) ((((uint64_t)x) & 0xFFFF) << 44)

#define SATP(table, asid)  (SATP_MODE_SV39 | SATP_SET_PPN(table) | SATP_SET_ASID(asid))

bool mmu_map(Page_Table *table, uint64_t vaddr, uint64_t paddr, uint64_t bits);
void mmu_free(Page_Table *table);
uint64_t mmu_translate(Page_Table *table, uint64_t vaddr);
void mmu_map_multiple(Page_Table *table, uint64_t v_start, uint64_t p_start, uint64_t bytes, uint64_t bits);
