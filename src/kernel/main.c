#define _GNU_SOURCE
#include "vfs/vnode.h"
#include "vfs/fs/null.h"
#include "vfs/fs/console.h"
#include "memalloc/page_data.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

extern char kernel_start, kernel_end;
void *kernel_start_ptr, *kernel_end_ptr;

char** environ = NULL;

#include "cpu/temp.h"
#include "cpu/util.h"
#include "pic/timer.h"
#include "multitasking/sched_lock.h"

#include "boot/limine.h"

#include <inttypes.h>
#include <limits.h>

#include "time/time.h"

#include "cpu/cpuid.h"
#include "cpu/msr.h"
#include "cpu/registers.h"
#include "graphics/linear_framebuffer.h"
#include "cpu/cpuid.h"
#include "fpu/sse.h"
#include "debug/out.h"
#include "vga/constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include "initrd/initrd.h"
#include "cmos/rtc.h"
#include "pic/apic.h"

#include "multitasking/task.h"
#include "multitasking/loader.h"
#include "terminal/textio.h"
#include "paging/paging.h"
#include "int/int.h"
#include "memalloc/page_frame_allocator.h"
#include "cpu/memory.h"
#include "fpu/fpu.h"
#include "gdt/gdt.h"
#include "int/idt.h"
#include "pic/pic.h"
#include "pic/apic.h"
#include "acpi/tables.h"
#include "ps2/ps2.h"
#include "ps2/keyboard.h"
#include "pci/pci.h"
#include "multitasking/startup_data.h"
#include "cpu/segbase.h"
#include "cpu/tsc.h"
#include "multitasking/signal.h"
#include "git/commit.h"
#include "memalloc/virtual_memory_allocator.h"

