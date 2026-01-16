#pragma once

#include "../../libc/include/stdint.h"

static inline uint8_t inb(uint16_t address)
{
	uint8_t byte;
	asm volatile ("in %0, %1"
		: "=a" (byte)
		: "Nd" (address)
		);
	return byte;
}

static inline void outb(uint16_t address, uint8_t byte)
{
	asm volatile ("out %1, %0" ::
	    "a"  (byte),
		"Nd" (address)
		);
}

static inline uint16_t inw(uint16_t address)
{
	uint16_t word;
	asm volatile ("in %0, %1"
		: "=a" (word)
		: "Nd" (address)
		);
	return word;
}

static inline void outw(uint16_t address, uint16_t word)
{
	asm volatile ("out %1, %0" ::
	    "a"  (word),
		"Nd" (address)
		);
}

static inline uint32_t ind(uint16_t address)
{
	uint32_t dWord;
	asm volatile ("in %0, %1"
		: "=a" (dWord)
		: "Nd" (address)
		);
	return dWord;
}

static inline void outd(uint16_t address, uint32_t dWord)
{
	asm volatile ("out %1, %0" ::
	    "a"  (dWord),
		"Nd" (address)
		);
}

static inline void io_wait()
{
	outb(0x80, 0);
}