#pragma once

struct Process;
int copy_from_user(void *kernel_addr, struct Process *p, const void *user_addr,
                   int bytes);
int copy_to_user(struct Process *p, void *user_addr, const void *kernel_addr,
                 int bytes);