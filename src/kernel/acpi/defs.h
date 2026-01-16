#pragma once



// struct rsdp_table
// {
//     char signature[8];
//     uint8_t checksum;
//     char oem_id[6];
//     uint8_t revision;
//     uint32_t rsdt_address;  // Deprecated since ACPI 2.0+

//     uint32_t length;
//     uint64_t xsdt_address;
//     uint8_t extended_checksum;
//     uint8_t reserved[3];
// } __attribute__((packed));

struct sdt_header
{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_tableid[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct rsdt_table
{
    struct sdt_header header;

    // uint32_t ptrs_to_sdt[(header.length - sizeof(header)) / 4];
} __attribute__((packed));

struct xsdt_table
{
    struct sdt_header header;

    // uint64_t ptrs_to_sdt[(header.length - sizeof(header)) / 8];
} __attribute__((packed));

struct generic_address_structure
{
    uint8_t address_space;  // 0: System memory; 1: System I/O; 2: PCI configuration space; 5: System CMOS; 6: PCI Device BAR Target
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;    // 0: Undefined (legacy reasons); 1: Byte access; 2: 16-bit (word) access; 3: 32-bit (dword) access; 4: 64-bit (qword) access 
    uint64_t address;
} __attribute__((packed));

struct fadt_table
{
    struct   sdt_header header;

    uint32_t firmware_control;
    uint32_t dsdt_address;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    uint8_t  reserved;

    uint8_t  preferred_power_management_profile;    // 0: Unspecified; 1: Desktop; 2: Mobile; 3: Workstation; 4: Enterprise Server; 5: SOHO Server; 6: Aplliance PC; 7: Performance Server
    uint16_t sci_interrupt;
    uint32_t smi_cmd_port;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;

    uint8_t  S4BIOS_REQ;
    uint8_t  PSTATE_control;
    uint32_t PM1a_event_block;
    uint32_t PM1b_event_block;
    uint32_t PM1a_control_block;
    uint32_t PM1b_control_block;
    uint32_t PM2_control_block;
    uint32_t PM_timer_block;
    uint32_t GPE0_block;
    uint32_t GPE1_block;
    uint8_t  PM1_event_length;
    uint8_t  PM1_control_length;
    uint8_t  PM2_control_length;
    uint8_t  PM_timer_length;
    uint8_t  GPE0_length;
    uint8_t  GPE1_length;
    uint8_t  GPE1_base;
    uint8_t  c_state_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alarm;
    uint8_t  month_alarm;
    uint8_t  century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    uint16_t boot_architecture_flags;

    uint8_t  reserved2;
    uint32_t flags;

    // 12 byte structure; see below for details
    struct generic_address_structure reset_reg;

    uint8_t  reset_value;
    uint8_t  reserved3[3];
  
    // 64bit pointers - Available on ACPI 2.0+
    uint64_t                x_firmware_control;
    uint64_t                x_dsdt;

    struct generic_address_structure x_PM1a_event_block;
    struct generic_address_structure x_PM1b_event_block;
    struct generic_address_structure x_PM1a_control_block;
    struct generic_address_structure x_PM1b_control_block;
    struct generic_address_structure x_PM2_control_block;
    struct generic_address_structure x_PM_timer_block;
    struct generic_address_structure x_GPE0_block;
    struct generic_address_structure x_GPE1_block;
} __attribute__((packed));

struct madt_table
{
    struct sdt_header header;

    uint32_t lapic_address;
    uint32_t flags;
} __attribute__((packed));

struct madt_entry_header
{
    uint8_t entry_type;
    uint8_t record_length;
} __attribute__((packed));

struct madt_ioapic_interrupt_source_override_entry
{
    struct madt_entry_header header;

    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed));

struct madt_ioapic_entry
{
    struct madt_entry_header header;

    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_address;
    uint32_t gsi_base;
} __attribute__((packed));