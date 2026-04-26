#include "command.h"
#include "kernel.h"
#include "string.h"
#include "heap.h"
#include "screen.h"
#include "terminal.h"
#include "color.h"
#include "time.h"
#include "file.h"
#include "keyboard.h"
#include "editor.h"
#include "font.h"
#include "ata.h"
#include "unit.h"
#include "io.h"
#include "script.h"
#include "rtc.h"
#include "config.h"
#include "acpi.h"
#include "cpu.h"

static void ata_print_string(uint16_t *w, int start, int end) {
    char str[64];
    int j = 0;

    for (int i = start; i < end; i++) {
        str[j++] = (char)((w[i] >> 8) & 0xFF);
        str[j++] = (char)(w[i] & 0xFF);
    }

    str[j] = '\0';

    strrtrim(str);
    term_write(str);
}

static int command_scale(int argc, char *argv[]) {
    if (argc > 0) {
        float scale = (float)doublestr(argv[0]);

        if (scale > 0)
            screen_scale = (float)doublestr(argv[0]);
    } else {
        char buf[10];
        strfmt(buf, "%f2", screen_scale);
        term_write(buf);
    }

    return 0;
}

static int command_scaleup(int argc, char *argv[]) {
    unused(argc); unused(argv);

    screen_scale += 1.0f;
    return 0;
}

static int command_scaledown(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (screen_scale > 1.0f)
        screen_scale -= 1.0f;

    return 0;
}

static int command_clear(int argc, char *argv[]) {
    unused(argc); unused(argv);

    screen_clear(term_bg);
    term_x = 0;
    term_y = 0;

    return 0;
}

static int command_shutdown(int argc, char *argv[]) {
    unused(argc); unused(argv);

    term_write("Shutting down...\n");

    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    outw(0x600, 0x34);

    if (acpi_mode_enabled())
        acpi_shutdown();

    abort();

    return 0;
}

static int command_fetch(int argc, char *argv[]) {
    int show_diskname = 0;
    int show_ribbon = 1;

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "diskname"))
            show_diskname = 1;
        else if (!strcmp(argv[i], "noribbon"))
            show_ribbon = 0;
    }

    char buff[128];

    if (show_ribbon) {
        term_write("\n");
        for (int i = 0; i < 6; i++) {
            term_write2("=", COLOR_YELLOW, term_bg);
            term_write2("=", COLOR_WHITE, term_bg);
            term_write2("=", COLOR_YELLOW, term_bg);
        }
    }
    term_write("\nKernel: Mango\n");

    strfmt(buff, "CPU: %s\n", cpu_name);
    term_write(buff);

    char mem_total[16];
    char mem_free[16];
    unit_get_size(heap_end - heap_start + (2 << 20), mem_total);
    unit_get_size(heap_end - heap_current + (2 << 20), mem_free);
    strfmt(buff, "Memory: %s (Free: %s)\n", mem_total, mem_free);
    term_write(buff);

    if (file_drive_status == FILE_DRIVE_OK) {
        uint8_t ata_id[512];
        ata_identify(file_port, ata_id);
        uint16_t *w = (uint16_t*) ata_id;
        uint32_t sectors = (uint32_t)w[60] | ((uint32_t)w[61] << 16);
        char disk_total[16];
        unit_get_size(sectors * 512, disk_total);

        term_write("Disk: ");
        
        if (show_diskname) {
            ata_print_string(w, 27, 46);
            term_write(" - ");
        }

        strfmt(buff, "%s ", disk_total);
        term_write(buff);
        if (file_is_formatted()) {
            file_superblock_t sb;
            file_read_sb(&sb);

            char disk_used[16];
            unit_get_size(sb.used * 512, disk_used);
            strfmt(buff, "(Used: %s) ", disk_used);
            term_write(buff);
        }
        term_write("\n");
    }

    strcpy(buff, "Uptime:");
    term_write(buff);
    if (uptime_hours > 0) {
        strfmt(buff, " %d hours", uptime_hours);
        term_write(buff);
    }
    if (uptime_minutes > 0) {
        strfmt(buff, " %d minutes", uptime_minutes);
        term_write(buff);
    }
    if (uptime_seconds > 0) {
        strfmt(buff, " %d seconds", uptime_seconds);
        term_write(buff);
    }
    term_write("\n");

    term_write("\n");
    term_write2("=", COLOR_YELLOW, COLOR_YELLOW);
    term_write2("=", COLOR_WHITE, COLOR_WHITE);
    term_write2("=", COLOR_PURPLE, COLOR_PURPLE);
    term_write2("=", COLOR_DARKGRAY, COLOR_DARKGRAY);
    term_write("\n\n");

    return 0;
}

