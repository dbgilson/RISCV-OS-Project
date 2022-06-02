#include <ring_buff.h>
#include <lock.h>
//Adding Mutex to Access Ring Buffer
Mutex buf_mutex;

enum {BUFF_SIZE = 10}; //10 Elements in ring buff 
char buf[BUFF_SIZE]; 
int end = 0; 
int start = 0;

//Function to put given char in ring buffer.
//Uses mutex to better ensure order in buffer.
void ring_put(char c) {
  mutex_spinlock(&buf_mutex);
  if (end < BUFF_SIZE){
    buf[(start + end) % BUFF_SIZE] = c;
    end += 1;
  }
  mutex_unlock(&buf_mutex);
}

//Function to return the oldest char in the ring buffer.
//Uses mutex to better ensure order of char return.
char ring_get() {
  char ret_c = 0xff;
  mutex_spinlock(&buf_mutex);
  if(end > 0){
    ret_c = buf[start];
    start = (start + 1) % BUFF_SIZE;
    end -= 1;
  }
  mutex_unlock(&buf_mutex);
  return ret_c;
}