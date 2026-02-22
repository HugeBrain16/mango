#include <cpuid.h>
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

static void command_help(int argc, char *argv[]) {
    unused(argc); unused(argv);

    term_write("Available commands:\n", COLOR_WHITE, COLOR_BLACK);
    term_write("help - show this message\n", COLOR_WHITE, COLOR_BLACK);
    term_write("scaleup - scale up screen by 1\n", COLOR_WHITE, COLOR_BLACK);
    term_write("scaledown - scale down screen by 1\n", COLOR_WHITE, COLOR_BLACK);
    term_write("clear - clear screen\n", COLOR_WHITE, COLOR_BLACK);
    term_write("shutdown - shut down the machine\n", COLOR_WHITE, COLOR_BLACK);
    term_write("fetch - show system info\n", COLOR_WHITE, COLOR_BLACK);
    term_write("echo - print words\n", COLOR_WHITE, COLOR_BLACK);
    term_write("list - list items in a folder\n", COLOR_WHITE, COLOR_BLACK);
    term_write("newfile - create a new file\n", COLOR_WHITE, COLOR_BLACK);
    term_write("delfile - delete a file\n", COLOR_WHITE, COLOR_BLACK);
    term_write("copyfile - copy a file\n", COLOR_WHITE, COLOR_BLACK);
    term_write("movefile - move a file\n", COLOR_WHITE, COLOR_BLACK);
    term_write("newfolder - create a new folder\n", COLOR_WHITE, COLOR_BLACK);
    term_write("delfolder - delete a folder and its content\n", COLOR_WHITE, COLOR_BLACK);
    term_write("copyfolder - copy a folder and its content\n", COLOR_WHITE, COLOR_BLACK);
    term_write("movefolder - move a folder and its content\n", COLOR_WHITE, COLOR_BLACK);
    term_write("edit - edit a file\n", COLOR_WHITE, COLOR_BLACK);
    term_write("printfile - show the content of a file\n", COLOR_WHITE, COLOR_BLACK);
    term_write("goto - go into a folder\n", COLOR_WHITE, COLOR_BLACK);
    term_write("goup - go up a folder\n", COLOR_WHITE, COLOR_BLACK);
    term_write("whereami - show full path to current location\n", COLOR_WHITE, COLOR_BLACK);
    term_write("formatdisk - format primary disk drive\n", COLOR_WHITE, COLOR_BLACK);
    term_write("nodeinfo - show info of a filesystem node\n", COLOR_WHITE, COLOR_BLACK);
    term_write("runscript - run a script file\n", COLOR_WHITE, COLOR_BLACK);
    term_write("time - show current time\n", COLOR_WHITE, COLOR_BLACK);
    term_write("date - show current date\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_scaleup(int argc, char *argv[]) {
    unused(argc); unused(argv);

    screen_scale++;
}

static void command_scaledown(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (screen_scale > 1)
        screen_scale--;
}

static void command_clear(int argc, char *argv[]) {
    unused(argc); unused(argv);

    screen_clear(COLOR_BLACK);
    term_x = 0;
    term_y = 0;
}

static void command_shutdown(int argc, char *argv[]) {
    unused(argc); unused(argv);

    term_write("Shutting down...\n", COLOR_WHITE, COLOR_BLACK);

    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);

    __asm__ volatile("cli; hlt");
}

