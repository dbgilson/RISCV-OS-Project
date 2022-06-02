#include <printf.h>
#include <sbi.h>
#include <csr.h>
#include <strhelp.h>
#include <pci.h>
#include <cpage.h>
#include <mmu.h>
#include <memhelp.h>
#include <symbols.h>
#include <test.h>
#include <virtio.h>
#include <virtioblk.h>
#include <virtio_gpu.h>
#include <virtio_inp.h>
#include <ringbuff.h>
#include <kmalloc.h>
#include <process.h>
#include <elf.h>
#include <minix3.h>
#include <ext4.h>
#include <vfs.h>

void test_hart(void); //Points to location in start.S
void run_command(char command[]);

//These variables are used in bin.S to link to a user elf file (for testing)
extern uint64_t userelf;
extern uint64_t userelfsize;

Process *main_idle_processes[8];

void plic_init(){
	plic_enable(0, 32); //Enable PCI Interrupts (with hart 0)
	plic_enable(0, 33);
	plic_enable(0, 34);
	plic_enable(0, 35);
	plic_set_priority(32, 7); // Set PCI Priority
	plic_set_priority(33, 7);
	plic_set_priority(34, 7);
	plic_set_priority(35, 7);
	plic_set_threshold(0, 0); // Hart 0 hears everything
}

void asm_trap_vector();

static void add_idle_procs(void){
    extern uint64_t idleproc;
    extern uint64_t idleprocsize;
    Process *idle;  // = process_new_os();

    for (int i = 0; i < 8; i++) {
        main_idle_processes[i] = idle = p_new(false); // OS PROCESS

        // We don't need a stack for idle
        idle->image                   = idle->stack;
        idle->image_pg_num            = 1;
        idle->stack_pg_num            = 0;
        idle->pid                     = 0xfffe - i;
        idle->stack                   = NULL;
        memcpy(idle->image, (void *)idleproc, idleprocsize);
		mmu_map(idle->ptable, P_DEFAULT_STACK_POINTER, (uint64_t)idle->image, PB_EXECUTE);
        idle->frame.sepc = P_DEFAULT_STACK_POINTER;
        // The default quantum is 100. The higher, the laggier.
        // The lower, the more CPU intensive.
        idle->quantum    = 50;
        idle->frame.satp = SATP(idle->ptable, idle->pid);
        SFENCE_ASID(idle->pid);
    }
}

//OS is now starting to map its mmu table as well as creating its heap space.
//It is still running the console 
ATTR_NORET
int main(int hart) {
	//Setup the BK bytes for the heap
	cpage_init();
	
	//Allocate a page for the kernel mmu and initialize it
	Page_Table *pt = cpage_znalloc(1);
	kernel_mmu_table = pt;
	mmu_kernel_init(kernel_mmu_table);

	//Initialize the kernel virtual heap address
	init_kernel_heap();
	plic_init();
	pci_init();

	//Getting SSCRATCH Ready for interrupts
	long int (*OS_GPREGS)[8][32] = kzalloc(sizeof(long int[8][32]));
	CSR_WRITE("sscratch", &OS_GPREGS[hart][0]);

	//Testing pci print function
	print_pci_connect();

	char c;
	char command[256];
	int at = 0;

	printf("\n\n\n");

	printf("VFS Parse testing\n");
	printf("/home/dev/last \n");
	char *path = (char *)cpage_znalloc(1);
	path = "/home/dev/last";
	char *first = (char *)cpage_znalloc(1);
	vfs_parse_first(path, first);
	vfs_parse_first(path, first);
	char *last = (char *)cpage_znalloc(1);
	vfs_parse_last(path, last);
	printf("First is: %s, Last is: %s\n", first, last);
	//Test setting up elf loader and processes
	uint64_t sz  = ALIGN_UP_POT(userelfsize, 512);
	char *buffer = (char *)cpage_znalloc(1);

	//Read buffer data that holds elf file
    if (!virtio_disk_rw(buffer, 0, sz, 0)) {
        printf("Error reading block\n");
        sbi_hart_stop();
    }

	//Load elf header data into a process in which we can execute
	bool user_b = true;
	bool os_b = false;
	Process *user = p_new(user_b);
	if (!elf_load(user, (void *)buffer)) {
        printf("Unable to load init!\n");
        p_free(user);
        sbi_hart_stop();
        WFI_LOOP();
    }
	
	cpage_free(buffer);

	gpu_init(gdev);

	
	//while(1){
	//	printf("IPT buffer size is : %d\n", ipt_ringbuf->num_elements);
	//}
	//while(1){
	//	printf("IPT device idx is %d\n", ipt->qdevice->idx);
	//	printf("Keyboard device idx is %d\n", keyboard->qdevice->idx);
	//}

	//printf("Finished with elf test, attempting minix and ext4 tests\n\n");
	//minix_test();
	//ext4_test();
	/*
	//Set the user state to running since it is eligible
	user->state = P_RUNNING;

	//Allocate memory for process trap stacks
	for (int i = 0; i < 8; i++) {
        p_trap_stacks[i] = (uint64_t)cpage_znalloc(1);
    }

	printf("Attempting to spawn idle processes on all other harts\n");

	user->frame.satp = SATP(user->ptable, user->pid);
	SFENCE_ASID(user->pid);
	//p_spawn_on_hart(user, 0);
	scheduling_init(1);
	schedule_add(user);
	add_idle_procs();
    for (int i = 1; i < 8; i++) {
        if (sbi_hart_get_status(i) == HS_STOPPED) {
            if (!p_spawn_on_hart(main_idle_processes[i], i)) {
                printf("[SPAWN]: Could not spawn idle process on hart %d\n", i);
            }
        }
    }
	schedule_hart_spawn(0);
	*/
	printf("~%d~> ",  hart);
	do {
		while ((c = sbi_getchar()) == 0xFF) { WFI(); }
		if (c == '\r' || c == '\n') {
			command[at] = '\0';
			printf("\n");
			run_command(command); //Attempts to run command after an "enter" or newline character
			printf("~%d~> ", hart);
			at = 0;
		}
		else if (c == '\b' || c == 127) {
			// Backspace or "delete"
			if (at > 0) {
				// Erase character 
				printf("\b \b");
				at--;
			}
		}
		else {
			if (at < 255) {
				command[at++] = c;
				// Echo it back out to the user
				sbi_putchar(c);
			}
		}
	} while (1);
	return 0;
}

