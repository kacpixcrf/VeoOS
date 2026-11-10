#include "util/format.h"

size_t format_uint(char* buffer, size_t max_len, size_t value) {
    if (buffer == NULL || max_len == 0) {
        return 0;
    }

    if (value == 0) {
        if (max_len > 1) {
            buffer[0] = '0';
            buffer[1] = '\0';
            return 1;
        }
        buffer[0] = '\0';
        return 0;
    }

    char temp[24];
    size_t digits = 0;
    while (value > 0) {
        temp[digits++] = (char)('0' + (value % 10));
        value /= 10;
    }

    size_t written = 0;
    while (digits > 0 && written + 1 < max_len) {
        buffer[written++] = temp[--digits];
    }
    buffer[written] = '\0';
    return written;
}

size_t format_fixed_uint(char* buffer, size_t max_len, size_t value, size_t min_digits) {
    if (buffer == NULL || max_len == 0) {
        return 0;
    }

    char temp[24];
    size_t digits = 0;

    if (value == 0) {
        temp[digits++] = '0';
    } else {
        while (value > 0) {
            temp[digits++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }

    while (digits < min_digits && digits < sizeof(temp)) {
        temp[digits++] = '0';
    }

    size_t written = 0;
    while (digits > 0 && written + 1 < max_len) {
        buffer[written++] = temp[--digits];
    }
    buffer[written] = '\0';
    return written;
}
