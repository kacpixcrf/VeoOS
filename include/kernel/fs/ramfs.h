#ifndef VEOOS_KERNEL_FS_RAMFS_H
#define VEOOS_KERNEL_FS_RAMFS_H

#include "util/types.h"

#define RAMFS_MAX_NODES 128
#define RAMFS_NAME_MAX 32
#define RAMFS_FILE_CAPACITY 1024

struct ramfs_node {
    char name[RAMFS_NAME_MAX];
    bool is_directory;
    bool is_used;
    bool is_base;
    int parent_index;
    size_t size;
    const char* read_only_data;
    char* heap_content;
};

void ramfs_initialize(void);
int ramfs_get_cwd(void);
int ramfs_get_home_dir(void);
void ramfs_get_cwd_path(char* buffer, size_t max_length);
bool ramfs_is_valid_filename(const char* name);
int ramfs_resolve_directory(const char* path);
int ramfs_resolve_parent(const char* path, char* out_filename, size_t max_filename_len);
bool ramfs_change_directory(const char* path);
bool ramfs_create_directory(const char* name);
bool ramfs_create_directory_in_dir(int parent, const char* name);
bool ramfs_create_file(const char* name);
bool ramfs_create_file_in_dir(int parent, const char* name);
struct ramfs_node* ramfs_find_child(int parent, const char* name);
bool ramfs_write_file(const char* name, const char* data, size_t length);
bool ramfs_write_file_in_dir(int parent, const char* name, const char* data, size_t length);
bool ramfs_remove(const char* name);
bool ramfs_remove_in_dir(int parent, const char* name);
bool ramfs_remove_recursive(int node_index);
bool ramfs_copy_file(const char* src_path, const char* dst_path);
bool ramfs_copy(const char* src_path, const char* dst_path, bool recursive);
bool ramfs_move(const char* src_path, const char* dst_path);
bool ramfs_has_children(int dir_index);
int ramfs_list_children(int parent, int* results, int max_results);
struct ramfs_node* ramfs_get_node(int index);
const char* ramfs_get_content(const struct ramfs_node* node);

#endif
