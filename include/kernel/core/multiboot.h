#ifndef VEOOS_KERNEL_CORE_MULTIBOOT_H
#define VEOOS_KERNEL_CORE_MULTIBOOT_H

#include "util/types.h"

#define MULTIBOOT_INFO_MEMORY (1 << 0)
#define MULTIBOOT_INFO_BOOTDEV (1 << 1)
#define MULTIBOOT_INFO_CMDLINE (1 << 2)
#define MULTIBOOT_INFO_MODS (1 << 3)
#define MULTIBOOT_INFO_AOUT_SYMS (1 << 4)
#define MULTIBOOT_INFO_ELF_SHDR (1 << 5)
#define MULTIBOOT_INFO_MEM_MAP (1 << 6)
#define MULTIBOOT_INFO_DRIVE_INFO (1 << 7)
#define MULTIBOOT_INFO_CONFIG_TABLE (1 << 8)
#define MULTIBOOT_INFO_BOOT_LOADER_NAME (1 << 9)
#define MULTIBOOT_INFO_APM_TABLE (1 << 10)
#define MULTIBOOT_INFO_VBE_INFO (1 << 11)
#define MULTIBOOT_INFO_FRAMEBUFFER_INFO (1 << 12)

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB 1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2

struct __attribute__((packed)) multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t framebuffer_color_info[6];
};

#endif
