#include "ps2.h"
#include <stdint.h>
#include <stdbool.h>
#include "../time/time.h"
#include "../io/io.h"
#include "../debug/out.h"
#include "keyboard.h"

uint8_t ps2_device_1_type = PS2_DEVICE_UNKNOWN;
uint8_t ps2_device_2_type = PS2_DEVICE_UNKNOWN;

bool ps2_device_1_interrupt = true, ps2_device_2_interrupt = true;

bool ps2_controller_connected;
bool ps2_device_1_connected, ps2_device_2_connected;
uint8_t ps2_data_buffer[PS2_READ_BUFFER_SIZE];
uint8_t ps2_data_bytes_received;

bool ps2_wait_for_output() 
{
    if (!ps2_controller_connected)
        return true;
    uint64_t start = precise_time_to_milliseconds(global_timer);
    while (precise_time_to_milliseconds(global_timer) - start < PS2_WAIT_TIME)
    { 
        uint8_t reg = inb(PS2_STATUS_REGISTER);
        if ((reg & PS2_STATUS_INPUT_FULL) == 0) // * Device has read all data
            return false;
    }
    LOG(WARNING, "PS/2 wait to output timeout");
    return true;
}

bool ps2_wait_for_input_with_timeout(uint64_t timeout)
{
    if (!ps2_controller_connected)
        return true;
    uint64_t start = precise_time_to_milliseconds(global_timer);
    while (precise_time_to_milliseconds(global_timer) - start < timeout)
    { 
        uint8_t reg = inb(PS2_STATUS_REGISTER);
        if (reg & PS2_STATUS_OUTPUT_FULL)   // * Device has data to send
            return false;
    }
    LOG(WARNING, "PS/2 wait for input timeout");
    return true;
}

bool ps2_wait_for_input()
{
    return ps2_wait_for_input_with_timeout(PS2_WAIT_TIME);
}

void ps2_flush_buffer() 
{
    if (!ps2_controller_connected)
        return;
    while (inb(PS2_STATUS_REGISTER) & PS2_STATUS_OUTPUT_FULL)
        inb(PS2_DATA);
}

uint8_t ps2_send_command(uint8_t command) 
{
    if (!ps2_controller_connected)
        return 0xff;

    // LOG(TRACE, "Sending command %#x to PS/2 controller", command);

    uint8_t return_val;
    for (int i = 0; i < PS2_MAX_RESEND; i++)
    {
        if (i > 0)
            LOG(WARNING, "PS/2 controller sent resend signal");

        if (ps2_wait_for_output())
            return 0xff;
        outb(PS2_COMMAND_REGISTER, command);

        if (ps2_wait_for_input()) 
            return 0xff;
        return_val = inb(PS2_DATA);

        if (return_val != PS2_RESEND)
            return return_val;
    }
    return return_val;  // PS2_RESEND
}

void ps2_send_command_no_response(uint8_t command)
{
    if (!ps2_controller_connected)
        return;
    // LOG(TRACE, "Sending command %#x to PS/2 controller", command);
    if (ps2_wait_for_output())
        return;
    outb(PS2_COMMAND_REGISTER, command);
}

uint8_t ps2_send_command_with_data(uint8_t command, uint8_t data) 
{
    if (!ps2_controller_connected)
        return 0xff;

    // LOG(TRACE, "Sending command %#x to PS/2 controller", command);

    uint8_t return_val;
    for (int i = 0; i < PS2_MAX_RESEND; i++)
    {
        if (i > 0)
            LOG(WARNING, "PS/2 controller sent resend signal");

        if (ps2_wait_for_output())
            return 0xff;
        outb(PS2_COMMAND_REGISTER, command);
        if (ps2_wait_for_output())
            return 0xff;
        outb(PS2_COMMAND_REGISTER, data);

        if (ps2_wait_for_input()) 
            return 0xff;
        return_val = inb(PS2_DATA);

        if (return_val != PS2_RESEND)
            return return_val;
    }
    return return_val;  // PS2_RESEND
}

void ps2_send_command_with_data_no_response(uint8_t command, uint8_t data) 
{
    if (!ps2_controller_connected)
        return;

    // LOG(TRACE, "Sending command %#x, %#x to PS/2 controller", command, data);

    if (ps2_wait_for_output()) 
        return;
    outb(PS2_COMMAND_REGISTER, command);
    if (ps2_wait_for_output()) 
        return;
    outb(PS2_DATA, data);
}

