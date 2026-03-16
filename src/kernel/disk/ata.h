#pragma once

#include "mbr.h"
#include "../vfs/vfs.h"
#include "../multitasking/mutex.h"
#include <stdint.h>

#include "ata_defs.h"

extern pci_ide_controller_data_t pci_ide_controller[IDE_MAX];
extern uint16_t connected_pci_ide_controllers;

ssize_t ata_iofunc(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction);

void pci_connect_ide_controller(uint8_t bus, uint8_t device, uint8_t function);
void ata_write_command_block_register(pci_ide_controller_data_t* controller, uint8_t channel, uint8_t reg, uint8_t data);
uint8_t ata_read_command_block_register(pci_ide_controller_data_t* controller, uint8_t channel, uint8_t reg);
void ata_write_control_block_register(pci_ide_controller_data_t* controller, uint8_t channel, uint8_t reg, uint8_t data);
uint8_t ata_read_control_block_register(pci_ide_controller_data_t* controller, uint8_t channel, uint8_t reg);

bool ata_poll(pci_ide_controller_data_t* controller, uint8_t channel);
bool ata_pio_read_sectors(pci_ide_controller_data_t* controller, uint8_t channel, uint8_t drive, uint64_t lba, uint8_t sector_count, uint16_t* buffer);