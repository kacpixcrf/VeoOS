#include "arch/x86_64/cpu/cpuid.h"

static void cpuid_query(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    __asm__ volatile(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0)
    );
}

void cpuid_get_model_name(char* buffer, size_t max_length) {
    if (max_length == 0) {
        return;
    }

    uint32_t eax, ebx, ecx, edx;
    cpuid_query(0x80000000, &eax, &ebx, &ecx, &edx);

    if (eax >= 0x80000004) {
        uint32_t brand_registers[12];

        cpuid_query(0x80000002, &brand_registers[0], &brand_registers[1], &brand_registers[2], &brand_registers[3]);
        cpuid_query(0x80000003, &brand_registers[4], &brand_registers[5], &brand_registers[6], &brand_registers[7]);
        cpuid_query(0x80000004, &brand_registers[8], &brand_registers[9], &brand_registers[10], &brand_registers[11]);

        const char* raw_string = (const char*)brand_registers;
        while (*raw_string == ' ') {
            raw_string++;
        }

        size_t index = 0;
        while (index < max_length - 1 && raw_string[index] != '\0' && index < 48) {
            buffer[index] = raw_string[index];
            index++;
        }
        buffer[index] = '\0';
    } else {
        cpuid_query(0, &eax, &ebx, &ecx, &edx);
        uint32_t vendor[3] = { ebx, edx, ecx };
        const char* vendor_string = (const char*)vendor;

        size_t index = 0;
        while (index < max_length - 1 && index < 12) {
            buffer[index] = vendor_string[index];
            index++;
        }
        buffer[index] = '\0';
    }
}
