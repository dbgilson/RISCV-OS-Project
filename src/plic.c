#include <plic.h>
#include <printf.h>
#include <pci.h>
#include <virtio.h>
//This file is the same as the plic in the SBI, but the mode is now changed to supervisor.

void plic_set_priority(int interrupt_id, char priority)
{
    uint32_t *base = (uint32_t *)PLIC_PRIORITY(interrupt_id);
    *base = priority & 0x7;
}
void plic_set_threshold(int hart, char priority)
{
    uint32_t *base = (uint32_t *)PLIC_THRESHOLD(hart, PLIC_MODE_SUPERVISOR);
    *base = priority & 0x7;
}
void plic_enable(int hart, int interrupt_id)
{
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, PLIC_MODE_SUPERVISOR);
    base[interrupt_id / 32] |= 1UL << (interrupt_id % 32);
}
void plic_disable(int hart, int interrupt_id)
{
    uint32_t *base = (uint32_t *)PLIC_ENABLE(hart, PLIC_MODE_SUPERVISOR);
    base[interrupt_id / 32] &= ~(1UL << (interrupt_id % 32));
}
uint32_t plic_claim(int hart)
{
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, PLIC_MODE_SUPERVISOR);
    return *base;
}
void plic_complete(int hart, int id)
{
    uint32_t *base = (uint32_t *)PLIC_CLAIM(hart, PLIC_MODE_SUPERVISOR);
    *base = id;
}

void plic_handle_irq(int hart) {
    int irq = plic_claim(hart);
    if((irq == 32) || (irq == 33) || (irq == 34) || (irq == 35)){
        //PCI IRQ
		//printf("IRQ is %d\n", irq);
        PciDriver_claim_irq(irq);
    }
    //printf("Done with driver claim irq\n");
    plic_complete(hart, irq);
}
