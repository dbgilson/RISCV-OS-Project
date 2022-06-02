#pragma once
#include <stdbool.h>
#define HART_DEFAULT_CTXSWT   100000

//Enumeration for 5 status options for harts
typedef enum HartStatus {
    HS_INVALID, // A HART that doesn't exist
    HS_STOPPED, // A HART that is parked waiting for interrupt
    HS_STARTING, // A Hart that is in the process of waking up
    HS_STARTED, // A Hart that is currently running code
    HS_STOPPING // A Hart that is in the process of becoming parked
} HartStatus;

//Enumeration for a Hart's privilege mode
typedef enum HartPrivMode {
    HPM_USER, //00 USER SPACE
    HPM_SUPERVISOR, //01 OS
    HPM_HYPERVISOR, //10 NOT USED
    HPM_MACHINE //11 SBI
} HartPrivMode;

//Structure for what kind of data the Hart table will hold
typedef struct HartData {
    HartStatus status;
    HartPrivMode priv_mode;
    unsigned long target_address;
    unsigned long scratch;
} HartData;

//We have 8 Harts, so we make a global Hart array of 8 Harts
extern HartData sbi_hart_data[8];

HartStatus hart_get_status(unsigned int hart);
bool hart_start(unsigned int hart, unsigned long target, unsigned long scratch);
bool hart_stop(unsigned int hart);
void hart_handle_msip(unsigned int hart);