static int command_echo(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        term_write(argv[i]);
        term_write(" ");
    }
    term_write("\n");

    return 0;
}

static int command_list(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    static char buff[FILE_MAX_NAME + 16];
    uint32_t parent;
    file_node_t parent_node;
    if (argc > 0)
        parent = file_get_node(argv[0]);
    else
        parent = file_current;
    file_node(parent, &parent_node);

    if (parent_node.child_head && (parent_node.flags & FILE_FOLDER)) {
        uint32_t current = parent_node.child_head;
        file_node_t current_node;

        while (current) {
            file_node(current, &current_node);

            if (current_node.flags & FILE_FOLDER) {
                strfmt(buff, "%s/\n", current_node.name);
                term_write(buff);
            }
            current = current_node.child_next;
        }

        current = parent_node.child_head;
        while (current) {
            file_node(current, &current_node);

            if (current_node.flags & FILE_DATA) {
                strfmt(buff, "%s\n", current_node.name);
                term_write(buff);
            }
            current = current_node.child_next;
        }
    } else if (!parent || !(parent_node.flags & FILE_FOLDER)) {
        term_write("Not a folder.\n");
        return 1;
    }
    else
        term_write("Empty folder.\n");

    return 0;
}

static int command_newfile(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc > 0) {
        uint32_t parent;

        int exit = 0;

        char *path_parent = heap_alloc(FILE_MAX_PATH - FILE_MAX_NAME);
        char *path_basename = heap_alloc(FILE_MAX_NAME);

        if (file_split_path(argv[0], path_parent, path_basename)) {
            if (path_parent[0] == '\0')
                parent = file_current;
            else
                parent = file_get_node(path_parent);

            if (parent) {
                if (strlen(path_basename) > FILE_MAX_NAME) {
                    term_write("File name is too long!\n");
                    exit = 1;
                } else if (file_exists(parent, path_basename)) {
                    term_write("File with the same name already exist!\n");
                    exit = 1;
                } else if (!file_create(parent, path_basename)) {
                    term_write("Failed creating a new file!\n");
                    exit = 1;
                }
            } else {
                term_write("Invalid path.\n");
                exit = 1;
            }
        } else {
            term_write("Invalid path.\n");
            exit = 1;
        }

        heap_free(path_parent);
        heap_free(path_basename);
        return exit;
    } else {
        term_write("Usage: newfile <path>\n");
        return 1;
    }
}

static int command_delfile(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc > 0) {
        uint32_t target = file_get_node(argv[0]);

        if (!target) {
            term_write("File not found!\n");
            return 1;
        }

        file_node_t target_node;
        file_node(target, &target_node);

        if (!file_delete(target_node.parent, target_node.name)) {
            term_write("Failed deleting file!\n");
            return 1;
        }

        return 0;
    } else {
        term_write("Usage: delfile <path>\n");
        return 1;
    }
}

static int command_edit(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc > 0) {
        uint32_t parent;

        int exit = 0;

        char *path_parent = heap_alloc(FILE_MAX_PATH - FILE_MAX_NAME);
        char *path_basename = heap_alloc(FILE_MAX_NAME);

        if (file_split_path(argv[0], path_parent, path_basename)) {
            if (path_parent[0] == '\0')
                parent = file_current;
            else
                parent = file_get_node(path_parent);

            if (parent) {
                if (file_exists(parent, path_basename)) {
                    keyboard_mode = KEYBOARD_MODE_EDIT;
                    command_clear(argc, argv);

                    edit_init(file_get(parent, path_basename));
                } else {
                    term_write("File does not exist!\n");
                    exit = 1;
                }
            } else {
                term_write("Invalid path!\n");
                exit = 1;
            }
        } else {
            term_write("Invalid path!\n");
            exit = 1;
        }

        heap_free(path_parent);
        heap_free(path_basename);
        return exit;
    } else {
        term_write("Usage: edit <path>\n");
        return 1;
    }
}

