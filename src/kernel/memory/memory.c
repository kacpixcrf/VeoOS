#include "kernel/memory/memory.h"
#include "kernel/core/kernel.h"

#define PAGE_SIZE       4096ULL
#define PAGE_SHIFT      12
#define MAX_PHYS_PAGES  (4ULL * 1024 * 1024 * 1024 / PAGE_SIZE)
#define BITMAP_BYTES    (MAX_PHYS_PAGES / 8)

extern uint8_t kernel_end;

static uint8_t  page_bitmap[BITMAP_BYTES];
static size_t   total_pages;
static size_t   used_pages;
static uintptr_t heap_start;
static uintptr_t heap_pointer;

static void pmm_mark_used(size_t idx) {
    page_bitmap[idx >> 3] |= (uint8_t)(1u << (idx & 7));
}

static void pmm_mark_free(size_t idx) {
    page_bitmap[idx >> 3] &= (uint8_t)~(1u << (idx & 7));
}

static bool pmm_is_used(size_t idx) {
    return (page_bitmap[idx >> 3] >> (idx & 7)) & 1;
}

void memory_initialize(void) {
    heap_pointer = ((uintptr_t)&kernel_end + 0xFFF) & ~(uintptr_t)0xFFF;
    heap_start   = heap_pointer;

    const struct multiboot_info* mb = kernel_get_multiboot_info();
    size_t total_ram = 64ULL * 1024 * 1024;
    if (mb != NULL && (mb->flags & MULTIBOOT_INFO_MEMORY) && mb->mem_upper > 0) {
        total_ram = (size_t)mb->mem_upper * 1024ULL;
    }

    total_pages = total_ram / PAGE_SIZE;
    if (total_pages > MAX_PHYS_PAGES) {
        total_pages = MAX_PHYS_PAGES;
    }

    size_t kernel_pages = (heap_pointer + PAGE_SIZE - 1) / PAGE_SIZE;
    if (kernel_pages > total_pages) {
        kernel_pages = total_pages;
    }

    for (size_t i = 0; i < kernel_pages; i++) {
        pmm_mark_used(i);
    }
    for (size_t i = kernel_pages; i < total_pages; i++) {
        pmm_mark_free(i);
    }

    used_pages = kernel_pages;
}

void* memory_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    size = (size + 0xF) & ~(size_t)0xF;
    void* ptr = (void*)heap_pointer;

    size_t old_top = (heap_pointer + PAGE_SIZE - 1) >> PAGE_SHIFT;
    heap_pointer += size;
    size_t new_top = (heap_pointer + PAGE_SIZE - 1) >> PAGE_SHIFT;

    for (size_t i = old_top; i < new_top && i < total_pages; i++) {
        if (!pmm_is_used(i)) {
            pmm_mark_used(i);
            used_pages++;
        }
    }

    return ptr;
}

uintptr_t memory_alloc_page(void) {
    for (size_t i = (heap_pointer >> PAGE_SHIFT); i < total_pages; i++) {
        if (!pmm_is_used(i)) {
            pmm_mark_used(i);
            used_pages++;
            return (uintptr_t)(i << PAGE_SHIFT);
        }
    }
    return 0;
}

void memory_free_page(uintptr_t addr) {
    size_t idx = (size_t)(addr >> PAGE_SHIFT);
    if (idx == 0 || idx >= total_pages) {
        return;
    }
    if (pmm_is_used(idx)) {
        pmm_mark_free(idx);
        if (used_pages > 0) {
            used_pages--;
        }
    }
}

uintptr_t memory_get_heap_start(void) {
    return heap_start;
}

size_t memory_get_used_bytes(void) {
    return used_pages * (size_t)PAGE_SIZE;
}

size_t memory_get_total_bytes(void) {
    return total_pages * (size_t)PAGE_SIZE;
}

