#include "kernel/shell/shell.h"
#include "kernel/shell/commands.h"
#include "kernel/fs/ramfs.h"
#include "kernel/core/user.h"
#include "drivers/video/vga.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/input.h"
#include "util/string.h"
#include "util/types.h"

#define SHELL_BUFFER_CAPACITY 256
#define SHELL_HISTORY_CAPACITY 16

static char shell_buffer[SHELL_BUFFER_CAPACITY];
static size_t shell_buffer_len;
static size_t shell_cursor_pos;

static size_t shell_prompt_col;
static size_t shell_prompt_row;

static char shell_history[SHELL_HISTORY_CAPACITY][SHELL_BUFFER_CAPACITY];
static size_t shell_history_count;
static size_t shell_history_index;
static bool shell_just_executed;

static bool tab_cycling_active;
static char tab_saved_prefix[64];
static size_t tab_saved_token_start;
static char tab_matches[32][RAMFS_NAME_MAX];
static bool tab_matches_is_dir[32];
static size_t tab_matches_count;
static size_t tab_match_index;

static void shell_prompt(void) {
    uint8_t brand_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t path_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t arrow_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    char cwd_path[128];
    ramfs_get_cwd_path(cwd_path, sizeof(cwd_path));

    char display_path[128];
    const char* home = VEOOS_HOME_DIR;
    size_t home_len = string_length(home);

    if (string_compare(cwd_path, home) == 0) {
        display_path[0] = '~';
        display_path[1] = '\0';
    } else if (string_compare_n(cwd_path, home, home_len) == 0 && cwd_path[home_len] == '/') {
        display_path[0] = '~';
        string_copy_n(display_path + 1, cwd_path + home_len, sizeof(display_path) - 1);
    } else {
        string_copy_n(display_path, cwd_path, sizeof(display_path));
    }

    vga_write_string_with_color("VeoOS ", brand_color);
    vga_write_string_with_color(display_path, path_color);
    vga_write_string_with_color(" > ", arrow_color);

    shell_prompt_col = vga_get_cursor_x();
    shell_prompt_row = vga_get_cursor_y();
}

static void shell_set_cursor(size_t pos) {
    size_t cols = vga_get_width();
    if (cols == 0) {
        cols = 80;
    }
    size_t total = shell_prompt_col + pos;
    size_t r = shell_prompt_row + (total / cols);
    size_t c = total % cols;
    vga_set_cursor_position(c, r);
}

static void shell_sync_after_typing(void) {
    size_t cols = vga_get_width();
    if (cols == 0) {
        cols = 80;
    }
    size_t cur_y = vga_get_cursor_y();
    size_t total = shell_prompt_col + shell_buffer_len;
    size_t expected_y = shell_prompt_row + (total / cols);
    if (expected_y > cur_y) {
        size_t diff = expected_y - cur_y;
        if (shell_prompt_row >= diff) {
            shell_prompt_row -= diff;
        } else {
            shell_prompt_row = 0;
        }
    }
}

static void shell_clear_input(void) {
    shell_set_cursor(0);
    for (size_t i = 0; i < shell_buffer_len; i++) {
        vga_put_character(' ');
    }
    shell_set_cursor(0);
    shell_buffer[0] = '\0';
    shell_buffer_len = 0;
    shell_cursor_pos = 0;
}

static void shell_history_add(const char* command) {
    if (shell_history_count > 0 &&
        string_compare(shell_history[shell_history_count - 1], command) == 0) {
        shell_history_index = shell_history_count;
        return;
    }

    if (shell_history_count < SHELL_HISTORY_CAPACITY) {
        string_copy_n(shell_history[shell_history_count], command, SHELL_BUFFER_CAPACITY);
        shell_history_count++;
    } else {
        for (size_t i = 0; i < SHELL_HISTORY_CAPACITY - 1; i++) {
            string_copy_n(shell_history[i], shell_history[i + 1], SHELL_BUFFER_CAPACITY);
        }
        string_copy_n(shell_history[SHELL_HISTORY_CAPACITY - 1], command, SHELL_BUFFER_CAPACITY);
    }
    shell_history_index = shell_history_count;
}