bool ps2_send_device_command(uint8_t device, uint8_t command) 
{
    if (!ps2_controller_connected)
        return false;

    // LOG(TRACE, "Sending command %#x to PS/2 device %u", command, device);

    for (int tries = 0; tries < PS2_MAX_RESEND; tries++) 
    {
        if (ps2_wait_for_output()) 
        {
            LOG(DEBUG, "Timeout waiting to send to device %d", device);
            continue;
        }

        if (device == 2) 
            outb(PS2_COMMAND_REGISTER, PS2_WRITE_DEVICE_2);

        if (ps2_wait_for_output()) 
        {
            LOG(DEBUG, "Timeout waiting to send to device %d", device);
            continue;
        }

        outb(PS2_DATA, command);

        // ksleep(100 * PRECISE_MILLISECONDS);

        return true;
    }
    return false;
}

bool ps2_send_device_full_command(uint8_t device, uint8_t command, uint8_t expected_bytes)
{
    uint8_t tries = 0;
    do
    {
        tries++;
        if(!ps2_send_device_command(device, command))
            return false;
        if (command == PS2_RESET)
            ps2_wait_for_input_with_timeout(PS2_RESET_TIMEOUT);
        ps2_read_data(expected_bytes);
    } while ((ps2_data_bytes_received == 0 || ps2_data_buffer[0] == PS2_RESEND) && tries < PS2_MAX_RESEND);
    if (tries >= PS2_MAX_RESEND)
        return false;
    return true;
}

bool ps2_send_device_full_command_with_data(uint8_t device, uint8_t command, uint8_t data, uint8_t expected_bytes)
{
    bool sending_command = true;
    uint8_t tries = 0;
    do
    {
        tries++;
        if (sending_command)
        {
            if(!ps2_send_device_command(device, command))
                return false;
            // ps2_read_data(expected_bytes);
            ps2_read_data(1);
        }
        else
        {
            if(!ps2_send_device_command(device, data))
                return false;
            ps2_read_data(expected_bytes);
            return true;
        }
        if (!((ps2_data_bytes_received == 0 || ps2_data_buffer[0] == PS2_RESEND || !sending_command)))
        {
            sending_command = false;
            tries--;
        }
    } while (tries < PS2_MAX_RESEND); //((ps2_data_bytes_received == 0 || ps2_data_buffer[0] == PS2_RESEND || !sending_command) && tries < PS2_MAX_RESEND);
    // if (tries >= PS2_MAX_RESEND)
    //     return false;
    // return true;
    return false;
}

void ps2_read_data(uint8_t expected_bytes) 
{
    ps2_data_bytes_received = 0;
    if (!ps2_controller_connected)
        return;
    for (uint8_t i = 0; !ps2_wait_for_input() && ps2_data_bytes_received < PS2_READ_BUFFER_SIZE; i++)   // && (inb(PS2_STATUS_REGISTER) & PS2_STATUS_OUTPUT_FULL)
    {
        if (i > expected_bytes)
            return;
        ps2_data_buffer[ps2_data_bytes_received++] = inb(PS2_DATA);
    }
}

void ps2_read_additional_data() 
{
    // ps2_data_bytes_received = 0;
    if (!ps2_controller_connected)
        return;
    while (!ps2_wait_for_input() && ps2_data_bytes_received < PS2_READ_BUFFER_SIZE)
        ps2_data_buffer[ps2_data_bytes_received++] = inb(PS2_DATA);
}

