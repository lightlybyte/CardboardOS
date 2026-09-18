#include <stddef.h>
#include <stdint.h>
#include "limine.h"

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[3] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[2] = LIMINE_REQUESTS_END_MARKER;

struct limine_framebuffer *get_framebuffer(void) {
    if (framebuffer_request.response == NULL) return NULL;
    if (framebuffer_request.response->framebuffer_count < 1) return NULL;
    return framebuffer_request.response->framebuffers[0];
}

uint64_t get_hhdm_offset(void) {
    if (hhdm_request.response == NULL) return 0;
    return hhdm_request.response->offset;
}