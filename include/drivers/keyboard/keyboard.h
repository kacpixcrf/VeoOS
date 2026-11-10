#ifndef VEOOS_DRIVERS_KEYBOARD_KEYBOARD_H
#define VEOOS_DRIVERS_KEYBOARD_KEYBOARD_H

#include "util/types.h"

void keyboard_initialize(void);
bool keyboard_has_input(void);
char keyboard_read_character(void);
void keyboard_drain(void);

#endif
