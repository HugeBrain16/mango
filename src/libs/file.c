#include "file.h"
#include "string.h"
#include "heap.h"
#include "ata.h"
#include "terminal.h"
#include "rtc.h"
#include "kernel.h"

uint16_t file_port = 0;
uint8_t file_drive = 0;
file_pointer_t file_current = {0};
int file_drive_status = FILE_DRIVE_UNSET;

void file_read_sb(file_superblock_t *sb) {
    uint8_t buffer[512] = {0};
    if (!ata_read_sector(file_port, file_drive, FILE_SECTOR_SUPERBLOCK, buffer))
        return;
    memcpy(sb, buffer, sizeof(*sb));
}

void file_write_sb(file_superblock_t *sb) {
    uint8_t buffer[512] = {0};
    memcpy(buffer, sb, sizeof(*sb));
    ata_write_sector(file_port, file_drive, FILE_SECTOR_SUPERBLOCK, buffer);
}

void file_format() {
    uint8_t buffer[512] = {0};

    file_superblock_t sb = {0};
    sb.magic = FILE_MAGIC;
    sb.version = FILE_VERSION;

    uint8_t ata_id[512];
    if (!ata_identify(file_port, file_drive, ata_id))
        return; // to be checked later

    sb.sectors = ata_count_sectors((uint16_t*)ata_id);
    sb.free = FILE_SECTOR_ROOT + 1;
    sb.free_list = 0;
    sb.free_pointer = (file_pointer_t){ .offset = 1, .lba = FILE_SECTOR_ROOT };
    sb.free_nodes = (file_pointer_t){0};
    sb.used = 2; // superblock + root

    memcpy(buffer, &sb, sizeof(sb));
    ata_write_sector(file_port, file_drive, FILE_SECTOR_SUPERBLOCK, buffer);

    file_node_t root = {0};
    root.time_created = datetime_packed();
    root.time_changed = datetime_packed();
    root.parent = (file_pointer_t){0};
    root.flags = FILE_FOLDER;
    root.child_head = (file_pointer_t){0};
    root.child_next = (file_pointer_t){0};
    root.size = 0;
    root.first_block = 0;
    strcpy(root.name, "root");

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, &root, sizeof(root));
    ata_write_sector(file_port, file_drive, FILE_SECTOR_ROOT, buffer);

    ata_flush(file_port);
}

int file_is_formatted() {
    file_superblock_t sb = {0};
    file_read_sb(&sb);

    return sb.magic == FILE_MAGIC && sb.version == FILE_VERSION;
}

int file_is_ready() {
    return file_drive_status == FILE_DRIVE_OK && file_is_formatted();
}

int file_drive_slot() {
    int slot = -1;

    if (file_port == ATA_PRIMARY) slot = 0;
    else if (file_port == ATA_SECONDARY) slot = 2;

    if (file_drive == ATA_MASTER) slot += 1;
    else if (file_drive == ATA_SLAVE) slot += 2;

    return slot;
}

int file_init(uint16_t base, uint8_t drive) {
    ata_select(base, drive);
    file_port = base;
    file_drive = drive;

    file_current = FILE_ROOT;
    file_drive_status = FILE_DRIVE_UNSET;

    char msg[64];
    uint8_t status = ata_status(file_port);

    if (status == 0x00 || status == 0x7F || status == 0xFF) {
        file_drive_status = FILE_DRIVE_ABSENT;
        strfmt(msg, "[ WARNING ] ATA: Could not detect drive. (status: 0x%x2)\n", status);
        log(msg);
    }

    if (file_drive_status != FILE_DRIVE_ABSENT) {
        uint8_t ata_id[512];

        if (!ata_identify(file_port, file_drive, ata_id)) {
            file_drive_status = FILE_DRIVE_INCOMPATIBLE;
            log("[ WARNING ] ATA: Failed to identify drive.\n");
        } else {
            uint16_t *w = (uint16_t*) ata_id;
            if (w[0] & (1 << 15)) {
                file_drive_status = FILE_DRIVE_INCOMPATIBLE;
                log("[ WARNING ] ATA: Drive is not compatible.\n");
            }
        }
    }

    if (file_drive_status == FILE_DRIVE_UNSET) {
        file_drive_status = FILE_DRIVE_OK;

        strfmt(msg, "[ INFO ] ATA: Drive OK. (status: 0x%x2)\n", status);
        log(msg);
    }

    return 1;
}

