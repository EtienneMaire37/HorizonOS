#include "../../libc/include/stdint.h"
#include "ata_defs.h"

pci_ide_controller_data_t pci_ide_controller[IDE_MAX];
uint16_t connected_pci_ide_controllers = 0;

#include "ata.h"
#include "../pci/pci.h"
#include "../vga/textio.h"
#include "../../libc/include/stdlib.h"
#include "../time/time.h"

void pci_connect_ide_controller(uint8_t bus, uint8_t device, uint8_t function)
{
    if (connected_pci_ide_controllers >= IDE_MAX)
        return;

    pci_ide_controller[connected_pci_ide_controllers].bus = bus;
    pci_ide_controller[connected_pci_ide_controllers].device = device;
    pci_ide_controller[connected_pci_ide_controllers].function = function;

    pci_ide_controller[connected_pci_ide_controllers].prog_if = (pci_configuration_address_space_read_byte(bus, device, function, 8) >> 8) & 0xff;

    pci_ide_controller[connected_pci_ide_controllers].bar0 = pci_configuration_address_space_read_dword(bus, device, function, 16);
    pci_ide_controller[connected_pci_ide_controllers].bar1 = pci_configuration_address_space_read_dword(bus, device, function, 20);
    pci_ide_controller[connected_pci_ide_controllers].bar2 = pci_configuration_address_space_read_dword(bus, device, function, 24);
    pci_ide_controller[connected_pci_ide_controllers].bar3 = pci_configuration_address_space_read_dword(bus, device, function, 28); 
    pci_ide_controller[connected_pci_ide_controllers].bar4 = pci_configuration_address_space_read_dword(bus, device, function, 32);

    pci_ide_controller[connected_pci_ide_controllers].channels[0].compatibility_mode = (pci_ide_controller[connected_pci_ide_controllers].prog_if & 0b1) == 0;
    pci_ide_controller[connected_pci_ide_controllers].channels[1].compatibility_mode = (pci_ide_controller[connected_pci_ide_controllers].prog_if & 0b100) == 0;
    
    if (pci_ide_controller[connected_pci_ide_controllers].channels[0].compatibility_mode)
    {
        pci_ide_controller[connected_pci_ide_controllers].channels[0].base_address = 0x1F0;
        pci_ide_controller[connected_pci_ide_controllers].channels[0].ctrl_base_address = 0x3F6;
        pci_ide_controller[connected_pci_ide_controllers].channels[0].irq = 14;
    }
    else
    {
        pci_ide_controller[connected_pci_ide_controllers].channels[0].base_address = pci_ide_controller[connected_pci_ide_controllers].bar0 & 0xfffffffc;
        pci_ide_controller[connected_pci_ide_controllers].channels[0].ctrl_base_address = pci_ide_controller[connected_pci_ide_controllers].bar1 & 0xfffffffc;
        pci_ide_controller[connected_pci_ide_controllers].channels[0].irq = pci_configuration_address_space_read_dword(bus, device, function, 0x3C) & 0xff;
    }

    if (pci_ide_controller[connected_pci_ide_controllers].channels[1].compatibility_mode)
    {
        pci_ide_controller[connected_pci_ide_controllers].channels[1].base_address = 0x170;
        pci_ide_controller[connected_pci_ide_controllers].channels[1].ctrl_base_address = 0x376;
        pci_ide_controller[connected_pci_ide_controllers].channels[1].irq = 15;
    }
    else
    {
        pci_ide_controller[connected_pci_ide_controllers].channels[1].base_address = pci_ide_controller[connected_pci_ide_controllers].bar2 & 0xfffffffc;
        pci_ide_controller[connected_pci_ide_controllers].channels[1].ctrl_base_address = pci_ide_controller[connected_pci_ide_controllers].bar3 & 0xfffffffc;
        pci_ide_controller[connected_pci_ide_controllers].channels[1].irq = pci_configuration_address_space_read_dword(bus, device, function, 0x3C) & 0xff;
    }

    // * Disable IRQs
    ata_write_control_block_register(&pci_ide_controller[connected_pci_ide_controllers], ATA_PRIMARY_CHANNEL, ATA_REG_CONTROL, 0b10); // ! Default value is 0
    ata_write_control_block_register(&pci_ide_controller[connected_pci_ide_controllers], ATA_SECONDARY_CHANNEL, ATA_REG_CONTROL, 0b10);

    for (uint8_t i = 0; i < 2; i++)
    {
        for (uint8_t j = 0; j < 2; j++) 
        {
            pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].connected = false;

            // * Select Drive
            ata_write_command_block_register(&pci_ide_controller[connected_pci_ide_controllers], i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); 
            ksleep(1);

            ata_write_command_block_register(&pci_ide_controller[connected_pci_ide_controllers], i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            ksleep(1); 

            if (ata_read_command_block_register(&pci_ide_controller[connected_pci_ide_controllers], i, ATA_REG_STATUS) == 0) 
                continue;

            uint8_t status;
            while (true) 
            {
                status = ata_read_command_block_register(&pci_ide_controller[connected_pci_ide_controllers], i, ATA_REG_STATUS);
                if (status & ATA_SR_ERR)
                    break;
                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) 
                    break;
            }

            if (status & ATA_SR_ERR)
            {
                // * Don't support ATAPI drives for now
                continue;
            }

            uint8_t ide_buf[512];
            for (uint16_t k = 0; k < 256; k++)
            {
                ((uint16_t*)ide_buf)[k] = inw(pci_ide_controller[connected_pci_ide_controllers].channels[i].base_address + ATA_REG_DATA);
                // LOG(TRACE, "IDE IDENTIFY data word %u : 0x%x", k, ((uint16_t*)ide_buf)[k]);
            }

            pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].connected = 1;
            pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].signature = *((uint16_t*)(ide_buf + ATA_IDENT_DEVICETYPE));
            pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].capabilities = *((uint16_t*)(ide_buf + ATA_IDENT_CAPABILITIES));
            pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].command_sets = *((uint32_t*)(ide_buf + ATA_IDENT_COMMANDSETS));

            if (pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].command_sets & (1 << 26))    
                // * 48-Bit LBA
                pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].size = (*((uint64_t*)(ide_buf + ATA_IDENT_MAX_LBA_EXT))) & 0xffffffffffff;
            else                 
                // * 28-Bit LBA or CHS
                pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].size = (*((uint32_t*)(ide_buf + ATA_IDENT_MAX_LBA))) & 0x0fffffff;

            uint8_t last_char_index = 0;
            for(uint8_t k = 0; k < 40; k += 2) 
            {
                pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
                if (ide_buf[ATA_IDENT_MODEL + k] != ' ')
                    last_char_index = k;
                pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
                if (ide_buf[ATA_IDENT_MODEL + k + 1] != ' ')
                    last_char_index = k + 1;
            }
            if (last_char_index <= 40)
                pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].model[last_char_index] = 0;
            else
                pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].model[40] = 0;

            pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].lock = MUTEX_INIT;
        }
    }

    for (uint8_t i = 0; i < 2; i++)
    {
        for (uint8_t j = 0; j < 2; j++)
        {
            if (pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].connected) 
            {
                uint64_t bytes = (uint64_t)pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].size * 512;
                uint8_t magnitude = 0;
                uint64_t magnitude_value = bytes;
                const char* magnitude_text[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
                while (magnitude_value >= 1024 * 1024)
                {
                    magnitude_value /= 1024;
                    magnitude++;
                }
                if (magnitude_value >= 1024)
                    magnitude++;
                if (magnitude == 0)
                    magnitude_value *= 1024;
                    
                LOG(INFO, "Found drive \"%s\" (%llu bytes) [%llu.%llu%llu %s]", 
                    pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].model, 
                   bytes, magnitude_value / 1024, (magnitude_value * 10 / 1024) % 10, (magnitude_value * 100 / 1024) % 10, magnitude_text[magnitude]);
                printf("Found drive ");
                tty_set_color(FG_LIGHTGREEN, BG_BLACK);
                printf("\"%s\" ", pci_ide_controller[connected_pci_ide_controllers].channels[i].devices[j].model);
                tty_set_color(FG_WHITE, BG_BLACK);
                printf("(%llu bytes) [", bytes);
                tty_set_color(FG_LIGHTCYAN, BG_BLACK);
                printf("%llu.%llu%llu ", magnitude_value / 1024, (magnitude_value * 10 / 1024) % 10, (magnitude_value * 100 / 1024) % 10);
                tty_set_color(FG_WHITE, BG_BLACK);
                printf("%s]\n", magnitude_text[magnitude]);
            }
        }
    }

    connected_pci_ide_controllers++;