void ps2_controller_init() 
{
    LOG(DEBUG, "PS/2 controller initialization sequence");

    ps2_device_1_connected = false;
    ps2_device_2_connected = false;
    enable_ps2_kb_input = false;
    ps2_device_1_interrupt = ps2_device_2_interrupt = false;

    if (!ps2_controller_connected)
    {
        LOG(INFO, "No PS/2 controller connected");
        return;
    }

    LOG(DEBUG, "Disabling device 1");
    ps2_send_command_no_response(PS2_DISABLE_DEVICE_1);
    LOG(DEBUG, "Disabling device 2");
    ps2_send_command_no_response(PS2_DISABLE_DEVICE_2);
    
    ps2_flush_buffer();

    LOG(DEBUG, "Setting up the Controller Configuration Byte");

    uint8_t config = ps2_send_command(PS2_GET_CONFIGURATION);
    config |=  0b00000100;
    config &= ~0b00000011;
    ps2_send_command_with_data_no_response(PS2_SET_CONFIGURATION, config);

    LOG(DEBUG, "Testing the controller");

    uint8_t self_test_code = ps2_send_command(PS2_TEST_CONTROLLER);

    if (self_test_code != PS2_SELF_TEST_OK) 
    {
        LOG(ERROR, "Controller self-test failed (code %#x)", self_test_code);
        printf("Controller self-test failed (code %#x)\n", self_test_code);
        ps2_controller_connected = false;
        return;
    }

    LOG(DEBUG, "Testing the ports");

    bool dual_channel = false;
    ps2_send_command_no_response(PS2_ENABLE_DEVICE_2);
    
        uint8_t new_config = ps2_send_command(PS2_GET_CONFIGURATION);
        dual_channel = !(new_config & 0x20);

    ps2_send_command_no_response(PS2_DISABLE_DEVICE_2);
    ps2_send_command_with_data_no_response(PS2_SET_CONFIGURATION, config);

    // ps2_flush_buffer();

    ps2_device_1_connected = (ps2_send_command(PS2_TEST_DEVICE_1) == PS2_DEVICE_TEST_PASS);
    ps2_device_2_connected = dual_channel && 
                           (ps2_send_command(PS2_TEST_DEVICE_2) == PS2_DEVICE_TEST_PASS);

    LOG(DEBUG, "Resetting the devices");

    if (ps2_device_1_connected) 
    {
        ps2_send_device_full_command(1, PS2_DISABLE_SCANNING, 0);
        ps2_send_command_no_response(PS2_ENABLE_DEVICE_1);
        if (ps2_send_device_full_command(1, PS2_RESET, 2))
        {
            if (ps2_data_bytes_received == 2 && 
                ps2_data_buffer[1] == PS2_DEVICE_BAT_OK) 
            {
                LOG(INFO, "Device 1 basic assurance test passed");
            }
            else
                ps2_device_1_connected = false;
        }
        else
            ps2_device_1_connected = false;
    }

    if (ps2_device_2_connected) 
    {
        ps2_send_device_full_command(2, PS2_DISABLE_SCANNING, 0);
        ps2_send_command_no_response(PS2_ENABLE_DEVICE_2);
        if (ps2_send_device_full_command(2, PS2_RESET, 2))
        {
            if (ps2_data_bytes_received == 2 && 
                ps2_data_buffer[1] == PS2_DEVICE_BAT_OK) 
            {
                LOG(INFO, "Device 2 basic assurance test passed");
            }
            else
                ps2_device_2_connected = false;
        }
        else
            ps2_device_2_connected = false;
    }

    LOG(DEBUG, "Setting up the ccb without irqs");

    config = ps2_send_command(PS2_GET_CONFIGURATION);
    config &= ~0b01000000;   // Disable translation
    // if (ps2_device_1_connected) config &= ~0b00000001; // Disable interrupts
    // if (ps2_device_2_connected) config &= ~0b00000010;
    ps2_send_command_with_data_no_response(PS2_SET_CONFIGURATION, config);

    LOG(INFO, "PS/2 Controller initialized. Devices: %u/%u", 
        ps2_device_1_connected, ps2_device_2_connected);
}

