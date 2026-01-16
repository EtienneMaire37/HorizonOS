#pragma once

#include "../multitasking/task.h"
#include "../cpu/memory.h"
#include "../debug/out.h"
#include "../cpu/util.h"
#include "../initrd/initrd.h"
#include "../vga/textio.h"
#include "int.h"
#include "../paging/paging.h"
#include "../../libc/src/math_utils.h"

extern initrd_file_t* commit_file;

#define log_registers() LOG(INFO, "RSP=%#.16llx RBP=%#.16llx RAX=%#.16llx RBX=%#.16llx RCX=%#.16llx RDX=%#.16llx", \
    registers->rsp, registers->rbp, registers->rax, registers->rbx, registers->rcx, registers->rdx);    \
    LOG(INFO, "R8=%#.16llx R9=%#.16llx R10=%#.16llx R11=%#.16llx R12=%#.16llx R13=%#.16llx R14=%#.16llx R15=%#.16llx",  \
    registers->r8, registers->r9, registers->r10, registers->r11, registers->r12, registers->r13, registers->r14, registers->r15);  \
    LOG(INFO, "RDI=%#.16llx RSI=%#.16llx", registers->rdi, registers->rsi);

#define is_a_valid_function(symbol_type) ((symbol_type) == 'T' || (symbol_type) == 'R' || (symbol_type) == 't' || (symbol_type) == 'r')  

void print_kernel_symbol_name(uintptr_t rip, uintptr_t rbp)
{
    initrd_file_t* file = kernel_symbols_file;
    if (file == NULL) return;
    if (file->size == 0 || file->data == NULL) return;
    
    uintptr_t symbol_address = 0, last_symbol_address = 0, current_symbol_address = 0;
    uintptr_t file_offset = 0, line_offset = 0;
    char last_symbol_buffer[64] = {0};
    uint16_t last_symbol_buffer_length = 0;
    char current_symbol_type = ' ', found_symbol_type = ' ';
    while (file_offset < file->size)
    {
        char ch = file->data[file_offset];
        if (ch == '\n')
        {
            if (last_symbol_address <= rip && symbol_address > rip && is_a_valid_function(found_symbol_type))
            {
                putchar(file == kernel_symbols_file ? '[' : '(');
                CONTINUE_LOG(INFO, file == kernel_symbols_file ? "[" : "(");
                
                if (found_symbol_type == 'T' || found_symbol_type == 't')
                    tty_set_color(FG_LIGHTCYAN, BG_BLACK);
                else if (found_symbol_type == 'R' || found_symbol_type == 'r')
                    tty_set_color(FG_LIGHTMAGENTA, BG_BLACK);
                else
                    tty_set_color(FG_LIGHTGRAY, BG_BLACK);

                uint8_t light_tty_color = tty_color;
                bool subfunction = false;

                for (uint8_t i = 0; i < minint(64, last_symbol_buffer_length); i++)
                {
                    if (last_symbol_buffer[i] == '.')
                    {
                        tty_set_color(FG_LIGHTGRAY, BG_BLACK);
                        subfunction = true;
                    }
                    putchar(last_symbol_buffer[i]);
                    CONTINUE_LOG(INFO, "%c", last_symbol_buffer[i]);
                    if (subfunction)
                    {
                        tty_set_color(light_tty_color & 0x07, light_tty_color & 0x70);
                        subfunction = false;
                    }
                }
                tty_set_color(FG_WHITE, BG_BLACK);
                putchar(file == kernel_symbols_file ? ']' : ')');
                CONTINUE_LOG(INFO, file == kernel_symbols_file ? "]" : ")");
                return;
            }
            else if (is_a_valid_function(current_symbol_type))
            {
                last_symbol_buffer_length = line_offset - 19;
            }
            line_offset = 0;
        }
        else
        {
            if (line_offset < 16)
            {
                uint64_t val = hex_char_to_int(ch);
                current_symbol_address &= ~((uint64_t)0xf << ((15 - line_offset) * 4));
                current_symbol_address |= val << ((15 - line_offset) * 4);
            }
            if (line_offset == 17)
            {
                current_symbol_type = ch;
                if (current_symbol_address != 0)    // && is_a_valid_function(current_symbol_type)
                {
                    last_symbol_address = symbol_address;
                    symbol_address = current_symbol_address;
                }
            }
            if (line_offset >= 19 && line_offset < 64 + 19) // && is_a_valid_function(current_symbol_type)
            {
                if (!(last_symbol_address <= rip && symbol_address > rip))
                {
                    last_symbol_buffer[line_offset - 19] = ch;
                    found_symbol_type = current_symbol_type;
                }
            }
            line_offset++;
        }
        file_offset++;
    }
}

