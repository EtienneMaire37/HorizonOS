#include "limine.h"

__attribute__((used, section(".limine_requests_start")))
volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".limine_requests_end")))
volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

__attribute__((used, section(".limine_requests")))
volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(5);

__attribute__((used, section(".limine_requests")))
volatile struct limine_framebuffer_request framebuffer_request = 
{
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};
__attribute__((used, section(".limine_requests")))
volatile struct limine_mp_request mp_request = 
{
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
    .flags = 0
};
__attribute__((used, section(".limine_requests")))
volatile struct limine_rsdp_request rsdp_request = 
{
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_memmap_request mmap_request = 
{
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_hhdm_request hhdm_request = 
{
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};


struct limine_internal_module initrd_module = 
{
    .path = "/boot/initrd.tar",
    .string = "initrd",
    .flags = LIMINE_INTERNAL_MODULE_REQUIRED
};

struct limine_internal_module* limine_modules = &initrd_module;

__attribute__((used, section(".limine_requests")))
volatile struct limine_module_request module_request = 
{
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 1,
    .internal_module_count = 1,
    .internal_modules = &limine_modules
};