#include "drivers/video/vga.h"
#include "drivers/video/framebuffer.h"
#include "drivers/serial/serial.h"
#include "util/string.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY_ADDRESS 0xB8000

static bool display_is_framebuffer = false;
static size_t vga_row;
static size_t vga_column;
static uint8_t vga_current_color = 0x07;
static volatile uint16_t* vga_buffer;

static void vga_scroll(void) {
    volatile uint16_t* dest = vga_buffer;
    volatile uint16_t* src = vga_buffer + VGA_WIDTH;
    size_t count = VGA_WIDTH * (VGA_HEIGHT - 1);

    for (size_t i = 0; i < count; i++) {
        dest[i] = src[i];
    }

    uint16_t blank = vga_entry(' ', vga_current_color);
    for (size_t col = 0; col < VGA_WIDTH; col++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = blank;
    }

    vga_row = VGA_HEIGHT - 1;
}

void vga_initialize(const struct multiboot_info* mb_info) {
    vga_row = 0;
    vga_column = 0;
    vga_current_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    if (framebuffer_initialize(mb_info)) {
        display_is_framebuffer = true;
        framebuffer_set_current_color(vga_current_color);
        vga_clear();
        return;
    }

    display_is_framebuffer = false;
    vga_buffer = (volatile uint16_t*)VGA_MEMORY_ADDRESS;
    vga_clear();
}

void vga_clear(void) {
    if (display_is_framebuffer) {
        framebuffer_clear();
        return;
    }

    uint16_t blank = vga_entry(' ', vga_current_color);
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = blank;
        }
    }
    vga_row = 0;
    vga_column = 0;
}

void vga_put_character_at(char character, uint8_t color, size_t column, size_t row) {
    if (display_is_framebuffer) {
        framebuffer_put_character_at(character, color, column, row);
        return;
    }

    if (column >= VGA_WIDTH || row >= VGA_HEIGHT) {
        return;
    }
    vga_buffer[row * VGA_WIDTH + column] = vga_entry((unsigned char)character, color);
}

void vga_put_character(char character) {
    if (character == '\b') {
        vga_backspace();
        return;
    }

    if (display_is_framebuffer) {
        framebuffer_put_character(character);
        return;
    }

    if (character == '\n') {
        vga_column = 0;
        vga_row++;
    } else if (character == '\r') {
        vga_column = 0;
    } else if (character == '\t') {
        vga_column = (vga_column + 4) & ~3;
        if (vga_column >= VGA_WIDTH) {
            vga_column = 0;
            vga_row++;
        }
    } else {
        vga_put_character_at(character, vga_current_color, vga_column, vga_row);
        vga_column++;
        if (vga_column >= VGA_WIDTH) {
            vga_column = 0;
            vga_row++;
        }
    }

    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
    }
}

void vga_backspace(void) {
    if (display_is_framebuffer) {
        framebuffer_backspace();
        return;
    }

    if (vga_column > 0) {
        vga_column--;
        vga_put_character_at(' ', vga_current_color, vga_column, vga_row);
    } else if (vga_row > 0) {
        vga_row--;
        vga_column = VGA_WIDTH - 1;
        vga_put_character_at(' ', vga_current_color, vga_column, vga_row);
    }
}

void vga_write_string(const char* string) {
    while (*string != '\0') {
        vga_put_character(*string);
        string++;
    }
}

static const char* const ansi_color_map[16] = {
    "\033[30m",
    "\033[34m",
    "\033[32m",
    "\033[36m",
    "\033[31m",
    "\033[35m",
    "\033[33m",
    "\033[37m",
    "\033[90m",
    "\033[94m",
    "\033[92m",
    "\033[96m",
    "\033[91m",
    "\033[95m",
    "\033[93m",
    "\033[97m"
};

void vga_write_string_with_color(const char* string, uint8_t color) {
    uint8_t old_color = vga_current_color;
    vga_current_color = color;
    if (display_is_framebuffer) {
        framebuffer_set_current_color(color);
    }
    serial_write_string(ansi_color_map[color & 0x0F]);
    vga_write_string(string);
    vga_current_color = old_color;
    if (display_is_framebuffer) {
        framebuffer_set_current_color(old_color);
    }
    serial_write_string(ansi_color_map[old_color & 0x0F]);
}

void vga_write_color_block(uint8_t color) {
    uint8_t old_color = vga_current_color;
    vga_current_color = vga_entry_color(color, color);
    if (display_is_framebuffer) {
        framebuffer_set_current_color(vga_current_color);
    }
    serial_write_string(ansi_color_map[color & 0x0F]);
    vga_put_character(' ');
    vga_put_character(' ');
    vga_current_color = old_color;
    if (display_is_framebuffer) {
        framebuffer_set_current_color(old_color);
    }
    serial_write_string(ansi_color_map[old_color & 0x0F]);
}

size_t vga_get_cursor_x(void) {
    if (display_is_framebuffer) {
        return framebuffer_get_cursor_x();
    }
    return vga_column;
}

size_t vga_get_cursor_y(void) {
    if (display_is_framebuffer) {
        return framebuffer_get_cursor_y();
    }
    return vga_row;
}

size_t vga_get_width(void) {
    if (display_is_framebuffer) {
        return framebuffer_get_width();
    }
    return VGA_WIDTH;
}

size_t vga_get_height(void) {
    if (display_is_framebuffer) {
        return framebuffer_get_height();
    }
    return VGA_HEIGHT;
}

void vga_set_cursor_position(size_t column, size_t row) {
    if (display_is_framebuffer) {
        framebuffer_set_cursor_position(column, row);
        return;
    }

    if (column < VGA_WIDTH && row < VGA_HEIGHT) {
        vga_column = column;
        vga_row = row;
    }
}

bool vga_is_framebuffer(void) {
    return display_is_framebuffer;
}
