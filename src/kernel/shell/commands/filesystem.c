#include "kernel/shell/commands/filesystem.h"
#include "kernel/shell/commands.h"
#include "kernel/fs/ramfs.h"
#include "kernel/core/user.h"
#include "kernel/editor/editor.h"
#include "drivers/video/vga.h"
#include "util/string.h"
#include "util/format.h"

static void handler_ls(const char* args) {
    char clean_path[128];
    string_trim(args, clean_path, sizeof(clean_path));

    int target_dir = ramfs_get_cwd();

    if (clean_path[0] != '\0') {
        int resolved = ramfs_resolve_directory(clean_path);
        if (resolved >= 0) {
            target_dir = resolved;
        } else {
            char filename[RAMFS_NAME_MAX];
            int parent = ramfs_resolve_parent(clean_path, filename, sizeof(filename));
            if (parent >= 0) {
                struct ramfs_node* node = ramfs_find_child(parent, filename);
                if (node != NULL && !node->is_directory) {
                    uint8_t file_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                    uint8_t size_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
                    vga_write_string("  ");
                    vga_write_string_with_color(node->name, file_color);
                    size_t name_len = string_length(node->name);
                    while (name_len < 20) {
                        vga_put_character(' ');
                        name_len++;
                    }
                    char size_buf[16];
                    format_uint(size_buf, sizeof(size_buf), node->size);
                    vga_write_string(size_buf);
                    vga_write_string_with_color(" B\n", size_color);
                    return;
                }
            }
            uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_write_string_with_color("ls: cannot access '", err_color);
            vga_write_string_with_color(clean_path, err_color);
            vga_write_string_with_color("': No such file or directory\n", err_color);
            return;
        }
    }

    int children[64];
    int count = ramfs_list_children(target_dir, children, 64);

    uint8_t dir_color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    uint8_t file_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    uint8_t size_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (int i = 0; i < count; i++) {
        struct ramfs_node* node = ramfs_get_node(children[i]);
        if (node == NULL) {
            continue;
        }
        vga_write_string("  ");
        if (node->is_directory) {
            vga_write_string_with_color(node->name, dir_color);
            vga_write_string_with_color("/", dir_color);
            size_t name_len = string_length(node->name) + 1;
            while (name_len < 20) {
                vga_put_character(' ');
                name_len++;
            }
            vga_write_string_with_color("<DIR>\n", dir_color);
        } else {
            vga_write_string_with_color(node->name, file_color);
            size_t name_len = string_length(node->name);
            while (name_len < 20) {
                vga_put_character(' ');
                name_len++;
            }
            char size_buf[16];
            format_uint(size_buf, sizeof(size_buf), node->size);
            vga_write_string(size_buf);
            vga_write_string_with_color(" B\n", size_color);
        }
    }
}

static void handler_pwd(const char* args) {
    (void)args;
    char path[128];
    ramfs_get_cwd_path(path, sizeof(path));
    vga_write_string(path);
    vga_put_character('\n');
}

static void handler_cd(const char* args) {
    char clean_path[128];
    string_trim(args, clean_path, sizeof(clean_path));

    if (clean_path[0] == '\0' || string_compare(clean_path, "~") == 0) {
        ramfs_change_directory("~");
        return;
    }

    if (string_compare(clean_path, "-") == 0) {
        if (ramfs_change_directory("-")) {
            char path[128];
            ramfs_get_cwd_path(path, sizeof(path));
            vga_write_string(path);
            vga_put_character('\n');
            return;
        }
        uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_string_with_color("cd: OLDPWD not set\n", err_color);
        return;
    }

    if (!ramfs_change_directory(clean_path)) {
        uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        char filename[RAMFS_NAME_MAX];
        int parent = ramfs_resolve_parent(clean_path, filename, sizeof(filename));
        if (parent >= 0) {
            struct ramfs_node* node = ramfs_find_child(parent, filename);
            if (node != NULL && !node->is_directory) {
                vga_write_string_with_color("cd: not a directory: ", err_color);
                vga_write_string_with_color(clean_path, err_color);
                vga_put_character('\n');
                return;
            }
        }
        vga_write_string_with_color("cd: no such directory: ", err_color);
        vga_write_string_with_color(clean_path, err_color);
        vga_put_character('\n');
    }
}

