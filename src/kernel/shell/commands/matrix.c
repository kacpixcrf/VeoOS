#include "kernel/shell/commands/matrix.h"
#include "kernel/shell/commands.h"
#include "drivers/video/vga.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/input.h"
#include "drivers/timer/timer.h"
#include "util/types.h"

struct matrix_drop {
    int y;
    int length;
    int speed;
    int tick;
    bool active;
};

static uint32_t matrix_rand_state = 123456789;
static uint32_t matrix_rand(void) {
    matrix_rand_state = matrix_rand_state * 1103515245 + 12345;
    return (matrix_rand_state >> 16) & 0x7FFF;
}

static char matrix_rand_char(void) {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz@#$%&*+-=<>/?!";
    return charset[matrix_rand() % (sizeof(charset) - 1)];
}

static void handler_matrix(const char* args) {
    (void)args;
    size_t cols = vga_get_width();
    size_t rows = vga_get_height();
    if (cols > 512) {
        cols = 512;
    }
    if (rows > 160) {
        rows = 160;
    }

    while (input_try_read_character() != 0) {
    }

    static struct matrix_drop drops[512];
    matrix_rand_state = (uint32_t)timer_get_ticks() ^ 0x5DEECE66D;

    for (size_t x = 0; x < cols; x++) {
        drops[x].active = (matrix_rand() % 3) == 0;
        drops[x].y = -(int)(matrix_rand() % (rows > 0 ? rows : 1));
        drops[x].length = 6 + (matrix_rand() % 16);
        drops[x].speed = 1 + (matrix_rand() % 3);
        drops[x].tick = 0;
    }

    vga_clear();
    framebuffer_draw_cursor(false);

    while (true) {
        if (input_try_read_character() != 0) {
            vga_clear();
            framebuffer_draw_cursor(true);
            return;
        }

        for (size_t x = 0; x < cols; x++) {
            if (!drops[x].active) {
                if ((matrix_rand() % 20) == 0) {
                    drops[x].active = true;
                    drops[x].y = 0;
                    drops[x].length = 6 + (matrix_rand() % 16);
                    drops[x].speed = 1 + (matrix_rand() % 3);
                    drops[x].tick = 0;
                }
                continue;
            }

            drops[x].tick++;
            if (drops[x].tick < drops[x].speed) {
                continue;
            }
            drops[x].tick = 0;

            int y = drops[x].y;
            int tail = y - drops[x].length;

            if (tail >= 0 && tail < (int)rows) {
                vga_put_character_at(' ', vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_BLACK), x, (size_t)tail);
            }

            if (y - 3 >= 0 && y - 3 < (int)rows) {
                vga_put_character_at(matrix_rand_char(), vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK), x, (size_t)(y - 3));
            }

            if (y - 2 >= 0 && y - 2 < (int)rows) {
                vga_put_character_at(matrix_rand_char(), vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK), x, (size_t)(y - 2));
            }

            if (y - 1 >= 0 && y - 1 < (int)rows) {
                vga_put_character_at(matrix_rand_char(), vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK), x, (size_t)(y - 1));
            }

            if (y >= 0 && y < (int)rows) {
                vga_put_character_at(matrix_rand_char(), vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK), x, (size_t)y);
            }

            if (drops[x].length > 4) {
                int rand_offset = 3 + (matrix_rand() % (drops[x].length - 3));
                int my = drops[x].y - rand_offset;
                if (my >= 0 && my < (int)rows) {
                    vga_put_character_at(matrix_rand_char(), vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK), x, (size_t)my);
                }
            }

            drops[x].y++;

            if (tail >= (int)rows) {
                drops[x].active = false;
                drops[x].y = -(int)(matrix_rand() % 8);
                drops[x].length = 6 + (matrix_rand() % 16);
                drops[x].speed = 1 + (matrix_rand() % 3);
                drops[x].tick = 0;
            }
        }

        for (int d = 0; d < 5; d++) {
            timer_sleep_ms(10);
            if (input_try_read_character() != 0) {
                vga_clear();
                framebuffer_draw_cursor(true);
                return;
            }
        }
    }
}

void commands_matrix_register(void) {
    commands_register(
        "matrix",
        NULL,
        "digital rain animation screen",
        "matrix",
        "  Action:\n    Cascading green digital rain screen. Press any key to exit.",
        handler_matrix
    );
}
