#ifndef VEOOS_DRIVERS_INPUT_UEFI_INPUT_H
#define VEOOS_DRIVERS_INPUT_UEFI_INPUT_H

#include "util/types.h"

void uefi_input_initialize(void);
bool uefi_input_is_available(void);
bool uefi_input_has_character(void);
char uefi_input_try_read_character(void);

#endif
