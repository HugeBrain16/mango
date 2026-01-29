#include "file.h"
#include "string.h"
#include "heap.h"
#include "ata.h"
#include "terminal.h"
#include "color.h"

void file_read_sb(file_superblock_t *sb) {
    uint16_t buffer[256];
    ata_read_sector(FILE_SECTOR_SUPERBLOCK, buffer);
    memcpy(sb, buffer, sizeof(*sb));
}

void file_write_sb(file_superblock_t *sb) {
    uint16_t buffer[256] = {0};
    memcpy(buffer, sb, sizeof(*sb));
    ata_write_sector(FILE_SECTOR_SUPERBLOCK, buffer);
}

void file_format() {
    uint16_t buffer[256] = {0};

    file_superblock_t sb;
    sb.magic = FILE_MAGIC;

    uint16_t ata_id[256]; ata_identify(ata_id);
    sb.sectors = (uint32_t)ata_id[60] | ((uint32_t)ata_id[61] << 16);
    sb.free = FILE_SECTOR_ROOT + 1;
    sb.free_list = 0;
    sb.used = 2; // superblock + root

    memcpy(buffer, &sb, sizeof(sb));
    ata_write_sector(FILE_SECTOR_SUPERBLOCK, buffer);

    file_node_t root = {0};
    root.parent = 0;
    root.flags = FILE_FOLDER;
    root.child_head = 0;
    root.child_next = 0;
    root.size = 0;
    root.first_block = 0;
    strcpy(root.name, "root");
    memcpy(buffer, &root, sizeof(root));
    ata_write_sector(FILE_SECTOR_ROOT, buffer);
}

int file_is_formatted() {
    file_superblock_t sb;
    file_read_sb(&sb);

    return sb.magic == FILE_MAGIC;
}

void file_init() {
    file_current = FILE_SECTOR_ROOT;
}

uint32_t file_get(uint32_t parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    uint32_t current = parent_node.child_head;
    file_node_t current_node;

    while (current) {
        file_node(current, &current_node);
        if (!strcmp(current_node.name, name) && (current_node.flags & FILE_DATA))
            return current;

        current = current_node.child_next;
    }

    return 0;
}

void file_node(uint32_t sector, file_node_t *node) {
    uint16_t buffer[256];
    ata_read_sector(sector, buffer);
    memcpy(node, buffer, sizeof(file_node_t));
}

void file_node_write(uint32_t sector, file_node_t *node) {
    uint16_t buffer[256];
    memcpy(buffer, node, sizeof(file_node_t));
    ata_write_sector(sector, buffer);
}

void file_data(uint32_t sector, file_data_t *data) {
    uint16_t buffer[256];
    ata_read_sector(sector, buffer);
    memcpy(data, buffer, sizeof(file_data_t));
}

void file_data_write(uint32_t sector, file_data_t *data) {
    uint16_t buffer[256];
    memcpy(buffer, data, sizeof(file_data_t));
    ata_write_sector(sector, buffer);
}

int file_write(uint32_t sector, const char *data, size_t size) {
    file_node_t file;
    file_node(sector, &file);

    size_t written = 0;
    uint32_t current = file.first_block;

    while (written < size) {
        file_data_t block;
        file_data(current, &block);

        size_t to_write = sizeof(block.data);
        if (size - written < to_write)
            to_write = size - written;

        memcpy(block.data, data + written, to_write);
        file_data_write(current, &block);
        written += to_write;

        if (written < size) {
            if (block.next == 0) {
                uint32_t new_block = file_sector_alloc();
                if (new_block == 0) return 0;

                block.next = new_block;
                file_data_write(current, &block);

                file.size++;
                file_node_write(sector, &file);

                file_data_t data = {0};
                data.next = 0;
                file_data_write(new_block, &data);

                current = new_block;
            } else {
                current = block.next;
            }
        }
    }

    return 1;
}

char *file_read(uint32_t sector) {
    file_node_t file;
    file_node(sector, &file);

    size_t offset = 0;
    size_t buffer_size;
    char *buffer = NULL;

    uint32_t current = file.first_block;
    while (current) {
        file_data_t block;
        file_data(current, &block);

        if (!buffer) {
            buffer_size = sizeof(block.data) * file.size;
            buffer = heap_alloc(buffer_size + 1);
        }

        memcpy(buffer + offset, block.data, sizeof(block.data));
        offset += sizeof(block.data);

        current = block.next;
    }

    if (buffer)
        buffer[buffer_size] = '\0';

    return buffer;
}

