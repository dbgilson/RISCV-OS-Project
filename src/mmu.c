#include <mmu.h>
#include <lock.h>
#include <cpage.h>

//Reference the kernel page table and make a lock for it
Page_Table *kernel_mmu_table;
Mutex kernel_mmu_lock;

//This function maps a given virtual address to a physical address in the page table
bool mmu_map(Page_Table *table, uint64_t vaddr, uint64_t paddr, uint64_t bits){
    int i;
    uint64_t table_entry;

    //Here are some arrays that hold the VPNs and PPNs of the given
    //virtual and physical addresses to make accessing them easier
    const uint64_t vpn[] = {
        (vaddr >> 12) & 0x1ff,
        (vaddr >> 21) & 0x1ff,
        (vaddr >> 30) & 0x1ff
    };
    const uint64_t ppn[] = {
        (paddr >> 12) & 0x1ff,
        (paddr >> 21) & 0x1ff,
        (paddr >> 30) & 0x3ffffff //PPN[2] is 26 bits
    };

    //Since we're accessing the global kernel mmu table,
    //we'll start a lock
    mutex_spinlock(&kernel_mmu_lock);
    
    //Use a for loop to translate through table levels (start at 2)
    for(i = 2; i >= 1; i--){
        //Get entry at current VPN[i] address
        table_entry = table->pt_entry[vpn[i]];

        //Check if the entry is made
        if(!(table_entry & 1)){
            //Need to make the entry
            Page_Table *new_table = cpage_znalloc(1);
            table_entry = (((uint64_t)new_table) >> 2) | PB_VALID;
            table->pt_entry[vpn[i]] = table_entry;
        }
        //Get link to next level page table
        table = (Page_Table *)((table_entry << 2) & ~0xfffUL);
    }

    //Now we're at last level (level 0), so now we just translate
    //all of the PPNs to the table entry, put in the given permission
    //bits, and set the entry to valid.
    table_entry = (ppn[2] << 28) | (ppn[1] << 19) | (ppn[0] << 10 ) | bits | PB_VALID;
    table->pt_entry[vpn[0]] = table_entry;

    //printf("Mapped vaddr 0x%08lx to table_entry: 0x%08lx.\n", vaddr, table_entry);

    //Now we're done
    mutex_unlock(&kernel_mmu_lock);
    return true;
}

//This function frees the page tables starting at the specified page
void mmu_free(Page_Table *table){

    uint64_t table_entry;

    //Go through all the table pt_entry and free them recursivly
    for(int i = 0; i < 512; i++){
        //Get the current entry in the table
        table_entry = table->pt_entry[i];

        //Check if the entry is valid
        if(table_entry & 1){
            //Check if the entry is a branch (Check XRW bits)
            if(!(table_entry & 0b1110)){
                //This is a branch, so we need to go to the next level
                table_entry = (table_entry << 2) & ~0xfffUL;
                mmu_free((Page_Table *)table_entry);
            }
            else{
                //This is a leaf, so just set clear this entry
                table->pt_entry[i] = 0;
            }
        }
    }
    //When we're here, we can just free the table
    cpage_free(table);
}

//This function translates a given virtual address into the table entry corresponding to the vaddr
uint64_t mmu_translate(Page_Table *table, uint64_t vaddr){
    //We'll want a vpn array to start translating the given virtual address
    const uint64_t vpn[] = {
        (vaddr >> 12) & 0x1ff,
        (vaddr >> 21) & 0x1ff,
        (vaddr >> 30) & 0x1ff
    };

    //Go through the page table and try to translate to a leaf to return
    //a valid physical address
    for(int i = 2; i >= 0; i--){
        //Get our current table entry
        uint64_t table_entry = table->pt_entry[vpn[i]];
        
        //Check if it is a valid entry, if not, return
        //an invalid value
        if(!(table_entry & PB_VALID)){
            return -1UL;
        }
        else if(!(table_entry & 0b1110)){
            //Now we're at a branch, so we need to go to the next level
            table = (Page_Table *)((table_entry << 2) & ~0xfffUL);
        }
        else{
            //If we reach here, we're at a leaf, so 
            //translate and return the physical address
            const uint64_t ppn[] = {
                (table_entry >> 10) & 0x1ff,
                (table_entry >> 19) & 0x1ff,
                (table_entry >> 28) & 0x3ffffff //PPN[2] is 26 bits
            };
            //Use switch statement to determine what address to return
            //based on the level given by i.
            switch(i){
                case 0: return (ppn[2] << 30) | (ppn[1] << 21) | (ppn[0] << 12) | (vaddr & 0xfff);
                case 1: return (ppn[2] << 30) | (ppn[1] << 21) | (vaddr & 0x1fffff);
                case 2: return (ppn[2] << 30) | (vaddr & 0x3fffffff);
            }
        }
    }
    //If we made it here, we couldn't translate it
    return -1UL;
}

void mmu_map_multiple(Page_Table *table, uint64_t v_start, uint64_t p_start, uint64_t bytes, uint64_t bits){
    for(uint64_t i = 0; i < bytes; i += PAGE_SIZE){
        mmu_map(table, v_start + i, p_start + i, bits);
    }
}