static void handler_mkdir(const char* args) {
    char clean_path[128];
    string_trim(args, clean_path, sizeof(clean_path));

    if (clean_path[0] == '\0') {
        vga_write_string("mkdir: missing operand\n");
        return;
    }

    char filename[RAMFS_NAME_MAX];
    int parent = ramfs_resolve_parent(clean_path, filename, sizeof(filename));
    uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    if (parent < 0) {
        vga_write_string_with_color("mkdir: cannot create directory '", err_color);
        vga_write_string_with_color(clean_path, err_color);
        vga_write_string_with_color("': No such directory\n", err_color);
        return;
    }

    struct ramfs_node* parent_node = ramfs_get_node(parent);
    if (parent_node != NULL && parent_node->is_base && parent != ramfs_get_home_dir()) {
        vga_write_string_with_color("mkdir: cannot create directory '", err_color);
        vga_write_string_with_color(clean_path, err_color);
        vga_write_string_with_color("': Protected system directory\n", err_color);
        return;
    }

    if (!ramfs_is_valid_filename(filename)) {
        vga_write_string_with_color("mkdir: invalid directory name: ", err_color);
        vga_write_string_with_color(filename, err_color);
        vga_put_character('\n');
        return;
    }

    if (ramfs_find_child(parent, filename) != NULL) {
        vga_write_string_with_color("mkdir: cannot create directory '", err_color);
        vga_write_string_with_color(clean_path, err_color);
        vga_write_string_with_color("': File exists\n", err_color);
        return;
    }

    if (!ramfs_create_directory_in_dir(parent, filename)) {
        vga_write_string_with_color("mkdir: filesystem full\n", err_color);
    }
}

static void handler_touch(const char* args) {
    char clean_path[128];
    string_trim(args, clean_path, sizeof(clean_path));

    if (clean_path[0] == '\0') {
        vga_write_string("touch: missing operand\n");
        return;
    }

    char filename[RAMFS_NAME_MAX];
    int parent = ramfs_resolve_parent(clean_path, filename, sizeof(filename));
    uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    if (parent < 0) {
        vga_write_string_with_color("touch: cannot touch '", err_color);
        vga_write_string_with_color(clean_path, err_color);
        vga_write_string_with_color("': No such directory\n", err_color);
        return;
    }

    struct ramfs_node* parent_touch_node = ramfs_get_node(parent);
    if (parent_touch_node != NULL && parent_touch_node->is_base && parent != ramfs_get_home_dir()) {
        vga_write_string_with_color("touch: cannot touch '", err_color);
        vga_write_string_with_color(clean_path, err_color);
        vga_write_string_with_color("': Protected system directory\n", err_color);
        return;
    }

    if (!ramfs_is_valid_filename(filename)) {
        vga_write_string_with_color("touch: invalid filename: ", err_color);
        vga_write_string_with_color(filename, err_color);
        vga_put_character('\n');
        return;
    }

    struct ramfs_node* existing = ramfs_find_child(parent, filename);
    if (existing != NULL) {
        if (existing->is_directory) {
            vga_write_string_with_color("touch: cannot touch '", err_color);
            vga_write_string_with_color(clean_path, err_color);
            vga_write_string_with_color("': Is a directory\n", err_color);
        }
        return;
    }

    if (!ramfs_create_file_in_dir(parent, filename)) {
        vga_write_string_with_color("touch: filesystem full\n", err_color);
    }
}