int file_split_path(const char *path, char *out_parent, char *out_name) {
    size_t len = strlen(path);
    if (len == 0)
        return 0;

    size_t i = len - 1;

    while (i > 0 && path[i] == '/')
        i--;

    size_t name_len = 0;
    while (path[i] != '/') {
        if (name_len >= FILE_MAX_NAME - 1)
            return 0;
        out_name[name_len++] = path[i];
        if (i == 0) break;
        i--;
    }

    if (name_len == 0)
        return 0;

    out_name[name_len] = '\0';
    strflip(out_name, 0, name_len - 1);

    if (i == 0 && path[0] != '/') {
        out_parent[0] = '\0';
        return 1;
    }

    if (i == 0) {
        strcpy(out_parent, "/");
        return 1;
    }

    memcpy(out_parent, path, i);
    out_parent[i] = '\0';
    return 1;
}

void file_get_abspath(uint32_t node, char *path, size_t size) {
    file_node_t node_node;
    size_t pos;
    size_t out = 0;

    if (!node || !path || size < 2) {
        if (path && size > 0)
            path[0] = '\0';
        return;
    }

    pos = size - 1;
    path[pos] = '\0';

    while (node && node != FILE_SECTOR_ROOT) {
        file_node(node, &node_node);
        size_t len = strlen(node_node.name);

        if (pos < len + 1) {
            path[0] = '\0';
            return;
        }

        pos -= len;
        memcpy(path + pos, node_node.name, len);

        path[--pos] = '/';
        node = node_node.parent;
    }

    if (pos == size - 1) {
        path[0] = '/';
        path[1] = '\0';
        return;
    }

    while (path[pos] != '\0') {
        path[out++] = path[pos++];
    }
    path[out] = '\0';
}

uint32_t file_get_node(const char *path) {
    if (!path) return 0;

    uint32_t current = (path[0] == '/') ? FILE_SECTOR_ROOT : file_current;

    if (path[0] == '/' && path[1] == '\0')
        return FILE_SECTOR_ROOT;

    char name[FILE_MAX_NAME];
    size_t name_length = 0;
    size_t path_length = strlen(path);

    size_t i = (path[0] == '/') ? 1 : 0;

    while(i <= path_length) {
        char c = path[i];

        if (c == '/' || c == '\0') {
            if (name_length > 0) {
                name[name_length] = '\0';

                file_node_t current_node;
                file_node(current, &current_node);

                uint32_t found = 0;
                uint32_t child = current_node.child_head;
                while (child) {
                    file_node_t child_node;
                    file_node(child, &child_node);

                    if (!strcmp(child_node.name, name)) {
                        found = child;
                        break;
                    }

                    child = child_node.child_next;
                }

                if (!found)
                    return 0;

                current = found;
                name_length = 0;
            }
        } else {
            if (name_length >= FILE_MAX_NAME - 1)
                return 0;

            name[name_length++] = c;
        }

        i++;
    }

    return current;
}

uint32_t file_get_node2(const char *parent, const char *basename) {
    uint32_t node;
    char *path = heap_alloc(FILE_MAX_PATH);
    strcat(path, parent);
    strcat(path, basename);

    node = file_get_node(path);
    heap_free(path);
    return node;
}

int file_exists(uint32_t parent, const char *name) {
    return file_get(parent, name) != 0;
}

int file_create(uint32_t parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    if (file_exists(parent, name) || !(parent_node.flags & FILE_FOLDER))
        return 0;

    uint32_t node_sector = file_sector_alloc();
    uint32_t data_sector = file_sector_alloc();
    if (node_sector == 0 || data_sector == 0) {
        if (node_sector) file_sector_free(node_sector);
        if (data_sector) file_sector_free(data_sector);
        return 0;
    }

    file_superblock_t sb; file_read_sb(&sb);
    file_node_t file = {0};
    file.parent = parent;
    file.flags = FILE_DATA;
    file.child_head = 0;
    file.child_next = 0;
    file.first_block = data_sector;
    file.size = 1;
    strcpy(file.name, name);

    if (parent_node.child_head) {
        uint32_t current = parent_node.child_head;
        file_node_t current_node;
        file_node(current, &current_node);

        while(current_node.child_next) {
            current = current_node.child_next;
            file_node(current, &current_node);
        }

        current_node.child_next = node_sector;
        file_node_write(current, &current_node);
    } else {
        parent_node.child_head = node_sector;
        file_node_write(parent, &parent_node);
    }
    file_node_write(node_sector, &file);

    file_data_t data = {0};
    data.next = 0;
    file_data_write(data_sector, &data);

    return 1;
}

