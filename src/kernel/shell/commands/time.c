#include "kernel/shell/commands/time.h"
#include "kernel/shell/commands.h"
#include "drivers/rtc/rtc.h"
#include "drivers/video/vga.h"
#include "util/format.h"

static void handler_time(const char* args) {
    (void)args;
    struct rtc_date_time dt;
    rtc_read_date_time(&dt);

    char buf[8];
    format_fixed_uint(buf, sizeof(buf), dt.hour, 2);
    vga_write_string(buf);
    vga_put_character(':');
    format_fixed_uint(buf, sizeof(buf), dt.minute, 2);
    vga_write_string(buf);
    vga_put_character(':');
    format_fixed_uint(buf, sizeof(buf), dt.second, 2);
    vga_write_string(buf);
    vga_put_character('\n');
}

static void handler_date(const char* args) {
    (void)args;
    struct rtc_date_time dt;
    rtc_read_date_time(&dt);

    char buf[8];
    format_fixed_uint(buf, sizeof(buf), dt.year, 4);
    vga_write_string(buf);
    vga_put_character('-');
    format_fixed_uint(buf, sizeof(buf), dt.month, 2);
    vga_write_string(buf);
    vga_put_character('-');
    format_fixed_uint(buf, sizeof(buf), dt.day, 2);
    vga_write_string(buf);
    vga_put_character('\n');
}

void commands_time_register(void) {
    commands_register(
        "time",
        NULL,
        "show current hardware time",
        "time",
        "  Output:\n    Reads real-time CMOS hardware clock in HH:MM:SS.",
        handler_time
    );

    commands_register(
        "date",
        NULL,
        "show current hardware date",
        "date",
        "  Output:\n    Reads real-time CMOS hardware clock in YYYY-MM-DD.",
        handler_date
    );
}
