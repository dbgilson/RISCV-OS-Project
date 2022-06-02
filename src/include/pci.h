#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#pragma once

#define MMIO_ECAM_BASE 0x30000000

// This struct holds all the ECAM registers and PCI header information, as well as 
// the different header types (type1 for bus connection and type0 for devices)
struct EcamHeader {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command_reg;
    uint16_t status_reg;
    uint8_t revision_id;
    uint8_t prog_if;
    union {
        uint16_t class_code;
        struct {
            uint8_t class_subcode;
            uint8_t class_basecode;
        };
    };
    uint8_t cacheline_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    union {
        struct {
            uint32_t bar[6];
            uint32_t cardbus_cis_pointer;
            uint16_t sub_vendor_id;
            uint16_t sub_device_id;
            uint32_t expansion_rom_addr;
            uint8_t  capes_pointer;
            uint8_t  reserved0[3];
            uint32_t reserved1;
            uint8_t  interrupt_line;
            uint8_t  interrupt_pin;
            uint8_t  min_gnt;
            uint8_t  max_lat;
        } type0;
        struct {
            uint32_t bar[2];
            uint8_t  primary_bus_no;
            uint8_t  secondary_bus_no;
            uint8_t  subordinate_bus_no;
            uint8_t  secondary_latency_timer;
            uint8_t  io_base;
            uint8_t  io_limit;
            uint16_t secondary_status;
            uint16_t memory_base;
            uint16_t memory_limit;
            uint16_t prefetch_memory_base;
            uint16_t prefetch_memory_limit;
            uint32_t prefetch_base_upper;
            uint32_t prefetch_limit_upper;
            uint16_t io_base_upper;
            uint16_t io_limit_upper;
            uint8_t  capes_pointer;
            uint8_t  reserved0[3];
            uint32_t expansion_rom_addr;
            uint8_t  interrupt_line;
            uint8_t  interrupt_pin;
            uint16_t bridge_control;
        } type1;
        struct {
            uint32_t reserved0[9];
            uint8_t  capes_pointer;
            uint8_t  reserved1[3];
            uint32_t reserved2;
            uint8_t  interrupt_line;
            uint8_t  interrupt_pin;
            uint8_t  reserved3[2];
        } common;
    };
};

typedef struct PciDriver{
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    bool (*probe)(uint8_t b, uint8_t s, uint8_t f);
    bool (*claim_irq)(int irq);
    void (*init)(void);
} PciDriver;

#define COMMAND_REG_MMIO    (1 << 1)

static volatile struct EcamHeader *pci_get_ecam(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg);
void print_pci_connect(void);
bool PciDriver_probe(uint8_t b, uint8_t s, uint8_t f);
bool PciDriver_claim_irq(int irq);
void PciDriver_init(void);
void pci_register_driver(uint16_t vendor, uint16_t device, const PciDriver *drv);
void pci_config_t0(uint8_t bus, uint8_t slot, uint8_t function);
void pci_config_t1(uint8_t bus, uint8_t slot);
PciDriver *pci_find_driver(uint16_t vendor, uint16_t device);
bool pci_dispatch_irq(int irq);
void pci_init(void);
void pci_cape_ptr_check(uint16_t bus, uint16_t slot);
int pci_allocate_bar(struct PciDriver *dev, int which);
uint64_t pci_get_bar(uint16_t bus, uint16_t slot, uint16_t function, uint16_t bar);
void pci_set_bar(uint16_t bus, uint16_t slot, uint16_t function, uint16_t bar, uint64_t value);