static void command_fetch(int argc, char *argv[]) {
    unused(argc); unused(argv);

    char buff[128];

    term_write("\n", COLOR_BLACK, COLOR_BLACK);
    for (int i = 0; i < 6; i++) {
        term_write("=", COLOR_YELLOW, COLOR_BLACK);
        term_write("=", COLOR_WHITE, COLOR_BLACK);
        term_write("=", COLOR_YELLOW, COLOR_BLACK);
    }
    term_write("\n", COLOR_BLACK, COLOR_BLACK);
    term_write("Kernel: Mango\n", COLOR_WHITE, COLOR_BLACK);

    uint32_t eax, ebx, ecx, edx;
    char cpu_name[64] = {0};
    uint32_t *p = (uint32_t *) cpu_name;

    __cpuid(0x80000000, eax, ebx, ecx, edx);

    if (eax >= 0x80000004) {
        for (int i = 0; i < 3; i++) {
            __cpuid(0x80000002 + i, eax, ebx, ecx, edx);
            *p++ = eax;
            *p++ = ebx;
            *p++ = ecx;
            *p++ = edx;
        }
    } else {
        __cpuid(0, eax, ebx, ecx, edx);
        p[0] = ebx;
        p[1] = edx;
        p[2] = ecx;

        p[12] = '\0';

        __cpuid(1, eax, ebx, ecx, edx);

        uint32_t base_model = (eax >> 4) & 0xF;
        uint32_t base_family = (eax >> 8) & 0xF;
        uint32_t ext_model = (eax >> 16) & 0xF;
        uint32_t ext_family = (eax >> 20) & 0xFF;

        uint32_t family = base_family;
        if (family == 0xF)
            family += ext_family;

        uint32_t model = base_model;
        if (base_family == 0x6 || base_family == 0xF)
            model |= (ext_model << 4);

        strfmt(cpu_name, "%s (Family %d Model %d)", cpu_name, family, model);
    }
    strfmt(buff, "CPU: %s\n", cpu_name);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);

    char mem_total[16];
    char mem_free[16];
    unit_get_size(heap_end - heap_start + (2 << 20), mem_total);
    unit_get_size(heap_end - heap_current + (2 << 20), mem_free);
    strfmt(buff, "Memory: %s (Free: %s)\n", mem_total, mem_free);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);

    uint8_t ata_id[512];
    ata_identify(file_port, ata_id);
    uint16_t *w = (uint16_t*) ata_id;
    uint32_t sectors = (uint32_t)w[60] | ((uint32_t)w[61] << 16);
    char disk_total[16];
    unit_get_size(sectors * 512, disk_total);
    strfmt(buff, "Disk: %s ", disk_total);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    if (file_is_formatted()) {
        file_superblock_t sb;
        file_read_sb(&sb);

        char disk_used[16];
        unit_get_size(sb.used * 512, disk_used);
        strfmt(buff, "(Used: %s) ", disk_used);
        term_write(buff, COLOR_WHITE, COLOR_BLACK);
    }
    term_write("\n", COLOR_WHITE, COLOR_BLACK);

    strcpy(buff, "Uptime:");
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    if (uptime_hours > 0) {
        strfmt(buff, " %d hours", uptime_hours);
        term_write(buff, COLOR_WHITE, COLOR_BLACK);
    }
    if (uptime_minutes > 0) {
        strfmt(buff, " %d minutes", uptime_minutes);
        term_write(buff, COLOR_WHITE, COLOR_BLACK);
    }
    if (uptime_seconds > 0) {
        strfmt(buff, " %d seconds", uptime_seconds);
        term_write(buff, COLOR_WHITE, COLOR_BLACK);
    }
    term_write("\n", COLOR_BLACK, COLOR_BLACK);

    term_write("\n", COLOR_BLACK, COLOR_BLACK);
    term_write("=", COLOR_YELLOW, COLOR_YELLOW);
    term_write("=", COLOR_WHITE, COLOR_WHITE);
    term_write("=", COLOR_PURPLE, COLOR_PURPLE);
    term_write("=", COLOR_DARKGRAY, COLOR_DARKGRAY);
    term_write("\n\n", COLOR_BLACK, COLOR_BLACK);
}

static void command_echo(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        term_write(argv[i], COLOR_WHITE, COLOR_BLACK);
        term_write(" ", COLOR_BLACK, COLOR_BLACK);
    }
    term_write("\n", COLOR_BLACK, COLOR_BLACK);
}

