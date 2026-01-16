#pragma once

#include <stdbool.h>
#include "../multitasking/mutex.h"

#define IDE_MAX     4

#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRDY    0x40    // Drive ready
#define ATA_SR_DF      0x20    // Drive write fault
#define ATA_SR_DSC     0x10    // Drive seek complete
#define ATA_SR_DRQ     0x08    // Data request ready
#define ATA_SR_CORR    0x04    // Corrected data
#define ATA_SR_IDX     0x02    // Index
#define ATA_SR_ERR     0x01    // Error

#define ATA_ER_BBK      0x80    // Bad block
#define ATA_ER_UNC      0x40    // Uncorrectable data
#define ATA_ER_MC       0x20    // Media changed
#define ATA_ER_IDNF     0x10    // ID mark not found
#define ATA_ER_MCR      0x08    // Media change request
#define ATA_ER_ABRT     0x04    // Command aborted
#define ATA_ER_TK0NF    0x02    // Track 0 not found
#define ATA_ER_AMNF     0x01    // No address mark

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

// * COMMAND BLOCK REGISTERS
#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT   0x02
#define ATA_REG_SECNUM     0x03
#define ATA_REG_CYLLOW     0x04
#define ATA_REG_CYLHIGH    0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07

// * CONTROL BLOCK REGISTERS
#define ATA_REG_CONTROL    0x00
#define ATA_REG_ALTSTATUS  0x00
#define ATA_REG_DEVADDRESS 0x01

#define ATA_PRIMARY_CHANNEL     0
#define ATA_SECONDARY_CHANNEL   1

typedef struct ide_device
{
    bool connected;         // 0: Drive not connected, 1: Drive connected
    uint16_t signature;     // Drive Signature
    uint16_t capabilities;
    uint32_t command_sets;
    uint64_t size;          // Sector count
    char model[41];         // Model string

    mutex_t lock;

    uint8_t boot_sector[512];
} ide_device_t;

typedef struct ide_channel_data
{
    bool compatibility_mode;
    uint8_t irq;
    uint32_t base_address;
    uint32_t ctrl_base_address;
    ide_device_t devices[2];
} ide_channel_data_t;

typedef struct pci_ide_controller_data
{
    uint8_t bus, device, function;
    uint8_t prog_if;
    uint32_t bar0, bar1, bar2, bar3, bar4;
    ide_channel_data_t channels[2];
} pci_ide_controller_data_t;