#include <memhelp.h>
#include <mmu.h>
#include <cpage.h>
#include <process.h>
#include <elf.h>

//This function loads elf header data into a process which can then later execute that elf.
bool elf_load(Process *p, const void *elf){
    //First, we need the elf header and program header
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf;
    Elf64_Phdr *phdr = (Elf64_Phdr *)(elf + ehdr->e_phoff);

    //Check if it is an elf file, RISCV, and an Executable
    if(!memcmp(ehdr->e_ident, ELFMAG, 4) || ehdr->e_machine != EM_RISCV || ehdr->e_type != ET_EXEC){
        return false;
    }
    uint64_t adr_start = 0;
    uint64_t adr_end = 0;
    bool check_load = false;
    
    //Go through virtual address ranges
    for(int i = 0; i < ehdr->e_phnum; i++){
        if(phdr[i].p_type == PT_LOAD){
            //Only look at program headers that are needing to be loaded
            if(!check_load){
                //First time seeing PT_LOAD, so setup starting and ending address space
                adr_start = phdr[i].p_vaddr;
                adr_end = phdr[i].p_vaddr;
                check_load = true;
            }
            else if(adr_start > phdr[i].p_vaddr){
                adr_start = phdr[i].p_vaddr;
            }
            else if(adr_end < (phdr[i].p_vaddr + phdr[i].p_memsz)){
                adr_end = phdr[i].p_vaddr + phdr[i].p_memsz;
            }
        }
    }

    if(!check_load){
        //If we made it here, we didn't find a PT_LOAD section
        return false;
    }

    //We got starting and ending address locations, so start setting up the process
    
    //Point process program counter to elf's entry point
    p->frame.sepc = ehdr->e_entry;

    //Get the number of image pages from adr_start to adr_end, which need to be page aligned for user space.
    p->image_pg_num = (ALIGN_UP_POT(adr_end, PAGE_SIZE) - ALIGN_DOWN_POT(adr_start, PAGE_SIZE)) / PAGE_SIZE;

    //Allocate pages to the image pointer
    p->image = cpage_znalloc(p->image_pg_num);

    //Now we need to load the data from the elf file to our process space
    for(int i = 0; i < ehdr->e_phnum; i++){
        if(phdr[i].p_type == PT_LOAD){
            memcpy((void *)p->image + phdr[i].p_vaddr - adr_start, (void *)elf + phdr[i].p_offset, phdr[i].p_memsz);
        }
    }

    //Finally, map the address range to the process's page table
    mmu_map_multiple(p->ptable, adr_start, adr_start + p->image_pg_num * PAGE_SIZE,
     (uint64_t)p->image, PB_USER | PB_READ | PB_WRITE | PB_EXECUTE);
    
    return true;
}
