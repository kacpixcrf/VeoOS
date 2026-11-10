#ifndef VEOOS_KERNEL_CORE_KERNEL_H
#define VEOOS_KERNEL_CORE_KERNEL_H

#include "kernel/core/multiboot.h"

void kernel_main(const struct multiboot_info* mb_info);
const struct multiboot_info* kernel_get_multiboot_info(void);

#endif
