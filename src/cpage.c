#include <cpage.h>
#include <symbols.h>
#include <lock.h>
#include <stddef.h>
#include <stdint.h>
#include <memhelp.h>
#include <printf.h>

//This is the file for contiguous page allocation (ncpage.c is non-contiguous page allocation)

Mutex cpage_lock;

//This function clears (initializes) the book keeping bytes by going
//through the start of the heap and setting the values to 0.
void cpage_init(void){
    uint64_t heap_start = sym_start(heap);  
    uint64_t heap_end = sym_end(heap);

    //To get number of book keeping bytes (and # of pages since we're doing
    //1 bk byte per page), we simply divide the heap size by 4096
    uint64_t bk_bytes = (heap_end - heap_start) / 4096;

    //Fill heap start space with 0s
    memset((void*)heap_start, 0, bk_bytes);
}

//This function allocates an available number of contiguous pages
//specified by the input and returns a pointer to the start of those pages.
void *cpage_znalloc(int num_pages){
    //printf("Got into cpage_znalloc\n");
    //Check if num_pages <= 0
    if(num_pages <= 0){
        return NULL;
    }

    //Similar to cpage_init, we first get the number of book keeping bytes
    uint64_t heap_start = sym_start(heap);
    uint64_t heap_end = sym_end(heap);
    uint64_t bk_bytes = (heap_end - heap_start) / 4096;

    //Get first book keeping byte to start out
    char *cur_byte = (char *)heap_start;

    //Use doubly nested for loop to go through book keeping bytes
    int i;
    int j;
    int taken_bit = 0;
    int cpage_num = 0;
    for(i = 0; i < bk_bytes; i++){
        //Check if cur_byte has a clear taken bit
        taken_bit = 2 * (i % 4);
        if(!((cur_byte[i] >> taken_bit) & 1)){
            cpage_num += 1;
            for(j = i + 1; j < bk_bytes; i++){
                //Now we try to match the number of pages requested
                //Check if the next bk byte has a taken bit and break if so (reset)
                taken_bit = 2 * (j % 4);
                if((cur_byte[j] >> taken_bit) & 1){
                    cpage_num = 0;
                    break;
                }
                cpage_num += 1;
                if(cpage_num >= num_pages){
                    //If we got here, we got the requested number of contiguous pages
                    for(j = 0; j < cpage_num; j++){
                        //Set taken bits in bk bytes
                        taken_bit = 2 * ((i + j) % 4);
                        cur_byte[i + j] |= 1 << (2 * ((i + j) % 4));
                    }
                    //Put the last bit up in the last bk byte
                    taken_bit = 2 * ((i + j - 1) % 4);
                    cur_byte[i + j - 1] |= 1 << ((2 * ((i + j - 1) % 4)) + 1);

                    //Return the location of the start of the cleared heap after the bk bytes
                    bk_bytes = (bk_bytes + 4095) & -4096;
                    return memset((void*)(heap_start + bk_bytes + PAGE_SIZE * i), 0, PAGE_SIZE * cpage_num);
                } 
            }
        }
        else{
            cpage_num = 0;
        }
    }
    //If we got here, then we couldn't find a large enough number of contiguous
    //pages specified by num_pages
    return NULL;
}

void cpage_free(void *page){
    uint64_t temp_page = (uint64_t)page;

    //Check if page is within heap bounds
    if (temp_page < sym_start(heap) || temp_page > sym_end(heap)){
        return;
    }

    //Similar to cpage_init, we first get the number of book keeping bytes
    uint64_t heap_start = sym_start(heap);
    uint64_t heap_end = sym_end(heap);
    uint64_t bk_bytes = (heap_end - heap_start) / 4096;

    //Get first book keeping byte to start out
    char *cur_byte = (char *)heap_start;

    //Now get the first book keeping byte location of the specified page to free
    int spec_byte = (temp_page - bk_bytes - heap_start) / 4096;

    //Now we go through the process of trying to clear the taken bits
    int taken_bit = 0;
    int last_bit = 0;
    do{
        last_bit = 2 * (spec_byte % 4) + 1;
        taken_bit = 2 * (spec_byte % 4);
        //Check if it is the last byte
        if((cur_byte[spec_byte] >> last_bit) & 1){
            //We're done, so clear the taken and last bit, then break
            cur_byte[spec_byte] &= ~(1 << taken_bit);
            cur_byte[spec_byte] &= ~(1 << last_bit);
            break;
        }
        //Clear the taken and last bit, then repeat with incremented byte
        cur_byte[spec_byte] &= ~(1 << taken_bit);
        cur_byte[spec_byte] &= ~(1 << last_bit);
        spec_byte += 1;
    }while(1);
}