static void shell_tab_complete(void) {
    if (!tab_cycling_active) {
        if (shell_cursor_pos == 0) {
            return;
        }

        tab_saved_token_start = 0;
        for (size_t i = 0; i < shell_cursor_pos; i++) {
            if (shell_buffer[i] == ' ') {
                tab_saved_token_start = i + 1;
            }
        }
        if (tab_saved_token_start < shell_cursor_pos && shell_buffer[tab_saved_token_start] == '"') {
            tab_saved_token_start++;
        }

        size_t prefix_len = shell_cursor_pos - tab_saved_token_start;
        if (prefix_len == 0 || prefix_len >= sizeof(tab_saved_prefix)) {
            return;
        }

        for (size_t i = 0; i < prefix_len; i++) {
            tab_saved_prefix[i] = shell_buffer[tab_saved_token_start + i];
        }
        tab_saved_prefix[prefix_len] = '\0';

        tab_matches_count = 0;
        tab_match_index = 0;

        bool command_only = (tab_saved_token_start == 0);

        if (command_only) {
            size_t total_commands = commands_get_count();
            for (size_t i = 0; i < total_commands && tab_matches_count < 32; i++) {
                const struct command_definition* def = commands_get_by_index(i);
                if (def != NULL) {
                    if (string_compare_n(def->name, tab_saved_prefix, prefix_len) == 0) {
                        string_copy_n(tab_matches[tab_matches_count], def->name, RAMFS_NAME_MAX);
                        tab_matches_is_dir[tab_matches_count] = false;
                        tab_matches_count++;
                    }
                    if (def->alias != NULL && tab_matches_count < 32 &&
                        string_compare_n(def->alias, tab_saved_prefix, prefix_len) == 0) {
                        string_copy_n(tab_matches[tab_matches_count], def->alias, RAMFS_NAME_MAX);
                        tab_matches_is_dir[tab_matches_count] = false;
                        tab_matches_count++;
                    }
                }
            }
        } else {
            int target_dir = ramfs_get_cwd();
            size_t search_prefix_len = prefix_len;
            const char* search_prefix = tab_saved_prefix;

            int last_slash = -1;
            for (size_t i = 0; i < prefix_len; i++) {
                if (tab_saved_prefix[i] == '/') {
                    last_slash = (int)i;
                }
            }

            if (last_slash != -1) {
                char dir_path[64];
                if (last_slash == 0) {
                    dir_path[0] = '/';
                    dir_path[1] = '\0';
                } else {
                    size_t copy_len = (size_t)last_slash;
                    if (copy_len >= sizeof(dir_path)) {
                        copy_len = sizeof(dir_path) - 1;
                    }
                    for (size_t i = 0; i < copy_len; i++) {
                        dir_path[i] = tab_saved_prefix[i];
                    }
                    dir_path[copy_len] = '\0';
                }

                target_dir = ramfs_resolve_directory(dir_path);
                if (target_dir < 0) {
                    return;
                }

                tab_saved_token_start += (size_t)last_slash + 1;
                search_prefix = tab_saved_prefix + last_slash + 1;
                search_prefix_len = prefix_len - ((size_t)last_slash + 1);
            }

            int children[64];
            int count = ramfs_list_children(target_dir, children, 64);
            for (int i = 0; i < count && tab_matches_count < 32; i++) {
                struct ramfs_node* node = ramfs_get_node(children[i]);
                if (node == NULL) {
                    continue;
                }
                if (search_prefix_len == 0 || string_compare_n(node->name, search_prefix, search_prefix_len) == 0) {
                    string_copy_n(tab_matches[tab_matches_count], node->name, RAMFS_NAME_MAX);
                    tab_matches_is_dir[tab_matches_count] = node->is_directory;
                    tab_matches_count++;
                }
            }
        }

        if (tab_matches_count == 0) {
            return;
        }

        tab_cycling_active = true;
    }

    if (tab_matches_count == 0) {
        return;
    }

    const char* current_match = tab_matches[tab_match_index % tab_matches_count];
    bool is_dir = tab_matches_is_dir[tab_match_index % tab_matches_count];
    tab_match_index++;

    size_t match_len = string_length(current_match);
    size_t needed_len = tab_saved_token_start + match_len + (is_dir ? 1 : 0);
    if (needed_len >= SHELL_BUFFER_CAPACITY - 1) {
        return;
    }

    size_t prev_len = shell_buffer_len;

    for (size_t i = 0; i < match_len; i++) {
        shell_buffer[tab_saved_token_start + i] = current_match[i];
    }
    size_t new_pos = tab_saved_token_start + match_len;
    if (is_dir) {
        shell_buffer[new_pos++] = '/';
    }
    shell_buffer[new_pos] = '\0';
    shell_buffer_len = new_pos;
    shell_cursor_pos = new_pos;

    shell_set_cursor(0);
    vga_write_string(shell_buffer);
    if (prev_len > shell_buffer_len) {
        for (size_t i = shell_buffer_len; i < prev_len; i++) {
            vga_put_character(' ');
        }
    }
    shell_set_cursor(shell_cursor_pos);
}

