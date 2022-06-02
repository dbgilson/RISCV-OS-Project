#pragma once

struct Process;

void schedule_init(int mode);
void schedule_hart_spawn(int hart);
void schedule_hart_check(int hart);
struct Process *schedule_find_next(int hart);
void schedule_add(struct Process *p);
void schedule_remove(struct Process *p);
