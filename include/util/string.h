#ifndef VEOOS_UTIL_STRING_H
#define VEOOS_UTIL_STRING_H

#include "util/types.h"

size_t string_length(const char* str);
int string_compare(const char* str1, const char* str2);
int string_compare_n(const char* str1, const char* str2, size_t count);
void string_copy(char* destination, const char* source);
void string_copy_n(char* destination, const char* source, size_t max_count);
void string_trim(const char* source, char* destination, size_t max_len);
bool string_split_two(const char* input, char* first, size_t first_max, char* second, size_t second_max);
bool string_next_token(const char** stream, char* token, size_t max_len);
void* memory_set(void* destination, int value, size_t count);
void* memory_copy(void* destination, const void* source, size_t count);

#endif
