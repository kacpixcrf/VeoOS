#ifndef VEOOS_DRIVERS_VIDEO_VGA_H
#define VEOOS_DRIVERS_VIDEO_VGA_H

#include "util/types.h"
#include "kernel/core/multiboot.h"

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return (uint8_t)(fg | (bg << 4));
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)(uc | ((uint16_t)color << 8));
}

void vga_initialize(const struct multiboot_info* mb_info);
void vga_clear(void);
void vga_put_character(char character);
void vga_put_character_at(char character, uint8_t color, size_t column, size_t row);
void vga_write_string(const char* string);
void vga_write_string_with_color(const char* string, uint8_t color);
void vga_write_color_block(uint8_t color);
void vga_backspace(void);
size_t vga_get_cursor_x(void);
size_t vga_get_cursor_y(void);
size_t vga_get_width(void);
size_t vga_get_height(void);
void vga_set_cursor_position(size_t column, size_t row);
bool vga_is_framebuffer(void);

#endif
