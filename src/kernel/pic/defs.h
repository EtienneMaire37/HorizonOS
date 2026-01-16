#pragma once

#include "../../libc/include/stdint.h"

#define apic_reg_offset(reg, next_reg) ((next_reg) - (reg) - 4)
#define apic_reg_next_offset()         (0x10 - 4)

struct local_apic_isr_register
{
    uint32_t value;
    uint8_t reserved[apic_reg_next_offset()];
} __attribute__((packed));

struct local_apic_tmr_register
{
    uint32_t value;
    uint8_t reserved[apic_reg_next_offset()];
} __attribute__((packed));

struct local_apic_irr_register
{
    uint32_t value;
    uint8_t reserved[apic_reg_next_offset()];
} __attribute__((packed));

struct local_apic_icr_register
{
    uint32_t value;
    uint8_t reserved[apic_reg_next_offset()];
} __attribute__((packed));

typedef struct __attribute__((packed)) local_apic_registers 
{
    uint8_t reserved0[0x20];
    uint32_t id_register;
    uint8_t reserved1[apic_reg_next_offset()];
    uint32_t version_register;
    uint8_t reserved2[apic_reg_offset(0x30, 0x80)];
    uint32_t task_priority_register;
    uint8_t reserved3[apic_reg_next_offset()];
    uint32_t arbitration_priority_register;
    uint8_t reserved4[apic_reg_next_offset()];
    uint32_t processor_priority_register;
    uint8_t reserved5[apic_reg_next_offset()];
    uint32_t end_of_interrupt_register;
    uint8_t reserved6[apic_reg_next_offset()];
    uint32_t remote_read_register;
    uint8_t reserved7[apic_reg_next_offset()];
    uint32_t logical_destination_register;
    uint8_t reserved8[apic_reg_next_offset()];
    uint32_t destination_format_register;
    uint8_t reserved9[apic_reg_next_offset()];
    uint32_t spurious_interrupt_vector_register;
    uint8_t reserved10[apic_reg_next_offset()];
    struct local_apic_isr_register in_service_registers[8];
    struct local_apic_tmr_register trigger_mode_registers[8];
    struct local_apic_irr_register interrupt_request_registers[8];
    uint32_t error_status_register;
    uint8_t reserved11[apic_reg_offset(0x280, 0x2f0)];
    uint32_t lvt_corrected_machine_check_interrupt_register;
    uint8_t reserved12[apic_reg_next_offset()];
    struct local_apic_icr_register interrupt_command_registers[2];
    uint32_t lvt_timer_register;
    uint8_t reserved13[apic_reg_next_offset()];
    uint32_t lvt_thermal_sensor_register;
    uint8_t reserved14[apic_reg_next_offset()];
    uint32_t lvt_performance_monitoring_counters_register;
    uint8_t reserved15[apic_reg_next_offset()];
    uint32_t lvt_lint0_register;
    uint8_t reserved16[apic_reg_next_offset()];
    uint32_t lvt_lint1_register;
    uint8_t reserved17[apic_reg_next_offset()];
    uint32_t lvt_error_register;
    uint8_t reserved18[apic_reg_next_offset()];
    uint32_t initial_count_register;
    uint8_t reserved19[apic_reg_next_offset()];
    uint32_t current_count_register;
    uint8_t reserved20[apic_reg_offset(0x390, 0x3e0)];
    uint32_t divide_configuration_register;
} local_apic_registers_t;

typedef struct __attribute__((packed)) io_apic_registers 
{
    uint32_t IOREGSEL;
    uint8_t reserved[apic_reg_next_offset()];
    uint32_t IOWIN;
} io_apic_registers_t;

#define IOAPICID          0x00
#define IOAPICVER         0x01
#define IOAPICARB         0x02
#define IOAPICREDTBL(n)   (0x10 + 2 * n)

#define APIC_DELIVERY_FIXED (0b000ULL << 8)

#define APIC_DESTINATION_PHYSICAL   (0ULL << 11)

#define APIC_POLARITY_ACTIVE_HIGH   (0ULL << 13)

#define APIC_TRIGGER_EDGE           (0ULL << 15)

#define APIC_MASK_ENABLED           (0ULL << 16)

#define APIC_TIMER_INT  0x80
#define APIC_PS2_1_INT  0x81
#define APIC_PS2_2_INT  0x82