int file_init_by_slot(uint8_t slot) {
    if (slot < 1 || slot > ATA_MAX_DEV) return 0;

    char msg[64];
    strfmt(msg, "[ INFO ] ATA: Initializing drive (slot: %d)\n", slot);
    log(msg);

    switch (slot) {
        case 1: return file_init(ATA_PRIMARY, ATA_MASTER);
        case 2: return file_init(ATA_PRIMARY, ATA_SLAVE);
        case 3: return file_init(ATA_SECONDARY, ATA_MASTER);
        case 4: return file_init(ATA_SECONDARY, ATA_SLAVE);
        default: return 0;
    }

    return 0;
}

file_pointer_t file_get(file_pointer_t *parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    file_pointer_t current = parent_node.child_head;
    file_node_t current_node;

    while (!FILE_NULL(current)) {
        file_node(&current, &current_node);
        if (!strcmp(current_node.name, name) && (current_node.flags & FILE_DATA))
            return current;

        current = current_node.child_next;
    }

    return (file_pointer_t){0};
}

void file_node(file_pointer_t *pointer, file_node_t *node) {
    uint8_t buffer[512] = {0};
    if (pointer->offset >= FILE_MAX_OFFSET || !ata_read_sector(file_port, file_drive, pointer->lba, buffer)) {
        memset(node, 0, sizeof(file_node_t));
        return;
    }
    memcpy(node, buffer + pointer->offset * sizeof(file_node_t), sizeof(file_node_t));
}

void file_node_write(file_pointer_t *pointer, file_node_t *node) {
    uint8_t buffer[512] = {0};
    if (pointer->offset >= FILE_MAX_OFFSET)
        return;
    if (!ata_read_sector(file_port, file_drive, pointer->lba, buffer))
        return;
    memcpy(buffer + pointer->offset * sizeof(file_node_t), node, sizeof(file_node_t));
    ata_write_sector(file_port, file_drive, pointer->lba, buffer);
}

void file_data(uint32_t sector, file_data_t *data) {
    uint8_t buffer[512] = {0};
    if (!ata_read_sector(file_port, file_drive, sector, buffer))
        return;
    memcpy(data, buffer, sizeof(file_data_t));
}

void file_data_write(uint32_t sector, file_data_t *data) {
    uint8_t buffer[512];
    memcpy(buffer, data, sizeof(file_data_t));
    ata_write_sector(file_port, file_drive, sector, buffer);
}

int file_write(file_pointer_t *pointer, const char *data, size_t size) {
    file_node_t file;
    file_node(pointer, &file);
    file.size = 0;

    size_t written = 0;
    uint32_t current = file.first_block;

    while (written < size) {
        file_data_t block;
        file_data(current, &block);

        size_t to_write = sizeof(block.data);
        if (size - written < to_write)
            to_write = size - written;

        memcpy(block.data, data + written, to_write);
        if (to_write < sizeof(block.data))
            memset(block.data + to_write, 0, sizeof(block.data) - to_write);
        file_data_write(current, &block);
        written += to_write;

        file.size += to_write;

        if (written < size) {
            if (block.next == 0) {
                uint32_t new_block = file_sector_alloc_data();
                if (new_block == 0) {
                    file_node_write(pointer, &file);
                    ata_flush(file_port);
                    return 0;
                }

                block.next = new_block;
                file_data_write(current, &block);

                file_data_t empty = {0};
                empty.next = 0;
                file_data_write(new_block, &empty);

                current = new_block;
            } else {
                current = block.next;
            }
        }
    }

    file.time_changed = datetime_packed();
    file_node_write(pointer, &file);

    ata_flush(file_port);
    return 1;
}

