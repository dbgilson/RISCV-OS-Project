CROSS_COMPILE?=riscv64-unknown-linux-gnu-
GDB=$(CROSS_COMPILE)gdb
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
OBJCOPY=$(CROSS_COMPILE)objcopy
CFLAGS=-g -O2 -Wall -Wextra -march=rv64gc -mabi=lp64d -ffreestanding -nostdlib -nostartfiles -Isrc/include -mcmodel=medany
LDFLAGS= -Tlds/sbi.lds $(CFLAGS)
LIBS=
ASM_DIR=asm
SOURCE_DIR=src
OBJ_DIR=objs
DEP_DIR=deps
SOURCES=$(wildcard $(SOURCE_DIR)/*.c)
SOURCES_ASM=$(wildcard $(ASM_DIR)/*.S)
OBJECTS=$(patsubst $(SOURCE_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
OBJECTS+= $(patsubst $(ASM_DIR)/%.S,$(OBJ_DIR)/%.o,$(SOURCES_ASM))
DEPS=$(patsubst $(SOURCE_DIR)/%.c,$(DEP_DIR)/%.d,$(SOURCES))
KERNEL=sbi.elf
all: $(KERNEL)
include $(wildcard $(DEP_DIR)/*.d)
$(KERNEL): $(OBJECTS) lds/sbi.lds
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
$(OBJ_DIR)/%.o: $(SOURCE_DIR)/%.c Makefile
	$(CC) -MD -MF ./deps/$*.d $(CFLAGS) -o $@ -c $<
$(OBJ_DIR)/%.o: $(ASM_DIR)/%.S Makefile
	$(CC) $(CFLAGS) -o $@ -c $<
clean:
	rm -f $(OBJ_DIR)/*.o $(DEP_DIR)/*.d $(KERNEL)