static void command_list(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    static char buff[FILE_MAX_NAME + 16];
    uint32_t parent;
    file_node_t parent_node;
    if (argc > 0)
        parent = file_get_node(argv[0]);
    else
        parent = file_current;
    file_node(parent, &parent_node);

    if (parent_node.child_head && (parent_node.flags & FILE_FOLDER)) {
        term_write("\nList of files:\n", COLOR_WHITE, COLOR_BLACK);

        uint32_t current = parent_node.child_head;
        file_node_t current_node;

        while (current) {
            file_node(current, &current_node);

            if (current_node.flags & FILE_DATA)
                strfmt(buff, "- %s\n", current_node.name);
            else if (current_node.flags & FILE_FOLDER)
                strfmt(buff, "-> %s\n", current_node.name);
            term_write(buff, COLOR_WHITE, COLOR_BLACK);
            current = current_node.child_next;
        }
    } else if (!parent || !(parent_node.flags & FILE_FOLDER))
        term_write("Not a folder.\n", COLOR_WHITE, COLOR_BLACK);
    else
        term_write("Empty folder.\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_newfile(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    if (argc > 0) {
        uint32_t parent;

        char *path_parent = heap_alloc(FILE_MAX_PATH - FILE_MAX_NAME);
        char *path_basename = heap_alloc(FILE_MAX_NAME);

        if (file_split_path(argv[0], path_parent, path_basename)) {
            if (path_parent[0] == '\0')
                parent = file_current;
            else
                parent = file_get_node(path_parent);

            if (parent) {
                if (strlen(path_basename) > FILE_MAX_NAME)
                    term_write("File name is too long!\n", COLOR_WHITE, COLOR_BLACK);
                else if (file_exists(parent, path_basename))
                    term_write("File with the same name already exist!\n", COLOR_WHITE, COLOR_BLACK);
                else if (!file_create(parent, path_basename))
                    term_write("Failed creating a new file!\n", COLOR_WHITE, COLOR_BLACK);
            } else
                term_write("Invalid path.\n", COLOR_WHITE, COLOR_BLACK);
        } else
            term_write("Invalid path.\n", COLOR_WHITE, COLOR_BLACK);

        heap_free(path_parent);
        heap_free(path_basename);
    } else
        term_write("Usage: newfile <path>\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_delfile(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    if (argc > 0) {
        uint32_t target = file_get_node(argv[0]);

        if (!target) return term_write("File not found!\n", COLOR_WHITE, COLOR_BLACK);

        file_node_t target_node;
        file_node(target, &target_node);

        if (!file_delete(target_node.parent, target_node.name))
            term_write("Failed deleting file!\n", COLOR_WHITE, COLOR_BLACK);
    } else
        term_write("Usage: delfile <path>\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_edit(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    if (argc > 0) {
        uint32_t parent;

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
                } else
                    term_write("File does not exist!\n", COLOR_WHITE, COLOR_BLACK);
            } else
                term_write("Invalid path!\n", COLOR_WHITE, COLOR_BLACK);
        } else
            term_write("Invalid path!\n", COLOR_WHITE, COLOR_BLACK);

        heap_free(path_parent);
        heap_free(path_basename);
    } else
        term_write("Usage: edit <path>\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_newfolder(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    if (argc > 0) {
        uint32_t parent;

        char *path_parent = heap_alloc(FILE_MAX_PATH - FILE_MAX_NAME);
        char *path_basename = heap_alloc(FILE_MAX_NAME);

        if (file_split_path(argv[0], path_parent, path_basename)) {
            if (path_parent[0] == '\0')
                parent = file_current;
            else
                parent = file_get_node(path_parent);

            if (parent) {
                if (strlen(path_basename) > FILE_MAX_NAME)
                    term_write("Folder name is too long!\n", COLOR_WHITE, COLOR_BLACK);
                else if (folder_exists(parent, path_basename))
                    term_write("Folder with the same name already exist!\n", COLOR_WHITE, COLOR_BLACK);
                else if (!folder_create(parent, path_basename))
                    term_write("Failed creating a new folder!\n", COLOR_WHITE, COLOR_BLACK);
            } else
                term_write("Invalid path.\n", COLOR_WHITE, COLOR_BLACK);
        } else
            term_write("Invalid path.\n", COLOR_WHITE, COLOR_BLACK);

        heap_free(path_parent);
        heap_free(path_basename);
    } else
        term_write("Usage: newfolder <path>\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_delfolder(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    if (argc > 0) {
        uint32_t target = file_get_node(argv[0]);
        file_node_t target_node;
        file_node(target, &target_node);

        if (target != FILE_SECTOR_ROOT) {
            if (!folder_delete(target_node.parent, target_node.name))
                term_write("Failed deleting folder!\n", COLOR_WHITE, COLOR_BLACK);
        } else
            term_write("Cannot delete root folder!\n", COLOR_WHITE, COLOR_BLACK);
    } else
        term_write("Usage: delfolder <path>\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_goto(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    if (argc > 0) {
        uint32_t target = file_get_node(argv[0]);
        file_node_t target_node;
        file_node(target, &target_node);

        if (target && (target_node.flags & FILE_FOLDER))
            file_current = target;
        else
            term_write("Not a folder.\n", COLOR_WHITE, COLOR_BLACK);
    } else
        term_write("Usage: goto <path>\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_goup(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    file_node_t parent_node;
    file_node(file_current, &parent_node);

    if (parent_node.parent)
        file_current = parent_node.parent;
    else
        term_write("Already at topmost folder!\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_whereami(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    char *path = heap_alloc(FILE_MAX_PATH);
    file_get_abspath(file_current, path, FILE_MAX_PATH);

    term_write(path, COLOR_WHITE, COLOR_BLACK);
    term_write("\n", COLOR_WHITE, COLOR_BLACK);
    heap_free(path);
}

static void command_copyfile(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    if (argc < 2) return term_write("Usage: copyfile <src> <dest>\n", COLOR_WHITE, COLOR_BLACK);

    uint32_t src = file_get_node(argv[0]);
    file_node_t src_node;
    file_node(src, &src_node);

    if (!src) return term_write("Source file doesn't exist!\n", COLOR_WHITE, COLOR_BLACK);
    if (!(src_node.flags & FILE_DATA)) return term_write("Not a file!\n", COLOR_WHITE, COLOR_BLACK);

    char *dest_parent = heap_alloc(FILE_MAX_PATH - FILE_MAX_NAME);
    char *dest_basename = heap_alloc(FILE_MAX_NAME);
    if (!file_split_path(argv[1], dest_parent, dest_basename)) {
        term_write("Invalid destination path!\n", COLOR_WHITE, COLOR_BLACK);
        goto cleanup;
    }

    uint32_t dest_parent_node;
    if (dest_parent[0] == '\0')
        dest_parent_node = file_current;
    else
        dest_parent_node = file_get_node(dest_parent);

    if (!dest_parent_node) {
        term_write("Parent folder doesn't exist!\n", COLOR_WHITE, COLOR_BLACK);
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
        if (strlen(dest_basename) > FILE_MAX_NAME) return term_write("File name is too long!\n", COLOR_WHITE, COLOR_BLACK);
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
}

static void command_movefile(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    if (argc < 2) return term_write("Usage: movefile <src> <dest>\n", COLOR_WHITE, COLOR_BLACK);

    command_copyfile(argc, argv);

    uint32_t src = file_get_node(argv[0]);
    file_node_t src_node;
    file_node(src, &src_node);

    file_delete(src_node.parent, src_node.name);
}

static void command_copyfolder(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    if (argc < 2) return term_write("Usage: copyfolder <src> <dest>\n", COLOR_WHITE, COLOR_BLACK);

    uint32_t src = file_get_node(argv[0]);
    file_node_t src_node;
    file_node(src, &src_node);

    if (!src) return term_write("Source folder doesn't exist!\n", COLOR_WHITE, COLOR_BLACK);
    if (!(src_node.flags & FILE_FOLDER)) return term_write("Not a folder!\n", COLOR_WHITE, COLOR_BLACK);

    char *dest_parent = heap_alloc(FILE_MAX_PATH - FILE_MAX_NAME);
    char *dest_basename = heap_alloc(FILE_MAX_NAME);
    if (!file_split_path(argv[1], dest_parent, dest_basename)) {
        term_write("Invalid destination path!\n", COLOR_WHITE, COLOR_BLACK);
        goto cleanup;
    }

    uint32_t dest_parent_node;
    if (dest_parent[0] == '\0')
        dest_parent_node = file_current;
    else
        dest_parent_node = file_get_node(dest_parent);

    if (!dest_parent_node) {
        term_write("Parent folder doesn't exist!\n", COLOR_WHITE, COLOR_BLACK);
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
        if (strlen(dest_basename) > FILE_MAX_NAME) return term_write("Folder name is too long!\n", COLOR_WHITE, COLOR_BLACK);
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
}

static void command_movefolder(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);

    if (argc < 2) return term_write("Usage: movefolder <src> <dest>\n", COLOR_WHITE, COLOR_BLACK);

    command_copyfolder(argc, argv);

    uint32_t src = file_get_node(argv[0]);
    file_node_t src_node;
    file_node(src, &src_node);

    folder_delete(src_node.parent, src_node.name);
}

static void command_formatdisk(int argc, char *argv[]) {
    unused(argc); unused(argv);

    char confirm[TERM_INPUT_SIZE];
    term_get_input("This will erase the whole disk. Are you sure? (type \"y\"): ",
            confirm, sizeof(confirm), COLOR_WHITE, COLOR_BLACK);

    if (!strcmp(confirm, "y")) {
        file_format();
        term_write("Disk formatted.\n", COLOR_WHITE, COLOR_BLACK);
    }
}

static void command_nodeinfo(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);
    if (argc < 1) return term_write("Usage: nodeinfo <path>\n", COLOR_WHITE, COLOR_BLACK);

    uint32_t node_sector = file_get_node(argv[0]);
    if (!node_sector) return term_write("Not found\n", COLOR_WHITE, COLOR_BLACK);

    file_node_t node;
    file_data_t block;
    file_node(node_sector, &node);
    file_data(node.first_block, &block);

    char buff[128];
    strfmt(buff, "NAME = %s\n", node.name);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);

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
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    datetime_unpack(&dt, node.time_changed);
    intpad(seconds, dt.seconds, 2, '0');
    intpad(minutes, dt.minutes, 2, '0');
    intpad(hours,   dt.hours,   2, '0');
    intpad(day,     dt.day,     2, '0');
    intpad(month,   dt.month,   2, '0');
    strfmt(buff, "CHANGED = %s:%s:%s %s-%s-%d\n",
        hours, minutes, seconds,
        day, month, dt.year);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    
    strfmt(buff, "SECTOR = %d\n", node_sector);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    if (node.flags & FILE_FOLDER)
        strcpy(buff, "TYPE = FOLDER\n");
    else if (node.flags & FILE_DATA) {
        strfmt(buff, "SIZE = %d (%d sectors)\n", sizeof(block.data) * node.size, node.size);
        term_write(buff, COLOR_WHITE, COLOR_BLACK);
        strcpy(buff, "TYPE = FILE\n");
    }
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
}

