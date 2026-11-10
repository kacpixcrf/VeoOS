#include "kernel/fs/ramfs.h"
#include "kernel/fs/base_files.h"
#include "kernel/core/user.h"
#include "kernel/core/version.h"
#include "kernel/memory/memory.h"
#include "util/string.h"

static struct ramfs_node nodes[RAMFS_MAX_NODES];
static int cwd_index = 0;
static int home_dir_index = 0;
static int previous_dir_index = -1;

static int ramfs_create_base_dir(int parent, const char* name) {
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        if (!nodes[i].is_used) {
            nodes[i].is_used = true;
            nodes[i].is_base = true;
            nodes[i].is_directory = true;
            nodes[i].parent_index = parent;
            string_copy_n(nodes[i].name, name, RAMFS_NAME_MAX);
            nodes[i].size = 0;
            nodes[i].read_only_data = NULL;
            nodes[i].heap_content = NULL;
            return i;
        }
    }
    return -1;
}

static int ramfs_create_base_file_ro(int parent, const char* name, const char* ro_data, size_t size) {
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        if (!nodes[i].is_used) {
            nodes[i].is_used = true;
            nodes[i].is_base = true;
            nodes[i].is_directory = false;
            nodes[i].parent_index = parent;
            string_copy_n(nodes[i].name, name, RAMFS_NAME_MAX);
            nodes[i].read_only_data = ro_data;
            nodes[i].size = size;
            nodes[i].heap_content = NULL;
            return i;
        }
    }
    return -1;
}

static int ramfs_create_base_file(int parent, const char* name, const char* content) {
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        if (!nodes[i].is_used) {
            nodes[i].is_used = true;
            nodes[i].is_base = true;
            nodes[i].is_directory = false;
            nodes[i].parent_index = parent;
            string_copy_n(nodes[i].name, name, RAMFS_NAME_MAX);
            nodes[i].read_only_data = NULL;
            size_t len = content ? string_length(content) : 0;
            if (len >= RAMFS_FILE_CAPACITY) {
                len = RAMFS_FILE_CAPACITY - 1;
            }
            if (len > 0 && content) {
                char* buf = (char*)memory_alloc(len + 1);
                memory_copy(buf, content, len);
                buf[len] = '\0';
                nodes[i].heap_content = buf;
            } else {
                nodes[i].heap_content = NULL;
            }
            nodes[i].size = len;
            return i;
        }
    }
    return -1;
}

void ramfs_initialize(void) {
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        nodes[i].is_used = false;
        nodes[i].is_base = false;
        nodes[i].is_directory = false;
        nodes[i].name[0] = '\0';
        nodes[i].parent_index = 0;
        nodes[i].size = 0;
        nodes[i].read_only_data = NULL;
        nodes[i].heap_content = NULL;
    }

    nodes[0].is_used = true;
    nodes[0].is_base = true;
    nodes[0].is_directory = true;
    nodes[0].name[0] = '/';
    nodes[0].name[1] = '\0';
    nodes[0].parent_index = 0;

    int src_dir = ramfs_create_base_dir(0, "src");
    ramfs_create_base_file_ro(src_dir, "main.c", base_main_c, base_main_c_len);
    ramfs_create_base_file_ro(src_dir, "version.h", base_version_h, base_version_h_len);
    ramfs_create_base_file_ro(src_dir, "kernel.h", base_kernel_h, base_kernel_h_len);

    int etc_dir = ramfs_create_base_dir(0, "etc");
    ramfs_create_base_file_ro(etc_dir, "hostname", base_hostname, base_hostname_len);
    ramfs_create_base_file_ro(etc_dir, "os-release", base_os_release, base_os_release_len);

    int home_dir = ramfs_create_base_dir(0, "home");
    int amnesis_dir = ramfs_create_base_dir(home_dir, VEOOS_USER_NAME);
    home_dir_index = (amnesis_dir >= 0) ? amnesis_dir : 0;

    int docs_dir = ramfs_create_base_dir(home_dir_index, "docs");
    if (docs_dir >= 0) {
        nodes[docs_dir].is_base = false;
    }

    const char* default_text =
        "Welcome to VeoOS.\n"
        "This is a clean bare metal system created by " VEOOS_AUTHOR ".\n"
        "User identity: " VEOOS_USER_NAME "\n"
        "Home directory: " VEOOS_HOME_DIR "\n"
        "Use edit to view and modify files.\n"
        "Press Ctrl+S to save changes and Ctrl+X to exit.\n";

    int readme_idx = ramfs_create_base_file(docs_dir, "readme.txt", default_text);
    if (readme_idx >= 0) {
        nodes[readme_idx].is_base = false;
    }

    cwd_index = home_dir_index;
    previous_dir_index = -1;
}

