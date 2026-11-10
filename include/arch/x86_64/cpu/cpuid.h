#ifndef VEOOS_ARCH_X86_64_CPU_CPUID_H
#define VEOOS_ARCH_X86_64_CPU_CPUID_H

#include "util/types.h"

void cpuid_get_model_name(char* buffer, size_t max_length);

#endif
