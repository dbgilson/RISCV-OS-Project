#include <ncpage.h>
#include <printf.h>
#include <stddef.h>
#include <lock.h>
#include <symbols.h>

Mutex ncp_lock;

//Since this is a non-contiguous page allocation method, all the page
//structure will hold is the pointer to the next page.
struct PAGE {
    struct PAGE *next;
} *page_head;

//This function will initialize the page table by setting the page head
//pointer to the start of the heap.
void ncpage_init(void){
    //Setup page_head
    page_head = (struct PAGE *)sym_start(heap);
    struct PAGE *page = page_head;
    struct PAGE *page_next = page;

    //Setup the page table list using PAGE_SIZE
    while((unsigned long)page_next < sym_end(heap)) {
        page->next = page_next;
        page = page_next;
        page_next = (struct PAGE *)(((unsigned long)page) + PAGE_SIZE);
    }
    //Set the final page_next to NULL since we've reached the end of the heap
    page->next = NULL;
}

//This function will allocate a page from the current page head and 
//return a pointer to this page.
void *ncpage_zalloc(void){
    //Check if there even is a page in the list
    if(page_head == NULL){
        return NULL;
    }

    //If we got here, then there is a page.  Allocate it
    //Locking since we're modifying a global struct
    mutex_spinlock(&ncp_lock);
    void *ret_page = page_head;
    page_head = page_head->next; //Shift page_head
    mutex_unlock(&ncp_lock);
    return ret_page;
}

//This function will free the page given by the input
void ncpage_free(void *page){
    //Check if NULL
    if(page == NULL){
        printf("Given pointer in ncpage_free is NULL, can't free\n");
        return;
    }

    //Locking since we're modifying a global struct
    mutex_spinlock(&ncp_lock);
    struct PAGE *p_temp = page;
    p_temp->next = page_head;
    page_head = p_temp;
    mutex_unlock(&ncp_lock);

    //We don't need to return anything
    return;
}