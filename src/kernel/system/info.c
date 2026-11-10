#include "kernel/system/info.h"
#include "kernel/core/version.h"
#include "kernel/core/kernel.h"
#include "kernel/core/user.h"
#include "kernel/memory/memory.h"
#include "arch/x86_64/cpu/cpuid.h"
#include "arch/x86_64/cpu/io.h"
#include "drivers/video/vga.h"
#include "util/format.h"
#include "util/string.h"

static uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC));
    io_outd(0xCF8, address);
    return io_ind(0xCFC);
}

struct gpu_device_entry {
    uint16_t vendor;
    uint16_t device;
    const char* name;
};

static const struct gpu_device_entry known_gpus[] = {
    { 0x10DE, 0x2684, "NVIDIA GeForce RTX 4090" },
    { 0x10DE, 0x2702, "NVIDIA GeForce RTX 4080 SUPER" },
    { 0x10DE, 0x2704, "NVIDIA GeForce RTX 4080" },
    { 0x10DE, 0x2705, "NVIDIA GeForce RTX 4070 Ti SUPER" },
    { 0x10DE, 0x2782, "NVIDIA GeForce RTX 4070 Ti" },
    { 0x10DE, 0x2783, "NVIDIA GeForce RTX 4070 SUPER" },
    { 0x10DE, 0x2786, "NVIDIA GeForce RTX 4070" },
    { 0x10DE, 0x2709, "NVIDIA GeForce RTX 4070" },
    { 0x10DE, 0x2803, "NVIDIA GeForce RTX 4060 Ti" },
    { 0x10DE, 0x2805, "NVIDIA GeForce RTX 4060 Ti" },
    { 0x10DE, 0x2788, "NVIDIA GeForce RTX 4060 Ti" },
    { 0x10DE, 0x2882, "NVIDIA GeForce RTX 4060" },
    { 0x10DE, 0x2885, "NVIDIA GeForce RTX 4060" },
    { 0x10DE, 0x28A0, "NVIDIA GeForce RTX 4060 Laptop" },
    { 0x10DE, 0x2204, "NVIDIA GeForce RTX 3090" },
    { 0x10DE, 0x2208, "NVIDIA GeForce RTX 3080 Ti" },
    { 0x10DE, 0x2206, "NVIDIA GeForce RTX 3080" },
    { 0x10DE, 0x2216, "NVIDIA GeForce RTX 3080" },
    { 0x10DE, 0x2482, "NVIDIA GeForce RTX 3070 Ti" },
    { 0x10DE, 0x2484, "NVIDIA GeForce RTX 3070" },
    { 0x10DE, 0x2487, "NVIDIA GeForce RTX 3060 Ti" },
    { 0x10DE, 0x2503, "NVIDIA GeForce RTX 3060" },
    { 0x10DE, 0x2504, "NVIDIA GeForce RTX 3060" },
    { 0x10DE, 0x2488, "NVIDIA GeForce RTX 3060" },
    { 0x10DE, 0x2507, "NVIDIA GeForce RTX 3050" },
    { 0x10DE, 0x2508, "NVIDIA GeForce RTX 3050" },
    { 0x10DE, 0x1E82, "NVIDIA GeForce RTX 2080 Ti" },
    { 0x10DE, 0x1E87, "NVIDIA GeForce RTX 2080 SUPER" },
    { 0x10DE, 0x1E84, "NVIDIA GeForce RTX 2080" },
    { 0x10DE, 0x1F02, "NVIDIA GeForce RTX 2070 SUPER" },
    { 0x10DE, 0x1E89, "NVIDIA GeForce RTX 2070" },
    { 0x10DE, 0x1F47, "NVIDIA GeForce RTX 2060 SUPER" },
    { 0x10DE, 0x1F06, "NVIDIA GeForce RTX 2060" },
    { 0x10DE, 0x1F07, "NVIDIA GeForce RTX 2060" },
    { 0x10DE, 0x1F42, "NVIDIA GeForce RTX 2060" },
    { 0x10DE, 0x2182, "NVIDIA GeForce GTX 1660 Ti" },
    { 0x10DE, 0x2184, "NVIDIA GeForce GTX 1660 SUPER" },
    { 0x10DE, 0x2188, "NVIDIA GeForce GTX 1650" },
    { 0x10DE, 0x1F82, "NVIDIA GeForce GTX 1650" },
    { 0x10DE, 0x1B06, "NVIDIA GeForce GTX 1080 Ti" },
    { 0x10DE, 0x1B80, "NVIDIA GeForce GTX 1080" },
    { 0x10DE, 0x1B82, "NVIDIA GeForce GTX 1070 Ti" },
    { 0x10DE, 0x1B81, "NVIDIA GeForce GTX 1070" },
    { 0x10DE, 0x1C02, "NVIDIA GeForce GTX 1060" },
    { 0x10DE, 0x1C03, "NVIDIA GeForce GTX 1060" },
    { 0x10DE, 0x1B83, "NVIDIA GeForce GTX 1060" },
    { 0x10DE, 0x1C82, "NVIDIA GeForce GTX 1050 Ti" },
    { 0x10DE, 0x1C81, "NVIDIA GeForce GTX 1050" },
    { 0x1002, 0x744C, "AMD Radeon RX 7900 XTX" },
    { 0x1002, 0x7448, "AMD Radeon RX 7900 XT" },
    { 0x1002, 0x747E, "AMD Radeon RX 7800 XT" },
    { 0x1002, 0x7470, "AMD Radeon RX 7700 XT" },
    { 0x1002, 0x7480, "AMD Radeon RX 7600" },
    { 0x1002, 0x73A5, "AMD Radeon RX 6900 XT" },
    { 0x1002, 0x73BF, "AMD Radeon RX 6800 / 6800 XT" },
    { 0x1002, 0x73DF, "AMD Radeon RX 6700 XT" },
    { 0x1002, 0x73FF, "AMD Radeon RX 6600 / 6600 XT" },
    { 0x1002, 0x743F, "AMD Radeon RX 6400 / 6500 XT" },
    { 0x1002, 0x731F, "AMD Radeon RX 5700 / 5700 XT" },
    { 0x1002, 0x7340, "AMD Radeon RX 5500 XT" },
    { 0x1002, 0x67DF, "AMD Radeon RX 470/480/570/580" },
    { 0x8086, 0x56A0, "Intel Arc A770" },
    { 0x8086, 0x56A1, "Intel Arc A750" },
    { 0x8086, 0x56A5, "Intel Arc A380" },
    { 0x8086, 0x56A6, "Intel Arc A310" },
    { 0x8086, 0x4680, "Intel UHD Graphics 770" },
    { 0x8086, 0x4690, "Intel UHD Graphics 770" },
    { 0x8086, 0x4692, "Intel UHD Graphics 730" },
    { 0x8086, 0x9BC5, "Intel UHD Graphics 630" },
    { 0x8086, 0x3E92, "Intel UHD Graphics 630" },
    { 0x8086, 0x5912, "Intel HD Graphics 630" },
    { 0x1234, 0x1111, "QEMU Standard VGA" },
    { 0x1AF4, 0x1050, "VirtIO GPU" },
    { 0x15AD, 0x0405, "VMware SVGA II" },
    { 0x80EE, 0xBEEF, "VirtualBox Graphics" }
};

