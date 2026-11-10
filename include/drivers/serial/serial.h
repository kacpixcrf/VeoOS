#ifndef VEOOS_DRIVERS_SERIAL_SERIAL_H
#define VEOOS_DRIVERS_SERIAL_SERIAL_H

#include "util/types.h"

void serial_initialize(void);
void serial_write_character(char character);
void serial_write_string(const char* string);
bool serial_has_input(void);
char serial_read_character(void);

#endif
