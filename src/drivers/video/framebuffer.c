#include "drivers/video/framebuffer.h"
#include "drivers/video/font.h"
#include "util/string.h"

#define FB_MAX_COLS 480
#define FB_MAX_ROWS 160

struct fb_cell {
    unsigned char ch;
    uint8_t color;
};

static const uint32_t vga_to_rgb_table[16] = {
    0x0014161B,
    0x003B82F6,
    0x0010B981,
    0x0006B6D4,
    0x00EF4444,
    0x00A855F7,
    0x00F59E0B,
    0x00E2E8F0,
    0x0064748B,
    0x0060A5FA,
    0x0034D399,
    0x0022D3EE,
    0x00F87171,
    0x00C084FC,
    0x00FBBF24,
    0x00F8FAFC
};

static bool fb_active = false;
static uint8_t* fb_address = NULL;
static uint32_t fb_pitch = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint8_t fb_bpp = 0;
static size_t fb_cols = 0;
static size_t fb_rows = 0;
static size_t fb_cursor_col = 0;
static size_t fb_cursor_row = 0;
static uint8_t fb_current_color_entry = 0x07;
static struct fb_cell fb_cells[FB_MAX_ROWS][FB_MAX_COLS];
static struct fb_cell fb_cells_displayed[FB_MAX_ROWS][FB_MAX_COLS];
#define SCROLLBACK_MAX_LINES 512
static struct fb_cell scrollback_buffer[SCROLLBACK_MAX_LINES][FB_MAX_COLS];
static size_t scrollback_count = 0;
static size_t scrollback_head = 0;
static size_t view_scroll_offset = 0;

static void framebuffer_draw_cell_direct(size_t col, size_t row, unsigned char character, uint8_t color) {
    if (col >= fb_cols || row >= fb_rows) {
        return;
    }

    uint32_t fg = vga_to_rgb_table[color & 0x0F];
    uint32_t bg = vga_to_rgb_table[(color >> 4) & 0x0F];
    uint32_t start_x = (uint32_t)(col * FONT_GLYPH_WIDTH);
    uint32_t start_y = (uint32_t)(row * FONT_GLYPH_HEIGHT);
    const uint8_t* glyph = font_get_glyph(character);

    if (fb_bpp == 32) {
        uint64_t bg64 = ((uint64_t)bg << 32) | bg;
        uint64_t fg64 = ((uint64_t)fg << 32) | fg;
        for (uint32_t gy = 0; gy < FONT_GLYPH_HEIGHT; gy++) {
            uint8_t bits = glyph[gy];
            uint64_t* dst = (uint64_t*)(fb_address + (start_y + gy) * fb_pitch + (start_x << 2));
            if (bits == 0) {
                dst[0] = bg64;
                dst[1] = bg64;
                dst[2] = bg64;
                dst[3] = bg64;
            } else if (bits == 0xFF) {
                dst[0] = fg64;
                dst[1] = fg64;
                dst[2] = fg64;
                dst[3] = fg64;
            } else {
                dst[0] = ((uint64_t)((bits & 0x40) ? fg : bg) << 32) | ((bits & 0x80) ? fg : bg);
                dst[1] = ((uint64_t)((bits & 0x10) ? fg : bg) << 32) | ((bits & 0x20) ? fg : bg);
                dst[2] = ((uint64_t)((bits & 0x04) ? fg : bg) << 32) | ((bits & 0x08) ? fg : bg);
                dst[3] = ((uint64_t)((bits & 0x01) ? fg : bg) << 32) | ((bits & 0x02) ? fg : bg);
            }
        }
    } else if (fb_bpp == 24) {
        for (uint32_t gy = 0; gy < FONT_GLYPH_HEIGHT; gy++) {
            uint8_t* dst = fb_address + (start_y + gy) * fb_pitch + start_x * 3;
            uint8_t bits = glyph[gy];
            for (uint32_t gx = 0; gx < 8; gx++) {
                uint32_t c = (bits & (0x80 >> gx)) ? fg : bg;
                dst[gx * 3 + 0] = (uint8_t)(c & 0xFF);
                dst[gx * 3 + 1] = (uint8_t)((c >> 8) & 0xFF);
                dst[gx * 3 + 2] = (uint8_t)((c >> 16) & 0xFF);
            }
        }
    }
}

static void framebuffer_draw_cell(size_t col, size_t row, unsigned char character, uint8_t color) {
    if (col >= fb_cols || row >= fb_rows) {
        return;
    }

    fb_cells[row][col].ch = character;
    fb_cells[row][col].color = color;

    if (view_scroll_offset == 0) {
        if (fb_cells_displayed[row][col].ch != character || fb_cells_displayed[row][col].color != color) {
            framebuffer_draw_cell_direct(col, row, character, color);
            fb_cells_displayed[row][col].ch = character;
            fb_cells_displayed[row][col].color = color;
        }
    }
}

