#include "drivers/timer/timer.h"
#include "arch/x86_64/cpu/io.h"

#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND_PORT  0x43
#define PIT_CONTROL_PORT  0x61

static uint64_t tsc_freq_hz = 0;
static uint64_t tsc_per_ms = 0;
static uint64_t tsc_per_us = 0;
static uint64_t boot_tsc = 0;

static void pit_wait_ticks(uint16_t ticks) {
    uint8_t control = io_inb(PIT_CONTROL_PORT);
    control &= 0xFC;
    io_outb(PIT_CONTROL_PORT, control);
    io_outb(PIT_COMMAND_PORT, 0xB0);
    io_outb(PIT_CHANNEL2_DATA, (uint8_t)(ticks & 0xFF));
    io_outb(PIT_CHANNEL2_DATA, (uint8_t)((ticks >> 8) & 0xFF));
    io_outb(PIT_CONTROL_PORT, control | 0x01);

    uint32_t timeout = 50000000;
    while ((io_inb(PIT_CONTROL_PORT) & 0x20) == 0 && --timeout > 0) {
        __asm__ volatile("pause");
    }

    io_outb(PIT_CONTROL_PORT, control);
}

void timer_initialize(void) {
    boot_tsc = cpu_rdtsc();

    uint64_t t0 = cpu_rdtsc();
    pit_wait_ticks(11932);
    uint64_t t1 = cpu_rdtsc();

    if (t1 > t0) {
        uint64_t diff = t1 - t0;
        tsc_per_ms = diff / 10;
        tsc_freq_hz = tsc_per_ms * 1000;
        tsc_per_us = tsc_per_ms / 1000;
        if (tsc_per_us == 0) {
            tsc_per_us = 1;
        }
    }

    if (tsc_per_ms < 100000) {
        tsc_freq_hz = 2500000000ULL;
        tsc_per_ms = 2500000;
        tsc_per_us = 2500;
    }
}

void timer_sleep_ms(uint32_t ms) {
    if (ms == 0) {
        return;
    }
    if (tsc_per_ms == 0) {
        timer_initialize();
    }
    uint64_t start = cpu_rdtsc();
    uint64_t target = (uint64_t)ms * tsc_per_ms;
    while (cpu_rdtsc() - start < target) {
        __asm__ volatile("pause");
    }
}

void timer_sleep_us(uint32_t us) {
    if (us == 0) {
        return;
    }
    if (tsc_per_us == 0) {
        timer_initialize();
    }
    uint64_t start = cpu_rdtsc();
    uint64_t target = (uint64_t)us * tsc_per_us;
    while (cpu_rdtsc() - start < target) {
        __asm__ volatile("pause");
    }
}

uint64_t timer_get_uptime_ms(void) {
    if (tsc_per_ms == 0) {
        return 0;
    }
    return (cpu_rdtsc() - boot_tsc) / tsc_per_ms;
}

uint64_t timer_get_ticks(void) {
    return cpu_rdtsc();
}

uint64_t timer_get_cpu_freq_hz(void) {
    return tsc_freq_hz;
}