static int command_newfolder(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc > 0) {
        uint32_t parent;

        int exit = 0;

        char *path_parent = heap_alloc(FILE_MAX_PATH - FILE_MAX_NAME);
        char *path_basename = heap_alloc(FILE_MAX_NAME);

        if (file_split_path(argv[0], path_parent, path_basename)) {
            if (path_parent[0] == '\0')
                parent = file_current;
            else
                parent = file_get_node(path_parent);

            if (parent) {
                if (strlen(path_basename) > FILE_MAX_NAME) {
                    term_write("Folder name is too long!\n");
                    exit = 1;
                } else if (folder_exists(parent, path_basename)) {
                    term_write("Folder with the same name already exist!\n");
                    exit = 1;
                } else if (!folder_create(parent, path_basename)) {
                    term_write("Failed creating a new folder!\n");
                    exit = 1;
                }
            } else {
                term_write("Invalid path.\n");
                exit = 1;
            }
        } else {
            term_write("Invalid path.\n");
            exit = 1;
        }

        heap_free(path_parent);
        heap_free(path_basename);
        return exit;
    } else {
        term_write("Usage: newfolder <path>\n");
        return 1;
    }
}

static int command_delfolder(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc > 0) {
        uint32_t target = file_get_node(argv[0]);
        file_node_t target_node;
        file_node(target, &target_node);

        if (target != FILE_SECTOR_ROOT) {
            if (!folder_delete(target_node.parent, target_node.name)) {
                term_write("Failed deleting folder!\n");
                return 1;
            }

            return 0;
        } else {
            term_write("Cannot delete root folder!\n");
            return 1;
        }
    } else {
        term_write("Usage: delfolder <path>\n");
        return 1;
    }
}

static int command_goto(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc > 0) {
        uint32_t target = file_get_node(argv[0]);
        file_node_t target_node;
        file_node(target, &target_node);

        if (target && (target_node.flags & FILE_FOLDER)) {
            file_current = target;
            return 0;
        } else {
            term_write("Not a folder.\n");
            return 1;
        }
    } else {
        term_write("Usage: goto <path>\n");
        return 1;
    }
}

static int command_goup(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    file_node_t parent_node;
    file_node(file_current, &parent_node);

    if (parent_node.parent) {
        file_current = parent_node.parent;
        return 0;
    } else {
        term_write("Already at topmost folder!\n");
        return 1;
    }
}

static int command_whereami(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    char *path = heap_alloc(FILE_MAX_PATH);
    file_get_abspath(file_current, path, FILE_MAX_PATH);

    term_write(path);
    term_write("\n");
    heap_free(path);

    return 0;
}

static int command_copyfile(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc < 2) {
        term_write("Usage: copyfile <src> <dest>\n");
        return 1;
    }

    uint32_t src = file_get_node(argv[0]);
    file_node_t src_node;
    file_node(src, &src_node);

    if (!src) {
        term_write("Source file doesn't exist!\n");
        return 1;
    }

    if (!(src_node.flags & FILE_DATA)) {
        term_write("Not a file!\n");
        return 1;
    }

    int exit = 0;

    char *dest_parent = heap_alloc(FILE_MAX_PATH - FILE_MAX_NAME);
    char *dest_basename = heap_alloc(FILE_MAX_NAME);
    if (!file_split_path(argv[1], dest_parent, dest_basename)) {
        term_write("Invalid destination path!\n");
        exit = 1;
        goto cleanup;
    }

    uint32_t dest_parent_node;
    if (dest_parent[0] == '\0')
        dest_parent_node = file_current;
    else
        dest_parent_node = file_get_node(dest_parent);

    if (!dest_parent_node) {
        term_write("Parent folder doesn't exist!\n");
        exit = 1;
        goto cleanup;
    }

    uint32_t dest = file_get_node2(dest_parent, dest_basename);
    file_node_t dest_node;
    file_node(dest, &dest_node);

    if (dest) {
        if ((dest_node.flags & FILE_FOLDER)) {
            file_create(dest, src_node.name);
            dest = file_get(dest, src_node.name);
            file_node(dest, &dest_node);
        }

        char *data = file_read(src);
        file_write(dest, data, strlen(data));
        heap_free(data);
    } else {
        if (strlen(dest_basename) > FILE_MAX_NAME) {
            term_write("File name is too long!\n");
            exit = 1;
            goto cleanup;
        }

        file_create(dest_parent_node, dest_basename);
        dest = file_get(dest_parent_node, dest_basename);
        file_node(dest, &dest_node);

        char *data = file_read(src);
        file_write(dest, data, strlen(data));
        heap_free(data);
    }