static void format_pci_fallback(char* buf, size_t max_len, const char* prefix, uint16_t dev_id) {
    const char hex_chars[] = "0123456789ABCDEF";
    char hex_str[7];
    hex_str[0] = '0';
    hex_str[1] = 'x';
    hex_str[2] = hex_chars[(dev_id >> 12) & 0xF];
    hex_str[3] = hex_chars[(dev_id >> 8) & 0xF];
    hex_str[4] = hex_chars[(dev_id >> 4) & 0xF];
    hex_str[5] = hex_chars[dev_id & 0xF];
    hex_str[6] = '\0';

    size_t plen = string_length(prefix);
    if (plen >= max_len) {
        string_copy_n(buf, prefix, max_len);
        return;
    }
    string_copy_n(buf, prefix, max_len);
    if (plen + 1 < max_len) {
        string_copy_n(buf + plen, hex_str, max_len - plen);
    }
}

static void pci_get_gpu_name(char* buffer, size_t max_len) {
    char best_name[64];
    best_name[0] = '\0';

    for (uint8_t bus = 0; bus < 8; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read((uint8_t)bus, slot, 0, 0x00);
            if ((id & 0xFFFF) == 0xFFFF || (id & 0xFFFF) == 0x0000) {
                continue;
            }
            uint32_t class_reg = pci_read((uint8_t)bus, slot, 0, 0x08);
            uint8_t base_class = (uint8_t)(class_reg >> 24);
            if (base_class == 0x03) {
                uint16_t vendor = (uint16_t)(id & 0xFFFF);
                uint16_t device = (uint16_t)(id >> 16);
                const char* matched_name = NULL;

                for (size_t k = 0; k < sizeof(known_gpus) / sizeof(known_gpus[0]); k++) {
                    if (known_gpus[k].vendor == vendor && known_gpus[k].device == device) {
                        matched_name = known_gpus[k].name;
                        break;
                    }
                }

                char current_name[64];
                if (matched_name != NULL) {
                    string_copy_n(current_name, matched_name, sizeof(current_name));
                } else if (vendor == 0x10DE) {
                    format_pci_fallback(current_name, sizeof(current_name), "NVIDIA GeForce ", device);
                } else if (vendor == 0x1002) {
                    format_pci_fallback(current_name, sizeof(current_name), "AMD Radeon ", device);
                } else if (vendor == 0x8086) {
                    format_pci_fallback(current_name, sizeof(current_name), "Intel Graphics ", device);
                } else if (vendor == 0x1234) {
                    string_copy_n(current_name, "QEMU Standard VGA", sizeof(current_name));
                } else if (vendor == 0x15AD) {
                    string_copy_n(current_name, "VMware SVGA", sizeof(current_name));
                } else if (vendor == 0x1AF4) {
                    string_copy_n(current_name, "VirtIO GPU", sizeof(current_name));
                } else if (vendor == 0x1414) {
                    string_copy_n(current_name, "Microsoft Basic Display", sizeof(current_name));
                } else {
                    format_pci_fallback(current_name, sizeof(current_name), "PCI Video Adapter ", device);
                }

                bool is_discrete = (vendor == 0x10DE) || (vendor == 0x1002 && device >= 0x6000);

                if (is_discrete) {
                    string_copy_n(buffer, current_name, max_len);
                    return;
                }

                if (best_name[0] == '\0') {
                    string_copy_n(best_name, current_name, sizeof(best_name));
                }
            }
        }
    }

    if (best_name[0] != '\0') {
        string_copy_n(buffer, best_name, max_len);
        return;
    }

    string_copy_n(buffer, "Standard GOP / VGA", max_len);
}