void shell_initialize(void) {
    commands_initialize();
    shell_buffer_len = 0;
    shell_cursor_pos = 0;
    shell_buffer[0] = '\0';
    shell_history_count = 0;
    shell_history_index = 0;
    tab_cycling_active = false;
    tab_matches_count = 0;
    tab_match_index = 0;
    shell_just_executed = false;
    shell_prompt();
}

void shell_run(void) {
    while (true) {
        char ch = input_read_character();
        if (ch == 0) {
            continue;
        }

        if (ch != '\t') {
            tab_cycling_active = false;
        }
        if (ch != '\n') {
            shell_just_executed = false;
        }

        if (ch == '\n') {
            if (shell_buffer_len == 0 && shell_just_executed) {
                shell_just_executed = false;
                continue;
            }
            shell_set_cursor(shell_buffer_len);
            vga_put_character('\n');
            shell_buffer[shell_buffer_len] = '\0';
            if (shell_buffer_len > 0) {
                shell_history_add(shell_buffer);
                command_execute(shell_buffer);
                shell_just_executed = true;
            }
            shell_buffer_len = 0;
            shell_cursor_pos = 0;
            shell_buffer[0] = '\0';
            shell_prompt();
        } else if (ch == '\t') {
            shell_tab_complete();
        } else if (ch == (char)INPUT_KEY_CTRL_C) {
            shell_set_cursor(shell_buffer_len);
            vga_write_string_with_color("^C", vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            vga_put_character('\n');
            shell_buffer_len = 0;
            shell_cursor_pos = 0;
            shell_buffer[0] = '\0';
            shell_history_index = shell_history_count;
            shell_prompt();
        } else if (ch == '\b') {
            if (shell_cursor_pos > 0) {
                bool is_ctrl_token = (shell_cursor_pos >= 2 &&
                                      shell_buffer[shell_cursor_pos - 2] == '^' &&
                                      shell_buffer[shell_cursor_pos - 1] >= 'A' &&
                                      shell_buffer[shell_cursor_pos - 1] <= 'Z');
                size_t del_count = is_ctrl_token ? 2 : 1;
                if (shell_cursor_pos == shell_buffer_len) {
                    shell_cursor_pos -= del_count;
                    shell_buffer_len -= del_count;
                    shell_buffer[shell_buffer_len] = '\0';
                    for (size_t d = 0; d < del_count; d++) {
                        vga_backspace();
                    }
                    shell_set_cursor(shell_cursor_pos);
                } else {
                    for (size_t i = shell_cursor_pos - del_count; i < shell_buffer_len; i++) {
                        shell_buffer[i] = shell_buffer[i + del_count];
                    }
                    shell_cursor_pos -= del_count;
                    shell_buffer_len -= del_count;
                    shell_buffer[shell_buffer_len] = '\0';

                    shell_set_cursor(0);
                    vga_write_string(shell_buffer);
                    for (size_t d = 0; d < del_count; d++) {
                        vga_put_character(' ');
                    }
                    shell_set_cursor(shell_cursor_pos);
                }
            }
        } else if (ch == (char)INPUT_KEY_LEFT) {
            if (shell_cursor_pos > 0) {
                shell_cursor_pos--;
                shell_set_cursor(shell_cursor_pos);
            }
        } else if (ch == (char)INPUT_KEY_RIGHT) {
            if (shell_cursor_pos < shell_buffer_len) {
                shell_cursor_pos++;
                shell_set_cursor(shell_cursor_pos);
            }
        } else if (ch == (char)INPUT_KEY_PAGE_UP) {
            framebuffer_scroll_view_page(-1);
        } else if (ch == (char)INPUT_KEY_PAGE_DOWN) {
            framebuffer_scroll_view_page(1);
        } else if (ch == (char)INPUT_KEY_UP) {
            if (shell_history_count > 0 && shell_history_index > 0) {
                shell_history_index--;
                shell_clear_input();
                string_copy_n(shell_buffer, shell_history[shell_history_index], SHELL_BUFFER_CAPACITY);
                shell_buffer_len = string_length(shell_buffer);
                shell_cursor_pos = shell_buffer_len;
                vga_write_string(shell_buffer);
            }
        } else if (ch == (char)INPUT_KEY_DOWN) {
            if (shell_history_index < shell_history_count) {
                shell_history_index++;
                shell_clear_input();
                if (shell_history_index < shell_history_count) {
                    string_copy_n(shell_buffer, shell_history[shell_history_index], SHELL_BUFFER_CAPACITY);
                    shell_buffer_len = string_length(shell_buffer);
                    shell_cursor_pos = shell_buffer_len;
                    vga_write_string(shell_buffer);
                }
            }
        } else if (ch >= 1 && ch <= 26 && ch != '\t' && ch != '\n' && ch != '\r' && ch != '\b') {
            char ctrl_letter = (char)('A' + ch - 1);
            if (shell_buffer_len < SHELL_BUFFER_CAPACITY - 2) {
                if (shell_cursor_pos == shell_buffer_len) {
                    shell_buffer[shell_cursor_pos++] = '^';
                    shell_buffer[shell_cursor_pos++] = ctrl_letter;
                    shell_buffer_len += 2;
                    shell_buffer[shell_buffer_len] = '\0';
                    vga_put_character('^');
                    vga_put_character(ctrl_letter);
                    shell_sync_after_typing();
                } else {
                    for (size_t i = shell_buffer_len + 2; i > shell_cursor_pos + 1; i--) {
                        shell_buffer[i] = shell_buffer[i - 2];
                    }
                    shell_buffer[shell_cursor_pos++] = '^';
                    shell_buffer[shell_cursor_pos++] = ctrl_letter;
                    shell_buffer_len += 2;
                    shell_buffer[shell_buffer_len] = '\0';

                    shell_set_cursor(0);
                    vga_write_string(shell_buffer);
                    shell_sync_after_typing();
                    shell_set_cursor(shell_cursor_pos);
                }
            }
        } else if (ch >= ' ' && ch <= '~') {
            if (shell_buffer_len < SHELL_BUFFER_CAPACITY - 1) {
                if (shell_cursor_pos == shell_buffer_len) {
                    shell_buffer[shell_cursor_pos] = ch;
                    shell_cursor_pos++;
                    shell_buffer_len++;
                    shell_buffer[shell_buffer_len] = '\0';
                    vga_put_character(ch);
                    shell_sync_after_typing();
                } else {
                    for (size_t i = shell_buffer_len + 1; i > shell_cursor_pos; i--) {
                        shell_buffer[i] = shell_buffer[i - 1];
                    }
                    shell_buffer[shell_cursor_pos] = ch;
                    shell_cursor_pos++;
                    shell_buffer_len++;
                    shell_buffer[shell_buffer_len] = '\0';

                    shell_set_cursor(0);
                    vga_write_string(shell_buffer);
                    shell_sync_after_typing();
                    shell_set_cursor(shell_cursor_pos);
                }
            }
        }
    }
}