cleanup:
    heap_free(dest_parent);
    heap_free(dest_basename);

    return exit;
}

static int command_movefile(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc < 2) {
        term_write("Usage: movefile <src> <dest>\n");
        return 1;
    }

    command_copyfile(argc, argv);

    uint32_t src = file_get_node(argv[0]);
    file_node_t src_node;
    file_node(src, &src_node);

    file_delete(src_node.parent, src_node.name);
    return 0;
}

static int command_copyfolder(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc < 2) {
        term_write("Usage: copyfolder <src> <dest>\n");
        return 1;
    }

    uint32_t src = file_get_node(argv[0]);
    file_node_t src_node;
    file_node(src, &src_node);

    if (!src) {
        term_write("Source folder doesn't exist!\n");
        return 1;
    }

    if (!(src_node.flags & FILE_FOLDER)) {
        term_write("Not a folder!\n");
        return 1;
    }

    int exit = 0;

    char *dest_parent = heap_alloc(FILE_MAX_PATH - FILE_MAX_NAME);
    char *dest_basename = heap_alloc(FILE_MAX_NAME);
    if (!file_split_path(argv[1], dest_parent, dest_basename)) {
        term_write("Invalid destination path!\n");
        exit = 1;
        goto cleanup;
    }

    uint32_t dest_parent_node;
    if (dest_parent[0] == '\0')
        dest_parent_node = file_current;
    else
        dest_parent_node = file_get_node(dest_parent);

    if (!dest_parent_node) {
        term_write("Parent folder doesn't exist!\n");
        exit = 1;
        goto cleanup;
    }

    uint32_t dest = file_get_node2(dest_parent, dest_basename);
    file_node_t dest_node;
    file_node(dest, &dest_node);

    if (dest) {
        if (!(dest_node.flags & FILE_FOLDER)) {
            folder_create(dest, src_node.name);
            dest = folder_get(dest, src_node.name);
            file_node(dest, &dest_node);
        }
    } else {
        if (strlen(dest_basename) > FILE_MAX_NAME) {
            term_write("Folder name is too long!\n");
            exit = 1;
            goto cleanup;
        }

        folder_create(dest_parent_node, dest_basename);
        dest = folder_get(dest_parent_node, dest_basename);
        file_node(dest, &dest_node);
    }

    uint32_t child = src_node.child_head;
    file_node_t child_node;

    while (child) {
        file_node(child, &child_node);
        file_create(dest, child_node.name);

        uint32_t dest_child = file_get(dest, child_node.name);
        char *data = file_read(child);
        file_write(dest_child, data, strlen(data));
        heap_free(data);

        child = child_node.child_next;
    }

cleanup:
    heap_free(dest_parent);
    heap_free(dest_basename);

    return exit;
}

static int command_movefolder(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc < 2) {
        term_write("Usage: movefolder <src> <dest>\n");
        return 1;
    }

    command_copyfolder(argc, argv);

    uint32_t src = file_get_node(argv[0]);
    file_node_t src_node;
    file_node(src, &src_node);

    folder_delete(src_node.parent, src_node.name);
    return 0;
}

