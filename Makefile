GCC_PATH=../cross/gcc
GCC=$(GCC_PATH)/xgcc
LD=../cross/ld/ld-new

export PATH := $(GCC_PATH):$(PATH)

OBJ_DIR=./build_files
SRC_DIR=./src

NASMFLAGS=-f elf64 -w all
CFLAGS=-mno-sse -O2 -ffreestanding -mcmodel=kernel -mno-red-zone -Wall -Wextra -Wno-unused-function 
CFLAGS+=-Wfloat-equal -Wundef -Wcast-align -Wwrite-strings -Wlogical-op -Wredundant-decls
CFLAGS+=-Wshadow -Wno-unused-parameter -Wstrict-prototypes -Wno-unused-variable -Werror
CFLAGS+=-I $(SRC_DIR)
LDFLAGS=-N --script=src/linker.ld

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

run-bios:
	qemu-system-x86_64 -serial file:serialOut.log -net none -boot d -smp 4 -m 32M --cdrom os.iso

run-uefi:
	qemu-system-x86_64 -serial file:serialOut.log -net none -boot d -smp 4 -m 128M --cdrom os.iso --bios bios/OVMF.fd

run-bochs:
	 bochs -f bochsrc.bxrc

clean:
	-rm -rf $(OBJ_DIR)
	-rm *.iso
	-rm *.sys

all-bios: clean build run-bios

all-uefi: clean build run-uefi
