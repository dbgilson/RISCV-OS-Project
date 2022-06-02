#include <kmalloc.h>
#include <cpage.h>
#include <stddef.h>
#include <printf.h>
#include <csr.h>
#include <stdbool.h>
#include <stdint.h>
#include <mmu.h>
#include <lock.h>
#include <memhelp.h>

#define MIN_MALLOC 8192
#define MIN_ALLOC_NODE 16

uint64_t kernel_heap_v_addr = KERNEL_HEAP_START_VADDR;

//Create our free list struct to use in malloc
typedef struct Flist{
    size_t size;
    struct Flist *prev;
    struct Flist *next;
} flist;

//Need a global pointer to the head of the free list
flist *kmalloc_head;
Mutex kmalloc_lock;

//Function to append (insert) a node (b) into the free list related to node (a)
static void node_append(flist *a, flist *b){
    b->next = a->next;
    b->prev = a;
    b->next->prev = b;
    b->prev->next = b;
}

//Function to remove a node from the free list
static void node_remove(flist *a){
    a->next->prev = a->prev;
    a->prev->next = a->next;
}

//This is the main heap allocator function (malloc)
void *kmalloc(size_t bytes){
    flist *temp, *p, *v;
    uint64_t p_addr;
    //Align the bytes and BK bytes to 16 to cover 4 byte, 8 byte, and 16 byte alignments
    bytes = (bytes + sizeof(size_t) + sizeof(flist)) & -16UL;
    //printf("Bytes after calculation is %d\n", bytes);
    //We're accessing global variables, so need to start locking
    mutex_spinlock(&kmalloc_lock);

    //Try to find an available page
    temp = kmalloc_head->next;
    while(1){
        if(temp->size < bytes){
            break; //We found a space big enough
        }
        if(temp == kmalloc_head){
            break; //We looped and couldn't find a big enough available space
        }
        temp = temp->next;
    }

    if (temp == kmalloc_head) { //Have to allocate a new page
        int num_pages = (bytes / PAGE_SIZE) + 1;  //If bytes < 4096, just allocate 1 page
        temp = (flist *)(kernel_heap_v_addr + PAGE_SIZE);
        for (int i = 0; i <= num_pages; i++) {
            //Go to next page in kernel page table
            kernel_heap_v_addr += PAGE_SIZE; 

            //Attempt to allocate a page (If returns NULL, then we couldn't allocate a page)
            p = cpage_znalloc(1);
            if (p == NULL) {
                printf("Couldn't allocate page in malloc\n");
                mutex_unlock(&kmalloc_lock);
                return NULL;
            }

            //If we made it here, we have a pointer to a page
            p_addr = (uint64_t)p;
            v = (flist *)kernel_heap_v_addr;

            //We can now map the page address to the kernel mmu page table (Also giving it R/W Access)
            mmu_map(kernel_mmu_table, kernel_heap_v_addr, p_addr, PB_READ | PB_WRITE);

            //Use SFENCE_ASID since we modified a Kernel PT Entry
            SFENCE_ASID(KERNEL_ASID);

            //Restore the kernel page size to 4k and append the node to the free list 
            v->size = PAGE_SIZE;
            node_append(kmalloc_head->prev, v);
        }

        //Coalesce the list to minimize fragmentation
        coalesce_free_list();
    }
    //This is the return value
    char *ret = (char *)temp + temp->size - bytes;

    //If temp->size - bytes < 16, then we have to remove it since it can't be alligned to 16 bytes
    if (temp->size - bytes < MIN_ALLOC_NODE) {
        node_remove(temp);
    }
    else {
        temp->size -= *(size_t *)ret = bytes;
    }

    mutex_unlock(&kmalloc_lock);
    return ret + sizeof(size_t); //Remember to add 16 byte offset to returned value
}

void *kzalloc(size_t bytes){
    return memset(kmalloc(bytes), 0, bytes - sizeof(size_t));
}

//This function frees the given node pointer from the free list
void kfree(void *ptr){
    //Pointer needs to not be NULL in order to freed
    if (NULL != ptr) {
        mutex_spinlock(&kmalloc_lock);
        ptr -= sizeof(size_t); //Subtract 16 byte offset of given
        if (*((size_t*)ptr) == 0) {
            mutex_unlock(&kmalloc_lock);
            printf("Error freeing 0x%08lx.\n", ptr);
            return;
        }
        flist *temp;
        temp = kmalloc_head->prev;
        while(1){
        if(temp == kmalloc_head || ptr > (void *)temp){
            break; //Either made it to kmalloc head or location of ptr
        }
        temp = temp->prev;
        }

        //Append the ptr node and coalesce the free list
        node_append(temp, ptr);
        coalesce_free_list();
        mutex_unlock(&kmalloc_lock);
    }
}

//This function coalesces the free list by trying to find contiguous allocations
//and combining them (adding their size together and removing the redundant node)
void coalesce_free_list(void){
    flist *temp = kmalloc_head->next;
    while (temp->next != kmalloc_head) {
        if ((char *)temp + temp->size == (char*)temp->next){
            //Found a contiguous allocation, so add the size to the current
            //allocation and remove the additional node
            temp->size += temp->next->size;
            node_remove(temp->next);
        }
        else {
            //Keep trying to find contiguous allocations
            temp = temp->next;
        }
    }
}

//This function initializes the heap space used by the kernel
void init_kernel_heap(void){
    flist *p;
    uint64_t p_addr;

    p = cpage_znalloc(1);
    p_addr = (uint64_t)p;
    //printf("P from cpage_znalloc is 0x%08lx \n", p_addr);

    //Map the start of the kernel heap virtual address to the physical address given from cpage_znalloc()
    mmu_map(kernel_mmu_table, kernel_heap_v_addr, p_addr, PB_READ | PB_WRITE);
    SFENCE_ASID(KERNEL_ASID);

    //Initialize kmalloc_head to the kernel heap virtual address as it 
    //will be the starting node in the free list.
    kmalloc_head = (flist *)kernel_heap_v_addr;
    kmalloc_head->size = 0;
    kmalloc_head->next = kmalloc_head->prev = kmalloc_head;
}