static void handler_rm(const char* args) {
    char token[128];
    const char* stream = args;

    bool recursive = false;
    bool force = false;
    char clean_path[128];
    clean_path[0] = '\0';

    while (string_next_token(&stream, token, sizeof(token))) {
        if (string_compare(token, "-r") == 0 || string_compare(token, "-R") == 0) {
            recursive = true;
        } else if (string_compare(token, "-f") == 0 || string_compare(token, "-F") == 0) {
            force = true;
        } else if (string_compare(token, "-rf") == 0 || string_compare(token, "-fr") == 0 ||
                   string_compare(token, "-Rf") == 0 || string_compare(token, "-rF") == 0 ||
                   string_compare(token, "-fR") == 0 || string_compare(token, "-FR") == 0) {
            recursive = true;
            force = true;
        } else if (clean_path[0] == '\0') {
            string_copy_n(clean_path, token, sizeof(clean_path));
        }
    }

    uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    if (clean_path[0] == '\0') {
        if (!force) {
            vga_write_string("rm: missing operand\n");
        }
        return;
    }

    if (string_compare(clean_path, "/") == 0 || string_compare(clean_path, "~") == 0 ||
        string_compare(clean_path, VEOOS_HOME_DIR) == 0) {
        vga_write_string_with_color("rm: cannot remove root or home directory\n", err_color);
        return;
    }

    char filename[RAMFS_NAME_MAX];
    int parent = ramfs_resolve_parent(clean_path, filename, sizeof(filename));

    if (parent < 0) {
        if (!force) {
            vga_write_string_with_color("rm: cannot remove '", err_color);
            vga_write_string_with_color(clean_path, err_color);
            vga_write_string_with_color("': No such file or directory\n", err_color);
        }
        return;
    }

    struct ramfs_node* node = ramfs_find_child(parent, filename);
    if (node == NULL) {
        if (!force) {
            vga_write_string_with_color("rm: cannot remove '", err_color);
            vga_write_string_with_color(clean_path, err_color);
            vga_write_string_with_color("': No such file or directory\n", err_color);
        }
        return;
    }

    if (node->is_base) {
        vga_write_string_with_color("rm: cannot remove '", err_color);
        vga_write_string_with_color(clean_path, err_color);
        vga_write_string_with_color("': Protected system file\n", err_color);
        return;
    }

    int node_idx = -1;
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        if (ramfs_get_node(i) == node) {
            node_idx = i;
            break;
        }
    }

    int curr = ramfs_get_cwd();
    while (curr != 0) {
        if (curr == node_idx) {
            vga_write_string_with_color("rm: cannot remove active working directory\n", err_color);
            return;
        }
        struct ramfs_node* curr_node = ramfs_get_node(curr);
        if (curr_node == NULL) {
            break;
        }
        curr = curr_node->parent_index;
    }

    if (node_idx == ramfs_get_cwd()) {
        vga_write_string_with_color("rm: cannot remove active working directory\n", err_color);
        return;
    }

    if (node->is_directory) {
        if (ramfs_has_children(node_idx)) {
            if (!recursive) {
                vga_write_string_with_color("rm: cannot remove '", err_color);
                vga_write_string_with_color(clean_path, err_color);
                vga_write_string_with_color("': Directory not empty (use -r)\n", err_color);
                return;
            }
            ramfs_remove_recursive(node_idx);
            return;
        }
    }

    ramfs_remove_in_dir(parent, filename);
}

static void handler_cp(const char* args) {
    char token[128];
    const char* stream = args;

    bool recursive = false;
    char src[128];
    char dst[128];
    src[0] = '\0';
    dst[0] = '\0';

    while (string_next_token(&stream, token, sizeof(token))) {
        if (string_compare(token, "-r") == 0 || string_compare(token, "-R") == 0) {
            recursive = true;
        } else if (src[0] == '\0') {
            string_copy_n(src, token, sizeof(src));
        } else if (dst[0] == '\0') {
            string_copy_n(dst, token, sizeof(dst));
        }
    }

    uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    if (src[0] == '\0' || dst[0] == '\0') {
        vga_write_string_with_color("cp: missing file operand\n", err_color);
        return;
    }

    char src_filename[RAMFS_NAME_MAX];
    int src_parent = ramfs_resolve_parent(src, src_filename, sizeof(src_filename));
    if (src_parent >= 0) {
        struct ramfs_node* src_node = ramfs_find_child(src_parent, src_filename);
        if (src_node != NULL && src_node->is_directory && !recursive) {
            vga_write_string_with_color("cp: -r not specified; omitting directory '", err_color);
            vga_write_string_with_color(src, err_color);
            vga_write_string_with_color("'\n", err_color);
            return;
        }
    }

    if (!ramfs_copy(src, dst, recursive)) {
        vga_write_string_with_color("cp: cannot copy '", err_color);
        vga_write_string_with_color(src, err_color);
        vga_write_string_with_color("' to '", err_color);
        vga_write_string_with_color(dst, err_color);
        vga_write_string_with_color("'\n", err_color);
    }
}