char *file_read(file_pointer_t *pointer) {
    file_node_t file;
    file_node(pointer, &file);

    size_t offset = 0;
    char *buffer = heap_alloc(file.size + 1);
    if (!buffer)
        return NULL;
    buffer[file.size] = '\0';

    uint32_t current = file.first_block;
    while (current && offset < file.size) {
        file_data_t block;
        file_data(current, &block);

        size_t remaining = file.size - offset;
        size_t size = remaining < sizeof(block.data) ? remaining : sizeof(block.data);

        memcpy(buffer + offset, block.data, size);
        offset += size;

        current = block.next;
    }

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

void file_get_abspath(file_pointer_t *pointer, char *path, size_t size) {
    file_node_t node_node;
    size_t pos;
    size_t out = 0;

    if (!pointer || FILE_NULL(*pointer) || !path || size < 2) {
        if (path && size > 0)
            path[0] = '\0';
        return;
    }

    file_pointer_t node = *pointer;
    file_pointer_t root = FILE_ROOT;

    pos = size - 1;
    path[pos] = '\0';

    while (!FILE_NULL(node) && !FILE_EQ(node, root)) {
        file_node(&node, &node_node);
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

file_pointer_t file_get_node(const char *path) {
    file_pointer_t none = {0};
    if (!path) return none;

    file_pointer_t current = (path[0] == '/') ? FILE_ROOT : file_current;

    if (path[0] == '/' && path[1] == '\0')
        return FILE_ROOT;

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
                file_node(&current, &current_node);

                file_pointer_t found = {0};
                file_pointer_t child = current_node.child_head;
                while (!FILE_NULL(child)) {
                    file_node_t child_node;
                    file_node(&child, &child_node);

                    if (!strcmp(child_node.name, name)) {
                        found = child;
                        break;
                    }

                    child = child_node.child_next;
                }

                if (FILE_NULL(found))
                    return none;

                current = found;
                name_length = 0;
            }
        } else {
            if (name_length >= FILE_MAX_NAME - 1)
                return none;

            name[name_length++] = c;
        }

        i++;
    }

    return current;
}

file_pointer_t file_get_node2(const char *parent, const char *basename) {
    file_pointer_t node = {0};
    size_t parent_length = strlen(parent);

    if (parent_length + strlen(basename) + 2 > FILE_MAX_PATH)
        return node;

    char *path = heap_alloc(FILE_MAX_PATH);
    if (!path)
        return node;

    path[0] = '\0';
    strcat(path, parent);
    if (parent_length > 0 && parent[parent_length - 1] != '/')
        strcat(path, "/");
    strcat(path, basename);

    node = file_get_node(path);
    heap_free(path);
    return node;
}

int file_path_isfile(const char *path) {
    file_node_t node;
    file_pointer_t pointer = file_get_node(path);

    if (FILE_NULL(pointer))
        return 0;

    file_node(&pointer, &node);
    return (node.flags & FILE_DATA) != 0;
}

int file_exists(file_pointer_t *parent, const char *name) {
    return !FILE_NULL(file_get(parent, name));
}

int file_create(file_pointer_t *parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    if (strlen(name) >= FILE_MAX_NAME || !(parent_node.flags & FILE_FOLDER))
        return 0;

    if (file_exists(parent, name) || folder_exists(parent, name))
        return 0;

    file_pointer_t node_pointer;
    int node_sector = file_sector_alloc_node(&node_pointer);
    uint32_t data_sector = file_sector_alloc_data();
    if (!node_sector || data_sector == 0) {
        if (node_sector) file_sector_free_node(&node_pointer);
        if (data_sector) file_sector_free_data(data_sector);
        ata_flush(file_port);
        return 0;
    }

    file_node_t file = {0};
    file.time_created = datetime_packed();
    file.time_changed = datetime_packed();
    file.parent = *parent;
    file.flags = FILE_DATA;
    file.child_head = (file_pointer_t){0};
    file.child_next = (file_pointer_t){0};
    file.first_block = data_sector;
    file.size = 0;
    strcpy(file.name, name);

    if (!FILE_NULL(parent_node.child_head)) {
        file_pointer_t current = parent_node.child_head;
        file_node_t current_node;
        file_node(&current, &current_node);

        while(!FILE_NULL(current_node.child_next)) {
            current = current_node.child_next;
            file_node(&current, &current_node);
        }

        current_node.child_next = node_pointer;
        file_node_write(&current, &current_node);
    } else {
        parent_node.child_head = node_pointer;
        file_node_write(parent, &parent_node);
    }
    file_node_write(&node_pointer, &file);

    file_data_t data = {0};
    data.next = 0;
    file_data_write(data_sector, &data);

    ata_flush(file_port);
    return 1;
}

