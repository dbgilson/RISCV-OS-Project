#include <plic.h>
#include <uart.h>

//Set priority level of a given interrupt
void plic_set_priority(int interrupt_id, char priority)
{
    uint32_t *base = (uint32_t *)PLIC_PRIORITY(interrupt_id);
    *base = priority & 0x7;
}

//Set the threshold that a given Hart can hear an interrupt
void plic_set_threshold(int hart, char priority)
{
    uint32_t *base = (uint32_t *)PLIC_THRESHOLD(hart, PLIC_MODE_MACHINE);
    *base = priority & 0x7;
}

//Enable a given interrupt in the plic
void plic_enable(int hart, int interrupt_id)
{
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, PLIC_MODE_MACHINE);
    base[interrupt_id / 32] |= 1UL << (interrupt_id % 32);
}

//Disable a given interrupt in the plic
void plic_disable(int hart, int interrupt_id)
{
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, PLIC_MODE_MACHINE);
    base[interrupt_id / 32] &= ~(1UL << (interrupt_id % 32));
}

//Claims a given interrupt and return the id of the interrupt
uint32_t plic_claim(int hart)
{
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, PLIC_MODE_MACHINE);
    return *base;
}

//Completes an interrupt claim on a given Hart
void plic_complete(int hart, int id)
{
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, PLIC_MODE_MACHINE);
    *base = id;
}

//Our plic handler called by the trap handler.
//We only have UART enabled, so that is the only interrupt we check for
void plic_handle_irq(int hart){
	int irq = plic_claim(hart);
	if(irq == 10)
		uart_handle_irq();
	plic_complete(hart, irq);
}