int ramfs_get_cwd(void) {
    return cwd_index;
}

int ramfs_get_home_dir(void) {
    return home_dir_index;
}

bool ramfs_is_valid_filename(const char* name) {
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    size_t len = string_length(name);
    if (len >= RAMFS_NAME_MAX) {
        return false;
    }

    if (string_compare(name, ".") == 0 || string_compare(name, "..") == 0) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || c < 32 || c > 126) {
            return false;
        }
    }

    return true;
}

struct ramfs_node* ramfs_get_node(int index) {
    if (index >= 0 && index < RAMFS_MAX_NODES && nodes[index].is_used) {
        return &nodes[index];
    }
    return NULL;
}

const char* ramfs_get_content(const struct ramfs_node* node) {
    if (node == NULL) {
        return "";
    }
    if (node->read_only_data != NULL) {
        return node->read_only_data;
    }
    if (node->heap_content != NULL) {
        return node->heap_content;
    }
    return "";
}

struct ramfs_node* ramfs_find_child(int parent, const char* name) {
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        if (!nodes[i].is_used || i == parent || nodes[i].parent_index != parent) {
            continue;
        }
        if (string_compare(nodes[i].name, name) == 0) {
            return &nodes[i];
        }
    }
    return NULL;
}

int ramfs_list_children(int parent, int* results, int max_results) {
    int count = 0;
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        if (!nodes[i].is_used || i == parent || nodes[i].parent_index != parent) {
            continue;
        }
        if (count < max_results) {
            results[count++] = i;
        }
    }
    return count;
}

bool ramfs_has_children(int dir_index) {
    for (int i = 0; i < RAMFS_MAX_NODES; i++) {
        if (nodes[i].is_used && i != dir_index && nodes[i].parent_index == dir_index) {
            return true;
        }
    }
    return false;
}

int ramfs_resolve_directory(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return cwd_index;
    }

    int current = cwd_index;
    size_t i = 0;

    if (path[0] == '~') {
        current = home_dir_index;
        i = 1;
        while (path[i] == '/') {
            i++;
        }
        if (path[i] == '\0') {
            return current;
        }
    } else if (path[0] == '/') {
        current = 0;
        i = 1;
        while (path[i] == '/') {
            i++;
        }
        if (path[i] == '\0') {
            return 0;
        }
    }

    char segment[RAMFS_NAME_MAX];

    while (path[i] != '\0') {
        size_t seg_len = 0;
        while (path[i] != '\0' && path[i] != '/') {
            if (seg_len < RAMFS_NAME_MAX - 1) {
                segment[seg_len++] = path[i];
            }
            i++;
        }
        segment[seg_len] = '\0';

        while (path[i] == '/') {
            i++;
        }

        if (string_compare(segment, ".") == 0) {
            continue;
        }
        if (string_compare(segment, "..") == 0) {
            current = nodes[current].parent_index;
            continue;
        }

        struct ramfs_node* child = ramfs_find_child(current, segment);
        if (child == NULL || !child->is_directory) {
            return -1;
        }
        current = (int)(child - nodes);
    }

    return current;
}

int ramfs_resolve_parent(const char* path, char* out_filename, size_t max_filename_len) {
    if (path == NULL || path[0] == '\0' || out_filename == NULL || max_filename_len == 0) {
        return -1;
    }

    int last_slash = -1;
    size_t len = string_length(path);
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') {
            last_slash = (int)i;
        }
    }

    if (last_slash == -1) {
        if (path[0] == '~' && path[1] == '\0') {
            struct ramfs_node* home_node = &nodes[home_dir_index];
            string_copy_n(out_filename, home_node->name, max_filename_len);
            return home_node->parent_index;
        }
        string_copy_n(out_filename, path, max_filename_len);
        return cwd_index;
    }

    if (last_slash == 0) {
        string_copy_n(out_filename, path + 1, max_filename_len);
        return 0;
    }

    char dir_part[128];
    if ((size_t)last_slash >= sizeof(dir_part)) {
        return -1;
    }

    for (int i = 0; i < last_slash; i++) {
        dir_part[i] = path[i];
    }
    dir_part[last_slash] = '\0';

    string_copy_n(out_filename, path + last_slash + 1, max_filename_len);
    return ramfs_resolve_directory(dir_part);
}

