#ifndef VEOOS_DRIVERS_VIDEO_FRAMEBUFFER_H
#define VEOOS_DRIVERS_VIDEO_FRAMEBUFFER_H

#include "util/types.h"
#include "kernel/core/multiboot.h"

bool framebuffer_initialize(const struct multiboot_info* mb_info);
bool framebuffer_is_active(void);
void framebuffer_clear(void);
void framebuffer_put_character(char character);
void framebuffer_put_character_at(char character, uint8_t color, size_t column, size_t row);
void framebuffer_backspace(void);
void framebuffer_draw_cursor(bool visible);
void framebuffer_set_cursor_position(size_t column, size_t row);
void framebuffer_set_current_color(uint8_t vga_color_entry);
size_t framebuffer_get_cursor_x(void);
size_t framebuffer_get_cursor_y(void);
size_t framebuffer_get_width(void);
size_t framebuffer_get_height(void);
uint32_t framebuffer_get_pixel_width(void);
uint32_t framebuffer_get_pixel_height(void);
uint8_t framebuffer_get_bpp(void);
uint32_t framebuffer_color_from_vga(uint8_t vga_color);
void framebuffer_scroll_view_lines(int delta);
void framebuffer_scroll_view_page(int direction);
void framebuffer_scroll_view_top(void);
void framebuffer_scroll_view_bottom(void);
size_t framebuffer_get_scroll_offset(void);

#endif