// #define LOG_BOOT_SECTORS
    for (uint8_t i = 0; i < 2; i++)
    {
        for (uint8_t j = 0; j < 2; j++)
        {
            if (pci_ide_controller[connected_pci_ide_controllers - 1].channels[i].devices[j].connected) 
            {
                uint16_t buffer[256] = {0};
                if (ata_pio_read_sectors(&pci_ide_controller[connected_pci_ide_controllers - 1], i, j, 0, 1, buffer))
                {
                    memcpy(pci_ide_controller[connected_pci_ide_controllers - 1].channels[i].devices[j].boot_sector, buffer, 512);
                #ifdef LOG_BOOT_SECTORS
                    LOG(TRACE, "First sector of primary master drive:");
                    for (int i = 0; i < 512; i++)
                    {
                        LOG(TRACE, "0x%x: 0x%x", i, ((uint8_t*)buffer)[i]);
                    }
                #endif
                }
            }
        }
    }

    // * Create block special files
    if (IDE_MAX >= 10) abort();
    for (uint8_t i = 0; i < 2; i++)
    {
        for (uint8_t j = 0; j < 2; j++)
        {
            if (pci_ide_controller[connected_pci_ide_controllers - 1].channels[i].devices[j].connected) 
            {
                const int bufsiz = (3 + 1) + 1 + 1 + (1);
                char file_name[bufsiz];

                snprintf(file_name, bufsiz, "ata%u_%u", connected_pci_ide_controllers - 1, i * 2 + j);

                vfs_file_tnode_t* tnode = vfs_add_special("/devices", file_name, BLK_MODE, ata_iofunc, 0, 0);
                if (!tnode)
                    continue;
                tnode->inode->file_data.ide.ide_idx = connected_pci_ide_controllers - 1;
                tnode->inode->file_data.ide.ata_idx = j + 2 * i;
                tnode->inode->st.st_size = 512 * pci_ide_controller[connected_pci_ide_controllers - 1].channels[i].devices[j].size;
            }
        }
    }

    LOG(INFO, "Partitions on connected drives:");
    for (uint8_t i = 0; i < 2; i++)
    {
        for (uint8_t j = 0; j < 2; j++)
        {
            if (pci_ide_controller[connected_pci_ide_controllers - 1].channels[i].devices[j].connected) 
            {
                LOG(INFO, "Drive %u:%u:", i, j);
                printf("Drive %u:%u:\n", i, j);

                mbr_boot_sector_t* data = (mbr_boot_sector_t*)&pci_ide_controller[connected_pci_ide_controllers - 1].channels[i].devices[j].boot_sector;
                for (uint8_t k = 0; k < 4; k++)
                {
                    if (data->partition_table[k].partition_type != 0)
                    {
                        LOG(INFO, "    Partition %u: Type 0x%x | Start LBA : %u | Size : %lluB%s", 
                            k + 1, 
                            data->partition_table[k].partition_type, data->partition_table[k].start_lba, data->partition_table[k].size_in_sectors * 512ULL,
                            data->partition_table[k].drive_attributes & 0x80 ? " [Bootable]" : "");
                        printf("  Partition %u: Type 0x%x | Start LBA : %u | Size : ", 
                            k + 1, 
                            data->partition_table[k].partition_type, data->partition_table[k].start_lba);
                        tty_set_color(FG_LIGHTCYAN, BG_BLACK);
                        printf("%llu", data->partition_table[k].size_in_sectors * 512ULL);
                        tty_set_color(FG_WHITE, BG_BLACK);
                        printf("B");
                        if (data->partition_table[k].drive_attributes & 0x80)
                        {
                            tty_set_color(FG_YELLOW, BG_BLACK);
                            printf(" [Bootable]");
                        }
                        tty_set_color(FG_WHITE, BG_BLACK);
                        printf("\n");
                    }
                }
            }
        }
    }

    LOG(DEBUG, "Connected PCI IDE controller at %u:%u:%u", bus, device, function);

    LOG(DEBUG, "    %s mode on primary channel", pci_ide_controller[connected_pci_ide_controllers - 1].channels[0].compatibility_mode ? "Compatibility" : "Native");
    LOG(DEBUG, "    %s mode on secondary channel", pci_ide_controller[connected_pci_ide_controllers - 1].channels[1].compatibility_mode ? "Compatibility" : "Native");

    LOG(DEBUG, "    Primary channel base address : 0x%x | Control base address : 0x%x | IRQ : %u",
        pci_ide_controller[connected_pci_ide_controllers - 1].channels[0].base_address,
        pci_ide_controller[connected_pci_ide_controllers - 1].channels[0].ctrl_base_address,
        pci_ide_controller[connected_pci_ide_controllers - 1].channels[0].irq);
    LOG(DEBUG, "    Secondary channel base address : 0x%x | Control base address : 0x%x | IRQ : %u",
        pci_ide_controller[connected_pci_ide_controllers - 1].channels[1].base_address,
        pci_ide_controller[connected_pci_ide_controllers - 1].channels[1].ctrl_base_address,
        pci_ide_controller[connected_pci_ide_controllers - 1].channels[1].irq);
}