static void command_printfile(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);
    if (argc < 1)
        return term_write("Usage: printfile <path>\n", COLOR_WHITE, COLOR_BLACK);

    uint32_t file_sector = file_get_node(argv[0]);
    if (!file_sector)
        return term_write("File not found!\n", COLOR_WHITE, COLOR_BLACK);

    file_node_t file;
    file_node(file_sector, &file);

    if (!(file.flags & FILE_DATA))
        return term_write("Not a file!\n", COLOR_WHITE, COLOR_BLACK);

    char *file_content = file_read(file_sector);
    term_write(file_content, COLOR_WHITE, COLOR_BLACK);

    heap_free(file_content);
}

static void command_runscript(int argc, char *argv[]) {
    if (!file_is_formatted()) return term_write("Disk is not formatted!\n", COLOR_WHITE, COLOR_BLACK);
    if (argc < 1)
        return term_write("Usage: runscript <path>\n", COLOR_WHITE, COLOR_BLACK);

    uint32_t file_sector = file_get_node(argv[0]);
    if (!file_sector)
        return term_write("File not found!\n", COLOR_WHITE, COLOR_BLACK);

    file_node_t file;
    file_node(file_sector, &file);

    if (!(file.flags & FILE_DATA))
        return term_write("Not a file!\n", COLOR_WHITE, COLOR_BLACK);

    script_run(argv[0]);
}