void ps2_detect_keyboards() 
{
    if (!ps2_controller_connected)
        return;

    LOG(INFO, "Detecting PS/2 keyboards");

    if (ps2_device_1_connected) 
        ps2_send_device_full_command(1, PS2_DISABLE_SCANNING, 1);
    if (ps2_device_2_connected)
        ps2_send_device_full_command(2, PS2_DISABLE_SCANNING, 1);
    
    if (ps2_device_1_connected) 
    {
        if (ps2_send_device_full_command(1, PS2_IDENTIFY, 3))
        {
            if (!(ps2_data_bytes_received >= 1 && ps2_data_buffer[0] == PS2_ACK))
                goto invalid_port_1;

            LOG(INFO, "PS/2 port 1 id : %#x %#x (%u bytes)", ps2_data_buffer[1], 
                ps2_data_buffer[2], ps2_data_bytes_received);
            
            if (ps2_data_bytes_received >= 3 && ps2_data_buffer[1] == 0xab) // Any PS/2 keyboard
            {
                ps2_device_1_type = PS2_DEVICE_KEYBOARD;
                LOG(INFO, "Keyboard detected on port 1");
                printf("Keyboard detected on port 1\n");
            }
        }
    }

    goto valid_port_1;
invalid_port_1:
    ps2_device_1_connected = false;
valid_port_1:

    if (ps2_device_2_connected) 
    {
        if (ps2_send_device_full_command(2, PS2_IDENTIFY, 3))
        {
            if (!(ps2_data_bytes_received >= 1 && ps2_data_buffer[0] == PS2_ACK))
                goto invalid_port_2;

            LOG(INFO, "PS/2 port 2 id : %#x %#x (%u bytes)", ps2_data_buffer[1], 
                ps2_data_buffer[2], ps2_data_bytes_received);
            
            if (ps2_data_bytes_received >= 3 && ps2_data_buffer[1] == 0xab) // Any PS/2 keyboard
            {
                ps2_device_2_type = PS2_DEVICE_KEYBOARD;
                LOG(INFO, "Keyboard detected on port 2");
                printf("Keyboard detected on port 2\n");
            }
        }
    }

    goto valid_port_2;
invalid_port_2:
    ps2_device_2_connected = false;
valid_port_2:

    if (ps2_device_1_connected)
        ps2_send_device_full_command(1, PS2_ENABLE_SCANNING, 1);
    if (ps2_device_2_connected)
        ps2_send_device_full_command(2, PS2_ENABLE_SCANNING, 1);

    return;
}

void ps2_enable_interrupts()
{
    LOG(INFO, "Enabling PS/2 IRQs");
    
    ps2_device_1_interrupt = ps2_device_1_connected;
    ps2_device_2_interrupt = ps2_device_2_connected;
    
    ps2_flush_buffer();

    uint8_t config = ps2_send_command(PS2_GET_CONFIGURATION);
    if (ps2_device_1_interrupt) config |= 0b00000001; // Enable interrupts
    if (ps2_device_2_interrupt) config |= 0b00000010;
    ps2_send_command_with_data_no_response(PS2_SET_CONFIGURATION, config);

    ksleep(10 * PRECISE_MILLISECONDS);
    
    ps2_flush_buffer();

    enable_ps2_kb_input = true;
}

void ps2_disable_interrupts()
{
    LOG(INFO, "Disabling PS/2 IRQs");
    
    ps2_device_1_interrupt = false;
    ps2_device_2_interrupt = false;
    
    ps2_flush_buffer();

    uint8_t config = ps2_send_command(PS2_GET_CONFIGURATION);
    if (ps2_device_1_interrupt) config &= ~0b00000001; // Disable interrupts
    if (ps2_device_2_interrupt) config &= ~0b00000010;
    ps2_send_command_with_data_no_response(PS2_SET_CONFIGURATION, config);

    ps2_flush_buffer();

    ksleep(10 * PRECISE_MILLISECONDS);

    enable_ps2_kb_input = false;
}

void handle_irq_1(bool* task_switch) 
{
    if (!ps2_controller_connected)
        return;
    
    if (!(inb(PS2_STATUS_REGISTER) & PS2_STATUS_OUTPUT_FULL))
        return;

    uint8_t data = inb(PS2_DATA);

    // LOG(DEBUG, "(%#x", data);

    // if (!ps2_device_1_interrupt)
    // {
    //     LOG(CRITICAL, "Kernel failed to poll PS/2 return value");
    //     abort();
    // }

    if (!ps2_device_1_connected) 
        return;
    
    if (ps2_device_1_type == PS2_DEVICE_KEYBOARD && enable_ps2_kb_input) 
        ps2_handle_keyboard_scancode(1, data, task_switch);
}

void handle_irq_12(bool* task_switch)
{
    if (!ps2_controller_connected)
        return;

    if (!(inb(PS2_STATUS_REGISTER) & PS2_STATUS_OUTPUT_FULL))
        return;

    uint8_t data = inb(PS2_DATA);

    // LOG(DEBUG, ")%#x", data);

    // if (!ps2_device_2_interrupt)
    // {
    //     LOG(CRITICAL, "Kernel failed to poll PS/2 return value");
    //     abort();
    // }

    if (!ps2_device_2_connected) 
        return;
    
    if (ps2_device_2_type == PS2_DEVICE_KEYBOARD && enable_ps2_kb_input) 
        ps2_handle_keyboard_scancode(2, data, task_switch);
}