static int command_formatdisk(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (file_drive_status == FILE_DRIVE_ABSENT) {
        log("[ ERROR ] No usable drive!\n");
        return 1;
    } else if (file_drive_status == FILE_DRIVE_INCOMPATIBLE) {
        log("[ ERROR ] Drive is not compatible!\n");
        return 1;
    }

    char confirm[TERM_INPUT_SIZE];
    term_get_input("This will erase the whole disk. Are you sure? (type \"y\"): ", confirm, sizeof(confirm));

    if (!strcmp(confirm, "y")) {
        file_format();
        term_write("Disk formatted.\n");
        return 0;
    }

    return 1;
}

static int command_nodeinfo(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc < 1) {
        term_write("Usage: nodeinfo <path>\n");
        return 1;
    }

    uint32_t node_sector = file_get_node(argv[0]);
    if (!node_sector) {
        term_write("Not found\n");
        return 1;
    }

    file_node_t node;
    file_data_t block;
    file_node(node_sector, &node);
    file_data(node.first_block, &block);

    char buff[128];
    strfmt(buff, "NAME = %s\n", node.name);
    term_write(buff);

    rtc_datetime_t dt;
    char seconds[3];
    char minutes[3];
    char hours[3];
    char day[3];
    char month[3];

    datetime_unpack(&dt, node.time_created);
    intpad(seconds, dt.seconds, 2, '0');
    intpad(minutes, dt.minutes, 2, '0');
    intpad(hours,   dt.hours,   2, '0');
    intpad(day,     dt.day,     2, '0');
    intpad(month,   dt.month,   2, '0');
    strfmt(buff, "CREATED = %s:%s:%s %s-%s-%d\n",
        hours, minutes, seconds,
        day, month, dt.year);
    term_write(buff);
    datetime_unpack(&dt, node.time_changed);
    intpad(seconds, dt.seconds, 2, '0');
    intpad(minutes, dt.minutes, 2, '0');
    intpad(hours,   dt.hours,   2, '0');
    intpad(day,     dt.day,     2, '0');
    intpad(month,   dt.month,   2, '0');
    strfmt(buff, "CHANGED = %s:%s:%s %s-%s-%d\n",
        hours, minutes, seconds,
        day, month, dt.year);
    term_write(buff);
    
    strfmt(buff, "SECTOR = %d\n", node_sector);
    term_write(buff);
    if (node.flags & FILE_FOLDER)
        strcpy(buff, "TYPE = FOLDER\n");
    else if (node.flags & FILE_DATA) {
        strfmt(buff, "SIZE = %d (%d sectors)\n", node.size, (int)node.size / sizeof(block.data));
        term_write(buff);
        strcpy(buff, "TYPE = FILE\n");
    }
    term_write(buff);
    return 0;
}

static int command_printfile(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc < 1) {
        term_write("Usage: printfile <path>\n");
        return 1;
    }

    uint32_t file_sector = file_get_node(argv[0]);
    if (!file_sector) {
        term_write("File not found!\n");
        return 1;
    }

    file_node_t file;
    file_node(file_sector, &file);

    if (!(file.flags & FILE_DATA)) {
        term_write("Not a file!\n");
        return 1;
    }

    char *file_content = file_read(file_sector);
    term_write(file_content);

    heap_free(file_content);
    return 0;
}

static int command_runscript(int argc, char *argv[]) {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (argc < 1) {
        term_write("Usage: runscript <path>\n");
        return 1;
    }

    uint32_t file_sector = file_get_node(argv[0]);
    if (!file_sector) {
        term_write("File not found!\n");
        return 1;
    }

    file_node_t file;
    file_node(file_sector, &file);

    if (!(file.flags & FILE_DATA)) {
        term_write("Not a file!\n");
        return 1;
    }

    script_run(argv[0], argc - 1, argv + 1);
    return script_exit;
}

static int command_time(int argc, char *argv[]) {
    rtc_datetime_t now;
    rtc_datetime(&now);

    int time_offset = 0;

    char *time_config = config_get("/system/config/time.cfg", "offset");
    if (time_config)
        time_offset = intstr(time_config);

    if (argc == 1)
        time_offset = intstr(argv[0]);

    if (time_offset != 0)
        rtc_to_local(&now, time_offset);

    char msg[8];
    char hrs[3];
    char min[3];
    intpad(hrs, now.hours, 2, '0');
    intpad(min, now.minutes, 2, '0');

    strfmt(msg, "%s:%s\n", hrs, min);
    term_write(msg);

    return 0;
}