//This funcction initializes the kernel page table
void mmu_kernel_init(Page_Table *pt){
	mmu_map_multiple(pt, sym_start(text), sym_start(text), (sym_end(text)-sym_start(text)), PB_READ | PB_EXECUTE);
	// printf("Mapping virtual address 0x%08lx to 0x%08lx.  MMU map translate returns this table entry: 0x%08lx \n", sym_start(text), sym_start(text), mmu_translate(pt, sym_start(text)));
	mmu_map_multiple(pt, sym_start(data), sym_start(data), (sym_end(data)-sym_start(data)), PB_READ | PB_WRITE);
	mmu_map_multiple(pt, sym_start(rodata), sym_start(rodata), (sym_end(rodata)-sym_start(rodata)), PB_READ);
	mmu_map_multiple(pt, sym_start(bss), sym_start(bss), (sym_end(bss)-sym_start(bss)), PB_READ | PB_WRITE);
	mmu_map_multiple(pt, sym_start(stack), sym_start(stack), (sym_end(stack)-sym_start(stack)), PB_READ | PB_WRITE);
	mmu_map_multiple(pt, sym_start(heap), sym_start(heap), (sym_end(heap)-sym_start(heap)), PB_READ | PB_WRITE);

	// MMIO hardware
	// PLIC
	mmu_map_multiple(pt, 0x0C000000, 0x0C000000, 0x0C2FFFFF - 0x0C000000, PB_READ | PB_WRITE);
	// PCIe MMIO
	mmu_map_multiple(pt, 0x30000000, 0x30000000, 0x10000000, PB_READ | PB_WRITE);
	// PCIe BARs
	mmu_map_multiple(pt, 0x40000000, 0x40000000, 0x10000000, PB_READ | PB_WRITE);

	CSR_WRITE("satp", SATP_MODE_SV39 | SATP_SET_ASID(KERNEL_ASID) | SATP_SET_PPN(pt));
	SFENCE();
}

//This function determines a command that the user puts into the
//console once they hit enter.  It then attempts to run it.
void run_command(char command[]) {
	if (!strcmp(command, "help")) {
		printf("Valid commands: 'stat' , 'start #hart_num#'\n");
	}
	else if (!strcmp(command, "stat")) {
		//Here we just want to print out the status of each Hart in the Hart table.
		printf("HART STATUS\n");
		printf("~~~~~~~~~~~\n");
		for (int i = 0;i < 8;i++) {
			int stat = sbi_hart_get_status(i);  //We use our sbi system call to get the hart status
			if (stat != 0) {
				printf("Hart %d: ", i);
				switch (stat) {
					case 1:
						printf("stopped\n");
					break;
					case 2:
						printf("starting\n");
					break;
					case 3:
						printf("started\n");
					break;
					case 4:
						printf("stopping\n");
					break;
				}
			}
		}
	}
	else if (!strncmp(command, "start", 5)) {  //Here we check for "start #hart_num#"
		int space = strfindchr(command, ' ');
		if (space == -1) {
			printf("Usage: start <hart>\n");
		}
		else {
			int hart = atoi(command + space + 1);
			if (hart < 0 || hart >= 8) {
				printf("Invalid hart '%d' specified.\n", hart);
			}
			else {
				int stop_check = sbi_hart_get_status(hart);  //Check if the hart is stopped before starting
				if(stop_check != 1){
					printf("Hart %d is not stopped\n", hart);
				}
				else{
					sbi_hart_start(hart, test_hart, 1);
				}
			}
		}
	}
	else if (command[0] != '\0') {
		printf("Unknown command '%s'.\n", command);
	}
}

