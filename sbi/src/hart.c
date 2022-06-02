#include <hart.h>
#include <lock.h>
#include <csr.h>
#include <printf.h>

//Hart Data structs and their Mutex
HartData sbi_hart_data[8];
Mutex sbi_hart_lock[8];

//WFI
void park();

//This function simply returns the status of a given Hart
HartStatus hart_get_status(unsigned int hart) {
    if (hart >= 8) {
        return HS_INVALID;
    }
    return sbi_hart_data[hart].status;
}

//This function attempts to start a given hart with a specified frame given by scratch
bool hart_start(unsigned int hart, unsigned long target, unsigned long scratch) {
    bool ret = true;
    if (!mutex_trylock(sbi_hart_lock + hart)) {
        return false;
    }

    if (sbi_hart_data[hart].status != HS_STOPPED) {
        ret = false;
    }
    else {
        sbi_hart_data[hart].status = HS_STARTING;
        sbi_hart_data[hart].target_address = target;
        sbi_hart_data[hart].scratch = scratch;
        clint_set_msip(hart);
    }
    mutex_unlock(sbi_hart_lock + hart);
    return ret;
}

//This function attempts to stop a specified hart
bool hart_stop(unsigned int hart) {
    if (!mutex_trylock(sbi_hart_lock + hart)) {
        return false; //Attempt to aquire the lock for the hart
    }
    if (sbi_hart_data[hart].status != HS_STARTED) {
        return false; //If the hart status isn't starteed, we can't try to stop it
    }
    else {
        //Update the hart's data table, then park the hart (WFI)
        //Make sure to enable proper interrupts.
        sbi_hart_data[hart].status = HS_STOPPED;
        CSR_WRITE("mepc", park);
        CSR_WRITE("mstatus", MSTATUS_MPP_MACHINE | MSTATUS_MPIE);
        CSR_WRITE("mie", MIE_MSIE);
        mutex_unlock(sbi_hart_lock + hart);
        MRET();
    }

    mutex_unlock(sbi_hart_lock + hart);
    return false;
}

//This function has a hart handle a wakeup call from the clint (msip).
//It clears the msip, then attempts to start the hart (similar to how we did in main)
void hart_handle_msip(unsigned int hart) {
    mutex_spinlock(sbi_hart_lock + hart);

    clint_clear_msip(hart);

    if (sbi_hart_data[hart].status == HS_STARTING) {
        CSR_WRITE("mepc", sbi_hart_data[hart].target_address); 
        CSR_WRITE("mstatus", (sbi_hart_data[hart].priv_mode << MSTATUS_MPP_BIT) | MSTATUS_MPIE | MSTATUS_FS_INITIAL);
        CSR_WRITE("mie", MIE_SSIE | MIE_STIE | MIE_MTIE);
        CSR_WRITE("mideleg", SIP_SEIP | SIP_SSIP | SIP_STIP);
        CSR_WRITE("medeleg", MEDELEG_ALL);
        CSR_WRITE("sscratch", hart);
        sbi_hart_data[hart].status = HS_STARTED;
    }

    mutex_unlock(sbi_hart_lock + hart);
    MRET();
}