int file_delete(uint32_t parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    uint32_t prev = 0;
    uint32_t current = parent_node.child_head;
    file_node_t prev_node;
    file_node_t current_node;

    while (current) {
        file_node(current, &current_node);
        if (!strcmp(current_node.name, name)) {
            if (!(current_node.flags & FILE_DATA))
                return 0;

            uint32_t current_data = current_node.first_block;
            while(current_data) {
                file_data_t data_block;
                file_data(current_data, &data_block);

                uint32_t next = data_block.next;
                file_sector_free(current_data);

                current_data = next;
            }

            file_sector_free(current);

            if (prev) {
                file_node(prev, &prev_node);
                prev_node.child_next = current_node.child_next;
                file_node_write(prev, &prev_node);
            } else {
                parent_node.child_head = current_node.child_next;
                file_node_write(parent, &parent_node);
            }

            return 1;
        }

        prev = current;
        current = current_node.child_next;
    }

    return 0;
}

uint32_t folder_get(uint32_t parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    uint32_t current = parent_node.child_head;
    file_node_t current_node;

    while (current) {
        file_node(current, &current_node);
        if (!strcmp(current_node.name, name) && (current_node.flags & FILE_FOLDER))
            return current;

        current = current_node.child_next;
    }

    return 0;
}

int folder_exists(uint32_t parent, const char *name) {
    return folder_get(parent, name) != 0;
}

int folder_create(uint32_t parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    if (folder_exists(parent, name) || !(parent_node.flags & FILE_FOLDER))
        return 0;

    uint32_t node_sector = file_sector_alloc();
    if (node_sector == 0) return 0;

    file_superblock_t sb; file_read_sb(&sb);
    file_node_t folder = {0};
    folder.parent = parent;
    folder.flags = FILE_FOLDER;
    folder.child_head = 0;
    folder.child_next = 0;
    folder.size = 0;
    folder.first_block = 0;
    strcpy(folder.name, name);

    if (parent_node.child_head) {
        uint32_t current = parent_node.child_head;
        file_node_t current_node;
        file_node(current, &current_node);

        while(current_node.child_next) {
            current = current_node.child_next;
            file_node(current, &current_node);
        }

        current_node.child_next = node_sector;
        file_node_write(current, &current_node);
    } else {
        parent_node.child_head = node_sector;
        file_node_write(parent, &parent_node);
    }
    file_node_write(node_sector, &folder);

    return 1;
}

int folder_delete(uint32_t parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    uint32_t prev = 0;
    uint32_t current = parent_node.child_head;
    file_node_t current_node;
    file_node_t prev_node;

    while (current) {
        file_node(current, &current_node);
        if (!strcmp(current_node.name, name)) {
            if (!(current_node.flags & FILE_FOLDER))
                return 0;

            if (current_node.child_head) {
                uint32_t current_file = current_node.child_head;
                file_node_t current_file_node;

                while (current_file) {
                    file_node(current_file, &current_file_node);

                    file_delete(current, current_file_node.name);
                    current_file = current_file_node.child_next;
                }
            }


            if (prev) {
                file_node(prev, &prev_node);
                prev_node.child_next = current_node.child_next;
                file_node_write(prev, &prev_node);
            } else {
                parent_node.child_head = current_node.child_next;
                file_node_write(parent, &parent_node);
            }

            file_sector_free(current);
            return 1;
        }

        prev = current;
        current = current_node.child_next;
    }

    return 0;
}

void file_sector_free(uint32_t sector) {
    file_superblock_t sb;
    file_read_sb(&sb);

    uint16_t buffer[256] = {0};
    memcpy(buffer, &sb.free_list, sizeof(uint32_t));
    ata_write_sector(sector, buffer);

    sb.free_list = sector;
    sb.used--;
    file_write_sb(&sb);
}

uint32_t file_sector_alloc() {
    file_superblock_t sb;
    file_read_sb(&sb);

    if (sb.used >= sb.sectors) {
        term_write("Error: Disk full!\n", COLOR_WHITE, COLOR_BLACK);
        return 0;
    }
    sb.used++;

    if (sb.free_list != 0) {
        uint32_t sector = sb.free_list;

        uint16_t buffer[256];
        ata_read_sector(sector, buffer);

        uint32_t free;
        memcpy(&free, buffer, sizeof(uint32_t));

        sb.free_list = free;
        file_write_sb(&sb);

        return sector;
    }

    uint32_t sector = sb.free;
    sb.free++;
    file_write_sb(&sb);
    return sector;
}
