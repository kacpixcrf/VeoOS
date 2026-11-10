#include "kernel/editor/editor.h"
#include "kernel/fs/ramfs.h"
#include "drivers/video/vga.h"
#include "drivers/input/input.h"
#include "util/string.h"
#include "util/format.h"

static inline size_t editor_get_cols(void) {
    return vga_get_width();
}

static inline size_t editor_get_text_rows(void) {
    size_t total = vga_get_height();
    return total > 3 ? total - 3 : 1;
}

static inline size_t editor_get_status_row(void) {
    size_t total = vga_get_height();
    return total > 2 ? total - 2 : 0;
}

static inline size_t editor_get_help_row(void) {
    size_t total = vga_get_height();
    return total > 1 ? total - 1 : 0;
}

static char editor_buffer[RAMFS_FILE_CAPACITY];
static size_t editor_length;
static size_t editor_cursor;
static bool editor_dirty;
static char editor_filename[RAMFS_NAME_MAX];
static int editor_parent_dir;
static bool editor_is_new_file;
static char editor_status[64];
static size_t editor_row_offset;

static void editor_render(void);

static void editor_calculate_line_col(size_t pos, size_t* out_line, size_t* out_col) {
    size_t line = 1;
    size_t col = 1;
    size_t editor_cols = editor_get_cols();

    for (size_t i = 0; i < pos && i < editor_length; i++) {
        if (editor_buffer[i] == '\n') {
            line++;
            col = 1;
        } else {
            col++;
            if (col > editor_cols) {
                line++;
                col = 1;
            }
        }
    }

    *out_line = line;
    *out_col = col;
}

static void editor_update_cursor_pos(void) {
    size_t editor_cols = editor_get_cols();
    size_t editor_text_rows = editor_get_text_rows();
    uint8_t help_color = vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    uint8_t key_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    size_t r = 1, c = 0;
    for (size_t i = 0; i < editor_length; i++) {
        if (i == editor_cursor) {
            break;
        }
        if (editor_buffer[i] == '\n') {
            r++;
            c = 0;
        } else {
            c++;
            if (c >= editor_cols) {
                r++;
                c = 0;
            }
        }
    }

    if (r <= editor_row_offset || r > editor_row_offset + editor_text_rows) {
        editor_render();
        return;
    }

    size_t cur_line = 1, cur_col = 1;
    editor_calculate_line_col(editor_cursor, &cur_line, &cur_col);

    char line_buf[16];
    char col_buf[16];
    format_uint(line_buf, sizeof(line_buf), cur_line);
    format_uint(col_buf, sizeof(col_buf), cur_col);

    size_t pos_info_len = 3 + string_length(line_buf) + 5 + string_length(col_buf) + 1;
    size_t header_len = 13 + string_length(editor_filename) + (editor_dirty ? 2 : 0);
    size_t pad_start = header_len;
    size_t pad_end = (editor_cols > pos_info_len) ? editor_cols - pos_info_len : header_len;

    vga_set_cursor_position(pad_start, 0);
    for (size_t p = pad_start; p < pad_end; p++) {
        vga_put_character(' ');
    }

    vga_set_cursor_position(pad_end, 0);
    vga_write_string_with_color("Ln ", help_color);
    vga_write_string_with_color(line_buf, key_color);
    vga_write_string_with_color(" Col ", help_color);
    vga_write_string_with_color(col_buf, key_color);
    vga_put_character(' ');

    size_t screen_row = r - editor_row_offset;
    size_t screen_col = c;
    if (screen_row > editor_text_rows) {
        screen_row = editor_text_rows;
    }
    if (screen_col >= editor_cols) {
        screen_col = editor_cols - 1;
    }
    vga_set_cursor_position(screen_col, screen_row);
}