bool ramfs_change_directory(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }
    int target = -1;
    if (string_compare(path, "-") == 0) {
        if (previous_dir_index < 0 || previous_dir_index >= RAMFS_MAX_NODES ||
            !nodes[previous_dir_index].is_used || !nodes[previous_dir_index].is_directory) {
            return false;
        }
        target = previous_dir_index;
    } else {
        target = ramfs_resolve_directory(path);
    }
    if (target >= 0 && target < RAMFS_MAX_NODES && nodes[target].is_used && nodes[target].is_directory) {
        previous_dir_index = cwd_index;
        cwd_index = target;
        return true;
    }
    return false;
}

void ramfs_get_cwd_path(char* buffer, size_t max_length) {
    if (max_length < 2) {
        return;
    }

    if (cwd_index == 0) {
        buffer[0] = '/';
        buffer[1] = '\0';
        return;
    }

    int ancestors[16];
    int count = 0;
    int curr = cwd_index;

    while (curr != 0 && count < 16) {
        ancestors[count++] = curr;
        curr = nodes[curr].parent_index;
    }

    size_t out_pos = 0;
    for (int i = count - 1; i >= 0; i--) {
        if (out_pos + 1 >= max_length) {
            break;
        }
        buffer[out_pos++] = '/';

        const char* name = nodes[ancestors[i]].name;
        while (*name != '\0' && out_pos + 1 < max_length) {
            buffer[out_pos++] = *name++;
        }
    }
    buffer[out_pos] = '\0';
}

bool ramfs_create_directory_in_dir(int parent, const char* name) {
    if (!ramfs_is_valid_filename(name)) {
        return false;
    }

    if (parent < 0 || parent >= RAMFS_MAX_NODES || !nodes[parent].is_used || !nodes[parent].is_directory) {
        return false;
    }

    if (nodes[parent].is_base && parent != home_dir_index) {
        return false;
    }

    if (ramfs_find_child(parent, name) != NULL) {
        return false;
    }

    for (int i = 1; i < RAMFS_MAX_NODES; i++) {
        if (!nodes[i].is_used) {
            nodes[i].is_used = true;
            nodes[i].is_base = false;
            nodes[i].is_directory = true;
            string_copy_n(nodes[i].name, name, RAMFS_NAME_MAX);
            nodes[i].parent_index = parent;
            nodes[i].size = 0;
            nodes[i].read_only_data = NULL;
            nodes[i].heap_content = NULL;
            return true;
        }
    }
    return false;
}

bool ramfs_create_directory(const char* name) {
    return ramfs_create_directory_in_dir(cwd_index, name);
}

bool ramfs_create_file_in_dir(int parent, const char* name) {
    if (!ramfs_is_valid_filename(name)) {
        return false;
    }

    if (parent < 0 || parent >= RAMFS_MAX_NODES || !nodes[parent].is_used || !nodes[parent].is_directory) {
        return false;
    }

    if (nodes[parent].is_base && parent != home_dir_index) {
        return false;
    }

    if (ramfs_find_child(parent, name) != NULL) {
        return false;
    }

    for (int i = 1; i < RAMFS_MAX_NODES; i++) {
        if (!nodes[i].is_used) {
            nodes[i].is_used = true;
            nodes[i].is_base = false;
            nodes[i].is_directory = false;
            string_copy_n(nodes[i].name, name, RAMFS_NAME_MAX);
            nodes[i].parent_index = parent;
            nodes[i].size = 0;
            nodes[i].read_only_data = NULL;
            nodes[i].heap_content = NULL;
            return true;
        }
    }
    return false;
}

bool ramfs_create_file(const char* name) {
    return ramfs_create_file_in_dir(cwd_index, name);
}

bool ramfs_write_file_in_dir(int parent, const char* name, const char* data, size_t length) {
    struct ramfs_node* node = ramfs_find_child(parent, name);

    if (node != NULL && (node->is_directory || node->is_base)) {
        return false;
    }

    if (length >= RAMFS_FILE_CAPACITY) {
        length = RAMFS_FILE_CAPACITY - 1;
    }

    if (node != NULL) {
        if (node->heap_content != NULL) {
            memory_free_page((uintptr_t)node->heap_content);
        }
        uintptr_t page = memory_alloc_page();
        if (page == 0) {
            return false;
        }
        char* buf = (char*)page;
        memory_copy(buf, data, length);
        buf[length] = '\0';
        node->heap_content = buf;
        node->size = length;
        return true;
    }

    if (!ramfs_create_file_in_dir(parent, name)) {
        return false;
    }
    node = ramfs_find_child(parent, name);
    if (node == NULL) {
        return false;
    }
    uintptr_t page = memory_alloc_page();
    if (page == 0) {
        return false;
    }
    char* buf = (char*)page;
    memory_copy(buf, data, length);
    buf[length] = '\0';
    node->heap_content = buf;
    node->size = length;
    return true;
}

