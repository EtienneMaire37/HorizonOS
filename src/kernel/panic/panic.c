#include "panic.h"
#include "../vga/constants.h"
#include "../terminal/textio.h"
#include "../util/math.h"
#include "../multitasking/multitasking.h"
#include "../paging/paging.h"
#include "../memalloc/page_data.h"
#include "../git/commit.h"
#include <stdio.h>

void print_kernel_symbol_name(uintptr_t rip, bool ttyout)
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
                if (ttyout)
                    putchar(file == kernel_symbols_file ? '[' : '(');
                CONTINUE_LOG(INFO, file == kernel_symbols_file ? "[" : "(");

                if (ttyout)
                {
                    if (found_symbol_type == 'T' || found_symbol_type == 't')
                        tty_set_color(FG_LIGHTCYAN, BG_BLACK);
                    else if (found_symbol_type == 'R' || found_symbol_type == 'r')
                        tty_set_color(FG_LIGHTMAGENTA, BG_BLACK);
                    else
                        tty_set_color(FG_LIGHTGRAY, BG_BLACK);
                }

                uint8_t light_tty_color = tty_color;
                bool subfunction = false;

                for (uint8_t i = 0; i < minint(64, last_symbol_buffer_length); i++)
                {
                    if (last_symbol_buffer[i] == '.')
                    {
                        if (ttyout)
                            tty_set_color(FG_LIGHTGRAY, BG_BLACK);
                        subfunction = true;
                    }
                    if (ttyout)
                        putchar(last_symbol_buffer[i]);
                    CONTINUE_LOG(INFO, "%c", last_symbol_buffer[i]);
                    if (subfunction)
                    {
                        if (ttyout)
                            tty_set_color(light_tty_color & 0x07, light_tty_color & 0x70);
                        subfunction = false;
                    }
                }
                if (ttyout)
                {
                    tty_set_color(FG_WHITE, BG_BLACK);
                    putchar(file == kernel_symbols_file ? ']' : ')');
                }
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
                current_symbol_address &= ~(0xfULL << ((15 - line_offset) * 4));
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

void print_stack_trace(uint64_t rip, uint64_t rbp_val, bool ttyout)
{
    if (ttyout)
        printf("Stack trace : \n");
    LOG(INFO, "Stack trace : ");

    typedef struct __attribute__((packed)) call_frame
    {
        uintptr_t rbp;
        uintptr_t rip;
    } call_frame_t;

    call_frame_t* rbp = (call_frame_t*)rbp_val;
    // asm volatile ("mov eax, rbp" : "=a"(rbp));

    // ~ Log the last function (the one the exception happened in)
    if (ttyout)
    {
        printf("rip : 0x");
        tty_set_color(FG_YELLOW, BG_BLACK);
        printf("%" PRIx64, rip);
        tty_set_color(FG_WHITE, BG_BLACK);
        putchar(' ');
    }

    LOG(INFO, "rip : %#" PRIx64 " ", rip);
    print_kernel_symbol_name(rip, ttyout);
    if (ttyout)
        putchar('\n');

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
            if (ttyout)
            {
                tty_set_color(FG_RED, BG_BLACK);
                printf("...");
                tty_set_color(FG_WHITE, BG_BLACK);
            }
            LOG(INFO, "...");
        }
        else
        {
            if (ttyout)
                printf("rip : %#" PRIx64 " | rbp : %#" PRIx64 " ", rbp->rip, (uint64_t)rbp);
            LOG(INFO, "rip : %#" PRIx64 " | rbp : %#" PRIx64 " ", rbp->rip, (uint64_t)rbp);
            print_kernel_symbol_name(rbp->rip - 1, ttyout);
            if (ttyout)
                putchar('\n');
            rbp = (call_frame_t*)rbp->rbp;
        }
    }
}

