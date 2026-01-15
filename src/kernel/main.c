// #include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "../libc/include/stdint.h"
#include "../libc/include/stddef.h"

#include <bootboot.h>

typedef uint64_t physical_address_t;
typedef uint64_t virtual_address_t;

extern BOOTBOOT bootboot;
extern uint8_t environment[4096];
extern uint8_t fb;

uint64_t* global_cr3 = NULL;

extern char kernel_start, kernel_end;
physical_address_t kernel_start_phys, kernel_end_phys;

#define enable_interrupts()     asm volatile ("sti")
#define disable_interrupts()    asm volatile ("cli")

#define hlt()                   asm volatile ("hlt")

void halt();

void __attribute__((noreturn)) _halt()
{
    while (true)
    {
        disable_interrupts();
        hlt();
    }
    __builtin_unreachable();
}

// * Only support 48-bit canonical addresses for now (4 level paging)
bool is_address_canonical(uint64_t address)
{
    return (address < 0x0000800000000000ULL) || (address >= 0xffff800000000000ULL);
}
uint64_t make_address_canonical(uint64_t address)
{
    if (address & 0x0000800000000000ULL)
        return address | 0xffff800000000000ULL;
    else
        return address & 0x00007fffffffffffULL;
}

#define KB 1024ULL
#define MB (1024ULL * KB)
#define GB (1024ULL * MB)
#define TB (1024ULL * GB)

#define BUILDING_C_LIB
#define BUILDING_KERNEL

#ifndef __CURRENT_FUNC__

#if __STDC_VERSION__ >= 199901L
#define __CURRENT_FUNC__    __func__
#elif __GNUC__ >= 2
#define __CURRENT_FUNC__    __FUNCTION__
#else
#define __CURRENT_FUNC__    ""
#endif

#endif

#define physical_null ((physical_address_t)0)

static int64_t minint(int64_t a, int64_t b);
static int64_t maxint(int64_t a, int64_t b);
static int64_t absint(int64_t x);
static int imod(int a, int b);

#include "../libc/include/inttypes.h"
#include "../libc/include/limits.h"

#define hex_char_to_int(ch) (ch >= '0' && ch <= '9' ? (ch - '0') : (ch >= 'a' && ch <= 'f' ? (ch - 'a' + 10) : 0))

#define bcd_to_binary(bcd) (((bcd) & 0x0f) + (((bcd) >> 4) & 0x0f) * 10 + (((bcd) >> 8) & 0x0f) * 100 + (((bcd) >> 12) & 0x0f) * 1000)

char** environ = NULL;
static int num_environ = 0;

int64_t system_seconds = 0, system_minutes = 0, system_hours = 0, system_day = 0, system_month = 0;
int64_t system_year = 0;
int64_t system_thousands = 0;

bool time_initialized = false;

#include "io/io.h"
#include "cpu/cpuid.h"
#include "cpu/msr.h"
#include "cpu/registers.h"
#include "multicore/spinlock.h"
#include "../libc/src/misc.h"
#include "graphics/linear_framebuffer.h"
#include "cpu/cpuid.h"
#include "fpu/sse.h"
#include "debug/out.h"

#include "../libc/include/errno.h"
#include "../libc/include/stdio.h"
#include "../libc/include/stdlib.h"
#include "../libc/src/string.c"
#include "../libc/include/string.h"
#include "../libc/include/unistd.h"
#include "../libc/include/termios.h"
#include "../libc/include/assert.h"
#include "../libc/include/sys/types.h"
#include "../libc/include/sys/wait.h"
#include "../libc/include/sys/stat.h"
#include "../libc/include/fcntl.h"
#include "../libc/include/dirent.h"

#include "vfs/vfs.h"
#include "time/ktime.h"
#include "initrd/initrd.h"
#include "time/gdn.h"
#include "time/ktime.h"
#include "time/time.h"
#include "cmos/rtc.h"

initrd_file_t* commit_file;

#include "vga/textio.c"
#include "pic/apic.c"
#include "fpu/fpu.c"