bool ramfs_write_file(const char* name, const char* data, size_t length) {
    return ramfs_write_file_in_dir(cwd_index, name, data, length);
}

bool ramfs_remove_in_dir(int parent, const char* name) {
    struct ramfs_node* node = ramfs_find_child(parent, name);
    if (node == NULL || node->is_base) {
        return false;
    }

    int node_idx = (int)(node - nodes);
    if (node->is_directory && ramfs_has_children(node_idx)) {
        return false;
    }

    if (node_idx == previous_dir_index) {
        previous_dir_index = -1;
    }

    node->is_used = false;
    node->name[0] = '\0';
    node->size = 0;
    node->read_only_data = NULL;
    if (node->heap_content != NULL) {
        memory_free_page((uintptr_t)node->heap_content);
    }
    node->heap_content = NULL;
    return true;
}

bool ramfs_remove(const char* name) {
    return ramfs_remove_in_dir(cwd_index, name);
}

bool ramfs_remove_recursive(int node_index) {
    if (node_index <= 0 || node_index >= RAMFS_MAX_NODES || !nodes[node_index].is_used) {
        return false;
    }

    struct ramfs_node* target_node = &nodes[node_index];
    if (target_node->is_base) {
        return false;
    }

    if (node_index == previous_dir_index) {
        previous_dir_index = -1;
    }

    if (target_node->is_directory) {
        int children[64];
        int count = ramfs_list_children(node_index, children, 64);
        for (int i = 0; i < count; i++) {
            ramfs_remove_recursive(children[i]);
        }
    }

    return ramfs_remove_in_dir(target_node->parent_index, target_node->name);
}

static bool ramfs_copy_node_recursive(int src_idx, int dst_parent_idx, const char* new_name) {
    struct ramfs_node* src_node = ramfs_get_node(src_idx);
    if (src_node == NULL) {
        return false;
    }

    if (!src_node->is_directory) {
        if (!ramfs_create_file_in_dir(dst_parent_idx, new_name)) {
            return false;
        }
        struct ramfs_node* new_file = ramfs_find_child(dst_parent_idx, new_name);
        if (new_file == NULL) {
            return false;
        }
        const char* src_content = ramfs_get_content(src_node);
        size_t copy_len = src_node->size;
        if (copy_len >= RAMFS_FILE_CAPACITY) {
            copy_len = RAMFS_FILE_CAPACITY - 1;
        }
        char* buf = (char*)memory_alloc(copy_len + 1);
        memory_copy(buf, src_content, copy_len);
        buf[copy_len] = '\0';
        new_file->heap_content = buf;
        new_file->size = copy_len;
        return true;
    }

    if (!ramfs_create_directory_in_dir(dst_parent_idx, new_name)) {
        return false;
    }
    struct ramfs_node* new_dir = ramfs_find_child(dst_parent_idx, new_name);
    if (new_dir == NULL) {
        return false;
    }
    int new_dir_idx = (int)(new_dir - nodes);

    int children[64];
    int count = ramfs_list_children(src_idx, children, 64);
    for (int i = 0; i < count; i++) {
        struct ramfs_node* child = ramfs_get_node(children[i]);
        if (child == NULL) {
            continue;
        }
        if (!ramfs_copy_node_recursive(children[i], new_dir_idx, child->name)) {
            return false;
        }
    }
    return true;
}