void __attribute__((noreturn)) kernel_panic_ex(interrupt_registers_t* registers, int error)
{
    disable_interrupts();

    LOG(CRITICAL, "Kernel panic");

    tty_set_color(FG_WHITE, BG_BLACK);
    tty_clear_screen(' ');

    tty_set_color(FG_LIGHTRED, BG_BLACK);
    printf("Kernel panic\n\n");

    tty_set_color(FG_LIGHTGREEN, BG_BLACK);

    if (multitasking_enabled)
        printf("Task : \"%s\" (pid = %d) | ring = %u\n\n", current_task->name, current_task->pid, current_task->ring);

    tty_set_color(FG_WHITE, BG_BLACK);

    const char* error_message = registers
        ? get_error_message(registers->interrupt_number, registers->error_code)
        : get_panic_message(error);

    if (registers)
        printf("Exception number: %" PRIu64 "\n", registers->interrupt_number);
    else
        printf("Error number:     %d\n", error);
    printf("Error:            ");
    tty_set_color(FG_YELLOW, BG_BLACK);
    puts(error_message);
    tty_set_color(FG_WHITE, BG_BLACK);
    if (registers)
        printf("Error code:       %#" PRIx64, registers->error_code);

    puts("\n"); // skip 2 lines

    if (registers)
    {
        if (registers->interrupt_number == 14)  // * Page fault
        {
            uint16_t pml4e =    (registers->cr2 >> 39) & 0x1ff;
            uint16_t pdpte =    (registers->cr2 >> 30) & 0x1ff;
            uint16_t pde =      (registers->cr2 >> 21) & 0x1ff;
            uint16_t pte =      ((registers->cr2 >> 12) & 0x1ff);
            if (registers->cr2)
            {
                printf("cr2:  %#" PRIx64 " (pml4e %u pdpte %u pde %u pte %u offset %#" PRIx64 ")\n", registers->cr2, pml4e, pdpte, pde, pte, registers->cr2 & 0xfff);
            }
            else
            {
                printf("cr2 : ");
                tty_set_color(FG_LIGHTMAGENTA, BG_BLACK);
                printf("NULL\n");
                tty_set_color(FG_WHITE, BG_BLACK);
            }
            printf("cr3: %#" PRIx64 "\n\n", registers->cr3);

            uint64_t* pml4 = (uint64_t*)(registers->cr3 + PHYS_MAP_BASE);

            uint64_t* pml4_entry = &pml4[pml4e];
            uint32_t pml4_flags = lock_page_table(pml4);

            printf("pml4 entry: %#016" PRIx64 "\n", *pml4_entry);
            LOG(INFO, "pml4 entry: %#016" PRIx64, *pml4_entry);

            if (is_pdpt_entry_present(pml4_entry))
            {
                uint64_t* pdpt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pml4_entry));
                uint32_t pdpt_flags = lock_page_table(pdpt);

                uint64_t* pdpt_entry = &pdpt[pdpte];
                printf("pdpt entry: %#016" PRIx64 "\n", *pdpt_entry);
                LOG(INFO, "pdpt entry: %#016" PRIx64, *pdpt_entry);

                if (is_pdpt_entry_present(pdpt_entry))
                {
                    uint64_t* pd = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pdpt_entry));
                    uint32_t pd_flags = lock_page_table(pd);

                    uint64_t* pd_entry = &pd[pde];
                    printf("pd entry: %#016" PRIx64 "\n", *pd_entry);
                    LOG(INFO, "pd entry: %#016" PRIx64, *pd_entry);

                    if (is_pdpt_entry_present(pd_entry))
                    {
                        uint64_t* pt = (uint64_t*)(PHYS_MAP_BASE + get_pdpt_entry_address(pd_entry));
                        uint32_t pt_flags = lock_page_table(pt);
                        uint64_t* pt_entry = &pt[pte];
                        printf("pt entry: %#016" PRIx64 "\n", *pt_entry);
                        LOG(INFO, "pt entry: %#016" PRIx64, *pt_entry);
                        unlock_page_table(pt, pt_flags);
                    }
                    unlock_page_table(pd, pd_flags);
                }
                unlock_page_table(pdpt, pdpt_flags);
            }
            unlock_page_table(pml4, pml4_flags);
            putchar('\n');
        }
    }

    if (registers)
    {
        log_registers();
        printf("RSP=%#016" PRIx64 " RBP=%#016" PRIx64 "\n",
        registers->rsp, registers->rbp);
        printf("RAX=%#016" PRIx64 " RBX=%#016" PRIx64 " RCX=%#016" PRIx64 " RDX=%#016" PRIx64 "\n", registers->rax, registers->rbx, registers->rcx, registers->rdx);
        printf("R8=%#016" PRIx64 " R9=%#016" PRIx64 " R10=%#016" PRIx64 " R11=%#016" PRIx64 "\n",
        registers->r8, registers->r9, registers->r10, registers->r11);
        printf("R12=%#016" PRIx64 " R13=%#016" PRIx64 " R14=%#016" PRIx64 " R15=%#016" PRIx64 "\n", registers->r12, registers->r13, registers->r14, registers->r15);
        printf("RDI=%#016" PRIx64 " RSI=%#016" PRIx64 "\n\n", registers->rdi, registers->rsi);

        print_stack_trace(registers->rip, registers->rbp, true);
    }
    else
        print_stack_trace((uint64_t)kernel_panic_ex, get_rbp(), true);

    putchar('\n');

    if (commit_file)
    {
        printf("commit hash: ");
        tty_set_color(FG_LIGHTMAGENTA, BG_BLACK);
        puts((const char*)commit_file->data);
        tty_set_color(FG_WHITE, BG_BLACK);

        LOG(INFO, "commit hash: %s", commit_file->data);
    }

    fflush(stdout);

    halt();
}