static void editor_render(void) {
    uint8_t bar_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t file_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    uint8_t text_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    uint8_t status_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t help_color = vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    uint8_t key_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    size_t editor_cols = editor_get_cols();
    size_t editor_text_rows = editor_get_text_rows();
    size_t editor_status_row = editor_get_status_row();
    size_t editor_help_row = editor_get_help_row();

    vga_set_cursor_position(0, 0);
    vga_write_string_with_color(" VeoEdit  |  ", bar_color);
    vga_write_string_with_color(editor_filename, file_color);

    if (editor_dirty) {
        vga_write_string_with_color(" *", bar_color);
    }

    size_t cur_line, cur_col;
    editor_calculate_line_col(editor_cursor, &cur_line, &cur_col);

    char line_buf[16];
    char col_buf[16];
    format_uint(line_buf, sizeof(line_buf), cur_line);
    format_uint(col_buf, sizeof(col_buf), cur_col);

    size_t pos_info_len = 3 + string_length(line_buf) + 5 + string_length(col_buf) + 1;
    size_t header_len = 13 + string_length(editor_filename) + (editor_dirty ? 2 : 0);
    size_t pad_end = (editor_cols > pos_info_len) ? editor_cols - pos_info_len : header_len;
    while (header_len < pad_end) {
        vga_put_character(' ');
        header_len++;
    }

    vga_write_string_with_color("Ln ", help_color);
    vga_write_string_with_color(line_buf, key_color);
    vga_write_string_with_color(" Col ", help_color);
    vga_write_string_with_color(col_buf, key_color);
    vga_put_character(' ');

    size_t cursor_row = 1;
    size_t cursor_col = 0;
    size_t r = 1;
    size_t c = 0;

    for (size_t i = 0; i < editor_length; i++) {
        if (i == editor_cursor) {
            cursor_row = r;
            cursor_col = c;
            break;
        }
        if (editor_buffer[i] == '\n') {
            r++;
            c = 0;
        } else {
            c++;
            if (c >= editor_cols) {
                r++;
                c = 0;
            }
        }
    }
    if (editor_cursor >= editor_length) {
        cursor_row = r;
        cursor_col = c;
    }

    if (cursor_row <= editor_row_offset) {
        editor_row_offset = (cursor_row > 0) ? (cursor_row - 1) : 0;
    } else if (cursor_row > editor_row_offset + editor_text_rows) {
        editor_row_offset = cursor_row - editor_text_rows;
    }

    r = 1;
    c = 0;
    for (size_t i = 0; i < editor_length; i++) {
        char ch = editor_buffer[i];
        if (r > editor_row_offset && r <= editor_row_offset + editor_text_rows) {
            size_t sr = r - editor_row_offset;
            if (ch == '\n') {
                while (c < editor_cols) {
                    vga_put_character_at(' ', text_color, c, sr);
                    c++;
                }
            } else {
                if (c < editor_cols) {
                    vga_put_character_at(ch, text_color, c, sr);
                }
            }
        }
        if (ch == '\n') {
            r++;
            c = 0;
            if (r > editor_row_offset + editor_text_rows) {
                break;
            }
        } else {
            c++;
            if (c >= editor_cols) {
                r++;
                c = 0;
                if (r > editor_row_offset + editor_text_rows) {
                    break;
                }
            }
        }
    }

    size_t clear_r = (r > editor_row_offset) ? (r - editor_row_offset) : 1;
    while (clear_r <= editor_text_rows) {
        while (c < editor_cols) {
            vga_put_character_at(' ', text_color, c, clear_r);
            c++;
        }
        clear_r++;
        c = 0;
    }

    vga_set_cursor_position(0, editor_status_row);
    for (size_t col_idx = 0; col_idx < editor_cols; col_idx++) {
        vga_put_character_at(' ', 0x07, col_idx, editor_status_row);
    }
    vga_set_cursor_position(1, editor_status_row);
    if (editor_status[0] != '\0') {
        vga_write_string_with_color(editor_status, status_color);
    }

    vga_set_cursor_position(0, editor_help_row);
    for (size_t col_idx = 0; col_idx < editor_cols; col_idx++) {
        vga_put_character_at(' ', 0x07, col_idx, editor_help_row);
    }
    vga_set_cursor_position(1, editor_help_row);
    vga_write_string_with_color("^S", key_color);
    vga_write_string_with_color(" Save   ", help_color);
    vga_write_string_with_color("^X", key_color);
    vga_write_string_with_color(" Exit   ", help_color);
    vga_write_string_with_color("^Y", key_color);
    vga_write_string_with_color(" Prev   ", help_color);
    vga_write_string_with_color("^V", key_color);
    vga_write_string_with_color(" Next", help_color);

    size_t screen_cursor_row = (cursor_row > editor_row_offset) ? (cursor_row - editor_row_offset) : 1;
    size_t screen_cursor_col = cursor_col;
    if (screen_cursor_row > editor_text_rows) {
        screen_cursor_row = editor_text_rows;
    }
    if (screen_cursor_col >= editor_cols) {
        screen_cursor_col = editor_cols - 1;
    }
    vga_set_cursor_position(screen_cursor_col, screen_cursor_row);
}