void ata_write_command_block_register(pci_ide_controller_data_t* controller, uint8_t channel, uint8_t reg, uint8_t data)
{
    outb(controller->channels[channel].base_address + reg, data);
}
uint8_t ata_read_command_block_register(pci_ide_controller_data_t* controller, uint8_t channel, uint8_t reg)
{
    return inb(controller->channels[channel].base_address + reg);
}

void ata_write_control_block_register(pci_ide_controller_data_t* controller, uint8_t channel, uint8_t reg, uint8_t data)
{
    outb(controller->channels[channel].ctrl_base_address + reg, data);
}
uint8_t ata_read_control_block_register(pci_ide_controller_data_t* controller, uint8_t channel, uint8_t reg)
{
    return inb(controller->channels[channel].ctrl_base_address + reg);
}

bool ata_poll(pci_ide_controller_data_t* controller, uint8_t channel)
{
    uint8_t status;
    do
    {
        status = ata_read_command_block_register(controller, channel, ATA_REG_STATUS);

    // * "Read the Regular Status port until bit 7 (BSY, value = 0x80) clears, and bit 3 (DRQ, value = 8) sets -- 
    // * or until bit 0 (ERR, value = 1) or bit 5 (DF, value = 0x20) sets. 
    // * If neither error bit is set, the device is ready right then."
    } while (!(!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) && !((status & ATA_SR_ERR) || (status & ATA_SR_DF)));
    return !(status & ATA_SR_ERR);
}

