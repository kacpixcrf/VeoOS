#include "drivers/input/input.h"
#include "drivers/input/uefi_input.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/serial/serial.h"
#include "drivers/video/framebuffer.h"
#include "drivers/timer/timer.h"

void input_initialize(void) {
    uefi_input_initialize();
    if (!uefi_input_is_available()) {
        keyboard_initialize();
    }
}

bool input_has_character(void) {
    if (uefi_input_is_available()) {
        return uefi_input_has_character() || serial_has_input();
    }
    return keyboard_has_input() || serial_has_input();
}

char input_read_character(void) {
    bool last_visible = true;
    if (framebuffer_get_scroll_offset() == 0) {
        framebuffer_draw_cursor(true);
    }

    while (true) {
        if (uefi_input_is_available()) {
            char uefi_ch = uefi_input_try_read_character();
            if (uefi_ch != 0) {
                framebuffer_draw_cursor(false);
                return uefi_ch;
            }
        } else {
            if (keyboard_has_input()) {
                char ch = keyboard_read_character();
                if (ch != 0) {
                    framebuffer_draw_cursor(false);
                    return ch;
                }
            }
        }

        if (serial_has_input()) {
            char ch = serial_read_character();
            framebuffer_draw_cursor(false);
            if (ch == '\r') {
                return '\n';
            }
            if (ch == 127) {
                return '\b';
            }
            return ch;
        }

        uint64_t ms = timer_get_uptime_ms();
        bool visible = ((ms / 500) & 1) == 0;
        if (visible != last_visible) {
            last_visible = visible;
            if (framebuffer_get_scroll_offset() == 0) {
                framebuffer_draw_cursor(visible);
            }
        }

        __asm__ volatile("pause");
    }
}

char input_try_read_character(void) {
    if (uefi_input_is_available()) {
        char uefi_ch = uefi_input_try_read_character();
        if (uefi_ch != 0) {
            return uefi_ch;
        }
    } else {
        while (keyboard_has_input()) {
            char ch = keyboard_read_character();
            if (ch != 0) {
                return ch;
            }
        }
    }

    if (serial_has_input()) {
        char ch = serial_read_character();
        if (ch == '\r') {
            return '\n';
        }
        if (ch == 127) {
            return '\b';
        }
        return ch;
    }

    return 0;
}