void framebuffer_draw_cursor(bool visible) {
    if (!fb_active || fb_cursor_col >= fb_cols || fb_cursor_row >= fb_rows) {
        return;
    }

    if (!visible) {
        framebuffer_draw_cell_direct(fb_cursor_col, fb_cursor_row,
                                     fb_cells[fb_cursor_row][fb_cursor_col].ch,
                                     fb_cells[fb_cursor_row][fb_cursor_col].color);
        return;
    }

    uint8_t cell_color = fb_cells[fb_cursor_row][fb_cursor_col].color;
    uint32_t color = vga_to_rgb_table[cell_color & 0x0F];
    uint32_t start_x = (uint32_t)(fb_cursor_col * FONT_GLYPH_WIDTH);
    uint32_t y1 = (uint32_t)(fb_cursor_row * FONT_GLYPH_HEIGHT + (FONT_GLYPH_HEIGHT - 2));
    uint32_t y2 = (uint32_t)(fb_cursor_row * FONT_GLYPH_HEIGHT + (FONT_GLYPH_HEIGHT - 1));

    if (fb_bpp == 32) {
        uint64_t col64 = ((uint64_t)color << 32) | color;
        uint64_t* line1 = (uint64_t*)(fb_address + y1 * fb_pitch + (start_x << 2));
        uint64_t* line2 = (uint64_t*)(fb_address + y2 * fb_pitch + (start_x << 2));
        line1[0] = col64;
        line1[1] = col64;
        line1[2] = col64;
        line1[3] = col64;
        line2[0] = col64;
        line2[1] = col64;
        line2[2] = col64;
        line2[3] = col64;
    } else if (fb_bpp == 24) {
        uint8_t* line1 = fb_address + y1 * fb_pitch + start_x * 3;
        uint8_t* line2 = fb_address + y2 * fb_pitch + start_x * 3;
        for (size_t i = 0; i < FONT_GLYPH_WIDTH; i++) {
            line1[i * 3 + 0] = (uint8_t)(color & 0xFF);
            line1[i * 3 + 1] = (uint8_t)((color >> 8) & 0xFF);
            line1[i * 3 + 2] = (uint8_t)((color >> 16) & 0xFF);
            line2[i * 3 + 0] = (uint8_t)(color & 0xFF);
            line2[i * 3 + 1] = (uint8_t)((color >> 8) & 0xFF);
            line2[i * 3 + 2] = (uint8_t)((color >> 16) & 0xFF);
        }
    }
}

static const struct fb_cell* framebuffer_get_view_line(size_t row, size_t offset) {
    if (row >= fb_rows) {
        return NULL;
    }
    if (row >= offset) {
        return fb_cells[row - offset];
    }
    size_t dist = offset - row;
    if (dist > scrollback_count) {
        return NULL;
    }
    size_t idx = (scrollback_head + SCROLLBACK_MAX_LINES - dist) % SCROLLBACK_MAX_LINES;
    return scrollback_buffer[idx];
}

static void framebuffer_sync_view(void) {
    framebuffer_draw_cursor(false);
    for (size_t r = 0; r < fb_rows; r++) {
        const struct fb_cell* line = framebuffer_get_view_line(r, view_scroll_offset);
        for (size_t c = 0; c < fb_cols; c++) {
            unsigned char ch = line ? line[c].ch : ' ';
            uint8_t color = line ? line[c].color : fb_current_color_entry;
            if (fb_cells_displayed[r][c].ch != ch || fb_cells_displayed[r][c].color != color) {
                framebuffer_draw_cell_direct(c, r, ch, color);
                fb_cells_displayed[r][c].ch = ch;
                fb_cells_displayed[r][c].color = color;
            }
        }
    }
    if (view_scroll_offset == 0) {
        framebuffer_draw_cursor(true);
    }
}

static void framebuffer_scroll(void) {
    framebuffer_draw_cursor(false);

    for (size_t c = 0; c < fb_cols; c++) {
        scrollback_buffer[scrollback_head][c] = fb_cells[0][c];
    }
    scrollback_head = (scrollback_head + 1) % SCROLLBACK_MAX_LINES;
    if (scrollback_count < SCROLLBACK_MAX_LINES) {
        scrollback_count++;
    }
    if (view_scroll_offset > 0 && view_scroll_offset < scrollback_count) {
        view_scroll_offset++;
    }

    memory_copy(fb_cells[0], fb_cells[1], (fb_rows - 1) * sizeof(fb_cells[0]));
    for (size_t c = 0; c < fb_cols; c++) {
        fb_cells[fb_rows - 1][c].ch = ' ';
        fb_cells[fb_rows - 1][c].color = fb_current_color_entry;
    }

    if (view_scroll_offset == 0) {
        framebuffer_sync_view();
    }

    fb_cursor_row = fb_rows - 1;
    if (view_scroll_offset == 0) {
        framebuffer_draw_cursor(true);
    }
}

