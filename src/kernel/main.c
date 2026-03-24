// #include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
#include "cpu/util.h"

#include "boot/limine.h"

extern char kernel_start, kernel_end;
void *kernel_start_ptr, *kernel_end_ptr;

char** environ = NULL;
int num_environ;

#include "cpu/units.h"

#include "util/cfunc.h"
#include "util/math.h"
#include "util/memory.h"

#include <inttypes.h>
#include <limits.h>

#include "time/time.h"

#include "io/io.h"
#include "cpu/cpuid.h"
#include "cpu/msr.h"
#include "cpu/registers.h"
#include "multicore/spinlock.h"
#include "graphics/linear_framebuffer.h"
#include "cpu/cpuid.h"
#include "fpu/sse.h"
#include "debug/out.h"

#include <errno.h>
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

#include "vfs/vfs.h"
#include "time/ktime.h"
#include "initrd/initrd.h"
#include "time/gdn.h"
#include "time/ktime.h"
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
#include "int/kernel_panic.h"
#include "memalloc/virtual_memory_allocator.h"
#include "vfs/table.h"

initrd_file_t* commit_file;

atomic_flag core_log_spinlock = ATOMIC_FLAG_INIT;

void _start()
{
    disable_interrupts();

    PHYS_MAP_BASE = hhdm_request.response->offset;

    uint32_t eax, ebx, ecx, edx;
    cpuid(0, cpuid_highest_function_parameter, ebx, ecx, edx);
    // * CPUID is guaranteed to be present on x86_64

    acquire_spinlock(&core_log_spinlock);
    fprintf(stderr, "a");
    first_log = true;
    release_spinlock(&core_log_spinlock);

    if (!is_bsp()) // * SMP not supported for now
        halt();

    LOG(DEBUG, "_start");

    kernel_start_ptr = (void*)&kernel_start;
    kernel_end_ptr = (void*)&kernel_end;

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1)
        halt();

    // assert(framebuffer_request.response->framebuffers[0]->memory_model == LIMINE_FRAMEBUFFER_RGB);
    assert(framebuffer_request.response->framebuffers[0]->bpp % 8 == 0);

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

    cpuid(0x80000000, cpuid_highest_extended_function_parameter, ebx, ecx, edx);
    LOG(INFO, "CPUID highest extended function parameter: %#x", cpuid_highest_extended_function_parameter);

    if (strcmp(manufacturer_id_string, "GenuineIntel") == 0)
        cpu_brand = CPU_INTEL;
    else if (strcmp(manufacturer_id_string, "AuthenticAMD") == 0)
        cpu_brand = CPU_AMD;
    else
        cpu_brand = CPU_UNKNOWN;

    if (cpu_brand == CPU_AMD)
    {
        char easter_egg_str[17];

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

    LOG(INFO, "cpu_id : %u", cpuid_get_cpu_id());

    struct limine_file* initrd = NULL;
    for (int i = 0; i < module_request.response->module_count; i++)
    {
        struct limine_file* file = module_request.response->modules[i];
        if (strcmp(initrd_module.path, file->path) == 0)
        {
            initrd = file;
            break;
        }
    }
    assert(initrd);

    initrd_parse((uint64_t)initrd->address, initrd->size);
    kernel_symbols_file = initrd_find_file("boot/symbols.txt");

    tty_font = psf_font_load_from_initrd("boot/ka8x16thin-1.psf");

    if (!tty_font.f)
    {
        LOG(DEBUG, "Couldn't find psf font in initrd");
        abort();
    }

    pfa_detect_usable_memory();

    tty_init();

// * vvv Now we can use stdout

    assert(LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision));

    commit_file = initrd_find_file("boot/commit.txt");

    LOG(INFO, "commit hash: %s", commit_file->data);
    printf("commit hash: ");
    tty_set_color(FG_LIGHTMAGENTA, BG_BLACK);
    puts((const char*)commit_file->data);
    tty_set_color(FG_WHITE, BG_BLACK);

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

    {
        global_cr3 = pfa_allocate_page();
        assert(global_cr3);
        LOG(DEBUG, "global_cr3: %p", global_cr3);
        memset(global_cr3, 0, 4096);

        uint64_t* boot_cr3 = (uint64_t*)(get_cr3_address() + PHYS_MAP_BASE);

        LOG(DEBUG, "boot_cr3: %#" PRIx64, (uint64_t)boot_cr3);

        printf("Copying mapping of range %p-%p from limine\n", kernel_start_ptr, kernel_end_ptr);
        LOG(DEBUG, "Copying mapping of range %p-%p from limine", kernel_start_ptr, kernel_end_ptr);

        copy_mapping(boot_cr3, global_cr3, (uintptr_t)kernel_start_ptr, (uint64_t)((uintptr_t)kernel_end_ptr - (uintptr_t)kernel_start_ptr) >> 12);

        for (int i = 0; i < mmap_request.response->entry_count; i++)
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
            if (len > MAX_MEMORY - ptr)
                len = MAX_MEMORY - ptr;

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
        uint64_t paddr = (uint64_t)virtual_to_physical(global_cr3, (uintptr_t)sighandler) - PHYS_MAP_BASE;
        unmap_range(global_cr3, (uintptr_t)sighandler, 1);
        remap_range(global_cr3, (uintptr_t)sighandler, paddr, 1, PG_USER, PG_READ_ONLY, CACHE_WB);
    }

    load_cr3((uint64_t)global_cr3 - PHYS_MAP_BASE);
    vmm_initialized = true;

    printf("Paging setup done\n");
    LOG(INFO, "Set up paging");

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
            LOG(INFO, "Detecting FPU");

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

    apic_init();

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

    printf("%" PRIu64 " core%s running\n", mp_request.response->cpu_count, mp_request.response->cpu_count == 1 ? "" : "s");
    LOG(INFO, "%" PRIu64 " core%s running", mp_request.response->cpu_count, mp_request.response->cpu_count == 1 ? "" : "s");

    printf("CPU manufacturer id : ");
    tty_set_color(FG_LIGHTRED, BG_BLACK);
    printf("\"%s\"\n", manufacturer_id_string);
    tty_set_color(FG_WHITE, BG_BLACK);

    printf("CPUID highest function parameter: %#x\n", cpuid_highest_function_parameter);
    printf("CPUID highest extended function parameter: %#x\n", cpuid_highest_extended_function_parameter);

    printf("Physical address is ");
    tty_set_color(FG_LIGHTBLUE, BG_BLACK);
    printf("%u ", physical_address_width);
    tty_set_color(FG_WHITE, BG_BLACK);
    printf("bits long\n");

    LOG(INFO, "Loading a GDT with TSS...");
    printf("Loading a GDT with TSS...");
    fflush(stdout);

    memset(&GDT[0], 0, sizeof(struct gdt_entry));       // NULL Descriptor
    setup_gdt_entry(&GDT[1], 0, 0xfffff, 0x9A, 0xA);    // Kernel mode code segment
    setup_gdt_entry(&GDT[2], 0, 0xfffff, 0x92, 0xC);    // Kernel mode data segment
    setup_gdt_entry(&GDT[3], 0, 0xfffff, 0xF2, 0xC);    // User mode data segment
    setup_gdt_entry(&GDT[4], 0, 0xfffff, 0xFA, 0xA);    // User mode code segment

    memset(&TSS, 0, sizeof(struct tss_entry));
    TSS.rsp0 = TASK_KERNEL_STACK_TOP_ADDRESS;
    setup_ssd_gdt_entry(&GDT[5], (physical_address_t)&TSS, sizeof(struct tss_entry) - 1, 0x89, 0);  // TSS

    install_gdt();
    load_tss();

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

    LOG(INFO, "Setting up the APIC timer");
    printf("Setting up the APIC timer");
    fflush(stdout);

    apic_timer_init();
    rtc_get_time();
    time_initialized = true;

    LOG(INFO, "Set up the APIC timer");
    printf(" | Done\n");

    printf("Time: ");

    tty_set_color(FG_LIGHTCYAN, BG_BLACK);
    printf("%" PRIu64 "-%" PRIu64 "-%" PRIu64 " %" PRIu64 ":%" PRIu64 ":%" PRIu64 "\n", system_year, system_month, system_day, system_hours, system_minutes, system_seconds);
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

    printf("Calibrating TSC...\n");
    LOG(DEBUG, "Calibrating TSC...");
    calibrate_tsc();
    LOG(INFO, "TSC clock running at approximatively %" PRIu64 " hz", tsc_cycles_per_second);
    printf("TSC clock running at approximatively %" PRIu64 " hz\n", tsc_cycles_per_second);

    LOG(INFO, "Parsing ACPI tables..");
    printf("Parsing ACPI tables...\n");
    acpi_find_tables();
    fadt_extract_data();

    disable_interrupts();
        madt_extract_data();
    enable_interrupts();

    printf("Done.\n");
    LOG(INFO, "Done parsing ACPI tables.");

    if (ps2_controller_connected)
    {
        LOG(INFO, "Detecting PS/2 devices");
        printf("Detecting PS/2 devices\n");

        ps2_device_1_interrupt = ps2_device_2_interrupt = false;

        ksleep(10 * PRECISE_MILLISECONDS);

        ps2_controller_init();
        ps2_detect_keyboards();
        ps2_init_keyboards();

        ksleep(10 * PRECISE_MILLISECONDS);

        ps2_enable_interrupts();

        if (ps2_device_1_connected)
        {
            LOG(INFO, "PS/2 device 1 connected");
            printf("PS/2 device 1 connected\n");
        }
        if (ps2_device_2_connected)
        {
            LOG(INFO, "PS/2 device 2 connected");
            printf("PS/2 device 2 connected\n");
        }
        if (!(ps2_device_1_connected || ps2_device_2_connected))
        {
            LOG(INFO, "No PS/2 devices detected");
            printf("No PS/2 devices detected\n");
        }

        ps2_flush_buffer();
    }

    LOG(INFO, "Setting up the VFS...");
    printf("Mounting initrd at root...\n");
    vfs_root = vfs_create_empty_folder_tnode("root", NULL, VFS_NODE_MOUNTPOINT | VFS_NODE_INIT,
        0,
        S_IFDIR |
        S_IRUSR | S_IXUSR |
        S_IRGRP | S_IXGRP |
        S_IROTH | S_IXOTH,
        0, 0,
        (drive_t){.type = DT_INITRD});
    if (!vfs_root)
    {
        LOG(DEBUG, "Couldn't create VFS root");
        abort();
    }
    vfs_root->inode->parent = vfs_root;
    vfs_explore(vfs_root);

    vfs_mount_device("mnt", "/", (drive_t){.type = DT_VIRTUAL}, 0, 0);
    // * Can't mount more than once
    // vfs_mount_device("initrd", "/mnt", (drive_t){.type = DT_INITRD}, 0, 0);
    vfs_mount_device("dev", "/", (drive_t){.type = DT_VIRTUAL}, 0, 0);

    vfs_add_special("/dev", "tty", CHR_MODE, task_chr_tty, 0, 0);
    LOG(INFO, "Set up the VFS.");

    LOG(INFO, "Scanning PCI buses...");
    printf("Scanning PCI buses...\n");

    pci_scan_buses();

    putchar('\n');

    LOG(INFO, "Done scanning PCI buses.");

    LOG(DEBUG, "VFS TREE:");
    vfs_log_tree(vfs_root, 0);

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

    assert((sizeof(interrupt_registers_t) % 16) == 0);

    LOG(DEBUG, "sizeof(thread_t): %lld", (unsigned long long)sizeof(thread_t));

    LOG(INFO, "cr4: %#" PRIx64, get_cr4());

    fflush(stdout);

    multitasking_init();

    startup_data_struct_t data = startup_data_init_from_command((char*[]){"/sbin/init", NULL}, (char*[]){NULL});
    thread_t* init_task = multitasking_add_task_from_vfs("init", "/sbin/init", 3, true, &data, vfs_root);
    if (!init_task)
    {
        LOG(CRITICAL, "init task couldn't start");
        printf("\x1b[31merror\x1b[0m: init task couldn't start\n");
        abort();
    }

    lock_scheduler();
    init_task->file_table[STDIN_FILENO].flags = 0;
    init_task->file_table[STDIN_FILENO].index = 0;
    file_table[init_task->file_table[STDIN_FILENO].index].used++;
    init_task->file_table[STDOUT_FILENO].flags = 0;
    init_task->file_table[STDOUT_FILENO].index = 1;
    file_table[init_task->file_table[STDOUT_FILENO].index].used++;
    init_task->file_table[STDERR_FILENO].flags = 0;
    init_task->file_table[STDERR_FILENO].index = 2;
    file_table[init_task->file_table[STDERR_FILENO].index].used++;
    unlock_scheduler();

    LOG(DEBUG, "Starting multitasking...");

    log_segbase();

    multitasking_start();

    while(true)
        hlt();

    halt();
}
