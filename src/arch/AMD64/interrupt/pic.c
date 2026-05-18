#include <arch/AMD64/cpu/ports.h>
#include <arch/AMD64/interrupt/pic.h>

#define PIC_COMMAND_MASTER  0x20
#define PIC_DATA_MASTER     0x21
#define PIC_COMMAND_SLAVE   0xA0
#define PIC_DATA_SLAVE      0xA1

#define PIC_EOI             0x20

#define PIC_ICW4_PRESENT    1 //Initialization command word 4 will be present
#define PIC_ICW1_INIT       0x10 //Initialization sequence

#define PIC_8086_MODE 1

void X86_PIC_remap(u8 master_offset, u8 slave_offset) {
    X86_PIC_disable();

    //ICW1
    outportb(PIC_COMMAND_MASTER, PIC_ICW1_INIT | PIC_ICW4_PRESENT);
    port_io_wait();
    outportb(PIC_COMMAND_SLAVE, PIC_ICW1_INIT | PIC_ICW4_PRESENT);
    port_io_wait();

    //ICW2
    outportb(PIC_DATA_MASTER, master_offset);
    port_io_wait();
    outportb(PIC_DATA_SLAVE, slave_offset);
    port_io_wait();

    //ICW3
    outportb(PIC_DATA_MASTER, 4); //Indicate Master PIC the IRQ of the Slave PIC. No idea why osdev wiki says its 4 and not 2 or whatever else, and the 8259A PIC docs dont mention anything 
    port_io_wait();
    outportb(PIC_DATA_SLAVE, 2); //Indicate Slave PIC its identity. Same comment as the previous outportb.
    port_io_wait();

    //ICW4
    outportb(PIC_DATA_MASTER, PIC_8086_MODE);
    port_io_wait();
    outportb(PIC_DATA_SLAVE, PIC_8086_MODE);

    X86_PIC_enable();
}

void X86_PIC_enable(void) {
    outportb(PIC_DATA_MASTER, 0); //Unask all interrupts
    outportb(PIC_DATA_SLAVE, 0); //Unask all interrupts
}

void X86_PIC_disable(void) {
    outportb(PIC_DATA_MASTER, 0xFF); //Mask all interrupts
    outportb(PIC_DATA_SLAVE, 0xFF); //Mask all interrupts
}

void X86_PIC_send_eoi(u8 IRQn) {
    if(IRQn > 7) outportb(PIC_COMMAND_SLAVE, PIC_EOI);
    outportb(PIC_COMMAND_MASTER, PIC_EOI);
}

void X86_PIC_mask(u8 IRQn) {
    u16 port = PIC_DATA_MASTER;
    if(IRQn >= 8) {
        port = PIC_DATA_SLAVE;
        IRQn -= 8;
    }

    outportb(port, inportb(port) | (1 << IRQn));
}

void X86_PIC_unmask(u8 IRQn) {
u16 port = PIC_DATA_MASTER;
    if(IRQn >= 8) {
        port = PIC_DATA_SLAVE;
        IRQn -= 8;
    }

    outportb(port, inportb(port) & ~(1 << IRQn));
}