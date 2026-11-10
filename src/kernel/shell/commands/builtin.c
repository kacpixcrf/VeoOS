#include "kernel/shell/commands/builtin.h"
#include "kernel/shell/commands.h"
#include "drivers/video/vga.h"
#include "util/string.h"

static void handler_echo(const char* args) {
    vga_write_string(args);
    vga_put_character('\n');
}

static void handler_help(const char* args) {
    char clean_cmd[32];
    string_trim(args, clean_cmd, sizeof(clean_cmd));

    uint8_t brand_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t desc_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t text_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    uint8_t hint_color = vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);

    if (clean_cmd[0] != '\0') {
        const struct command_definition* def = commands_find(clean_cmd);
        if (def == NULL) {
            uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_write_string_with_color("help: no help entry for: ", err_color);
            vga_write_string_with_color(clean_cmd, err_color);
            vga_put_character('\n');
            return;
        }

        vga_put_character('\n');
        vga_write_string_with_color("  Command:     ", desc_color);
        vga_write_string_with_color(def->name, brand_color);
        if (def->alias != NULL) {
            vga_write_string_with_color(" (alias: ", desc_color);
            vga_write_string_with_color(def->alias, brand_color);
            vga_write_string_with_color(")", desc_color);
        }
        vga_put_character('\n');

        vga_write_string_with_color("  Usage:       ", desc_color);
        vga_write_string_with_color(def->usage, text_color);
        vga_put_character('\n');

        vga_write_string_with_color("  Description: ", desc_color);
        vga_write_string_with_color(def->description, text_color);
        vga_put_character('\n');

        if (def->details != NULL && def->details[0] != '\0') {
            vga_put_character('\n');
            vga_write_string_with_color(def->details, text_color);
            vga_put_character('\n');
        }
        vga_put_character('\n');
        return;
    }

    vga_put_character('\n');
    vga_write_string_with_color("  VeoOS Shell Commands:\n\n", brand_color);

    size_t count = commands_get_count();
    for (size_t i = 0; i < count; i++) {
        const struct command_definition* def = commands_get_by_index(i);
        if (def == NULL) {
            continue;
        }

        vga_write_string("    ");
        size_t len = 0;
        if (def->alias != NULL) {
            vga_write_string_with_color(def->name, brand_color);
            vga_write_string_with_color(", ", hint_color);
            vga_write_string_with_color(def->alias, brand_color);
            len = string_length(def->name) + 2 + string_length(def->alias);
        } else {
            vga_write_string_with_color(def->name, brand_color);
            len = string_length(def->name);
        }
        while (len < 22) {
            vga_put_character(' ');
            len++;
        }
        vga_write_string_with_color(def->description, desc_color);
        vga_put_character('\n');
    }
    vga_put_character('\n');
    vga_write_string_with_color("  Use 'help <command>' for flags and usage.\n\n", hint_color);
}

void commands_builtin_register(void) {
    commands_register(
        "echo",
        NULL,
        "print text",
        "echo [text]",
        "  Action:\n    Prints argument text to standard output.",
        handler_echo
    );

    commands_register(
        "help",
        NULL,
        "show available commands or details",
        "help [command]",
        "  Arguments:\n    [command]  show flags and usage for a specific command",
        handler_help
    );
}
