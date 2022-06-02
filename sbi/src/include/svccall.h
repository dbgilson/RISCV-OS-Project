#pragma once
#include <sbicalls.h>

// Handles machine level traps/interrupts
void svccall_handle(int hart);