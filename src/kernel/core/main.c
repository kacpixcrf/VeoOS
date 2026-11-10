#include "kernel/core/kernel.h"
#include "kernel/core/multiboot.h"
#include "kernel/memory/memory.h"
#include "kernel/fs/ramfs.h"
#include "kernel/system/info.h"
#include "kernel/shell/shell.h"
#include "drivers/video/vga.h"
#include "drivers/serial/serial.h"
#include "drivers/input/input.h"
#include "drivers/timer/timer.h"

static const struct multiboot_info* kernel_mb_info = NULL;

const struct multiboot_info* kernel_get_multiboot_info(void) {
    return kernel_mb_info;
}

void kernel_main(const struct multiboot_info* mb_info) {
    kernel_mb_info = mb_info;
    serial_initialize();
    vga_initialize(mb_info);
    timer_initialize();
    memory_initialize();
    input_initialize();
    ramfs_initialize();

    system_info_display();

    shell_initialize();
    shell_run();

    while (1) {
        __asm__ volatile("hlt");
    }
}
