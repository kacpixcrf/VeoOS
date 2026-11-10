#ifndef VEOOS_UTIL_FORMAT_H
#define VEOOS_UTIL_FORMAT_H

#include "util/types.h"

size_t format_uint(char* buffer, size_t max_len, size_t value);
size_t format_fixed_uint(char* buffer, size_t max_len, size_t value, size_t min_digits);

#endif
