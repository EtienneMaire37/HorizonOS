#include "pic.h"
#include "../io/io.h"

void pic_send_eoi(uint8_t irq)
{
	if(irq > 0b111) // Slave PIC
    {
        outb(PIC2_CMD, PIC_EOI);
        io_wait();
    }
 
    // Master PIC
	outb(PIC1_CMD, PIC_EOI);
    io_wait();
}

void pic_disable() 
{
    pic_set_mask(0xffff);
}

void pic_enable() 
{
    pic_set_mask(0x0000);
}

void pic_remap(uint8_t master_offset, uint8_t slave_offset)
{
    // Init sequence
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4 | ICW1_CASCADE);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4 | ICW1_CASCADE);
    io_wait();
    
    outb(PIC1_DATA, master_offset); // ~ Master ICW2
    io_wait();
    outb(PIC2_DATA, slave_offset);  // ~ Slave ICW2
    io_wait();

    // ^ Use the IRQ 2 (Cascade) for the PIC (never actually raised)
    outb(PIC1_DATA, ICW3_MASTER_SLAVE_IQR(2));
    io_wait();
    outb(PIC2_DATA, ICW3_SLAVE_MASTER_IRQ(2));
    io_wait();

    outb(PIC1_DATA, ICW4_8086 | ICW4_NORMAL);
    io_wait();
    outb(PIC2_DATA, ICW4_8086 | ICW4_NORMAL);
    io_wait();

    pic_enable();
}

void pic_set_mask(uint16_t mask) 
{
    outb(PIC1_DATA, mask & 0xff);      
	io_wait();
	outb(PIC2_DATA, mask >> 8);      
	io_wait();
}
 
uint16_t pic_get_register(int32_t OCW3)
{
    outb(PIC1_CMD, OCW3);
    outb(PIC2_CMD, OCW3);
    return (inb(PIC2_CMD) << 8) | inb(PIC1_CMD);
}
 
uint16_t pic_get_irr()
{
    return pic_get_register(PIC_READ_IRR);
}
 
uint16_t pic_get_isr()
{
    return pic_get_register(PIC_READ_ISR);
}