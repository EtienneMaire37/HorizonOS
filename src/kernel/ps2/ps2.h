#pragma once

#define PS2_DATA                0x60
#define PS2_STATUS_REGISTER     0x64
#define PS2_COMMAND_REGISTER    0x64

#define PS2_WAIT_TIME           20      // ms
#define PS2_RESET_TIMEOUT       2500    // ms

#define PS2_MAX_RESEND          5
#define PS2_READ_BUFFER_SIZE    8     // 5 should work though

// Status Register Flags
#define PS2_STATUS_OUTPUT_FULL  0x01  // Bit 0: Output buffer full (data available)
#define PS2_STATUS_INPUT_FULL   0x02  // Bit 1: Input buffer full  (controller busy)

// Controller Commands
#define PS2_GET_CONFIGURATION   0x20
#define PS2_SET_CONFIGURATION   0x60
#define PS2_DISABLE_DEVICE_2    0xa7
#define PS2_ENABLE_DEVICE_2     0xa8
#define PS2_TEST_DEVICE_2       0xa9
#define PS2_TEST_CONTROLLER     0xaa
#define PS2_TEST_DEVICE_1       0xab
#define PS2_DISABLE_DEVICE_1    0xad
#define PS2_ENABLE_DEVICE_1     0xae
#define PS2_WRITE_DEVICE_2      0xd4

#define PS2_DEVICE_TEST_PASS    0x00
#define PS2_SELF_TEST_OK        0x55
#define PS2_DEVICE_BAT_OK       0xaa

// Device Commands
#define PS2_IDENTIFY            0xf2
#define PS2_ENABLE_SCANNING     0xf4
#define PS2_DISABLE_SCANNING    0xf5
#define PS2_ACK                 0xfa
#define PS2_RESEND              0xfe
#define PS2_RESET               0xff

// Device Types
#define PS2_DEVICE_UNKNOWN      0
#define PS2_DEVICE_KEYBOARD     1
#define PS2_DEVICE_MOUSE        2

#define ps2_print_read_buffer() // { printf("PS/2 read buffer : "); for (uint8_t i = 0; i < ps2_data_bytes_received; i++) printf("%#llx ", ps2_data_buffer[i]); putchar('\n'); }

uint8_t ps2_device_1_type = PS2_DEVICE_UNKNOWN;
uint8_t ps2_device_2_type = PS2_DEVICE_UNKNOWN;

bool ps2_device_1_interrupt = true, ps2_device_2_interrupt = true;

bool ps2_controller_connected;
bool ps2_device_1_connected, ps2_device_2_connected;
uint8_t ps2_data_buffer[PS2_READ_BUFFER_SIZE];
uint8_t ps2_data_bytes_received;

bool ps2_wait_for_output();
bool ps2_wait_for_input();
void ps2_read_data(uint8_t expected_bytes);
void ps2_read_additional_data();
void ps2_controller_init();
// void ps2_detect_devices();
void ps2_detect_keyboards();
void ps2_enable_interrupts();
uint8_t ps2_send_command(uint8_t command);
uint8_t ps2_send_command_with_data(uint8_t command, uint8_t data);
void ps2_send_command_no_response(uint8_t command);
void ps2_send_command_with_data_no_response(uint8_t command, uint8_t data);
bool ps2_send_device_command(uint8_t device, uint8_t command);
bool ps2_send_device_full_command(uint8_t device, uint8_t command, uint8_t expected_bytes);
bool ps2_send_device_full_command_with_data(uint8_t device, uint8_t command, uint8_t data, uint8_t expected_bytes);
void ps2_flush_buffer();

void handle_irq_1(bool* ts);
void handle_irq_12(bool* ts);