void framebuffer_scroll_view_lines(int delta) {
    if (scrollback_count == 0 && view_scroll_offset == 0) {
        return;
    }
    size_t old_offset = view_scroll_offset;

    if (delta < 0) {
        size_t steps = (size_t)(-delta);
        if (view_scroll_offset + steps > scrollback_count) {
            view_scroll_offset = scrollback_count;
        } else {
            view_scroll_offset += steps;
        }
    } else if (delta > 0) {
        size_t steps = (size_t)delta;
        if (view_scroll_offset > steps) {
            view_scroll_offset -= steps;
        } else {
            view_scroll_offset = 0;
        }
    }

    if (view_scroll_offset != old_offset) {
        framebuffer_sync_view();
    }
}

void framebuffer_scroll_view_page(int direction) {
    if (scrollback_count == 0 && view_scroll_offset == 0) {
        return;
    }
    size_t page_step = (fb_rows > 6) ? (fb_rows / 2) : 1;
    size_t old_offset = view_scroll_offset;

    if (direction < 0) {
        view_scroll_offset += page_step;
        if (view_scroll_offset > scrollback_count) {
            view_scroll_offset = scrollback_count;
        }
    } else {
        if (view_scroll_offset > page_step) {
            view_scroll_offset -= page_step;
        } else {
            view_scroll_offset = 0;
        }
    }

    if (view_scroll_offset != old_offset) {
        framebuffer_sync_view();
    }
}

void framebuffer_scroll_view_top(void) {
    if (view_scroll_offset != scrollback_count) {
        view_scroll_offset = scrollback_count;
        framebuffer_sync_view();
    }
}

void framebuffer_scroll_view_bottom(void) {
    if (view_scroll_offset != 0) {
        view_scroll_offset = 0;
        framebuffer_sync_view();
    }
}

size_t framebuffer_get_scroll_offset(void) {
    return view_scroll_offset;
}

bool framebuffer_initialize(const struct multiboot_info* mb_info) {
    if (mb_info == NULL) {
        return false;
    }
    if (!(mb_info->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)) {
        return false;
    }
    if (mb_info->framebuffer_addr == 0) {
        return false;
    }
    if (mb_info->framebuffer_width == 0 || mb_info->framebuffer_height == 0) {
        return false;
    }
    if (mb_info->framebuffer_bpp != 32 && mb_info->framebuffer_bpp != 24) {
        return false;
    }

    fb_address = (uint8_t*)mb_info->framebuffer_addr;
    fb_pitch = mb_info->framebuffer_pitch;
    if (fb_pitch == 0) {
        fb_pitch = mb_info->framebuffer_width * (mb_info->framebuffer_bpp >> 3);
    }
    fb_width = mb_info->framebuffer_width;
    fb_height = mb_info->framebuffer_height;
    fb_bpp = mb_info->framebuffer_bpp;
    fb_cols = fb_width / FONT_GLYPH_WIDTH;
    fb_rows = fb_height / FONT_GLYPH_HEIGHT;
    if (fb_cols > FB_MAX_COLS) {
        fb_cols = FB_MAX_COLS;
    }
    if (fb_rows > FB_MAX_ROWS) {
        fb_rows = FB_MAX_ROWS;
    }
    fb_cursor_col = 0;
    fb_cursor_row = 0;
    fb_current_color_entry = 0x07;
    scrollback_count = 0;
    scrollback_head = 0;
    view_scroll_offset = 0;
    fb_active = true;

    for (size_t r = 0; r < fb_rows; r++) {
        for (size_t c = 0; c < fb_cols; c++) {
            fb_cells[r][c].ch = ' ';
            fb_cells[r][c].color = fb_current_color_entry;
        }
    }
    memory_set(fb_cells_displayed, 0xFF, sizeof(fb_cells_displayed));

    return true;
}

bool framebuffer_is_active(void) {
    return fb_active;
}

