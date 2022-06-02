#include <printf.h>
#include <virtio_gpu.h>
#include <input-event-codes.h>
#include <sbi.h>
#include <csr.h>
#include <strhelp.h>
#include <virtio_inp.h>
#include <ringbuff.h>
#include <cpage.h>
#include <mmu.h>


void paint(void){
    struct Rectangle *rect;
    rect = (struct Rectangle *)cpage_znalloc(1);
    rect->x = 160;
    rect->y = 0;
    rect->width = 80;
    rect->height = 80;

    struct Pixel *p_color;
    p_color = (struct Pixel *)cpage_znalloc(1);
    p_color->a = 0;
    p_color->r = 255;
    p_color->g = 0;
    p_color->b = 0;

    fill_rect(screen_buff.width, screen_buff.height, screen_buff.pixels, rect, p_color);
    gpu_redraw(rect);
    
    rect->x = 240;
    p_color->a = 0;
    p_color->r = 0;
    p_color->g = 255;
    p_color->b = 0;

    fill_rect(screen_buff.width, screen_buff.height, screen_buff.pixels, rect, p_color);
    gpu_redraw(rect);

    rect->x = 320;
    p_color->a = 0;
    p_color->r = 0;
    p_color->g = 0;
    p_color->b = 255;

    fill_rect(screen_buff.width, screen_buff.height, screen_buff.pixels, rect, p_color);
    gpu_redraw(rect);

    rect->x = 400;
    p_color->a = 0;
    p_color->r = 255;
    p_color->g = 0;
    p_color->b = 255;

    fill_rect(screen_buff.width, screen_buff.height, screen_buff.pixels, rect, p_color);
    gpu_redraw(rect);

    rect->x = 0;
    rect->width = 160;
    p_color->a = 0;
    p_color->r = 200;
    p_color->g = 200;
    p_color->b = 200;

    fill_rect(screen_buff.width, screen_buff.height, screen_buff.pixels, rect, p_color);
    gpu_redraw(rect);

    rect->x = 480;

    fill_rect(screen_buff.width, screen_buff.height, screen_buff.pixels, rect, p_color);
    gpu_redraw(rect);

    rect->width = 10;
    rect->height = 10;
    //This is the main loop for the paint program
    InputEvent *elem;
    elem = (InputEvent *)cpage_znalloc(1);
    
    int x, y = 0;
    bool btn_hold = false;
    int color_value = 0;
    while(1){
        //Really only need to do anything if there is a
        //ring buffer input value
        if(ipt_ringbuf->capacity != 0){
            ring_pop(ipt_ringbuf, elem);
            if(elem->type == EV_ABS){
                //We got a mouse input, update latest x, y location
                if(elem->code == ABS_X){ //X Coordinate
                    x = (elem->value / 32767) * 640;
                }
                if(elem->code == ABS_Y){ //Y Coordinate
                    y = (elem->value / 32767) * 480;
                }
            }
            else if(elem->type == EV_KEY){
                //See if it's a mouse press
                if(elem->code == BTN_LEFT && elem->value == 1){
                    btn_hold = true;
                    //Check if we need to change color value
                    if(y <= 80 && x >= 160 && x <= 239){
                         p_color->r = 255;
                         p_color->g = 0;
                         p_color->b = 0;
                    }
                    else if(y <= 80 && x >= 240 && x <= 319){
                        p_color->r = 0;
                        p_color->g = 255;
                        p_color->b = 0;
                    }
                    else if(y <= 80 && x >= 320 && x <= 399){
                        p_color->r = 0;
                        p_color->g = 0;
                        p_color->b = 255;
                    }
                    else if(y <= 80 && x >= 400 && x <= 480){
                        p_color->r = 255;
                        p_color->g = 0;
                        p_color->b = 255;
                    }
                }
                if(elem->code == BTN_LEFT && elem->value == 0){
                    btn_hold = false;  
                }
            }
        }
        if(y >= 90 && btn_hold){
            //Draw a pixel to the screen
            rect->x = x;
            rect->y = y;
            fill_rect(screen_buff.width, screen_buff.height, screen_buff.pixels, rect, p_color);
            gpu_redraw(rect);
        }
    }
    return;
}