int file_delete(file_pointer_t *parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    file_pointer_t prev = {0};
    file_pointer_t current = parent_node.child_head;
    file_node_t prev_node;
    file_node_t current_node;

    while (!FILE_NULL(current)) {
        file_node(&current, &current_node);
        if (!strcmp(current_node.name, name)) {
            if (!(current_node.flags & FILE_DATA))
                return 0;

            if (!FILE_NULL(prev)) {
                file_node(&prev, &prev_node);
                prev_node.child_next = current_node.child_next;
                file_node_write(&prev, &prev_node);
            } else {
                parent_node.child_head = current_node.child_next;
                file_node_write(parent, &parent_node);
            }

            uint32_t current_data = current_node.first_block;
            while(current_data) {
                file_data_t data_block;
                file_data(current_data, &data_block);

                uint32_t next = data_block.next;
                file_sector_free_data(current_data);

                current_data = next;
            }

            file_sector_free_node(&current);
            ata_flush(file_port);
            return 1;
        }

        prev = current;
        current = current_node.child_next;
    }

    return 0;
}

file_pointer_t folder_get(file_pointer_t *parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    file_pointer_t current = parent_node.child_head;
    file_node_t current_node;

    while (!FILE_NULL(current)) {
        file_node(&current, &current_node);
        if (!strcmp(current_node.name, name) && (current_node.flags & FILE_FOLDER))
            return current;

        current = current_node.child_next;
    }

    return (file_pointer_t){0};
}

int file_path_isfolder(const char *path) {
    file_node_t node;
    file_pointer_t pointer = file_get_node(path);

    if (FILE_NULL(pointer))
        return 0;

    file_node(&pointer, &node);
    return (node.flags & FILE_FOLDER) != 0;
}

int folder_exists(file_pointer_t *parent, const char *name) {
    return !FILE_NULL(folder_get(parent, name));
}

int folder_create(file_pointer_t *parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    if (strlen(name) >= FILE_MAX_NAME || !(parent_node.flags & FILE_FOLDER))
        return 0;

    if (folder_exists(parent, name) || file_exists(parent, name))
        return 0;

    file_pointer_t node_pointer;
    if (!file_sector_alloc_node(&node_pointer)) {
        ata_flush(file_port);
        return 0;
    }

    file_node_t folder = {0};
    folder.time_created = datetime_packed();
    folder.time_changed = datetime_packed();
    folder.parent = *parent;
    folder.flags = FILE_FOLDER;
    folder.child_head = (file_pointer_t){0};
    folder.child_next = (file_pointer_t){0};
    folder.size = 0;
    folder.first_block = 0;
    strcpy(folder.name, name);

    if (!FILE_NULL(parent_node.child_head)) {
        file_pointer_t current = parent_node.child_head;
        file_node_t current_node;
        file_node(&current, &current_node);

        while(!FILE_NULL(current_node.child_next)) {
            current = current_node.child_next;
            file_node(&current, &current_node);
        }

        current_node.child_next = node_pointer;
        file_node_write(&current, &current_node);
    } else {
        parent_node.child_head = node_pointer;
        file_node_write(parent, &parent_node);
    }
    file_node_write(&node_pointer, &folder);

    ata_flush(file_port);
    return 1;
}

