#include <printf.h>
#include <virtio_gpu.h>
#include <input-event-codes.h>
#include <virtio.h>
#include <sbi.h>
#include <csr.h>
#include <strhelp.h>
#include <virtio_inp.h>
#include <ringbuff.h>
#include <cpage.h>
#include <mmu.h>

typedef struct tile{
    uint64_t position;
    uint64_t win_position;
    struct Rectangle *texture;
} tile;

typedef struct game{
    tile *board;
    uint64_t mv_count;
} game;


void slide(void){

}


void game_setup(game *g){
    char bytes[16] = {0};
	if(rng_fill(bytes, sizeof(bytes))){
        WFI();
    }
    

}