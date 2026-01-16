#pragma once

#include "../initrd/initrd.h"

#define PCI_CONFIG_ADDRESS      0xCF8
#define PCI_CONFIG_DATA         0xCFC

extern initrd_file_t* pci_ids;

uint32_t pci_configuration_address_space_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_configuration_address_space_read_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_scan_buses();