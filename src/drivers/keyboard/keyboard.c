#include "drivers/keyboard/keyboard.h"
#include "drivers/keyboard/scancode.h"
#include "drivers/input/input.h"
#include "drivers/video/framebuffer.h"
#include "arch/x86_64/cpu/io.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_COMMAND_PORT 0x64
#define KEYBOARD_STATUS_OUTPUT_BUFFER_FULL 0x01
#define KEYBOARD_STATUS_INPUT_BUFFER_FULL 0x02

static bool left_shift_pressed = false;
static bool right_shift_pressed = false;
static bool shift_pressed = false;
static bool left_ctrl_pressed = false;
static bool right_ctrl_pressed = false;
static bool ctrl_pressed = false;
static bool caps_lock_active = false;
static bool caps_lock_down = false;
static bool extended_mode = false;

static const char scancode_table_lower[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static const char scancode_table_upper[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

static bool keyboard_handle_scroll(uint8_t scancode) {
    if (scancode == SCANCODE_ARROW_UP) {
        framebuffer_scroll_view_lines(-1);
        return true;
    }
    if (scancode == SCANCODE_ARROW_DOWN) {
        framebuffer_scroll_view_lines(1);
        return true;
    }
    if (scancode == SCANCODE_PAGE_UP || scancode == SCANCODE_F11) {
        framebuffer_scroll_view_page(-1);
        return true;
    }
    if (scancode == SCANCODE_PAGE_DOWN || scancode == SCANCODE_F12) {
        framebuffer_scroll_view_page(1);
        return true;
    }
    if (scancode == SCANCODE_HOME) {
        framebuffer_scroll_view_top();
        return true;
    }
    if (scancode == SCANCODE_END) {
        framebuffer_scroll_view_bottom();
        return true;
    }
    return false;
}

static bool keyboard_handle_shift_scroll(uint8_t scancode) {
    if (scancode == SCANCODE_PAGE_UP || scancode == SCANCODE_ARROW_UP || scancode == SCANCODE_F11) {
        framebuffer_scroll_view_page(-1);
        return true;
    }
    if (scancode == SCANCODE_PAGE_DOWN || scancode == SCANCODE_ARROW_DOWN || scancode == SCANCODE_F12) {
        framebuffer_scroll_view_page(1);
        return true;
    }
    if (scancode == SCANCODE_HOME) {
        framebuffer_scroll_view_top();
        return true;
    }
    if (scancode == SCANCODE_END) {
        framebuffer_scroll_view_bottom();
        return true;
    }
    return false;
}

static bool keyboard_wait_write(void) {
    uint32_t timeout = 100000;
    while ((io_inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_INPUT_BUFFER_FULL) && timeout--) {
        io_wait();
    }
    return (io_inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_INPUT_BUFFER_FULL) == 0;
}

static bool keyboard_wait_read(void) {
    uint32_t timeout = 100000;
    while (!(io_inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_BUFFER_FULL) && timeout--) {
        io_wait();
    }
    return (io_inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_BUFFER_FULL) != 0;
}

static void keyboard_update_leds(void) {
    uint8_t leds = 0;
    if (caps_lock_active) {
        leds |= 0x04;
    }

    if (keyboard_wait_write()) {
        io_outb(KEYBOARD_DATA_PORT, 0xED);
        if (keyboard_wait_read()) {
            uint8_t ack = io_inb(KEYBOARD_DATA_PORT);
            if (ack == 0xFA) {
                if (keyboard_wait_write()) {
                    io_outb(KEYBOARD_DATA_PORT, leds);
                    if (keyboard_wait_read()) {
                        io_inb(KEYBOARD_DATA_PORT);
                    }
                }
            }
        }
    }
}

void keyboard_initialize(void) {
    left_shift_pressed = false;
    right_shift_pressed = false;
    shift_pressed = false;
    left_ctrl_pressed = false;
    right_ctrl_pressed = false;
    ctrl_pressed = false;
    caps_lock_active = false;
    caps_lock_down = false;
    extended_mode = false;

    uint8_t init_status = io_inb(KEYBOARD_STATUS_PORT);
    if (init_status == 0xFF) {
        return;
    }

    uint32_t flush_count = 1000;
    while ((io_inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_BUFFER_FULL) && flush_count--) {
        io_inb(KEYBOARD_DATA_PORT);
    }

    if (keyboard_wait_write()) {
        io_outb(KEYBOARD_COMMAND_PORT, 0x20);
        if (keyboard_wait_read()) {
            uint8_t config = io_inb(KEYBOARD_DATA_PORT);
            config &= ~(1 << 4);
            config |= (1 << 6);
            if (keyboard_wait_write()) {
                io_outb(KEYBOARD_COMMAND_PORT, 0x60);
                if (keyboard_wait_write()) {
                    io_outb(KEYBOARD_DATA_PORT, config);
                }
            }
        }
    }

    if (keyboard_wait_write()) {
        io_outb(KEYBOARD_COMMAND_PORT, 0xAE);
    }

    if (keyboard_wait_write()) {
        io_outb(KEYBOARD_DATA_PORT, 0xF4);
        if (keyboard_wait_read()) {
            io_inb(KEYBOARD_DATA_PORT);
        }
    }

    flush_count = 1000;
    while ((io_inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_BUFFER_FULL) && flush_count--) {
        io_inb(KEYBOARD_DATA_PORT);
    }

    keyboard_update_leds();
}

bool keyboard_has_input(void) {
    uint8_t status = io_inb(KEYBOARD_STATUS_PORT);
    if (status == 0xFF) {
        return false;
    }
    return (status & KEYBOARD_STATUS_OUTPUT_BUFFER_FULL) != 0;
}

char keyboard_read_character(void) {
    uint32_t timeout = 100000;
    while (!keyboard_has_input() && timeout--) {
        __asm__ volatile("pause");
    }

    if (!keyboard_has_input()) {
        return 0;
    }

    uint8_t scancode = io_inb(KEYBOARD_DATA_PORT);

    if (scancode == 0xFA) {
        return 0;
    }

    if (scancode == SCANCODE_EXTENDED) {
        extended_mode = true;
        return 0;
    }

    if (extended_mode) {
        extended_mode = false;
        if (scancode == SCANCODE_LEFT_CTRL) {
            right_ctrl_pressed = true;
            ctrl_pressed = left_ctrl_pressed || right_ctrl_pressed;
            return 0;
        }
        if (scancode == SCANCODE_LEFT_CTRL_RELEASE) {
            right_ctrl_pressed = false;
            ctrl_pressed = left_ctrl_pressed || right_ctrl_pressed;
            return 0;
        }

        if (scancode == SCANCODE_LEFT_SHIFT || scancode == SCANCODE_LEFT_SHIFT_RELEASE ||
            scancode == SCANCODE_RIGHT_SHIFT || scancode == SCANCODE_RIGHT_SHIFT_RELEASE) {
            return 0;
        }

        if (ctrl_pressed && shift_pressed) {
            keyboard_handle_scroll(scancode);
            return 0;
        }

        if (ctrl_pressed) {
            keyboard_handle_scroll(scancode);
            return 0;
        }

        if (shift_pressed) {
            keyboard_handle_shift_scroll(scancode);
            return 0;
        }

        if (scancode >= 0x80) {
            return 0;
        }

        if (scancode == SCANCODE_ARROW_UP) {
            return (char)INPUT_KEY_UP;
        }
        if (scancode == SCANCODE_ARROW_DOWN) {
            return (char)INPUT_KEY_DOWN;
        }
        if (scancode == SCANCODE_ARROW_LEFT) {
            return (char)INPUT_KEY_LEFT;
        }
        if (scancode == SCANCODE_ARROW_RIGHT) {
            return (char)INPUT_KEY_RIGHT;
        }
        if (scancode == SCANCODE_PAGE_UP || scancode == SCANCODE_F11) {
            return (char)INPUT_KEY_PAGE_UP;
        }
        if (scancode == SCANCODE_PAGE_DOWN || scancode == SCANCODE_F12) {
            return (char)INPUT_KEY_PAGE_DOWN;
        }
        return 0;
    }

    if (scancode == SCANCODE_LEFT_CTRL) {
        left_ctrl_pressed = true;
        ctrl_pressed = left_ctrl_pressed || right_ctrl_pressed;
        return 0;
    }

    if (scancode == SCANCODE_LEFT_CTRL_RELEASE) {
        left_ctrl_pressed = false;
        ctrl_pressed = left_ctrl_pressed || right_ctrl_pressed;
        return 0;
    }

    if (scancode == SCANCODE_LEFT_SHIFT) {
        left_shift_pressed = true;
        shift_pressed = left_shift_pressed || right_shift_pressed;
        return 0;
    }

    if (scancode == SCANCODE_RIGHT_SHIFT) {
        right_shift_pressed = true;
        shift_pressed = left_shift_pressed || right_shift_pressed;
        return 0;
    }

    if (scancode == SCANCODE_LEFT_SHIFT_RELEASE) {
        left_shift_pressed = false;
        shift_pressed = left_shift_pressed || right_shift_pressed;
        return 0;
    }

    if (scancode == SCANCODE_RIGHT_SHIFT_RELEASE) {
        right_shift_pressed = false;
        shift_pressed = left_shift_pressed || right_shift_pressed;
        return 0;
    }

    if (ctrl_pressed && shift_pressed) {
        keyboard_handle_scroll(scancode);
        return 0;
    }

    if (ctrl_pressed || shift_pressed) {
        if (scancode == SCANCODE_ARROW_UP) {
            framebuffer_scroll_view_lines(-2);
            return 0;
        }
        if (scancode == SCANCODE_ARROW_DOWN) {
            framebuffer_scroll_view_lines(2);
            return 0;
        }
        keyboard_handle_scroll(scancode);
    }

    if (scancode == SCANCODE_CAPS_LOCK) {
        if (!caps_lock_down) {
            caps_lock_down = true;
            caps_lock_active = !caps_lock_active;
            keyboard_update_leds();
        }
        return 0;
    }

    if (scancode == (SCANCODE_CAPS_LOCK | 0x80)) {
        caps_lock_down = false;
        return 0;
    }

    if (scancode >= 0x80) {
        return 0;
    }

    if (ctrl_pressed) {
        if (framebuffer_get_scroll_offset() > 0) {
            framebuffer_scroll_view_bottom();
        }
        if (scancode < sizeof(scancode_table_lower)) {
            char ch_low = scancode_table_lower[scancode];
            if (ch_low >= 'a' && ch_low <= 'z') {
                return (char)(ch_low - 'a' + 1);
            }
        }
        return 0;
    }

    if (scancode == SCANCODE_PAGE_UP || scancode == SCANCODE_F11) {
        return (char)INPUT_KEY_PAGE_UP;
    }
    if (scancode == SCANCODE_PAGE_DOWN || scancode == SCANCODE_F12) {
        return (char)INPUT_KEY_PAGE_DOWN;
    }
    if (scancode == SCANCODE_ARROW_UP) {
        return (char)INPUT_KEY_UP;
    }
    if (scancode == SCANCODE_ARROW_DOWN) {
        return (char)INPUT_KEY_DOWN;
    }
    if (scancode == SCANCODE_ARROW_LEFT) {
        return (char)INPUT_KEY_LEFT;
    }
    if (scancode == SCANCODE_ARROW_RIGHT) {
        return (char)INPUT_KEY_RIGHT;
    }

    if (scancode < sizeof(scancode_table_lower)) {
        char ch_low = scancode_table_lower[scancode];
        char ch_up = scancode_table_upper[scancode];
        char result;

        if (ch_low >= 'a' && ch_low <= 'z') {
            bool uppercase = shift_pressed ^ caps_lock_active;
            result = uppercase ? ch_up : ch_low;
        } else if (shift_pressed) {
            result = ch_up;
        } else {
            result = ch_low;
        }

        if (result != 0 && framebuffer_get_scroll_offset() > 0) {
            framebuffer_scroll_view_bottom();
        }
        return result;
    }

    return 0;
}

void keyboard_drain(void) {
    while (io_inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_BUFFER_FULL) {
        io_inb(KEYBOARD_DATA_PORT);
    }
}
