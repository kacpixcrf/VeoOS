#ifndef VEOOS_ARCH_X86_64_CPU_IO_H
#define VEOOS_ARCH_X86_64_CPU_IO_H

#include "util/types.h"

uint8_t io_inb(uint16_t port);
void io_outb(uint16_t port, uint8_t value);
uint16_t io_inw(uint16_t port);
void io_outw(uint16_t port, uint16_t value);
uint32_t io_ind(uint16_t port);
void io_outd(uint16_t port, uint32_t value);
void io_wait(void);

static inline uint64_t cpu_rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

#endif
