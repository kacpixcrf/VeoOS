#ifndef VEOOS_DRIVERS_INPUT_INPUT_H
#define VEOOS_DRIVERS_INPUT_INPUT_H

#include "util/types.h"

#define INPUT_KEY_CTRL_C 3
#define INPUT_KEY_CTRL_E 5
#define INPUT_KEY_CTRL_S 19
#define INPUT_KEY_CTRL_V 22
#define INPUT_KEY_CTRL_X 24
#define INPUT_KEY_CTRL_Y 25
#define INPUT_KEY_UP 128
#define INPUT_KEY_DOWN 129
#define INPUT_KEY_LEFT 130
#define INPUT_KEY_RIGHT 131
#define INPUT_KEY_PAGE_UP 132
#define INPUT_KEY_PAGE_DOWN 133

void input_initialize(void);
bool input_has_character(void);
char input_read_character(void);
char input_try_read_character(void);

#endif
