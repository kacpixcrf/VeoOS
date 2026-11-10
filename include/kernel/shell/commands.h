#ifndef VEOOS_KERNEL_SHELL_COMMANDS_H
#define VEOOS_KERNEL_SHELL_COMMANDS_H

#include "util/types.h"

typedef void (*command_handler_t)(const char* args);

struct command_definition {
    const char* name;
    const char* alias;
    const char* description;
    const char* usage;
    const char* details;
    command_handler_t handler;
};

void commands_initialize(void);
bool commands_register(const char* name, const char* alias, const char* description, const char* usage, const char* details, command_handler_t handler);
void command_execute(const char* command_line);
size_t commands_get_count(void);
const struct command_definition* commands_get_by_index(size_t index);
const struct command_definition* commands_find(const char* name);

#endif