static int command_date(int argc, char *argv[]) {
    rtc_datetime_t now;
    rtc_datetime(&now);

    int time_offset = 0;

    char *time_config = config_get("/system/config/time.cfg", "offset");
    if (time_config)
        time_offset = intstr(time_config);

    if (argc == 1)
        time_offset = intstr(argv[0]);

    if (time_offset != 0)
        rtc_to_local(&now, time_offset);

    char msg[16];
    char day[3];
    char month[3];
    intpad(day, now.day, 2, '0');
    intpad(month, now.month, 2, '0');

    strfmt(msg, "%s-%s-%d\n", day, month, now.year);
    term_write(msg);

    return 0;
}

static int command_datetime(int argc, char *argv[]) {
    rtc_datetime_t now;
    rtc_datetime(&now);

    int time_offset = 0;

    char *time_config = config_get("/system/config/time.cfg", "offset");
    if (time_config)
        time_offset = intstr(time_config);

    if (argc == 1)
        time_offset = intstr(argv[0]);

    if (time_offset != 0)
        rtc_to_local(&now, time_offset);

    char msg[18];
    char hrs[3];
    char min[3];
    char day[3];
    char month[3];
    intpad(hrs, now.hours, 2, '0');
    intpad(min, now.minutes, 2, '0');
    intpad(day, now.day, 2, '0');
    intpad(month, now.month, 2, '0');

    strfmt(msg, "%s:%s %s-%s-%d\n", hrs, min, day, month, now.year);
    term_write(msg);

    return 0;
}

static int command_diskinfo(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    char buffer[64];

    uint8_t ata_id[512];
    ata_identify(file_port, ata_id);
    uint16_t *w = (uint16_t*) ata_id;

    term_write("PORT = ");
    if (file_port == ATA_PRIMARY)
        term_write("PRIMARY");
    else if (file_port == ATA_SECONDARY)
        term_write("SECONDARY");
    term_write("\n");

    term_write("DRIVE = ");
    if (file_drive == ATA_MASTER)
        term_write("MASTER");
    else if (file_drive == ATA_SLAVE)
        term_write("SLAVE");
    term_write("\n");

    term_write("SERIAL NUMBER = ");
    ata_print_string(w, 10, 19);
    term_write("\n");

    term_write("FIRMWARE REV = ");
    ata_print_string(w, 23, 26);
    term_write("\n");

    term_write("MODEL NUMBER = ");
    ata_print_string(w, 27, 46);
    term_write("\n");

    uint32_t sectors = (uint32_t)w[60] | ((uint32_t)w[61] << 16);
    char disk_total[16];
    unit_get_size(sectors * 512, disk_total);
    strfmt(buffer, "SECTORS = %d (%s)\n", sectors, disk_total);
    term_write(buffer);

    return 0;
}

static int command_setupsystem(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    if (!file_path_isfolder("/system")) command_handle("newfolder /system", 0);
    if (!file_path_isfolder("/system/config")) command_handle("newfolder /system/config", 0);
    if (!file_path_isfolder("/system/scripts")) command_handle("newfolder /system/scripts", 0);
    if (!file_path_isfile("/system/init.sc")) command_handle("newfile /system/init.sc", 0);

    term_write("Setup successful!\n");
    return 0;
}

static int command_reloadconfig(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    term_load_config();
    return 0;
}

