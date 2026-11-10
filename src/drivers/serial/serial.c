#include "drivers/serial/serial.h"
#include "arch/x86_64/cpu/io.h"

#define COM1_PORT 0x3F8

static bool serial_present = false;

void serial_initialize(void) {
    io_outb(COM1_PORT + 1, 0x00);
    io_outb(COM1_PORT + 3, 0x80);
    io_outb(COM1_PORT + 0, 0x03);
    io_outb(COM1_PORT + 1, 0x00);
    io_outb(COM1_PORT + 3, 0x03);
    io_outb(COM1_PORT + 2, 0xC7);
    io_outb(COM1_PORT + 4, 0x1E);
    io_outb(COM1_PORT + 0, 0xAE);
    if (io_inb(COM1_PORT + 0) != 0xAE) {
        serial_present = false;
        return;
    }
    io_outb(COM1_PORT + 4, 0x0F);
    serial_present = true;
}

void serial_write_character(char character) {
    if (!serial_present) {
        return;
    }
    uint32_t timeout = 50;
    while ((io_inb(COM1_PORT + 5) & 0x20) == 0 && timeout > 0) {
        timeout--;
        __asm__ volatile("pause");
    }
    if (timeout > 0) {
        io_outb(COM1_PORT, (uint8_t)character);
    }
}

void serial_write_string(const char* string) {
    if (!serial_present) {
        return;
    }
    while (*string != '\0') {
        if (*string == '\n') {
            serial_write_character('\r');
        }
        serial_write_character(*string);
        string++;
    }
}

bool serial_has_input(void) {
    if (!serial_present) {
        return false;
    }
    return (io_inb(COM1_PORT + 5) & 1) != 0;
}

char serial_read_character(void) {
    if (!serial_present) {
        return 0;
    }
    uint32_t timeout = 10;
    while (!serial_has_input() && timeout > 0) {
        timeout--;
        __asm__ volatile("pause");
    }
    if (serial_has_input()) {
        return (char)io_inb(COM1_PORT);
    }
    return 0;
}