bool ata_pio_read_sectors(pci_ide_controller_data_t* controller, uint8_t channel, uint8_t drive, uint64_t lba, uint8_t sector_count, uint16_t* buffer)
{
    if (!buffer)
    {
        LOG(ERROR, "ata_pio_read_sectors: NULL buffer");
        return false;
    }

    if (sector_count == 0)
    {
        LOG(ERROR, "ata_pio_read_sectors: sector_count is 0");
        return false;
    }

    if (channel > 1 || drive > 1)
    {
        LOG(ERROR, "ata_pio_read_sectors: Invalid channel or drive");
        return false;
    }

    if (!controller->channels[channel].devices[drive].connected)
    {
        LOG(ERROR, "ata_pio_read_sectors: Drive not connected");
        return false;
    }

    if (lba + sector_count > controller->channels[channel].devices[drive].size)
    {
        LOG(ERROR, "ata_pio_read_sectors: Invalid read (LBA out of range)");
        if (lba >= controller->channels[channel].devices[drive].size)
            return false;
        sector_count = controller->channels[channel].devices[drive].size - lba;
    }

    acquire_mutex(&controller->channels[channel].devices[drive].lock);

    ata_write_command_block_register(controller, channel, ATA_REG_HDDEVSEL, 0xe0 | (drive << 4) | ((lba >> 24) & 0x0f));
    // ata_write_command_block_register(controller, channel, ATA_REG_FEATURES, 0);

    ata_write_command_block_register(controller, channel, ATA_REG_SECCOUNT, sector_count);
    ata_write_command_block_register(controller, channel, ATA_REG_SECNUM, (uint8_t)(lba & 0xff));
    ata_write_command_block_register(controller, channel, ATA_REG_CYLLOW, (uint8_t)((lba >> 8) & 0xff));
    ata_write_command_block_register(controller, channel, ATA_REG_CYLHIGH, (uint8_t)((lba >> 16) & 0xff));

    ata_write_command_block_register(controller, channel, ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    for (uint8_t i = 0; i < sector_count; i++)
    {
        if (!ata_poll(controller, channel))
        {
            release_mutex(&controller->channels[channel].devices[drive].lock);
            return false;
        }

        for (uint16_t j = 0; j < 256; j++)
            buffer[i * 256 + j] = inw(controller->channels[channel].base_address + ATA_REG_DATA);
    }

    release_mutex(&controller->channels[channel].devices[drive].lock);

    return true;
}

bool ata_pio_read_bytes(pci_ide_controller_data_t* controller, uint8_t channel, uint8_t drive, uint64_t address, uint64_t byte_count, uint8_t* buffer)
{
    if (!buffer)
    {
        LOG(ERROR, "ata_pio_read_sectors: NULL buffer");
        return false;
    }

    uint64_t lba = address / 512;
    uint64_t sector_count = (address + byte_count - 512 * lba + 511) / 512;

    if (sector_count == 0 || byte_count == 0)
        return true;

    if (channel > 1 || drive > 1)
    {
        LOG(ERROR, "ata_pio_read_sectors: Invalid channel or drive");
        return false;
    }

    if (!controller->channels[channel].devices[drive].connected)
    {
        LOG(ERROR, "ata_pio_read_sectors: Drive not connected");
        return false;
    }

    if (lba + sector_count > controller->channels[channel].devices[drive].size)
    {
        LOG(ERROR, "ata_pio_read_sectors: Invalid read (LBA out of range)");
        if (lba >= controller->channels[channel].devices[drive].size)
            return false;
        sector_count = controller->channels[channel].devices[drive].size - lba;
    }

    acquire_mutex(&controller->channels[channel].devices[drive].lock);

    ata_write_command_block_register(controller, channel, ATA_REG_HDDEVSEL, 0xe0 | (drive << 4) | ((lba >> 24) & 0x0f));
    // ata_write_command_block_register(controller, channel, ATA_REG_FEATURES, 0);

    ata_write_command_block_register(controller, channel, ATA_REG_SECCOUNT, sector_count);
    ata_write_command_block_register(controller, channel, ATA_REG_SECNUM, (uint8_t)(lba & 0xff));
    ata_write_command_block_register(controller, channel, ATA_REG_CYLLOW, (uint8_t)((lba >> 8) & 0xff));
    ata_write_command_block_register(controller, channel, ATA_REG_CYLHIGH, (uint8_t)((lba >> 16) & 0xff));

    ata_write_command_block_register(controller, channel, ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    uint64_t buf_idx = 0;

    for (uint8_t i = 0; i < sector_count; i++)
    {
        if (!ata_poll(controller, channel))
        {
            release_mutex(&controller->channels[channel].devices[drive].lock);
            return false;
        }

        for (uint16_t j = 0; j < 256; j++)
        {
            uint16_t word = inw(controller->channels[channel].base_address + ATA_REG_DATA);
            uint64_t addr1 = 512 * lba + i * 512 + 2 * j;
            uint64_t addr2 = addr1 + 1;
            if (addr1 >= address && addr1 < address + byte_count)
                buffer[buf_idx++] = word & 0xff;
            if (addr2 >= address && addr2 < address + byte_count)
                buffer[buf_idx++] = word >> 8;
        }
    }

    release_mutex(&controller->channels[channel].devices[drive].lock);

    return true;
}

ssize_t ata_iofunc(file_entry_t* entry, uint8_t* buf, size_t count, uint8_t direction)
{
    if (direction == CHR_DIR_READ)
    {
        ide_descriptor_t ide = entry->tnode.file->inode->file_data.ide;
        if (!ata_pio_read_bytes(&pci_ide_controller[ide.ide_idx], ide.ata_idx / 2, ide.ata_idx % 2, entry->position, count, buf))
            return 0;
        return count;
    }

    return 0;
}