static void command_time(int argc, char *argv[]) {
    rtc_datetime_t now;
    rtc_datetime(&now);
    if (argc == 1)
        rtc_to_local(&now, intstr(argv[0]));

    char msg[8];
    char hrs[3];
    char min[3];
    intpad(hrs, now.hours, 2, '0');
    intpad(min, now.minutes, 2, '0');

    strfmt(msg, "%s:%s\n", hrs, min);
    term_write(msg, COLOR_WHITE, COLOR_BLACK);
}

static void command_date(int argc, char *argv[]) {
    rtc_datetime_t now;
    rtc_datetime(&now);
    if (argc == 1)
        rtc_to_local(&now, intstr(argv[0]));

    char msg[16];
    char day[3];
    char month[3];
    intpad(day, now.day, 2, '0');
    intpad(month, now.month, 2, '0');

    strfmt(msg, "%s-%s-%d\n", day, month, now.year);
    term_write(msg, COLOR_WHITE, COLOR_BLACK);
}

void command_handle(const char *command, int printcaret) {
    char cmd[COMMAND_MAX_NAME] = {0};
    static char args[COMMAND_MAX_ARGC][COMMAND_MAX_ARGV] = {0};
    char *argv[32];
    int argc = 0;
    int idx = 0;
    int in_cmd = 1;

    for (size_t i = 0; command[i] != '\0'; i++) {
        if (command[i] == ' ') {
            if (in_cmd) {
                cmd[idx] = '\0';
                in_cmd = 0;
                idx = 0;
            } else if (idx > 0) {
                args[argc][idx] = '\0';
                argc++;
                if (argc >= 32) break;
                idx = 0;
            }
        } else {
            if (in_cmd) {
                cmd[idx++] = command[i];
            } else {
                args[argc][idx++] = command[i];
            }
        }
    }

    if (!in_cmd && idx > 0) {
        args[argc][idx] = '\0';
        argc++;
    }

    for (int i = 0; i < argc; i++) {
        argv[i] = args[i];
    }

    if (!strcmp(cmd, "scaleup")) command_scaleup(argc, argv);
    else if (!strcmp(cmd, "scaledown")) command_scaledown(argc, argv);
    else if (!strcmp(cmd, "clear")) command_clear(argc, argv);
    else if (!strcmp(cmd, "shutdown")) command_shutdown(argc, argv);
    else if (!strcmp(cmd, "fetch")) command_fetch(argc, argv);
    else if (!strcmp(cmd, "echo")) command_echo(argc, argv);
    else if (!strcmp(cmd, "list")) command_list(argc, argv);
    else if (!strcmp(cmd, "newfile")) command_newfile(argc, argv);
    else if (!strcmp(cmd, "delfile")) command_delfile(argc, argv);
    else if (!strcmp(cmd, "edit")) command_edit(argc, argv);
    else if (!strcmp(cmd, "newfolder")) command_newfolder(argc, argv);
    else if (!strcmp(cmd, "delfolder")) command_delfolder(argc, argv);
    else if (!strcmp(cmd, "goto")) command_goto(argc, argv);
    else if (!strcmp(cmd, "goup")) command_goup(argc, argv);
    else if (!strcmp(cmd, "whereami")) command_whereami(argc, argv);
    else if (!strcmp(cmd, "copyfile")) command_copyfile(argc, argv);
    else if (!strcmp(cmd, "movefile")) command_movefile(argc, argv);
    else if (!strcmp(cmd, "copyfolder")) command_copyfolder(argc, argv);
    else if (!strcmp(cmd, "movefolder")) command_movefolder(argc, argv);
    else if (!strcmp(cmd, "formatdisk")) command_formatdisk(argc, argv);
    else if (!strcmp(cmd, "nodeinfo")) command_nodeinfo(argc, argv);
    else if (!strcmp(cmd, "printfile")) command_printfile(argc, argv);
    else if (!strcmp(cmd, "runscript")) command_runscript(argc, argv);
    else if (!strcmp(cmd, "time")) command_time(argc, argv);
    else if (!strcmp(cmd, "date")) command_date(argc, argv);
    else if (!strcmp(cmd, "help")) command_help(argc, argv);
    else
        if (cmd[0] != '\0') term_write("Unknown command\n", COLOR_WHITE, COLOR_BLACK);

    if (keyboard_mode == KEYBOARD_MODE_TERM) {
        if (!term_input_buffer && printcaret) {
            term_write("\n> ", COLOR_WHITE, COLOR_BLACK);
        }

        term_prompt = term_x;
    }
}
