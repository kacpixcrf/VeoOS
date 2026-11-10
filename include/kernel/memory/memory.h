#ifndef VEOOS_KERNEL_MEMORY_MEMORY_H
#define VEOOS_KERNEL_MEMORY_MEMORY_H

#include "util/types.h"

void memory_initialize(void);
void* memory_alloc(size_t size);
uintptr_t memory_alloc_page(void);
void memory_free_page(uintptr_t addr);
uintptr_t memory_get_heap_start(void);
size_t memory_get_used_bytes(void);
size_t memory_get_total_bytes(void);

#endif