void _start()
{
    /* * rsp is set to point to the top of a stack, in bootloader-reclaimable memory, which is at least 64KiB (65536 bytes) in size */

    disable_interrupts();

    PHYS_MAP_BASE = hhdm_request.response->offset;

    cpuid_highest_function_parameter = 0;
    uint32_t eax, ebx, ecx, edx;
    cpuid_no_check(0, cpuid_highest_function_parameter, ebx, ecx, edx);
    // * CPUID is guaranteed to be present on x86_64

    {
        // * "Beginning with the P6 family processors, the presence or absence of an on-chip local APIC can be detected using
        // * the CPUID instruction. When the CPUID instruction is executed with a source operand of 1 in the EAX register, bit 9
        // * of the CPUID feature flags returned in the EDX register indicates the presence (set) or absence (clear) of a local
        // * APIC." -> Intel manual Vol. 3A 11-7
        uint32_t eax, ebx, ecx, edx = 0;
        cpuid(1, eax, ebx, ecx, edx);
        ASSERT(edx & (1ULL << 9));
    }

    if (!is_bsp()) // * SMP not supported for now
        halt();

    LOG(DEBUG, "_start");

    kernel_start_ptr = (void*)&kernel_start;
    kernel_end_ptr = (void*)&kernel_end;

    ASSERT(framebuffer_request.response && framebuffer_request.response->framebuffer_count >= 1);

    ASSERT(framebuffer_request.response->framebuffers[0]->memory_model == LIMINE_FRAMEBUFFER_RGB);
    ASSERT(framebuffer_request.response->framebuffers[0]->bpp % 8 == 0);

    framebuffer.width = framebuffer_request.response->framebuffers[0]->width;
    framebuffer.height = framebuffer_request.response->framebuffers[0]->height;
    framebuffer.stride = framebuffer_request.response->framebuffers[0]->pitch;
    framebuffer.address = framebuffer_request.response->framebuffers[0]->address;
    framebuffer.format = framebuffer_request.response->framebuffers[0]->memory_model;
    framebuffer.bytes_per_pixel = framebuffer_request.response->framebuffers[0]->bpp / 8;
    framebuffer.red_shift = framebuffer_request.response->framebuffers[0]->red_mask_shift;
    framebuffer.green_shift = framebuffer_request.response->framebuffers[0]->green_mask_shift;
    framebuffer.blue_shift = framebuffer_request.response->framebuffers[0]->blue_mask_shift;

    LOG(INFO, "Kernel booted successfully with limine (%p-%p)", kernel_start_ptr, kernel_end_ptr);
    LOG(INFO, "Kernel is %" PRIu64 " bytes long", (uint64_t)kernel_end_ptr - (uint64_t)kernel_start_ptr);
    LOG(INFO, "Framebuffer : (%u, %u) (scanline %u bytes) at %p", framebuffer.width, framebuffer.height, framebuffer.stride, framebuffer.address);

    LOG(INFO, "CPUID highest function parameter: %#x", cpuid_highest_function_parameter);

    *(uint32_t*)&manufacturer_id_string[0] = ebx;
    *(uint32_t*)&manufacturer_id_string[4] = edx;
    *(uint32_t*)&manufacturer_id_string[8] = ecx;
    manufacturer_id_string[12] = 0;

    LOG(INFO, "CPU manufacturer id : \"%s\"", manufacturer_id_string);

    cpuid_highest_extended_function_parameter = 0;
    cpuid_no_check(0x80000000, cpuid_highest_extended_function_parameter, ebx, ecx, edx);
    if (cpuid_highest_extended_function_parameter <= 0x80000000)
        cpuid_highest_extended_function_parameter = 0;
    else
        LOG(INFO, "CPUID highest extended function parameter: %#x", cpuid_highest_extended_function_parameter);

    if (strcmp(manufacturer_id_string, "GenuineIntel") == 0)
        cpu_brand = CPU_INTEL;
    else if (strcmp(manufacturer_id_string, "AuthenticAMD") == 0)
        cpu_brand = CPU_AMD;
    else
        cpu_brand = CPU_UNKNOWN;

    if (cpu_brand == CPU_AMD)
    {
        _Alignas(sizeof(uint32_t)) char easter_egg_str[17] = { 0 };

        uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
        cpuid_no_check(0x8ffffffe, eax, ebx, ecx, edx);

        *(uint32_t*)&easter_egg_str[0] = eax;
        *(uint32_t*)&easter_egg_str[4] = ebx;
        *(uint32_t*)&easter_egg_str[8] = edx;
        *(uint32_t*)&easter_egg_str[12] = ecx;
        easter_egg_str[16] = 0;

        if (strcmp(easter_egg_str, "") != 0)
            LOG(INFO, "AMD Easter egg string: \"%s\"", easter_egg_str);

        eax = ebx = ecx = edx = 0;
        cpuid_no_check(0x8fffffff, eax, ebx, ecx, edx);

        *(uint32_t*)&easter_egg_str[0] = eax;
        *(uint32_t*)&easter_egg_str[4] = ebx;
        *(uint32_t*)&easter_egg_str[8] = edx;
        *(uint32_t*)&easter_egg_str[12] = ecx;
        easter_egg_str[16] = 0;

        if (strcmp(easter_egg_str, "") != 0)
            LOG(INFO, "AMD Easter egg string: \"%s\"", easter_egg_str);

    }

    if (cpuid_highest_extended_function_parameter >= 0x80000008)
    {
        uint32_t eax = 0, ebx, ecx, edx;
        cpuid(0x80000008, eax, ebx, ecx, edx);
        physical_address_width = eax & 0xff;
    }
    else
    {
    // * The MAXPHYADDR is 36 bits for processors that do not
    // * support CPUID leaf 80000008H, or indicated by
    // * CPUID.80000008H:EAX[bits 7:0] for processors that support CPUID leaf 80000008H.
    // * -> Intel manual Vol. 3A 11-8
        physical_address_width = 36;
    }

    LOG(INFO, "Physical address is %u bits long", physical_address_width);

    init_pat();

    struct limine_file* initrd = NULL;
    for (uint64_t i = 0; i < module_request.response->module_count; i++)
    {
        struct limine_file* file = module_request.response->modules[i];
        if (strcmp(initrd_module.path, file->path) == 0)
        {
            initrd = file;
            break;
        }
    }
    ASSERT(initrd);

    initrd_parse((uint64_t)initrd->address, initrd->size);
    kernel_symbols_file = initrd_find_file("/boot/symbols.txt");

    tty_font = psf_font_load_from_initrd("/boot/ka8x16thin-1.psf");
    tty_font_bold = psf_font_load_from_initrd("/boot/tcvn8x16.psf");

    if (!tty_font.f || !tty_font_bold.f)
    {
        LOG(DEBUG, "Couldn't find psf font in initrd");
        abort();
    }

    pfa_detect_usable_memory();
    page_data_table_init();

    tty_init();

// * vvv Now we can use stdout

    ASSERT(LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision));

    commit_file = initrd_find_file("/boot/commit.txt");

    LOG(INFO, "commit hash: %s", commit_file->data);
    printf("commit hash: ");
    tty_set_color(FG_LIGHTMAGENTA, BG_BLACK);
    puts((const char*)commit_file->data);
    tty_set_color(FG_WHITE, BG_BLACK);

    printf("Physical address is ");
    tty_set_color(FG_LIGHTBLUE, BG_BLACK);
    printf("%u ", physical_address_width);
    tty_set_color(FG_WHITE, BG_BLACK);
    printf("bits long\n");

    printf("Detected ");
    tty_set_color(FG_LIGHTBLUE, BG_BLACK);
    printf("%" PRIu64 " ", allocatable_memory);
    tty_set_color(FG_WHITE, BG_BLACK);
    printf("bytes of allocatable memory\n");

    if (pat_enabled)
    {
        LOG(INFO, "PAT successfully enabled");
        printf("PAT successfully enabled\n");
    }
    else
    {
        LOG(WARNING, "PAT not supported");
        printf("warning: PAT not supported (this might cause poor performance on graphical intensive programs)\n");
    }

    LOG(INFO, "Setting up paging...");
    printf("Setting up paging...\n");

    LOG(DEBUG, "PHYS_MAP_BASE: %#" PRIx64, PHYS_MAP_BASE);

    {
        global_cr3 = create_empty_pdpt();
        ASSERT(global_cr3);
        LOG(DEBUG, "global_cr3: %p", global_cr3);

    // * That way each pml4 entry in the higher half is a reference to a pdpt and modifying any kernel mapping in the hh will take effect on all vas
        for (int i = 256; i < 512; i++)
            set_pdpt_entry(&global_cr3[i], create_empty_pdpt_phys(), PG_USER, PG_READ_WRITE, CACHE_WB);

        uint64_t* boot_cr3 = (uint64_t*)(get_cr3_address() + PHYS_MAP_BASE);

        LOG(DEBUG, "boot_cr3: %#" PRIx64, (uint64_t)boot_cr3);

        printf("Copying mapping of range %p-%p from limine\n", kernel_start_ptr, kernel_end_ptr);
        LOG(DEBUG, "Copying mapping of range %p-%p from limine", kernel_start_ptr, kernel_end_ptr);

        copy_mapping(boot_cr3, global_cr3, (uintptr_t)kernel_start_ptr, (uint64_t)((uintptr_t)kernel_end_ptr - (uintptr_t)kernel_start_ptr) >> 12);

        for (uint64_t i = 0; i < mmap_request.response->entry_count; i++)
        {
            struct limine_memmap_entry* entry = mmap_request.response->entries[i];

            if (entry->type == LIMINE_MEMMAP_RESERVED || entry->type == LIMINE_MEMMAP_BAD_MEMORY)
                continue;
            if (entry->base & 0xfff)
                continue;
            if (entry->length & 0xfff)
                continue;

            uint64_t ptr = entry->base;
            uint64_t len = entry->length;

            if (ptr >= MAX_MEMORY)
                continue;
            if (ptr >= (1ULL << physical_address_width))
                continue;
            if (len > (1ULL << physical_address_width) - ptr)
                len = (1ULL << physical_address_width) - ptr;
            if (len > MAX_MEMORY - ptr)
                len = MAX_MEMORY - ptr;

            if (len == 0)
                continue;

            // ? Write-combining cache for the framebuffer
            // ? Write-back for usable memory
            // ? Uncacheable for MMIO and SMBIOS
            int cache = (entry->type == LIMINE_MEMMAP_USABLE ||
                entry->type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES ||
                entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
                entry->type == LIMINE_MEMMAP_ACPI_RECLAIMABLE) ? CACHE_WB
             : (entry->type == LIMINE_MEMMAP_FRAMEBUFFER ? CACHE_WC
             : CACHE_UC);

            LOG(DEBUG, "Mapping range %#" PRIx64 "-%#" PRIx64 " to %#" PRIx64 "-%#" PRIx64, ptr, ptr + len, ptr + PHYS_MAP_BASE, ptr + len + PHYS_MAP_BASE);
            printf("Mapping range %#" PRIx64 "-%#" PRIx64 " to %#" PRIx64 "-%#" PRIx64 "\n", ptr, ptr + len, ptr + PHYS_MAP_BASE, ptr + len + PHYS_MAP_BASE);
            remap_range(global_cr3, ptr + PHYS_MAP_BASE, ptr, len >> 12, PG_SUPERVISOR, PG_READ_WRITE, cache);
        }

        // * signal handler wrapper function
        LOG(DEBUG, "Setting %#" PRIx64 "-%#" PRIx64 " as user accessible", (uint64_t)sighandler, (uint64_t)sighandler + 0x1000);
        printf("Setting %#" PRIx64 "-%#" PRIx64 " as user accessible\n", (uint64_t)sighandler, (uint64_t)sighandler + 0x1000);
        uint64_t addr_off = (uint64_t)virtual_to_physical(global_cr3, (uintptr_t)sighandler);
        ASSERT(addr_off);
        uint64_t paddr = addr_off - PHYS_MAP_BASE;
        unmap_range(global_cr3, (uintptr_t)sighandler, 1);
        remap_range(global_cr3, (uintptr_t)sighandler, paddr, 1, PG_USER, PG_READ_ONLY, CACHE_WB);
    }

    load_cr3((uint64_t)global_cr3 - PHYS_MAP_BASE);
    vmm_initialized = true;

    printf("Paging setup done\n");
    LOG(INFO, "Set up paging");

    LOG(INFO, "Detecting FPU");

    if (cpuid_highest_function_parameter >= 1)
    {
        uint32_t eax = 0, ebx, ecx = 0, edx;

        cpuid(1, eax, ebx, ecx, edx);
        fxsave_supported = (edx & (1 << 24)) != 0;
        if (!fxsave_supported)
            xsave_supported = false;
        else
            xsave_supported = (ecx & (1 << 26)) != 0;
    }
    else
        xsave_supported = false;

    if (fxsave_supported)
    {
        if (xsave_supported)
        {
            if (cpuid_highest_function_parameter >= 0x0d)
            {
                uint32_t eax = 0, ebx, ecx = 0, edx;
                cpuid_with_ecx(0x0d, 1, eax, ebx, ecx, edx);

                xsave_instruction =
                    // ((eax & (1 << 3)) ? XSAVES :
                    ((eax & (1 << 0)) ? XSAVEOPT :
                    ((eax & (1 << 1)) ? XSAVEC : XSAVE));
            }
            else
                xsave_instruction = XSAVE;
        }
        else
        {
            LOG(WARNING, "XSAVE isn't supported: Defaulting to using FXSAVE");
            tty_set_color(FG_LIGHTRED, BG_BLACK);
            printf("XSAVE isn't supported: Defaulting to using FXSAVE\n");
            tty_set_color(FG_WHITE, BG_BLACK);

            fpu_state_component_bitmap = 0;
            xsave_instruction = FXSAVE;
        }
    }
    else
    {
        LOG(WARNING, "FXSAVE isn't supported: Defaulting to using FSAVE");
        tty_set_color(FG_LIGHTRED, BG_BLACK);
        printf("FXSAVE isn't supported: Defaulting to using FSAVE\n");
        tty_set_color(FG_WHITE, BG_BLACK);

        fpu_state_component_bitmap = 0;
        xsave_instruction = FSAVE;
    }

    LOG(INFO, "Enabling FPU");
    enable_fpu();
    LOG(DEBUG, "Setting up FPU support");
    fpu_init_defaults();

    LOG(INFO, "Using the %s FPU family of instructions", fpu_get_save_instruction_name(xsave_instruction));
    printf("Using the %s FPU family of instructions\n", fpu_get_save_instruction_name(xsave_instruction));

    if (cpuid_highest_function_parameter >= 7)
    {
        uint32_t eax, ebx = 0, ecx, edx;
        cpuid_with_ecx(7, 0, eax, ebx, ecx, edx);
        if (ebx & 1)    // * fsgsbase
        {
            uint64_t cr4 = get_cr4();
            cr4 |= (1ULL << 16);   // * FSGSBASE
            load_cr4(cr4);
            fsgsbase = true;

            LOG(DEBUG, "FSGSBASE is set");
        }
        else
            LOG(DEBUG, "FSGSBASE is not set");
    }
    else
        LOG(DEBUG, "FSGSBASE is not set");

    lapic_init();

    if (lapic)
    {
        printf("Using xAPIC; LAPIC base: %p\n", lapic);
        LOG(INFO, "Using xAPIC; LAPIC base: %p", lapic);
    }
    else
    {
        printf("Using x2APIC\n");
        LOG(INFO, "Using x2APIC");
    }

    LOG(INFO, "cpu_id : %u", lapic_get_cpu_id());

    printf("1/%" PRIu64 " core%s running\n", mp_request.response->cpu_count, mp_request.response->cpu_count == 1 ? "" : "s");
    LOG(INFO, "1/%" PRIu64 " core%s running", mp_request.response->cpu_count, mp_request.response->cpu_count == 1 ? "" : "s");

    printf("CPU manufacturer id : ");
    tty_set_color(FG_LIGHTRED, BG_BLACK);
    printf("\"%s\"\n", manufacturer_id_string);
    tty_set_color(FG_WHITE, BG_BLACK);

    printf("CPUID highest function parameter: %#x\n", cpuid_highest_function_parameter);
    printf("CPUID highest extended function parameter: %#x\n", cpuid_highest_extended_function_parameter);

    LOG(INFO, "Loading a GDT with TSS...");
    printf("Loading a GDT with TSS...");
    fflush(stdout);

    setup_gdt_tss();

    printf(" | Done\n");
    LOG(INFO, "GDT and TSS loaded");

    printf("Loading an IDT...");
    fflush(stdout);
    LOG(INFO, "Loading an IDT...");
    install_idt();
    printf(" | Done\n");
    LOG(INFO, "IDT loaded");

    LOG(INFO, "Disabling the PIC");
    printf("Disabling the PIC...");
    fflush(stdout);
    pic_disable();
    printf(" | Done\n");
    LOG(INFO, "PIC disabled");

    LOG(INFO, "Enabling the APIC");
    printf("Enabling the APIC...");
    fflush(stdout);

    lapic_set_spurious_interrupt_number(0xff);
    lapic_enable();
    lapic_set_tpr(0);

    printf(" | Done\n");
    LOG(INFO, "APIC enabled");

    LOG(INFO, "Setting up the APIC timer and calibrating the TSC");
    printf("Setting up the APIC timer and calibrating the TSC");
    fflush(stdout);

    apic_timer_and_tsc_init();
    last_tsc = rdtsc();
    rtc_get_time();
    time_initialized = true;

    {
        uint32_t eax, ebx, ecx, edx = 0;
        cpuid_with_ecx(7, 0, eax, ebx, ecx, edx);
        tpause_supported = !!(ecx & (1ULL << 5));

        if (tpause_supported)
            LOG(INFO, "TPAUSE supported");
        else
            LOG(WARNING, "TPAUSE not supported");
    }

    LOG(INFO, "Set up the APIC timer and TSC");
    printf(" | Done\n");

    ASSERT(!(get_rflags() & (1 << 9)));

    LOG(INFO, "TSC clock running at approximatively %" PRIu64 " hz", tsc_cycles_per_second);
    printf("TSC clock running at approximatively %" PRIu64 " hz\n", tsc_cycles_per_second);

    printf("Unix time: ");

    tty_set_color(FG_LIGHTCYAN, BG_BLACK);
    printf("%" PRIu64 "\n", (uint64_t)current_time.tv_sec);
    tty_set_color(FG_WHITE, BG_BLACK);

    LOG(DEBUG, "Setting up FS/GS segment bases");

    sc_data.kernel_rsp = TASK_KERNEL_STACK_TOP_ADDRESS;
    wrmsr(IA32_KERNEL_GS_BASE_MSR, (uint64_t)&sc_data);

    wrmsr(IA32_EFER_MSR, rdmsr(IA32_EFER_MSR) | 1); // * enable syscalls
    // * In Long Mode, userland CS will be loaded from STAR 63:48 + 16 and userland SS from STAR 63:48 + 8 on SYSRET.
    wrmsr(IA32_STAR_MSR, ((uint64_t)(KERNEL_DATA_SEGMENT | 3) << 48) | ((uint64_t)KERNEL_CODE_SEGMENT << 32));
    wrmsr(IA32_LSTAR_MSR, (uint64_t)syscall_handler);
    wrmsr(IA32_FMASK_MSR, (1 << 9)); // * disable interrupts

    wrgsbase(rdmsr(IA32_KERNEL_GS_BASE_MSR));

    LOG(DEBUG, "Done setting up FS/GS segment bases");

    enable_interrupts();

    LOG(INFO, "Parsing ACPI tables..");
    printf("Parsing ACPI tables...\n");
    acpi_find_tables();

    ps2_controller_connected = acpi_revision == ACPI_1_0 ? true : (fadt->boot_architecture_flags & 0b10) == 0b10;
    LOG(INFO, "Preferred power management profile : %s (%u)", fadt->preferred_power_management_profile > 7 ? "Unknown" :
        preferred_power_management_profile_text[fadt->preferred_power_management_profile], fadt->preferred_power_management_profile);
    printf("Preferred power management profile : %s (%u)\n", fadt->preferred_power_management_profile > 7 ? "Unknown" :
        preferred_power_management_profile_text[fadt->preferred_power_management_profile], fadt->preferred_power_management_profile);

    disable_interrupts();
    {
        if (ps2_controller_connected)
        {
            apic_map_irq_from_source(1, APIC_PS2_1_INT);
            apic_map_irq_from_source(12, APIC_PS2_2_INT);
        }
        // TODO: Add OSPM support and map fadt->sci_interrupt
    }
    enable_interrupts();

    printf("Done.\n");
    LOG(INFO, "Done parsing ACPI tables.");

    if (ps2_controller_connected)
    {
        LOG(INFO, "Detecting PS/2 devices");
        printf("Detecting PS/2 devices\n");

        ps2_device_1_interrupt = ps2_device_2_interrupt = false;

        ksleep(PS2_WAIT_TIME * PRECISE_MILLISECONDS);

        ps2_controller_init();
        ps2_detect_devices();
        ps2_init_keyboards();

        ksleep(PS2_WAIT_TIME * PRECISE_MILLISECONDS);

        ps2_enable_interrupts();
    }
    else
        printf("No PS/2 Controller\n");

    LOG(INFO, "Setting up the VFS...");
    printf("Mounting initrd at root...\n");
    vfs_create_root_node();
    LOG(DEBUG, "Mounting root");
    int _errno = vfs_mount(vfs_root_node, (vnode_ref_t){ NULL }, "initrd");
    if (_errno)
    {
        pstrerror(_errno, "Couldn't mount root");
        abort();
    }

    vnode_ref_t dev_node = vfs_get_vnode_from_path(&_errno, 0, 0, "/dev", (vnode_ref_t){ NULL }, (vnode_ref_t){ NULL }, false);
    if (_errno)
    {
        pstrerror(_errno, "Couldn't find vnode for /dev");
        abort();
    }

    _errno = vfs_mount(dev_node, (vnode_ref_t){ NULL }, "virt");
    if (_errno)
    {
        pstrerror(_errno, "Couldn't mount /dev");
        abort();
    }

    vfs_create("null", dev_node, S_IFCHR | 0666, 0, 0, null_read, null_write);
    vfs_create("tty", dev_node, S_IFCHR | 0666, 0, 0, console_read, console_write, .isatty = true);
    vfs_create("console", dev_node, S_IFCHR | 0666, 0, 0, NULL, NULL);
    
    vnode_delete_ref(&dev_node);
    
    LOG(TRACE, "Done");

    LOG(INFO, "Set up the VFS.");

    LOG(INFO, "Scanning PCI buses...");
    printf("Scanning PCI buses...\n");

    pci_scan_buses();

    LOG(INFO, "Done scanning PCI buses.");

    LOG(INFO, "Reading CPU thermal sensor...");
    printf("Reading CPU thermal sensor...\n");
    cpu_init_sensor();
    if (temperature_sensor)
    {
        cpu_read_tcc();
        printf("TCC activation temperature : %dC\n", tcc_activation_temperature);
        LOG(INFO, "TCC activation temperature : %dC", tcc_activation_temperature);
        int cpu_temp = cpu_read_temp();
        LOG(INFO, "CPU is at %d°C", cpu_temp);
        printf("CPU is at %dC\n", cpu_temp);
    }
    else
    {
        LOG(INFO, "No temperature sensor");
        printf("No temperature sensor\n");
    }

    // XXX: Terrible
    // TODO: Actually implement pseudoterminals
    tty_ts = (struct termios)
    {
        .c_iflag = ICRNL | IXON,
        .c_oflag = OPOST | ONLCR,
        .c_cflag = B38400 | CS8 | CREAD | HUPCL,
        .c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | IEXTEN,
        .c_cc =
        {
            [VINTR]    = 0x03,
            [VQUIT]    = 0x1C,
            [VERASE]   = 0x7F,
            [VKILL]    = 0x15,
            [VEOF]     = 0x04,
            [VTIME]    = 0,
            [VMIN]     = 1,
            [VSTART]   = 0x11,
            [VSTOP]    = 0x13,
            [VSUSP]    = 0x1A,
            [VEOL]     = 0,
            [VREPRINT] = 0x12,
            [VDISCARD] = 0x0F,
            [VWERASE]  = 0x17,
            [VLNEXT]   = 0x16,
            [VEOL2]    = 0
        }
    };

    ASSERT((sizeof(interrupt_registers_t) % 16) == 0);

    LOG(DEBUG, "sizeof(interrupt_registers_t): %zu", sizeof(interrupt_registers_t));
    LOG(DEBUG, "sizeof(thread_t): %zu", sizeof(thread_t));

    LOG(INFO, "cr4: %#" PRIx64, get_cr4());

    fflush(stdout);

    multitasking_init();

    LOG(TRACE, "Loading init task...");

    startup_data_struct_t data = startup_data_init_from_command((char*[]){"/sbin/init", NULL}, (char*[]){NULL});
    uint32_t flags = lock_scheduler();
    thread_t* init_task = __multitasking_add_task_from_vfs("init", "/sbin/init", 3, true, &data, vfs_root_node);
    unlock_scheduler(flags);
    if (!init_task)
    {
        LOG(CRITICAL, "init task couldn't start");
        printf("\x1b[31merror\x1b[0m: init task couldn't start\n");
        abort();
    }

    LOG(DEBUG, "Starting multitasking...");
    printf("Starting multitasking...\n\n");

    log_segbase();
    vfs_log_structure(vfs_root_node);

    multitasking_start();

    FATAL("Fatal error");
}