bool ramfs_copy(const char* src_path, const char* dst_path, bool recursive) {
    if (src_path == NULL || src_path[0] == '\0' || dst_path == NULL || dst_path[0] == '\0') {
        return false;
    }

    char src_filename[RAMFS_NAME_MAX];
    int src_parent = ramfs_resolve_parent(src_path, src_filename, sizeof(src_filename));
    if (src_parent < 0) {
        return false;
    }

    struct ramfs_node* src_node = ramfs_find_child(src_parent, src_filename);
    if (src_node == NULL) {
        return false;
    }

    int src_node_idx = (int)(src_node - nodes);

    if (src_node->is_directory && !recursive) {
        return false;
    }

    int target_parent = -1;
    char target_filename[RAMFS_NAME_MAX];

    int existing_dir = ramfs_resolve_directory(dst_path);
    if (existing_dir >= 0) {
        target_parent = existing_dir;
        string_copy_n(target_filename, src_node->name, sizeof(target_filename));
    } else {
        target_parent = ramfs_resolve_parent(dst_path, target_filename, sizeof(target_filename));
    }

    if (target_parent < 0 || !ramfs_is_valid_filename(target_filename)) {
        return false;
    }

    if (nodes[target_parent].is_base && target_parent != home_dir_index) {
        return false;
    }

    if (src_node->is_directory) {
        int check = target_parent;
        while (check != 0) {
            if (check == src_node_idx) {
                return false;
            }
            check = nodes[check].parent_index;
        }
        if (target_parent == src_node_idx) {
            return false;
        }

        struct ramfs_node* existing = ramfs_find_child(target_parent, target_filename);
        if (existing != NULL) {
            if (!existing->is_directory) {
                return false;
            }
            int existing_idx = (int)(existing - nodes);
            int check_exist = existing_idx;
            while (check_exist != 0) {
                if (check_exist == src_node_idx) {
                    return false;
                }
                check_exist = nodes[check_exist].parent_index;
            }
            target_parent = existing_idx;
            string_copy_n(target_filename, src_node->name, sizeof(target_filename));
            if (ramfs_find_child(target_parent, target_filename) != NULL) {
                return false;
            }
        }

        return ramfs_copy_node_recursive(src_node_idx, target_parent, target_filename);
    }

    struct ramfs_node* dst_node = ramfs_find_child(target_parent, target_filename);
    const char* src_content = ramfs_get_content(src_node);
    if (dst_node != NULL) {
        if (dst_node->is_directory) {
            return false;
        }
        return ramfs_write_file_in_dir(target_parent, target_filename, src_content, src_node->size);
    }

    if (!ramfs_create_file_in_dir(target_parent, target_filename)) {
        return false;
    }

    return ramfs_write_file_in_dir(target_parent, target_filename, src_content, src_node->size);
}

bool ramfs_copy_file(const char* src_path, const char* dst_path) {
    return ramfs_copy(src_path, dst_path, false);
}

bool ramfs_move(const char* src_path, const char* dst_path) {
    if (src_path == NULL || src_path[0] == '\0' || dst_path == NULL || dst_path[0] == '\0') {
        return false;
    }

    char src_filename[RAMFS_NAME_MAX];
    int src_parent = ramfs_resolve_parent(src_path, src_filename, sizeof(src_filename));
    if (src_parent < 0) {
        return false;
    }

    struct ramfs_node* src_node = ramfs_find_child(src_parent, src_filename);
    if (src_node == NULL) {
        return false;
    }

    int src_node_idx = (int)(src_node - nodes);
    if (src_node_idx == 0 || src_node_idx == cwd_index || src_node_idx == home_dir_index) {
        return false;
    }

    if (src_node->is_base) {
        return false;
    }

    int target_parent = -1;
    char target_filename[RAMFS_NAME_MAX];

    int existing_dir = ramfs_resolve_directory(dst_path);
    if (existing_dir >= 0) {
        target_parent = existing_dir;
        string_copy_n(target_filename, src_node->name, sizeof(target_filename));
    } else {
        target_parent = ramfs_resolve_parent(dst_path, target_filename, sizeof(target_filename));
    }

    if (target_parent < 0 || !ramfs_is_valid_filename(target_filename)) {
        return false;
    }

    if (nodes[target_parent].is_base && target_parent != home_dir_index) {
        return false;
    }

    if (src_node->is_directory) {
        int check = target_parent;
        while (check != 0) {
            if (check == src_node_idx) {
                return false;
            }
            check = nodes[check].parent_index;
        }
        if (target_parent == src_node_idx) {
            return false;
        }
    }

    struct ramfs_node* dst_node = ramfs_find_child(target_parent, target_filename);
    if (dst_node != NULL) {
        int dst_node_idx = (int)(dst_node - nodes);
        if (dst_node_idx == src_node_idx) {
            return true;
        }
        if (dst_node->is_directory || src_node->is_directory) {
            return false;
        }
        ramfs_remove_in_dir(target_parent, target_filename);
    }

    src_node->parent_index = target_parent;
    string_copy_n(src_node->name, target_filename, RAMFS_NAME_MAX);
    return true;
}
