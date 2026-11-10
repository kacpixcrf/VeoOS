#include "kernel/shell/commands.h"
#include "kernel/shell/commands/system.h"
#include "kernel/shell/commands/power.h"
#include "kernel/shell/commands/time.h"
#include "kernel/shell/commands/filesystem.h"
#include "kernel/shell/commands/matrix.h"
#include "kernel/shell/commands/builtin.h"
#include "drivers/video/vga.h"
#include "util/string.h"

#define MAX_COMMANDS 64

static struct command_definition command_table[MAX_COMMANDS];
static size_t command_count = 0;

bool commands_register(const char* name, const char* alias, const char* description, const char* usage, const char* details, command_handler_t handler) {
    if (command_count >= MAX_COMMANDS || name == NULL || handler == NULL) {
        return false;
    }

    command_table[command_count].name = name;
    command_table[command_count].alias = alias;
    command_table[command_count].description = description;
    command_table[command_count].usage = usage;
    command_table[command_count].details = details;
    command_table[command_count].handler = handler;
    command_count++;
    return true;
}

const struct command_definition* commands_find(const char* name) {
    if (name == NULL || name[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0; i < command_count; i++) {
        if (string_compare(command_table[i].name, name) == 0 ||
            (command_table[i].alias != NULL && string_compare(command_table[i].alias, name) == 0)) {
            return &command_table[i];
        }
    }
    return NULL;
}

void commands_initialize(void) {
    command_count = 0;
    commands_system_register();
    commands_power_register();
    commands_time_register();
    commands_filesystem_register();
    commands_matrix_register();
    commands_builtin_register();
}

size_t commands_get_count(void) {
    return command_count;
}

const struct command_definition* commands_get_by_index(size_t index) {
    if (index < command_count) {
        return &command_table[index];
    }
    return NULL;
}

void command_execute(const char* command_line) {
    while (*command_line == ' ') {
        command_line++;
    }

    if (*command_line == '\0') {
        return;
    }

    char cmd_name[32];
    size_t i = 0;
    while (command_line[i] != '\0' && command_line[i] != ' ' && i < sizeof(cmd_name) - 1) {
        cmd_name[i] = command_line[i];
        i++;
    }
    cmd_name[i] = '\0';

    const char* args = command_line + i;
    while (*args == ' ') {
        args++;
    }

    for (size_t c = 0; c < command_count; c++) {
        if (string_compare(command_table[c].name, cmd_name) == 0 ||
            (command_table[c].alias != NULL && string_compare(command_table[c].alias, cmd_name) == 0)) {
            command_table[c].handler(args);
            return;
        }
    }

    uint8_t error_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write_string_with_color("VeoOS: command not found: ", error_color);
    vga_write_string_with_color(cmd_name, error_color);
    vga_put_character('\n');
}
