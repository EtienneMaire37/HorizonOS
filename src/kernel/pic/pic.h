#pragma once

#include <stdint.h>

#define PIC1		    0x20
#define PIC2		    0xA0
#define PIC1_CMD	    (PIC1 + 0)
#define PIC1_DATA	    (PIC1 + 1)
#define PIC2_CMD    	(PIC2 + 0)
#define PIC2_DATA	    (PIC2 + 1)

#define PIC_EOI		    0x20

#define PIC_READ_IRR                0x0a
#define PIC_READ_ISR                0x0b

// ^ Initialization command words
#define ICW1_ICW4	            0x01        // Use ICW4
#define ICW1_SINGLE	            0x02		// Single mode
#define ICW1_CASCADE	        0x00		// Cascade mode
#define ICW1_INTERVAL4	        0x04
#define ICW1_INTERVAL8	        0x00
#define ICW1_LEVEL_TRIGGERED	0x08
#define ICW1_EDGE_TRIGGERED	    0x00
#define ICW1_INIT	            0x10		// Init command

#define ICW3_MASTER_SLAVE_IQR(n)    (1 << n)
#define ICW3_SLAVE_MASTER_IRQ(n)    (n & 0b111)

#define ICW4_8086	            0x01		// 8086/88 mode
#define ICW4_8080	            0x00		// MCS-80/85 mode
#define ICW4_AUTO	            0x02		// Auto EOI
#define ICW4_NORMAL	            0x00		// Normal EOI
#define ICW4_BUF_SLAVE	        0x08		// Slave buffered mode
#define ICW4_BUF_MASTER	        0x0C		// Master buffered mode
#define ICW4_SFNM	            0x10		// Special fully nested
 
void pic_send_eoi(uint8_t irq);
void pic_disable();
void pic_remap(uint8_t master_offset, uint8_t slave_offset);
void pic_set_mask(uint16_t mask);
uint16_t pic_get_register(int32_t OCW3);
uint16_t pic_get_irr();
uint16_t pic_get_isr();