#include "../libc/src/kernel.c"
#include "../libc/src/stdio.c"
#include "../libc/src/stdlib.c"
#include "../libc/src/unistd.c"
#include "../libc/src/time.c"
#include "gdt/gdt.c"
#include "int/idt.c"
#include "int/int.c"
#include "memalloc/page_frame_allocator.c"
#include "paging/paging.c"
#include "multitasking/task.c"
#include "io/keyboard.c"
#include "ps2/keyboard.c"
#include "ps2/ps2.c"
#include "acpi/tables.c"
#include "pci/pci.c"
#include "disk/ata.c"
#include "multitasking/loader.c"
#include "../libc/src/startup_data.c"
#include "vfs/vfs.c"
#include "multitasking/mutex.c"
#include "../liballoc/liballoc.c"
#include "memalloc/liballoc_hooks.c"

static int64_t minint(int64_t a, int64_t b)
{
    return a < b ? a : b;
}
static int64_t maxint(int64_t a, int64_t b)
{
    return a > b ? a : b;
}
static int64_t absint(int64_t x)
{
    return x < 0 ? -x : x;
}
static int imod(int a, int b)
{
    if (b <= 0) 
    {
        LOG(WARNING, "Kernel tried to compute %lld %% %lld", a, b);
        return 0;
    }
    int ret = a - (a / b) * b;
    if (ret < 0) ret += b;
    return ret;
}

void __attribute__((noreturn)) cause_halt(const char* func, const char* file, int line)
{
    disable_interrupts();
    // LOG(ERROR, "Kernel halted in function \"%s\" at line %d in file \"%s\"", func, line, file);
    _halt();
}

void simple_cause_halt()
{
    disable_interrupts();
    // LOG(ERROR, "Kernel halted");
    _halt();
}

void __attribute__((noreturn)) halt()
{
    // fflush(stdout);
    cause_halt(__CURRENT_FUNC__, __FILE__, __LINE__);
}

FILE _stdin, _stdout, _stderr;

uint8_t stdin_buffer[BUFSIZ];
uint8_t stdout_buffer[BUFSIZ];
uint8_t stderr_buffer[BUFSIZ];

#define _init_file_flags(f) { f->fd = -1; f->buffer_size = BUFSIZ; f->buffer_index = 0; f->buffer_mode = 0; f->flags = FILE_FLAGS_BF_ALLOC; f->current_flags = 0; f->buffer_end_index = 0;}

void kernel_init_std()
{
    stdin = &_stdin;
    stdout = &_stdout;
    stderr = &_stderr;

    stdin->buffer = stdin_buffer;
    stdout->buffer = stdout_buffer;
    stderr->buffer = stderr_buffer;

    _init_file_flags(stdin);
    _init_file_flags(stdout);
    _init_file_flags(stderr);
    
    stdin->fd = STDIN_FILENO;
    stdin->flags = FILE_FLAGS_READ;

    stdout->fd = STDOUT_FILENO;
    stdout->flags = FILE_FLAGS_WRITE | FILE_FLAGS_LBF;

    stderr->fd = STDERR_FILENO;
    stderr->flags = FILE_FLAGS_WRITE | FILE_FLAGS_NBF;
}

atomic_flag print_spinlock = ATOMIC_FLAG_INIT;
atomic_bool did_init_std = false;

const char* fb_type_string[] = 
{
    "FB_ARGB",
    "FB_RGBA",
    "FB_ABGR",
    "FB_BGRA"
};