static void editor_move_up_lines(size_t lines) {
    while (lines-- > 0) {
        size_t line_start = editor_cursor;
        while (line_start > 0 && editor_buffer[line_start - 1] != '\n') {
            line_start--;
        }
        if (line_start == 0) {
            break;
        }
        size_t col = editor_cursor - line_start;
        size_t prev_line_end = line_start - 1;
        size_t prev_line_start = prev_line_end;
        while (prev_line_start > 0 && editor_buffer[prev_line_start - 1] != '\n') {
            prev_line_start--;
        }
        size_t prev_len = prev_line_end - prev_line_start;
        size_t target_col = (col < prev_len) ? col : prev_len;
        editor_cursor = prev_line_start + target_col;
    }
    editor_update_cursor_pos();
}

static void editor_move_down_lines(size_t lines) {
    while (lines-- > 0) {
        size_t line_start = editor_cursor;
        while (line_start > 0 && editor_buffer[line_start - 1] != '\n') {
            line_start--;
        }
        size_t col = editor_cursor - line_start;
        size_t cur_line_end = editor_cursor;
        while (cur_line_end < editor_length && editor_buffer[cur_line_end] != '\n') {
            cur_line_end++;
        }
        if (cur_line_end >= editor_length) {
            break;
        }
        size_t next_line_start = cur_line_end + 1;
        size_t next_line_end = next_line_start;
        while (next_line_end < editor_length && editor_buffer[next_line_end] != '\n') {
            next_line_end++;
        }
        size_t next_len = next_line_end - next_line_start;
        size_t target_col = (col < next_len) ? col : next_len;
        editor_cursor = next_line_start + target_col;
    }
    editor_update_cursor_pos();
}

