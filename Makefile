GCC=/opt/gcc-cross/gcc/xgcc
LD=/opt/binutils-cross/bin/x86_64-elf-ld

export PATH := $(GCC_PATH):$(PATH)

OBJ_DIR=./build_files
SRC_DIR=./src
INC_DIR=./include/

NASMFLAGS=-f elf64 -w all
CFLAGS=-nostdlib -lgcc -mno-sse -ffreestanding -mcmodel=kernel -mno-red-zone -Wall -Wextra -Wno-unused-function
CFLAGS+=-Wfloat-equal -Wundef -Wcast-align -Wwrite-strings -Wlogical-op -Wredundant-decls
CFLAGS+=-Wshadow -Wno-unused-parameter -Wstrict-prototypes -Wno-unused-variable -Werror
CFLAGS+=-I $(INC_DIR)
LDFLAGS=-N --script=src/linker.ld
QEMUFLAGS=-serial file:serialOut.log -cpu Nehalem-v1,+invtsc,+x2apic -net none -boot d -smp 1 -m 32M --cdrom os.iso

ifeq ($(DEBUG),true)
  NASMFLAGS+= -g -F dwarf
  CFLAGS+= -g -O0
  QEMUFLAGS+=-s -daemonize
else
  CFLAGS+= -O2
endif

ASM_SOURCES=$(shell find $(SRC_DIR) -type f -name '*.asm')
C_SOURCES=$(shell find $(SRC_DIR) -type f -name '*.c')
ASM_OBJS=$(patsubst $(SRC_DIR)/%.asm, $(OBJ_DIR)/%.o, $(ASM_SOURCES))
C_OBJS=$(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(C_SOURCES))

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.asm
	mkdir -p $(@D)
	nasm $(NASMFLAGS) $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(@D)
	$(GCC) $(CFLAGS) -c $< -o $@

build: clean .WAIT $(ASM_OBJS) $(C_OBJS)
	$(LD) $(LDFLAGS) $(ASM_OBJS) $(C_OBJS) --output=iso/boot/kernel.bin
	grub-mkrescue -o os.iso iso/
	./objdump.sh

run-qemu-bios:
	qemu-system-x86_64 $(QEMUFLAGS)

run-qemu-uefi:
	qemu-system-x86_64 $(QEMUFLAGS) -enable-kvm --bios UEFI/OVMF.fd

run-bochs:
#add -dbg when ussing bochs debugger
	bochs -f bochsrc.bxrc -q

clean:
	-rm -rf $(OBJ_DIR)
	-rm *.iso
	-rm *.sys

all-qemu-bios: clean build run-qemu-bios

all-qemu-uefi: clean build run-qemu-uefi

all-bochs: clean build run-bochs