#pragma once

#include <stdint.h>

#include "../boot/stivale2.h"
#include "../ds/string.h"
#include "../ds/vector.h"

struct vfs_node_t;

struct vfs_opened_file_t {
    uint64_t seek;
    uint64_t file_size;
    vfs_node_t *node;
};

struct vfs_file_system_t {
    virtual uint64_t open(vfs_opened_file_t *file, const string_t &path) = 0;
    virtual uint64_t close(vfs_opened_file_t *file, uint64_t fd) = 0;
    virtual uint64_t read(vfs_opened_file_t *file, uint8_t *buffer, uint64_t size) = 0;
    virtual uint64_t write(vfs_opened_file_t *file, const uint8_t *buffer, uint64_t size) = 0;
};

struct vfs_node_t {
    string_t name;
    vector_t<vfs_node_t *> children;
    vfs_node_t *parent;
    vfs_file_system_t *file_system;
};

namespace vfs {

void init(stivale2_struct_modules_tag_t *modules);

vfs_node_t *get(vfs_node_t *parent, const string_t &path);

void mount(vfs_file_system_t *fs, vfs_node_t *parent, vfs_node_t *node);
void append_child(vfs_node_t *parent, vfs_node_t *node);
void remove_child(vfs_node_t *parent, vfs_node_t *node);

uint64_t open(const string_t &fs_path, const string_t &path);
uint64_t close(uint64_t fd);
uint64_t read(uint64_t fd, uint8_t *buffer, uint64_t size);
uint64_t write(uint64_t fd, const uint8_t *buffer, uint64_t size);

} // namespace vfs