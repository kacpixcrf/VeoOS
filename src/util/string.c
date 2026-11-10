#include "util/string.h"

size_t string_length(const char* str) {
    if (str == NULL) {
        return 0;
    }
    const char* s = str;
    while (*s != '\0') {
        s++;
    }
    return (size_t)(s - str);
}

int string_compare(const char* str1, const char* str2) {
    if (str1 == str2) {
        return 0;
    }
    if (str1 == NULL) {
        return -1;
    }
    if (str2 == NULL) {
        return 1;
    }
    while (*str1 != '\0' && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

int string_compare_n(const char* str1, const char* str2, size_t count) {
    if (count == 0 || str1 == str2) {
        return 0;
    }
    if (str1 == NULL) {
        return -1;
    }
    if (str2 == NULL) {
        return 1;
    }
    while (count > 0 && *str1 != '\0' && (*str1 == *str2)) {
        str1++;
        str2++;
        count--;
    }
    if (count == 0) {
        return 0;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

void string_copy(char* destination, const char* source) {
    if (destination == NULL || source == NULL) {
        return;
    }
    while ((*destination++ = *source++) != '\0') {
    }
}

void string_copy_n(char* destination, const char* source, size_t max_count) {
    if (destination == NULL || max_count == 0) {
        return;
    }
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }
    size_t i = 0;
    while (source[i] != '\0' && i + 1 < max_count) {
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
}

void string_trim(const char* source, char* destination, size_t max_len) {
    if (destination == NULL || max_len == 0) {
        return;
    }
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }
    while (*source == ' ') {
        source++;
    }

    if (*source == '"') {
        source++;
        size_t len = 0;
        while (*source != '\0' && *source != '"' && len + 1 < max_len) {
            destination[len++] = *source++;
        }
        destination[len] = '\0';
        return;
    }

    size_t len = 0;
    while (source[len] != '\0' && len + 1 < max_len) {
        destination[len] = source[len];
        len++;
    }
    while (len > 0 && destination[len - 1] == ' ') {
        len--;
    }
    destination[len] = '\0';
}

bool string_split_two(const char* input, char* first, size_t first_max, char* second, size_t second_max) {
    if (input == NULL || first == NULL || second == NULL || first_max == 0 || second_max == 0) {
        return false;
    }
    while (*input == ' ') {
        input++;
    }
    if (*input == '\0') {
        return false;
    }

    size_t i = 0;
    if (*input == '"') {
        input++;
        while (*input != '\0' && *input != '"' && i + 1 < first_max) {
            first[i++] = *input++;
        }
        if (*input == '"') {
            input++;
        }
    } else {
        while (*input != '\0' && *input != ' ' && i + 1 < first_max) {
            first[i++] = *input++;
        }
    }
    first[i] = '\0';

    while (*input == ' ') {
        input++;
    }
    if (*input == '\0') {
        return false;
    }

    size_t j = 0;
    if (*input == '"') {
        input++;
        while (*input != '\0' && *input != '"' && j + 1 < second_max) {
            second[j++] = *input++;
        }
        if (*input == '"') {
            input++;
        }
    } else {
        while (*input != '\0' && *input != ' ' && j + 1 < second_max) {
            second[j++] = *input++;
        }
    }
    second[j] = '\0';
    return true;
}

bool string_next_token(const char** stream, char* token, size_t max_len) {
    if (stream == NULL || *stream == NULL || token == NULL || max_len == 0) {
        return false;
    }

    const char* p = *stream;
    while (*p == ' ') {
        p++;
    }

    if (*p == '\0') {
        token[0] = '\0';
        *stream = p;
        return false;
    }

    size_t i = 0;
    if (*p == '"') {
        p++;
        while (*p != '\0' && *p != '"' && i + 1 < max_len) {
            token[i++] = *p++;
        }
        if (*p == '"') {
            p++;
        }
    } else {
        while (*p != '\0' && *p != ' ' && i + 1 < max_len) {
            token[i++] = *p++;
        }
    }

    token[i] = '\0';
    *stream = p;
    return true;
}

void* memory_set(void* destination, int value, size_t count) {
    uint8_t byte_value = (uint8_t)value;
    uint8_t* byte_dest = (uint8_t*)destination;

    if (count >= sizeof(uint64_t) && ((uintptr_t)byte_dest % sizeof(uint64_t) == 0)) {
        uint64_t word_value = (uint64_t)byte_value;
        word_value |= word_value << 8;
        word_value |= word_value << 16;
        word_value |= word_value << 32;

        uint64_t* word_dest = (uint64_t*)byte_dest;
        while (count >= sizeof(uint64_t)) {
            *word_dest++ = word_value;
            count -= sizeof(uint64_t);
        }
        byte_dest = (uint8_t*)word_dest;
    }

    while (count > 0) {
        *byte_dest++ = byte_value;
        count--;
    }

    return destination;
}

void* memory_copy(void* destination, const void* source, size_t count) {
    uint8_t* byte_dest = (uint8_t*)destination;
    const uint8_t* byte_src = (const uint8_t*)source;

    if (count >= sizeof(uint64_t) &&
        ((uintptr_t)byte_dest % sizeof(uint64_t) == 0) &&
        ((uintptr_t)byte_src % sizeof(uint64_t) == 0)) {
        uint64_t* word_dest = (uint64_t*)byte_dest;
        const uint64_t* word_src = (const uint64_t*)byte_src;

        while (count >= sizeof(uint64_t)) {
            *word_dest++ = *word_src++;
            count -= sizeof(uint64_t);
        }
        byte_dest = (uint8_t*)word_dest;
        byte_src = (const uint8_t*)word_src;
    }

    while (count > 0) {
        *byte_dest++ = *byte_src++;
        count--;
    }

    return destination;
}