void editor_open(const char* path) {
    if (path == NULL || path[0] == '\0') {
        vga_write_string("edit: missing filename\n");
        return;
    }

    char filename[RAMFS_NAME_MAX];
    int parent = ramfs_resolve_parent(path, filename, sizeof(filename));
    if (parent < 0) {
        uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string_with_color("edit: cannot open '", err_color);
        vga_write_string_with_color(path, err_color);
        vga_write_string_with_color("': No such directory\n", err_color);
        return;
    }

    if (!ramfs_is_valid_filename(filename)) {
        uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string_with_color("edit: invalid characters in filename: ", err_color);
        vga_write_string_with_color(filename, err_color);
        vga_put_character('\n');
        return;
    }

    struct ramfs_node* existing = ramfs_find_child(parent, filename);
    if (existing != NULL && existing->is_directory) {
        uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string_with_color("edit: cannot edit directory: ", err_color);
        vga_write_string_with_color(filename, err_color);
        vga_put_character('\n');
        return;
    }

    editor_parent_dir = parent;
    string_copy_n(editor_filename, filename, RAMFS_NAME_MAX);
    editor_status[0] = '\0';
    editor_dirty = false;
    editor_cursor = 0;
    editor_length = 0;
    editor_row_offset = 0;
    editor_buffer[0] = '\0';

    bool editor_is_base = false;
    if (existing != NULL) {
        editor_is_new_file = false;
        editor_is_base = existing->is_base;
        const char* src = ramfs_get_content(existing);
        string_copy_n(editor_buffer, src, sizeof(editor_buffer));
        editor_length = string_length(editor_buffer);
        if (editor_is_base) {
            string_copy_n(editor_status, "[Protected system file - read only]", sizeof(editor_status));
        }
    } else {
        editor_is_new_file = true;
    }

    vga_clear();
    editor_render();

    while (true) {
        char ch = input_read_character();
        if (ch == 0) {
            continue;
        }

        if (ch == (char)INPUT_KEY_CTRL_S) {
            if (editor_is_base) {
                string_copy_n(editor_status, "[Save Failed: Protected system file]", sizeof(editor_status));
                editor_render();
                continue;
            }
            if (editor_is_new_file) {
                if (ramfs_create_file_in_dir(editor_parent_dir, editor_filename)) {
                    editor_is_new_file = false;
                }
            }
            if (!editor_is_new_file) {
                if (ramfs_write_file_in_dir(editor_parent_dir, editor_filename, editor_buffer, editor_length)) {
                    editor_dirty = false;
                    string_copy_n(editor_status, "[Saved]", sizeof(editor_status));
                } else {
                    string_copy_n(editor_status, "[Save Failed: Write Error]", sizeof(editor_status));
                }
            } else {
                string_copy_n(editor_status, "[Save Failed: FS Full]", sizeof(editor_status));
            }
            editor_render();
        } else if (ch == (char)INPUT_KEY_CTRL_X || ch == (char)INPUT_KEY_CTRL_E) {
            if (!editor_dirty) {
                vga_clear();
                return;
            }

            size_t exit_status_row = editor_get_status_row();
            size_t exit_cols = editor_get_cols();
            vga_set_cursor_position(0, exit_status_row);
            for (size_t c = 0; c < exit_cols; c++) {
                vga_put_character_at(' ', 0x07, c, exit_status_row);
            }
            vga_set_cursor_position(1, exit_status_row);
            uint8_t warn_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_write_string_with_color("Unsaved changes. Exit without saving? (y/n): ", warn_color);

            while (true) {
                char ans = input_read_character();
                if (ans == 'y' || ans == 'Y') {
                    vga_clear();
                    return;
                }
                if (ans == 'n' || ans == 'N' || ans == 27) {
                    editor_status[0] = '\0';
                    editor_render();
                    break;
                }
            }
        } else if (ch == '\b') {
            if (editor_cursor > 0) {
                for (size_t i = editor_cursor - 1; i < editor_length; i++) {
                    editor_buffer[i] = editor_buffer[i + 1];
                }
                editor_length--;
                editor_cursor--;
                editor_dirty = true;
                editor_status[0] = '\0';
                editor_render();
            }
        } else if (ch == '\n') {
            if (editor_length < RAMFS_FILE_CAPACITY - 1) {
                for (size_t i = editor_length + 1; i > editor_cursor; i--) {
                    editor_buffer[i] = editor_buffer[i - 1];
                }
                editor_buffer[editor_cursor] = '\n';
                editor_length++;
                editor_cursor++;
                editor_dirty = true;
                editor_status[0] = '\0';
                editor_render();
            }
        } else if (ch == (char)INPUT_KEY_UP) {
            editor_move_up_lines(1);
        } else if (ch == (char)INPUT_KEY_DOWN) {
            editor_move_down_lines(1);
        } else if (ch == (char)INPUT_KEY_PAGE_UP || ch == (char)INPUT_KEY_CTRL_Y) {
            editor_move_up_lines(editor_get_text_rows());
        } else if (ch == (char)INPUT_KEY_PAGE_DOWN || ch == (char)INPUT_KEY_CTRL_V) {
            editor_move_down_lines(editor_get_text_rows());
        } else if (ch == (char)INPUT_KEY_LEFT) {
            if (editor_cursor > 0) {
                editor_cursor--;
                editor_update_cursor_pos();
            }
        } else if (ch == (char)INPUT_KEY_RIGHT) {
            if (editor_cursor < editor_length) {
                editor_cursor++;
                editor_update_cursor_pos();
            }
        } else if (ch >= ' ' && ch <= '~') {
            if (editor_length < RAMFS_FILE_CAPACITY - 1) {
                for (size_t i = editor_length + 1; i > editor_cursor; i--) {
                    editor_buffer[i] = editor_buffer[i - 1];
                }
                editor_buffer[editor_cursor] = ch;
                editor_length++;
                editor_cursor++;
                editor_dirty = true;
                editor_status[0] = '\0';
                editor_render();
            }
        }
    }
}