static void handler_mv(const char* args) {
    char token[128];
    const char* stream = args;

    char src[128];
    char dst[128];
    src[0] = '\0';
    dst[0] = '\0';

    while (string_next_token(&stream, token, sizeof(token))) {
        if (src[0] == '\0') {
            string_copy_n(src, token, sizeof(src));
        } else if (dst[0] == '\0') {
            string_copy_n(dst, token, sizeof(dst));
        }
    }

    uint8_t err_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    if (src[0] == '\0' || dst[0] == '\0') {
        vga_write_string_with_color("mv: missing file operand\n", err_color);
        return;
    }

    if (!ramfs_move(src, dst)) {
        vga_write_string_with_color("mv: cannot move '", err_color);
        vga_write_string_with_color(src, err_color);
        vga_write_string_with_color("' to '", err_color);
        vga_write_string_with_color(dst, err_color);
        vga_write_string_with_color("'\n", err_color);
    }
}

static void handler_edit(const char* args) {
    char clean_path[128];
    string_trim(args, clean_path, sizeof(clean_path));

    if (clean_path[0] == '\0') {
        vga_write_string("edit: missing filename\n");
        return;
    }

    editor_open(clean_path);
}

void commands_filesystem_register(void) {
    commands_register(
        "ls",
        NULL,
        "list directory contents",
        "ls [path]",
        "  Arguments:\n    [path]     directory or file to inspect (default: current directory)",
        handler_ls
    );

    commands_register(
        "pwd",
        NULL,
        "print working directory",
        "pwd",
        "  Output:\n    Prints the full absolute path of the current working directory.",
        handler_pwd
    );

    commands_register(
        "cd",
        "chdir",
        "change working directory",
        "cd [path] | chdir [path]",
        "  Paths:\n    ~          go to home directory (" VEOOS_HOME_DIR ")\n    -          switch to previous working directory\n    /          go to root directory\n    ..         go to parent directory\n    [dir]      go to relative or absolute path",
        handler_cd
    );

    commands_register(
        "mkdir",
        NULL,
        "create new directory",
        "mkdir <directory>",
        "  Arguments:\n    <directory> path of directory to create (e.g. 'docs/newdir')",
        handler_mkdir
    );

    commands_register(
        "touch",
        NULL,
        "create empty file",
        "touch <filename>",
        "  Arguments:\n    <filename>  path of file to create",
        handler_touch
    );

    commands_register(
        "edit",
        NULL,
        "open fullscreen text editor",
        "edit <path>",
        "  Shortcuts:\n    Ctrl+S     save changes to file\n    Ctrl+E     exit editor",
        handler_edit
    );

    commands_register(
        "rm",
        NULL,
        "remove file or directory",
        "rm [-r] [-f] <path>",
        "  Flags:\n    -r, -R     remove directories and their contents recursively\n    -f         force: ignore nonexistent files and never prompt\n    -rf, -fr   combine recursive and force options",
        handler_rm
    );

    commands_register(
        "cp",
        NULL,
        "copy file or directory to destination",
        "cp [-r] <source> <destination>",
        "  Flags:\n    -r         copy directories recursively\n  Arguments:\n    <source>       file or directory to copy\n    <destination>  target path or directory",
        handler_cp
    );

    commands_register(
        "mv",
        NULL,
        "move or rename file or directory",
        "mv <source> <destination>",
        "  Arguments:\n    <source>       file or directory to move\n    <destination>  target path or new name",
        handler_mv
    );
}
