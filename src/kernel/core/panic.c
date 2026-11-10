#include "kernel/core/panic.h"
#include "drivers/video/vga.h"

void kernel_panic(const char* message) {
    uint8_t panic_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED);

    vga_put_character('\n');
    vga_write_string_with_color(" [KERNEL PANIC] ", panic_color);
    vga_put_character(' ');
    vga_write_string_with_color(message, panic_color);
    vga_put_character('\n');

    while (1) {
        __asm__ volatile("cli; hlt");
    }
}