void __attribute__((noreturn)) kernel_panic(interrupt_registers_t* registers)
{
    disable_interrupts();

    LOG(CRITICAL, "Kernel panic");

    tty_set_color(FG_WHITE, BG_BLACK);
    tty_clear_screen(' ');

    tty_cursor_blink = false;

    tty_set_color(FG_LIGHTRED, BG_BLACK);
    printf("Kernel panic\n\n");

    tty_set_color(FG_LIGHTGREEN, BG_BLACK);

    if (multitasking_enabled)
        printf("Task : \"%s\" (pid = %d) | ring = %u\n\n", __CURRENT_TASK.name, __CURRENT_TASK.pid, __CURRENT_TASK.ring);

    tty_set_color(FG_WHITE, BG_BLACK);

    const char* error_message = get_error_message(registers->interrupt_number, registers->error_code);

    printf("Exception number: %llu\n", registers->interrupt_number);
    printf("Error:       ");
    tty_set_color(FG_YELLOW, BG_BLACK);
    puts(error_message);
    tty_set_color(FG_WHITE, BG_BLACK);
    printf("Error code:  %#llx\n\n", registers->error_code);

    if (registers->interrupt_number == 14)  // * Page fault
    {
        uint16_t pml4e =    (registers->cr2 >> 39) & 0x1ff;
        uint16_t pdpte =    (registers->cr2 >> 30) & 0x1ff;
        uint16_t pde =      (registers->cr2 >> 21) & 0x1ff;
        uint16_t pte =      ((registers->cr2 >> 12) & 0x1ff);
        if (registers->cr2)
        {
            printf("cr2:  %#llx (pml4e %llu pdpte %llu pde %llu pte %llu offset %#llx)\n", registers->cr2, pml4e, pdpte, pde, pte, registers->cr2 & 0xfff);
        }
        else
        {
            printf("cr2 : ");
            tty_set_color(FG_LIGHTMAGENTA, BG_BLACK);
            printf("NULL\n");
            tty_set_color(FG_WHITE, BG_BLACK);
        }
        printf("cr3: %#llx\n\n", registers->cr3);

        uint64_t* pml4 = (uint64_t*)(registers->cr3 + PHYS_MAP_BASE);

        uint64_t* pml4_entry = &pml4[pml4e];
        printf("pml4 entry: %#.16llx\n", *pml4_entry);
        LOG(INFO, "pml4 entry: %#.16llx", *pml4_entry);

        if (is_pdpt_entry_present(pml4_entry))
        {
            uint64_t* pdpt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pml4_entry));
            uint64_t* pdpt_entry = &pdpt[pdpte];
            printf("pdpt entry: %#.16llx\n", *pdpt_entry);
            LOG(INFO, "pdpt entry: %#.16llx", *pdpt_entry);

            if (is_pdpt_entry_present(pdpt_entry))
            {
                uint64_t* pd = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pdpt_entry));
                uint64_t* pd_entry = &pd[pde];
                printf("pd entry: %#.16llx\n", *pd_entry);
                LOG(INFO, "pd entry: %#.16llx", *pd_entry);

                if (is_pdpt_entry_present(pd_entry))
                {
                    uint64_t* pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pd_entry));
                    uint64_t* pt_entry = &pt[pte];
                    printf("pt entry: %#.16llx\n", *pt_entry);
                    LOG(INFO, "pt entry: %#.16llx", *pt_entry);
                }
            }
        }
        putchar('\n');
    }

    log_registers();

    printf("RSP=%#.16llx RBP=%#.16llx\n",
    registers->rsp, registers->rbp);
    printf("RAX=%#.16llx RBX=%#.16llx RCX=%#.16llx RDX=%#.16llx\n", registers->rax, registers->rbx, registers->rcx, registers->rdx);
    printf("R8=%#.16llx R9=%#.16llx R10=%#.16llx R11=%#.16llx\n",
    registers->r8, registers->r9, registers->r10, registers->r11);
    printf("R12=%#.16llx R13=%#.16llx R14=%#.16llx R15=%#.16llx\n", registers->r12, registers->r13, registers->r14, registers->r15);
    printf("RDI=%#.16llx RSI=%#.16llx\n\n", registers->rdi, registers->rsi);

    printf("Stack trace : \n");
    LOG(INFO, "Stack trace : ");

    typedef struct __attribute__((packed)) call_frame
    {
        uintptr_t rbp;
        uintptr_t rip;
    } call_frame_t;

    call_frame_t* rbp = (call_frame_t*)registers->rbp;
    // asm volatile ("mov eax, rbp" : "=a"(rbp));

    // ~ Log the last function (the one the exception happened in)
    printf("rip : 0x");
    tty_set_color(FG_YELLOW, BG_BLACK);
    printf("%llx", registers->rip);
    tty_set_color(FG_WHITE, BG_BLACK);
    putchar(' ');

    LOG(INFO, "rip : %#llx ", registers->rip);
    print_kernel_symbol_name(registers->rip, (uintptr_t)rbp);
    putchar('\n');

    // while (rbp != NULL && rbp->rip != 0 && i <= max_stack_frames && is_address_canonical((uintptr_t)rbp) && is_address_canonical((uintptr_t)rbp->rip) && (uintptr_t)rbp != 0 && (uintptr_t)rbp->rip != 0 && 
    // (((!multitasking_enabled || (multitasking_enabled && first_task_switch)) &&
    //         (((uintptr_t)rbp > 0xffffffffffffffff - 1024 * bootboot.numcores) && ((uintptr_t)rbp <= 0xffffffffffffffff))) || 
    //     (((uintptr_t)rbp < TASK_STACK_TOP_ADDRESS) && ((uintptr_t)rbp >= TASK_STACK_BOTTOM_ADDRESS))))

    const int max_stack_frames = 12;

    for (int i = 0; i <= max_stack_frames; i++)
    {
        if (rbp == NULL)
            break;
        // * &rbp->rip == NULL
        if ((uint64_t)rbp == 0xfffffffffffffff8)
            break;
        if (rbp->rip == 0)
            break;
        if (i == max_stack_frames)
        {
            tty_set_color(FG_RED, BG_BLACK);
            printf("...");
            tty_set_color(FG_WHITE, BG_BLACK);
            LOG(INFO, "...");
        }
        else
        {
            printf("rip : %#llx | rbp : %#llx ", rbp->rip, rbp);
            LOG(INFO, "rip : %#llx | rbp : %#llx ", rbp->rip, rbp);
            print_kernel_symbol_name(rbp->rip - 1, (uintptr_t)rbp);
            putchar('\n');
            rbp = (call_frame_t*)rbp->rbp;
        }
    }

    putchar('\n');

    printf("commit hash: ");
    tty_set_color(FG_LIGHTMAGENTA, BG_BLACK);
    puts((const char*)commit_file->data);
    tty_set_color(FG_WHITE, BG_BLACK);

    LOG(INFO, "commit hash: %s", commit_file->data);

    halt();
}