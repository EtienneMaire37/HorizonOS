#include <limine.h>

extern volatile uint64_t limine_requests_start_marker[];
extern volatile uint64_t limine_requests_end_marker[];
extern volatile uint64_t limine_base_revision[];
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_mp_request mp_request;
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_memmap_request mmap_request;
extern volatile struct limine_hhdm_request hhdm_request;