int folder_delete(file_pointer_t *parent, const char *name) {
    file_node_t parent_node;
    file_node(parent, &parent_node);

    file_pointer_t prev = {0};
    file_pointer_t current = parent_node.child_head;
    file_node_t current_node;
    file_node_t prev_node;

    while (!FILE_NULL(current)) {
        file_node(&current, &current_node);
        if (!strcmp(current_node.name, name)) {
            if (!(current_node.flags & FILE_FOLDER))
                return 0;

            file_pointer_t current_file = current_node.child_head;
            file_node_t current_file_node;

            while (!FILE_NULL(current_file)) {
                file_node(&current_file, &current_file_node);
                file_pointer_t next = current_file_node.child_next;

                if (current_file_node.flags & FILE_DATA)
                    file_delete(&current, current_file_node.name);
                else
                    folder_delete(&current, current_file_node.name);

                current_file = next;
            }

            if (!FILE_NULL(prev)) {
                file_node(&prev, &prev_node);
                prev_node.child_next = current_node.child_next;
                file_node_write(&prev, &prev_node);
            } else {
                parent_node.child_head = current_node.child_next;
                file_node_write(parent, &parent_node);
            }

            file_sector_free_node(&current);
            ata_flush(file_port);
            return 1;
        }

        prev = current;
        current = current_node.child_next;
    }

    return 0;
}

void file_sector_free_data(uint32_t sector) {
    file_superblock_t sb;
    file_read_sb(&sb);

    uint8_t buffer[512] = {0};
    memcpy(buffer, &sb.free_list, sizeof(uint32_t));
    ata_write_sector(file_port, file_drive, sector, buffer);

    sb.free_list = sector;
    sb.used--;
    file_write_sb(&sb);
}

void file_sector_free_node(file_pointer_t *pointer) {
    file_superblock_t sb;
    file_read_sb(&sb);

    file_node_t node = {0};
    node.child_next = sb.free_nodes;
    file_node_write(pointer, &node);

    sb.free_nodes = *pointer;
    file_write_sb(&sb);
}

int file_sector_alloc_node(file_pointer_t *pointer) {
    file_superblock_t sb;
    file_read_sb(&sb);

    if (!FILE_NULL(sb.free_nodes)) {
        file_node_t node;
        *pointer = sb.free_nodes;
        file_node(pointer, &node);

        sb.free_nodes = node.child_next;
        file_write_sb(&sb);
        return 1;
    }

    if (!FILE_NULL(sb.free_pointer) && sb.free_pointer.offset < FILE_MAX_OFFSET) {
        *pointer = sb.free_pointer;

        sb.free_pointer.offset++;
        file_write_sb(&sb);
        return 1;
    }

    if (sb.used >= sb.sectors) {
        term_write("Error: Disk full!\n");
        return 0;
    }

    uint8_t buffer[512] = {0};
    uint32_t sector;

    if (sb.free_list != 0) {
        sector = sb.free_list;
        if (!ata_read_sector(file_port, file_drive, sector, buffer))
            return 0;

        uint32_t free;
        memcpy(&free, buffer, sizeof(uint32_t));

        sb.free_list = free;
        memset(buffer, 0, sizeof(buffer));
    } else {
        sector = sb.free;
        sb.free++;
    }

    ata_write_sector(file_port, file_drive, sector, buffer);

    pointer->offset = 0;
    pointer->lba = sector;
    sb.free_pointer.offset = 1;
    sb.free_pointer.lba = sector;

    sb.used++;
    file_write_sb(&sb);

    return 1;
}

uint32_t file_sector_alloc_data() {
    file_superblock_t sb;
    file_read_sb(&sb);

    if (sb.used >= sb.sectors) {
        term_write("Error: Disk full!\n");
        return 0;
    }

    uint8_t buffer[512];
    uint32_t sector;

    if (sb.free_list != 0) {
        sector = sb.free_list;
        if (!ata_read_sector(file_port, file_drive, sector, buffer))
            return 0;

        uint32_t free;
        memcpy(&free, buffer, sizeof(uint32_t));

        sb.free_list = free;
    } else {
        sector = sb.free;
        sb.free++;
    }

    sb.used++;
    file_write_sb(&sb);

    memset(buffer, 0, sizeof(buffer));
    ata_write_sector(file_port, file_drive, sector, buffer);
    return sector;
}

int file_drive_spec(drive_t *drive) {
    uint8_t ata_id[512];
    if (!ata_identify(file_port, file_drive, ata_id))
        return 0;

    uint16_t *w = (uint16_t*) ata_id;
    ata_get_string(w, 10, 19, drive->serial, sizeof(drive->serial));
    ata_get_string(w, 23, 26, drive->rev, sizeof(drive->rev));
    ata_get_string(w, 27, 46, drive->model, sizeof(drive->model));
    return 1;
}