void system_info_display(void) {
    uint8_t logo_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t label_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t value_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    char cpu_model[64];
    cpuid_get_model_name(cpu_model, sizeof(cpu_model));

    char gpu_model[64];
    pci_get_gpu_name(gpu_model, sizeof(gpu_model));

    const struct multiboot_info* mb = kernel_get_multiboot_info();

    vga_put_character('\n');
    vga_write_string_with_color("   __   __          ____   _____ \n", logo_color);
    vga_write_string_with_color("   \\ \\ / /__  ___  / __ \\ / ____|\n", logo_color);
    vga_write_string_with_color("    \\ V / -_)/ _ \\| |  | | \\___ \\ \n", logo_color);
    vga_write_string_with_color("     \\_/ \\___/\\___/\\____/ |_____/ \n", logo_color);
    vga_put_character('\n');

    vga_write_string_with_color("  os        ", label_color);
    vga_write_string_with_color(VEOOS_NAME " v" VEOOS_VERSION "\n", value_color);

    vga_write_string_with_color("  author    ", label_color);
    vga_write_string_with_color(VEOOS_AUTHOR "\n", value_color);

    vga_write_string_with_color("  user      ", label_color);
    vga_write_string_with_color(user_get_current_name(), value_color);
    vga_put_character('\n');

    vga_write_string_with_color("  arch      ", label_color);
    vga_write_string_with_color(VEOOS_ARCH "\n", value_color);

    vga_write_string_with_color("  cpu       ", label_color);
    vga_write_string_with_color(cpu_model, value_color);
    vga_put_character('\n');

    vga_write_string_with_color("  gpu       ", label_color);
    vga_write_string_with_color(gpu_model, value_color);
    if (mb != NULL && (mb->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) && mb->framebuffer_width > 0) {
        char num_buf[16];
        vga_write_string_with_color(" (", label_color);
        format_uint(num_buf, sizeof(num_buf), mb->framebuffer_width);
        vga_write_string_with_color(num_buf, value_color);
        vga_write_string_with_color("x", label_color);
        format_uint(num_buf, sizeof(num_buf), mb->framebuffer_height);
        vga_write_string_with_color(num_buf, value_color);
        vga_write_string_with_color("@", label_color);
        format_uint(num_buf, sizeof(num_buf), mb->framebuffer_bpp);
        vga_write_string_with_color(num_buf, value_color);
        vga_write_string_with_color("bpp)", label_color);
    }
    vga_put_character('\n');

    vga_write_string_with_color("  memory    ", label_color);
    size_t used_bytes = memory_get_used_bytes();
    char mem_buf[16];
    if (used_bytes < 1024 * 1024) {
        size_t used_kb = (used_bytes + 1023) / 1024;
        format_uint(mem_buf, sizeof(mem_buf), used_kb);
        vga_write_string_with_color(mem_buf, value_color);
        vga_write_string_with_color(" KB / ", label_color);
    } else {
        size_t used_mb = (used_bytes + 1048575) / (1024 * 1024);
        format_uint(mem_buf, sizeof(mem_buf), used_mb);
        vga_write_string_with_color(mem_buf, value_color);
        vga_write_string_with_color(" MB / ", label_color);
    }

    if (mb != NULL && (mb->flags & MULTIBOOT_INFO_MEMORY) && mb->mem_upper > 0) {
        size_t total_mb = (size_t)(mb->mem_upper / 1024);
        format_uint(mem_buf, sizeof(mem_buf), total_mb);
        vga_write_string_with_color(mem_buf, value_color);
        vga_write_string_with_color(" MB", value_color);
        if (total_mb >= 1024) {
            size_t total_gb = total_mb / 1024;
            size_t rem_dec = ((total_mb % 1024) * 10) / 1024;
            vga_write_string_with_color(" (", label_color);
            format_uint(mem_buf, sizeof(mem_buf), total_gb);
            vga_write_string_with_color(mem_buf, value_color);
            vga_write_string_with_color(".", label_color);
            format_uint(mem_buf, sizeof(mem_buf), rem_dec);
            vga_write_string_with_color(mem_buf, value_color);
            vga_write_string_with_color(" GB)", label_color);
        }
    } else {
        vga_write_string_with_color("64 MB", value_color);
    }
    vga_put_character('\n');
    vga_put_character('\n');
}