void framebuffer_clear(void) {
    if (!fb_active) {
        return;
    }

    scrollback_count = 0;
    scrollback_head = 0;
    view_scroll_offset = 0;

    for (size_t r = 0; r < fb_rows; r++) {
        for (size_t c = 0; c < fb_cols; c++) {
            fb_cells[r][c].ch = ' ';
            fb_cells[r][c].color = fb_current_color_entry;
            fb_cells_displayed[r][c].ch = ' ';
            fb_cells_displayed[r][c].color = fb_current_color_entry;
        }
    }

    uint32_t bg = vga_to_rgb_table[(fb_current_color_entry >> 4) & 0x0F];

    if (fb_bpp == 32) {
        uint64_t bg64 = ((uint64_t)bg << 32) | (uint64_t)bg;
        size_t qwords = fb_width >> 1;
        bool odd = (fb_width & 1) != 0;
        for (size_t y = 0; y < fb_height; y++) {
            uint64_t* line64 = (uint64_t*)(fb_address + y * fb_pitch);
            for (size_t x = 0; x < qwords; x++) {
                line64[x] = bg64;
            }
            if (odd) {
                ((uint32_t*)line64)[fb_width - 1] = bg;
            }
        }
    } else if (fb_bpp == 24) {
        for (size_t y = 0; y < fb_height; y++) {
            uint8_t* line = fb_address + y * fb_pitch;
            for (size_t x = 0; x < fb_width; x++) {
                line[x * 3 + 0] = (uint8_t)(bg & 0xFF);
                line[x * 3 + 1] = (uint8_t)((bg >> 8) & 0xFF);
                line[x * 3 + 2] = (uint8_t)((bg >> 16) & 0xFF);
            }
        }
    }

    fb_cursor_col = 0;
    fb_cursor_row = 0;
    framebuffer_draw_cursor(true);
}

void framebuffer_put_character(char character) {
    if (!fb_active) {
        return;
    }

    if (view_scroll_offset > 0) {
        framebuffer_scroll_view_bottom();
    }

    framebuffer_draw_cursor(false);

    if (character == '\n') {
        fb_cursor_col = 0;
        fb_cursor_row++;
    } else if (character == '\r') {
        fb_cursor_col = 0;
    } else if (character == '\t') {
        fb_cursor_col = (fb_cursor_col + 4) & ~3;
        if (fb_cursor_col >= fb_cols) {
            fb_cursor_col = 0;
            fb_cursor_row++;
        }
    } else if (character == '\b') {
        framebuffer_backspace();
        return;
    } else {
        framebuffer_draw_cell(fb_cursor_col, fb_cursor_row, (unsigned char)character, fb_current_color_entry);
        fb_cursor_col++;
        if (fb_cursor_col >= fb_cols) {
            fb_cursor_col = 0;
            fb_cursor_row++;
        }
    }

    if (fb_cursor_row >= fb_rows) {
        framebuffer_scroll();
    }
}

void framebuffer_put_character_at(char character, uint8_t color, size_t column, size_t row) {
    if (!fb_active || column >= fb_cols || row >= fb_rows) {
        return;
    }

    framebuffer_draw_cell(column, row, (unsigned char)character, color);
}

void framebuffer_backspace(void) {
    if (!fb_active) {
        return;
    }

    framebuffer_draw_cursor(false);

    if (fb_cursor_col > 0) {
        fb_cursor_col--;
        framebuffer_draw_cell(fb_cursor_col, fb_cursor_row, ' ', fb_current_color_entry);
    } else if (fb_cursor_row > 0) {
        fb_cursor_row--;
        fb_cursor_col = fb_cols - 1;
        framebuffer_draw_cell(fb_cursor_col, fb_cursor_row, ' ', fb_current_color_entry);
    }

    framebuffer_draw_cursor(true);
}

void framebuffer_set_cursor_position(size_t column, size_t row) {
    if (!fb_active || column >= fb_cols || row >= fb_rows) {
        return;
    }

    framebuffer_draw_cursor(false);
    fb_cursor_col = column;
    fb_cursor_row = row;
    framebuffer_draw_cursor(true);
}

void framebuffer_set_current_color(uint8_t vga_color_entry) {
    fb_current_color_entry = vga_color_entry;
}

size_t framebuffer_get_cursor_x(void) {
    return fb_cursor_col;
}

size_t framebuffer_get_cursor_y(void) {
    return fb_cursor_row;
}

size_t framebuffer_get_width(void) {
    return fb_cols;
}

size_t framebuffer_get_height(void) {
    return fb_rows;
}

uint32_t framebuffer_get_pixel_width(void) {
    return fb_width;
}

uint32_t framebuffer_get_pixel_height(void) {
    return fb_height;
}

uint8_t framebuffer_get_bpp(void) {
    return fb_bpp;
}

uint32_t framebuffer_color_from_vga(uint8_t vga_color) {
    return vga_to_rgb_table[vga_color & 0x0F];
}