void _start()
{
    disable_interrupts();

    enable_sse();

    if (!bootboot.fb_scanline) 
        abort();

    apic_init();
    uint8_t cpu_id = apic_get_cpu_id();

    if (bootboot.bspid != cpu_id) // * Only one core supported for now
        halt();

    if (bootboot.bspid == cpu_id)
    {
        kernel_init_std();

        kernel_start_phys = (physical_address_t)&kernel_start;
        kernel_end_phys = (physical_address_t)&kernel_end;

        LOG(INFO, "Kernel booted successfully with BOOTBOOT (%#llx-%#llx)", kernel_start_phys, kernel_end_phys);
        LOG(INFO, "Kernel is %llu bytes long", kernel_end_phys - kernel_start_phys);
        LOG(INFO, "Framebuffer : (%u, %u) (scanline %u bytes) at %#llx", bootboot.fb_width, bootboot.fb_height, bootboot.fb_scanline, bootboot.fb_ptr);
        LOG(INFO, "Type: %s", fb_type_string[bootboot.fb_type]);

        framebuffer.width = bootboot.fb_width;
        framebuffer.height = bootboot.fb_height;
        framebuffer.stride = bootboot.fb_scanline;
        framebuffer.address = (uintptr_t)bootboot.fb_ptr;
        framebuffer.format = bootboot.fb_type;

        LOG(INFO, "LAPIC base: %p", lapic);

        uint32_t ebx, ecx, edx;
        cpuid(0, cpuid_highest_function_parameter, ebx, ecx, edx);
        // !! Actually will cause a triple fault on CPUs that don't support CPUID but you shouldn't be running this OS on such hardware anyway

        LOG(INFO, "CPUID highest function parameter: 0x%x", cpuid_highest_function_parameter);

        *(uint32_t*)&manufacturer_id_string[0] = ebx;
        *(uint32_t*)&manufacturer_id_string[4] = edx;
        *(uint32_t*)&manufacturer_id_string[8] = ecx;
        manufacturer_id_string[12] = 0;

        LOG(INFO, "CPU manufacturer id : \"%s\"", manufacturer_id_string);

        cpuid(0x80000000, cpuid_highest_extended_function_parameter, ebx, ecx, edx);
        LOG(INFO, "CPUID highest extended function parameter: 0x%x", cpuid_highest_extended_function_parameter);

        if (cpuid_highest_extended_function_parameter >= 0x80000008)
        {
            uint32_t eax = 0, ebx, ecx, edx; // Just so gcc doesn't complain but cant possibly be used uninitialized
            cpuid(0x80000008, eax, ebx, ecx, edx);
            physical_address_width = eax & 0xff;
        }
        else
            abort();

        LOG(INFO, "Physical address is %u bits long", physical_address_width);

        init_pat();

        atomic_store(&did_init_std, true);
    }

    while (!atomic_load(&did_init_std));

    acquire_spinlock(&print_spinlock);

    LOG(INFO, "cpu_id : %u", cpu_id);

    release_spinlock(&print_spinlock);

    initrd_parse(bootboot.initrd_ptr, bootboot.initrd_size);
    kernel_symbols_file = initrd_find_file("symbols.txt");

    tty_font = psf_font_load_from_initrd("ka8x16thin-1.psf");

    if(!tty_font.f)
        abort();

// * vvv Now we can use printf

    tty_clear_screen(' ');

    commit_file = initrd_find_file("commit.txt");

    LOG(INFO, "commit hash: %s", commit_file->data);
    printf("commit hash: ");
    tty_set_color(FG_LIGHTMAGENTA, BG_BLACK);
    puts((const char*)commit_file->data);
    tty_set_color(FG_WHITE, BG_BLACK);

    pfa_detect_usable_memory();

    printf("Detected ");
    tty_set_color(FG_LIGHTBLUE, BG_BLACK);
    printf("%llu ", allocatable_memory);
    tty_set_color(FG_WHITE, BG_BLACK);
    printf("bytes of allocatable memory\n");
    
    if (cpuid_highest_function_parameter < 0x0d)
    {
        LOG(CRITICAL, "Modern FPU not supported...");
        tty_set_color(FG_LIGHTRED, BG_BLACK);
        printf("Modern FPU not supported...\n");
        tty_set_color(FG_WHITE, BG_BLACK);
        abort();
    }

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
        global_cr3 = (uint64_t*)pfa_allocate_physical_page();
        uint64_t* bootboot_cr3 = (uint64_t*)get_cr3();

        LOG(DEBUG, "bootboot_cr3: %#llx", bootboot_cr3);

    // * "When the kernel gains control, the memory mapping looks like this:"
    // *  -128M         "mmio" area           (0xFFFFFFFFF8000000)
    // *   -64M         "fb" framebuffer      (0xFFFFFFFFFC000000)
    // *    -2M         "bootboot" structure  (0xFFFFFFFFFFE00000)
    // *    -2M+1page   "environment" string  (0xFFFFFFFFFFE01000)
    // *    -2M+2page.. code segment   v      (0xFFFFFFFFFFE02000)
    // *     ..0        stack          ^      (0x0000000000000000)
    // *    0-16G       RAM identity mapped   (0x0000000400000000)

    // * bootboot + environment + code segment + stack
        printf("Copying mapping of range %#llx-%#llx from bootboot\n", 0xFFFFFFFFFFE00000, 0ULL);
        LOG(DEBUG, "Copying mapping of range %#llx-%#llx from bootboot", 0xFFFFFFFFFFE00000, 0ULL);

        copy_mapping(bootboot_cr3, global_cr3, 0xFFFFFFFFFFE00000, (uint64_t)(-0xFFFFFFFFFFE00000) >> 12);

        for (MMapEnt* mmap_ent = &bootboot.mmap; (uintptr_t)mmap_ent < (uintptr_t)&bootboot + (uintptr_t)bootboot.size; mmap_ent++)
        {
            uint64_t ptr = MMapEnt_Ptr(mmap_ent) & 0xfffffffffffff000;
            uint64_t len = ((MMapEnt_Size(mmap_ent) + 0xfff) / 0x1000) * 0x1000 + 0x1000;

            if (!MMapEnt_IsFree(mmap_ent))
                continue;

            if (ptr >= 1 * TB)
                continue;
            if (ptr + len >= 1 * TB)
                len = 1 * TB - ptr;
                
            LOG(DEBUG, "Mapping range %#llx-%#llx to %#llx-%#llx", ptr, ptr + len, ptr + PHYS_MAP_OFFSET, ptr + len + PHYS_MAP_OFFSET);
            printf("Mapping range %#llx-%#llx to %#llx-%#llx\n", ptr, ptr + len, ptr + PHYS_MAP_OFFSET, ptr + len + PHYS_MAP_OFFSET);
            remap_range(global_cr3, ptr + PHYS_MAP_OFFSET, ptr, len >> 12, PG_SUPERVISOR, PG_READ_WRITE, CACHE_WB);
        }

    // * LAPIC registers
        printf("Mapping range %#llx-%#llx to %#llx-%#llx\n", lapic, (uint64_t)lapic + 0x1000, (uint64_t)lapic + PHYS_MAP_OFFSET, (uint64_t)lapic + PHYS_MAP_OFFSET + 0x1000);
        LOG(DEBUG, "Mapping range %#llx-%#llx to %#llx-%#llx", lapic, (uint64_t)lapic + 0x1000, (uint64_t)lapic + PHYS_MAP_OFFSET, (uint64_t)lapic + PHYS_MAP_OFFSET + 0x1000);
        remap_range(global_cr3, (uint64_t)lapic + PHYS_MAP_OFFSET, (uint64_t)lapic, 1, PG_SUPERVISOR, PG_READ_WRITE, CACHE_WT);

    // * Framebuffer
        printf("Mapping range %#llx-%#llx to %#llx-%#llx\n", framebuffer.address, ((framebuffer.address + framebuffer.stride * framebuffer.height + 0xfff) / 0x1000) * 0x1000, framebuffer.address + PHYS_MAP_OFFSET, ((framebuffer.address + framebuffer.stride * framebuffer.height + 0xfff) / 0x1000) * 0x1000 + PHYS_MAP_OFFSET);
        LOG(DEBUG, "Mapping range %#llx-%#llx to %#llx-%#llx", framebuffer.address, ((framebuffer.address + framebuffer.stride * framebuffer.height + 0xfff) / 0x1000) * 0x1000, framebuffer.address + PHYS_MAP_OFFSET, ((framebuffer.address + framebuffer.stride * framebuffer.height + 0xfff) / 0x1000) * 0x1000 + PHYS_MAP_OFFSET);
        
    // ? Write-combining cache
        remap_range(global_cr3, (uint64_t)framebuffer.address + PHYS_MAP_OFFSET, (uint64_t)framebuffer.address, (framebuffer.stride * framebuffer.height + 0xfff) / 0x1000, 
            PG_SUPERVISOR, PG_READ_WRITE, CACHE_WC);

    // * initrd
        printf("Mapping range %#llx-%#llx to %#llx-%#llx\n", bootboot.initrd_ptr & 0xfffffffffffff000, (bootboot.initrd_ptr & 0xfffffffffffff000) + ((bootboot.initrd_size + 0x1fff) / 0x1000) * 0x1000, PHYS_MAP_OFFSET + (bootboot.initrd_ptr & 0xfffffffffffff000), PHYS_MAP_OFFSET + (bootboot.initrd_ptr & 0xfffffffffffff000) + ((bootboot.initrd_size + 0x1fff) / 0x1000) * 0x1000);
        LOG(DEBUG, "Mapping range %#llx-%#llx to %#llx-%#llx", bootboot.initrd_ptr & 0xfffffffffffff000, (bootboot.initrd_ptr & 0xfffffffffffff000) + ((bootboot.initrd_size + 0x1fff) / 0x1000) * 0x1000);
        remap_range(global_cr3, ((uint64_t)bootboot.initrd_ptr & 0xfffffffffffff000) + PHYS_MAP_OFFSET, (uint64_t)bootboot.initrd_ptr & 0xfffffffffffff000, (bootboot.initrd_size + 0x1fff) / 0x1000, PG_SUPERVISOR, PG_READ_WRITE, CACHE_WB);
    }

    PHYS_MAP_BASE = PHYS_MAP_OFFSET;
    global_cr3 = (uint64_t*)((physical_address_t)global_cr3 + PHYS_MAP_OFFSET);

    load_cr3((uint64_t)global_cr3 - PHYS_MAP_OFFSET);

    bitmap = (uint8_t*)((physical_address_t)bitmap + PHYS_MAP_OFFSET);
    lapic = (volatile local_apic_registers_t*)((physical_address_t)lapic + PHYS_MAP_OFFSET);
    framebuffer.address += PHYS_MAP_OFFSET;
    for (int i = 0; i < initrd_files_count; i++)
    {
        initrd_files[i].name += PHYS_MAP_OFFSET;
        initrd_files[i].data += PHYS_MAP_OFFSET;
        initrd_files[i].link += PHYS_MAP_OFFSET;
    }

    printf("Paging setup done\n");
    LOG(INFO, "Set up paging");

    if (cpuid_highest_function_parameter >= 1)
    {
        uint32_t eax, ebx, ecx = 0, edx; // same as above
        cpuid(1, eax, ebx, ecx, edx);
        if (((ecx >> 26) & 1) && ((ecx >> 28) & 1)) // * AVX and XSAVE supported
        {
            LOG(INFO, "Enabling AVX");
            enable_avx();
            LOG(DEBUG, "Setting up FPU support");
            fpu_init_defaults();
        }
        else
        {
            LOG(CRITICAL, "Modern FPU not supported...");
            tty_set_color(FG_LIGHTRED, BG_BLACK);
            printf("Modern FPU not supported...\n");
            tty_set_color(FG_WHITE, BG_BLACK);
            abort();
        }
    }

    LOG(INFO, "XSAVE area is %u bytes", xsave_area_size);
    printf("XSAVE area is %u bytes\n", xsave_area_size);

    printf("LAPIC base: %p\n", lapic);

    printf("%u core%s running\n", bootboot.numcores, bootboot.numcores == 1 ? "" : "s");
    LOG(INFO, "%u core%s running", bootboot.numcores, bootboot.numcores == 1 ? "" : "s");

    printf("CPU manufacturer id : ");
    tty_set_color(FG_LIGHTRED, BG_BLACK);
    printf("\"%s\"\n", manufacturer_id_string);
    tty_set_color(FG_WHITE, BG_BLACK);

    printf("CPUID highest function parameter: 0x%x\n", cpuid_highest_function_parameter);
    printf("CPUID highest extended function parameter: 0x%x\n", cpuid_highest_extended_function_parameter);

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
    setup_gdt_entry(&GDT[3], 0, 0xfffff, 0xFA, 0xA);    // User mode code segment
    setup_gdt_entry(&GDT[4], 0, 0xfffff, 0xF2, 0xC);    // User mode data segment

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

    {
        rtc_wait_while_updating();

        lapic->divide_configuration_register = 3;
        lapic->initial_count_register = 0xffffffff;

        rtc_get_time();

        lapic->lvt_timer_register = 0x10000;    // mask it

        uint32_t ticks_in_1_sec = 0xffffffff - lapic->current_count_register;

        lapic->lvt_timer_register = APIC_TIMER_INT | 0x20000; // APIC_TIMER_INT | PERIODIC
        lapic->divide_configuration_register = 3;
        lapic->initial_count_register = ticks_in_1_sec / GLOBAL_TIMER_FREQUENCY;

        time_initialized = true;
    }

    LOG(INFO, "Set up the APIC timer");
    printf(" | Done\n");

    printf("Time: ");

    tty_set_color(FG_LIGHTCYAN, BG_BLACK);
    printf("%llu-%llu-%llu %llu:%llu:%llu\n", system_year, system_month, system_day, system_hours, system_minutes, system_seconds);
    tty_set_color(FG_WHITE, BG_BLACK);

    enable_interrupts(); 

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
    vfs_root = vfs_create_empty_folder_tnode("root", NULL, VFS_NODE_EXPLORED, 
        0, 
        S_IFDIR | 
        S_IRUSR | S_IXUSR |
        S_IRGRP | S_IXGRP |
        S_IROTH | S_IXOTH, 
        0, 0,
        (drive_t){.type = DT_VIRTUAL});
    vfs_root->inode->parent = vfs_root;
    if (!vfs_root) abort();
    vfs_mount_device("initrd", (drive_t){.type = DT_INITRD}, 0, 0);
    vfs_mount_device("devices", (drive_t){.type = DT_VIRTUAL}, 0, 0);
    vfs_get_folder_tnode("/devices", NULL)->inode->flags |= VFS_NODE_EXPLORED;

    vfs_add_special("/devices", "stdin", CHR_MODE, task_chr_stdin, 0, 0);
    vfs_add_special("/devices", "stdout", CHR_MODE, task_chr_stdout, 0, 0);
    vfs_add_special("/devices", "stderr", CHR_MODE, task_chr_stderr, 0, 0);
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

    // TODO: Find out how to use efi_ptr (System Table) to get access to runtime uefi functions

    // asm volatile("div rcx" :: "c"(0));

    fflush(stdout);

    multitasking_init();

    // {
    //     uint64_t rsp;
    //     asm volatile("mov rax, rsp" : "=a"(rsp));
    //     LOG(DEBUG, "rsp: %#llx", rsp);
    //     LOG(DEBUG, "%llu bytes left", 1024 + rsp);
    //     while (true);
    // }

    startup_data_struct_t data = startup_data_init_from_command((char*[]){"/initrd/bin/init.elf", NULL}, (char*[]){"PATH=/initrd/bin/", NULL});
    if (!multitasking_add_task_from_vfs("init", "/initrd/bin/init.elf", 0, true, &data, vfs_root))
    {
        LOG(CRITICAL, "init task couldn't start");
        abort();
    }

    // multitasking_add_task_from_function("test", test_func);

    LOG(DEBUG, "Starting multitasking...");

    multitasking_start();

    while(true)
        hlt();

    halt();
}