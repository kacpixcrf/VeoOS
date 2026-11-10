#include "kernel/shell/commands/system.h"
#include "kernel/shell/commands.h"
#include "kernel/system/info.h"
#include "kernel/core/version.h"
#include "kernel/core/user.h"
#include "arch/x86_64/cpu/cpuid.h"
#include "drivers/video/vga.h"
#include "util/string.h"

static void handler_info(const char* args) {
    (void)args;
    system_info_display();
}

static void handler_uname(const char* args) {
    char token[32];
    const char* stream = args;

    bool flag_all = false;
    bool flag_kernel = false;
    bool flag_machine = false;
    bool flag_release = false;
    bool has_any_flag = false;

    uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    uint8_t text_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    while (string_next_token(&stream, token, sizeof(token))) {
        has_any_flag = true;
        if (string_compare(token, "-a") == 0) {
            flag_all = true;
        } else if (string_compare(token, "-s") == 0) {
            flag_kernel = true;
        } else if (string_compare(token, "-m") == 0 || string_compare(token, "-p") == 0) {
            flag_machine = true;
        } else if (string_compare(token, "-r") == 0 || string_compare(token, "-v") == 0) {
            flag_release = true;
        } else {
            vga_write_string_with_color("uname: invalid option -- '", err_color);
            vga_write_string_with_color(token, err_color);
            vga_write_string_with_color("'\n", err_color);
            return;
        }
    }

    if (!has_any_flag) {
        vga_write_string_with_color(VEOOS_NAME " v" VEOOS_VERSION "\n", text_color);
        return;
    }

    if (flag_all) {
        char cpu_model[64];
        cpuid_get_model_name(cpu_model, sizeof(cpu_model));
        vga_write_string_with_color(VEOOS_NAME " v" VEOOS_VERSION " " VEOOS_ARCH " ", text_color);
        vga_write_string_with_color(cpu_model, text_color);
        vga_write_string_with_color(" (" VEOOS_AUTHOR ")\n", text_color);
        return;
    }

    if (flag_kernel) {
        vga_write_string_with_color(VEOOS_NAME "\n", text_color);
        return;
    }

    if (flag_machine) {
        vga_write_string_with_color(VEOOS_ARCH "\n", text_color);
        return;
    }

    if (flag_release) {
        vga_write_string_with_color(VEOOS_VERSION "\n", text_color);
        return;
    }
}

static void handler_whoami(const char* args) {
    (void)args;
    uint8_t text_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write_string_with_color(user_get_current_name(), text_color);
    vga_put_character('\n');
}

static void handler_clear(const char* args) {
    (void)args;
    vga_clear();
}

void commands_system_register(void) {
    commands_register(
        "info",
        NULL,
        "show system information",
        "info",
        "  Output:\n    Displays VeoOS banner, version, author and hardware CPU string.",
        handler_info
    );

    commands_register(
        "uname",
        NULL,
        "print system information",
        "uname [-a|-s|-m|-r]",
        "  Flags:\n    -a         print all system information\n    -s         print kernel name (default)\n    -m         print machine hardware name\n    -r         print kernel release version",
        handler_uname
    );

    commands_register(
        "whoami",
        NULL,
        "print current user identity",
        "whoami",
        "  Output:\n    Displays the current user identity name.",
        handler_whoami
    );

    commands_register(
        "clear",
        NULL,
        "clear screen",
        "clear",
        "  Action:\n    Clears VGA display and resets cursor to top-left.",
        handler_clear
    );
}