int command_handle(char *command, int printprompt) {
    int exit = 0;

    string_t *cmd = string_init();
    string_t *args[COMMAND_MAX_ARG] = { NULL };

    int argc = 0;
    char *argv[COMMAND_MAX_ARG];

    int read_cmd = 1;

    strtrim(command);
    while (*command != '\0') {
        if (argc >= COMMAND_MAX_ARG) break;

        char c = *command;
        if (c == ' ') {
            if (read_cmd)
                read_cmd = 0;
            else if (!string_empty(args[argc]))
                argc++;
        } else {
            if (read_cmd)
                string_putc(cmd, c);
            else {
                if (!args[argc])
                    args[argc] = string_init();
                string_putc(args[argc], c);
            }
        }

        command++;
    }

    if (!string_empty(args[argc]))
        argc++;

    for (int i = 0; i < argc; i++)
        argv[i] = args[i]->value;

    if (!strcmp(cmd->value, "scale")) exit = command_scale(argc, argv);
    else if (!strcmp(cmd->value, "scaleup")) exit = command_scaleup(argc, argv);
    else if (!strcmp(cmd->value, "scaledown")) exit = command_scaledown(argc, argv);
    else if (!strcmp(cmd->value, "clear")) exit = command_clear(argc, argv);
    else if (!strcmp(cmd->value, "shutdown")) exit = command_shutdown(argc, argv);
    else if (!strcmp(cmd->value, "fetch")) exit = command_fetch(argc, argv);
    else if (!strcmp(cmd->value, "echo")) exit = command_echo(argc, argv);
    else if (!strcmp(cmd->value, "list")) exit = command_list(argc, argv);
    else if (!strcmp(cmd->value, "newfile")) exit = command_newfile(argc, argv);
    else if (!strcmp(cmd->value, "delfile")) exit = command_delfile(argc, argv);
    else if (!strcmp(cmd->value, "edit")) exit = command_edit(argc, argv);
    else if (!strcmp(cmd->value, "newfolder")) exit = command_newfolder(argc, argv);
    else if (!strcmp(cmd->value, "delfolder")) exit = command_delfolder(argc, argv);
    else if (!strcmp(cmd->value, "goto")) exit = command_goto(argc, argv);
    else if (!strcmp(cmd->value, "goup")) exit = command_goup(argc, argv);
    else if (!strcmp(cmd->value, "whereami")) exit = command_whereami(argc, argv);
    else if (!strcmp(cmd->value, "copyfile")) exit = command_copyfile(argc, argv);
    else if (!strcmp(cmd->value, "movefile")) exit = command_movefile(argc, argv);
    else if (!strcmp(cmd->value, "copyfolder")) exit = command_copyfolder(argc, argv);
    else if (!strcmp(cmd->value, "movefolder")) exit = command_movefolder(argc, argv);
    else if (!strcmp(cmd->value, "formatdisk")) exit = command_formatdisk(argc, argv);
    else if (!strcmp(cmd->value, "nodeinfo")) exit = command_nodeinfo(argc, argv);
    else if (!strcmp(cmd->value, "printfile")) exit = command_printfile(argc, argv);
    else if (!strcmp(cmd->value, "runscript")) exit = command_runscript(argc, argv);
    else if (!strcmp(cmd->value, "time")) exit = command_time(argc, argv);
    else if (!strcmp(cmd->value, "date")) exit = command_date(argc, argv);
    else if (!strcmp(cmd->value, "datetime")) exit = command_datetime(argc, argv);
    else if (!strcmp(cmd->value, "diskinfo")) exit = command_diskinfo(argc, argv);
    else if (!strcmp(cmd->value, "setupsystem")) exit = command_setupsystem(argc, argv);
    else if (!strcmp(cmd->value, "reloadconfig")) exit = command_reloadconfig(argc, argv);
    else {
        int found_script = 0;

        if (file_drive_status == FILE_DRIVE_OK && !config_has("/system/config/system.cfg", "disable_user_scripts")) {
            char *script_path = heap_alloc(20 + cmd->size);
            strfmt(script_path, "/system/scripts/%s.sc", cmd->value);

            if (file_path_isfile(script_path)) {
                found_script = 1;
                script_run(script_path, argc, argv);
                exit = script_exit;
            }

            heap_free(script_path);
        }

        if (!found_script && !string_empty(cmd)) {
            term_write("Unknown command\n");
            exit = 1;
        }
    }

    if (keyboard_mode == KEYBOARD_MODE_TERM) {
        if (!term_input_buffer && printprompt)
            term_draw_prompt();

        term_prompt = term_x;
    }

    string_free(cmd);
    for (int i = 0; i < argc; i++)
        string_free(args[i]);

    return exit;
}
