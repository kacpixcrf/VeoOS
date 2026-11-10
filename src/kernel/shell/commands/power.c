#include "kernel/shell/commands/power.h"
#include "kernel/shell/commands.h"
#include "arch/x86_64/cpu/io.h"
#include "drivers/video/vga.h"
#include "util/types.h"

#define UEFI_RESET_FUNC_ADDR 0x100030
#define PCI_RESET_PORT       0xCF9
#define PS2_STATUS_PORT      0x64

typedef void (__attribute__((ms_abi)) *efi_reset_system_t)(
    uint32_t reset_type,
    uint64_t reset_status,
    uint64_t data_size,
    const void *reset_data
);

static void uefi_reset_system(uint32_t type) {
    efi_reset_system_t fn = (efi_reset_system_t)(*(uint64_t*)UEFI_RESET_FUNC_ADDR);
    if (fn) {
        fn(type, 0, 0, NULL);
    }
}

static void handler_reboot(const char* args) {
    (void)args;
    vga_write_string("Rebooting system...\n");

    uefi_reset_system(0);

    io_outb(PCI_RESET_PORT, 0x02);
    io_outb(PCI_RESET_PORT, 0x06);
    io_outb(PCI_RESET_PORT, 0x0E);

    uint8_t status = io_inb(PS2_STATUS_PORT);
    if (status != 0xFF) {
        uint32_t timeout = 10000;
        while ((io_inb(PS2_STATUS_PORT) & 2) && timeout--) {
            io_wait();
        }
        io_outb(PS2_STATUS_PORT, 0xFE);
    }

    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) null_idt = {0, 0};
    __asm__ volatile("lidt %0; int $3" : : "m"(null_idt));

    while (1) {
        __asm__ volatile("hlt");
    }
}

static void handler_shutdown(const char* args) {
    (void)args;
    vga_write_string("Shutting down...\n");

    uefi_reset_system(2);

    io_outw(0x604, 0x2000);
    io_outw(0x4004, 0x3400);
    io_outw(0xB004, 0x2000);

    for (uint16_t port = 0x404; port <= 0x1804; port += 0x100) {
        io_outw(port, 0x2000);
        io_outw(port, 0x3400);
    }

    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

void commands_power_register(void) {
    commands_register(
        "reboot",
        NULL,
        "reboot machine",
        "reboot",
        "  Action:\n    Reboots the machine via PCI reset or triple fault.",
        handler_reboot
    );

    commands_register(
        "shutdown",
        "poweroff",
        "power off system",
        "shutdown",
        "  Action:\n    Powers off or halts the machine safely.",
        handler_shutdown
    );
}
