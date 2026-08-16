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
#include "sound.h"
#include "pci.h"
#include "desktop.h"
#include "media.h"
#include "modules.h"
#include <external/spng/spng.h>

#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION
#include <external/minimp3/minimp3.h>

static int nodisk() {
    if (file_drive_status != FILE_DRIVE_OK) {
        term_write("No drive.\n");
        return 1;
    }

    if (!file_is_formatted()) {
        term_write("Disk is not formatted!\n");
        return 1;
    }

    return 0;
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
    int show_colors = 1;

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "diskname"))
            show_diskname = 1;
        else if (!strcmp(argv[i], "noribbon"))
            show_ribbon = 0;
        else if (!strcmp(argv[i], "nocolors"))
            show_colors = 0;
    }

    char buff[128];

    if (show_ribbon) {
        term_write("\n");
        for (int i = 0; i < 6; i++) {
            term_write2("=", term_fg != COLOR_WHITE ? term_fg : COLOR_YELLOW, term_bg);
            term_write2("=", COLOR_WHITE, term_bg);
            term_write2("=", term_fg != COLOR_WHITE ? term_fg : COLOR_YELLOW, term_bg);
        }
    }
    strfmt(buff, "\nKernel: Mango b%d\n", BUILD_NUMBER);
    term_write(buff);

    strfmt(buff, "CPU: %s\n", cpu_name);
    term_write(buff);

    char mem_total[16];
    char mem_free[16];
    unit_get_size(heap_end - heap_start + (2 << 20), mem_total);
    unit_get_size(heap_free_bytes() + (2 << 20), mem_free);
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
            char name[64];
            ata_get_string(w, 27, 46, name, 64);
            term_write(name);
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

    if (show_colors) {
        term_write("\n");
        term_write2("=", COLOR_YELLOW, COLOR_YELLOW);
        term_write2("=", COLOR_WHITE, COLOR_WHITE);
        term_write2("=", COLOR_PURPLE, COLOR_PURPLE);
        term_write2("=", COLOR_DARKGRAY, COLOR_DARKGRAY);
    }
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
    if (nodisk()) return 1;

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
    if (nodisk()) return 1;

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
    if (nodisk()) return 1;

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
    if (nodisk()) return 1;

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
        edit_init(0);
        return 0;
    }
}

static int command_newfolder(int argc, char *argv[]) {
    if (nodisk()) return 1;

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
    if (nodisk()) return 1;

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
    if (nodisk()) return 1;

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

    if (nodisk()) return 1;

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

    if (nodisk()) return 1;

    char *path = heap_alloc(FILE_MAX_PATH);
    file_get_abspath(file_current, path, FILE_MAX_PATH);

    term_write(path);
    term_write("\n");
    heap_free(path);

    return 0;
}

static int command_copyfile(int argc, char *argv[]) {
    if (nodisk()) return 1;

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
        file_write(dest, data, src_node.size);
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
        file_write(dest, data, src_node.size);
        heap_free(data);
    }

cleanup:
    heap_free(dest_parent);
    heap_free(dest_basename);

    return exit;
}

static int command_movefile(int argc, char *argv[]) {
    if (nodisk()) return 1;

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
    if (nodisk()) return 1;

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
        file_write(dest_child, data, child_node.size);
        heap_free(data);

        child = child_node.child_next;
    }

cleanup:
    heap_free(dest_parent);
    heap_free(dest_basename);

    return exit;
}

static int command_movefolder(int argc, char *argv[]) {
    if (nodisk()) return 1;

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

        if (file_is_formatted())
            term_write("Disk formatted.\n");
        else
            term_write("Failed to format disk.\n");
        return 0;
    }

    return 1;
}

static int command_nodeinfo(int argc, char *argv[]) {
    if (nodisk()) return 1;

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
    if (nodisk()) return 1;

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
    if (nodisk()) return 1;

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
    char msg[8];
    module_time(msg, argc > 1 ? intstr(argv[0]) : 0);
    term_write(msg);
    term_write("\n");
    return 0;
}

static int command_date(int argc, char *argv[]) {
    char msg[16];
    module_date(msg, argc > 1 ? intstr(argv[0]) : 0);
    term_write(msg);
    term_write("\n");
    return 0;
}

static int command_datetime(int argc, char *argv[]) {
    char time[8];
    char date[16];
    module_time(time, argc > 1 ? intstr(argv[0]) : 0);
    module_date(date, argc > 1 ? intstr(argv[0]) : 0);
    term_write(date);
    term_write(" ");
    term_write(time);
    term_write("\n");
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
    if (ata_identify(file_port, ata_id)) {
        uint16_t *w = (uint16_t*) ata_id;
        drive_t drive;
        file_drive_spec(&drive);

        strfmt(buffer, "SLOT = %d\n", file_drive_slot());
        term_write(buffer);

        term_write("SERIAL NUMBER = ");
        term_write(drive.serial);
        term_write("\n");

        term_write("FIRMWARE REV = ");
        term_write(drive.rev);
        term_write("\n");

        term_write("MODEL NUMBER = ");
        term_write(drive.model);
        term_write("\n");

        uint32_t sectors = (uint32_t)w[60] | ((uint32_t)w[61] << 16);
        char disk_total[16];
        unit_get_size(sectors * 512, disk_total);
        strfmt(buffer, "SECTORS = %d (%s)\n", sectors, disk_total);
        term_write(buffer);
    } else term_write("Couldn't identify disk.\n");

    return 0;
}

static int command_reloadconfig(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (nodisk()) return 1;

    term_load_config();
    return command_clear(argc, argv);
}

static int command_viewimage(int argc, char *argv[]) {
    if (nodisk()) return 1;

    if (argc < 1) {
        term_write("Usage: viewimage <path>");
        return 1;
    }

    uint32_t file_sector = file_get_node(argv[0]);
    if (!file_sector) {
        term_write("Not found!");
        return 1;
    }

    file_node_t file;
    file_node(file_sector, &file);

    if (!(file.flags & FILE_DATA)) {
        term_write("Not a file!");
        return 1;
    }

    char *content = file_read(file_sector);
    image_t *image = image_png(content, file.size);

    screen_draw_rgba(image->data, image->size, 0, term_y + (FONT_HEIGHT * screen_scale), image->width, image->height, 0);
    term_y += image->height + (FONT_HEIGHT * screen_scale);

    heap_free(content);
    image_free(image);
    return 0;
}

static int command_playaudio(int argc, char *argv[]) {
    if (nodisk()) return 1;

    if (argc < 1) {
        term_write("Usage: playaudio <path>");
        return 1;
    }

    uint32_t file_sector = file_get_node(argv[0]);
    if (!file_sector) {
        term_write("Not found!");
        return 1;
    }

    file_node_t file;
    file_node(file_sector, &file);

    if (!(file.flags & FILE_DATA)) {
        term_write("Not a file!");
        return 1;
    }

    uint8_t *mp3_data = (uint8_t*)file_read(file_sector);
    uint32_t mp3_size = file.size;

    mp3dec_t *decoder = heap_alloc(sizeof(mp3dec_t));
    mp3dec_init(decoder);

    int16_t *pcm_buff = heap_alloc(mp3_size * 12);
    uint32_t pcm_total = 0;

    mp3dec_frame_info_t info;
    int16_t *frame_buf = heap_alloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(int16_t));

    while (mp3_size > 0) {
        int samples = mp3dec_decode_frame(decoder, mp3_data, mp3_size, frame_buf, &info);
        if (samples == 0 || info.frame_bytes == 0) break;

        uint32_t count = samples * info.channels;
        memcpy(pcm_buff + pcm_total, frame_buf, count * 2);
        pcm_total += count;

        mp3_data += info.frame_bytes;
        mp3_size -= info.frame_bytes;
    }

    heap_free(frame_buf);
    heap_free(decoder);
    sound_play_pcm(pcm_buff, pcm_total);
    return 0;
}

static int command_listpci(int argc, char *argv[]) {
    int show_bus = 1;
    int show_device = 1;
    int show_rev = 1;
    int show_class = 0;
    int show_subclass = 0;

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "nobus"))
            show_bus = 0;
        else if (!strcmp(argv[i], "nodevice"))
            show_device = 0;
        else if (!strcmp(argv[i], "norev"))
            show_rev = 0;
        else if (!strcmp(argv[i], "class"))
            show_class = 1;
        else if (!strcmp(argv[i], "subclass"))
            show_subclass = 1;
    }

    char buffer[64];
    for (int x = 0; x < PCI_MAX_BUS; x++) {
        for (int y = 0; y < PCI_MAX_DEV; y++) {
            pci_device_t dev = {0};

            char idstr[9];
            if (pci_get_device(&dev, x, y)) {
                strhex(idstr, (uint32_t)dev.vendor_id);
                term_write(strsub(idstr, 4));
                term_write(":");
                strhex(idstr, (uint32_t)dev.device_id);
                term_write(strsub(idstr, 4));

                if (show_bus) {
                    strfmt(buffer, " Bus:%d", x);
                    term_write(buffer);
                }

                if (show_device) {
                    strfmt(buffer, " Device:%d", y);
                    term_write(buffer);
                }

                if (dev.revision && show_rev) {
                    strfmt(buffer, " Rev:%d", dev.revision);
                    term_write(buffer);
                }

                if (show_class) {
                    strfmt(buffer, " Class:%d", dev.class_code);
                    term_write(buffer);
                }

                if (show_subclass) {
                    strfmt(buffer, " Subclass:%d", dev.subclass);
                    term_write(buffer);
                }

                term_write("\n");
            }
        }
    }

    return 0;
}

static int command_meminfo(int argc, char *argv[]) {
    unused(argc); unused(argv);

    char buffer[64];

    size_t used;
    size_t usable;
    size_t free;
    int blocks;
    heap_stat(&used, &usable, &free, &blocks);

    strfmt(buffer, "USED = %d (%d KB)\n", used, used >> 10);
    term_write(buffer);
    strfmt(buffer, "USABLE = %d (%d KB)\n", usable, usable >> 10);
    term_write(buffer);
    strfmt(buffer, "FREE = %d (%d KB)\n", free, free >> 10);
    term_write(buffer);
    strfmt(buffer, "BLOCKS = %d\n", blocks);
    term_write(buffer);
    return 0;
}

static int command_desktop(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (!desktop_active)
        desktop_init();
    else
        term_write("Desktop is already running.\n");
    return 0;
}

static int command_exit(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (desktop_active) {
        term_session = 0;
        desktop_init();
    } else
        term_write("Root instance, can't exit.\n");
    return 0;
}

typedef int (*command_t)(int, char**);
typedef struct {
    const char *name;
    command_t func;
} commands_t;

static commands_t commands[] = {
    { "scale", command_scale },
    { "scaleup", command_scaleup },
    { "scaledown", command_scaledown },
    { "clear", command_clear },
    { "shutdown", command_shutdown },
    { "fetch", command_fetch },
    { "echo", command_echo },
    { "list", command_list },
    { "newfile", command_newfile },
    { "delfile", command_delfile },
    { "edit", command_edit },
    { "newfolder", command_newfolder },
    { "delfolder", command_delfolder },
    { "goto", command_goto },
    { "goup", command_goup },
    { "whereami" ,command_whereami },
    { "copyfile" ,command_copyfile },
    { "movefile" ,command_movefile },
    { "copyfolder" ,command_copyfolder },
    { "movefolder" ,command_movefolder },
    { "formatdisk" ,command_formatdisk },
    { "nodeinfo", command_nodeinfo },
    { "printfile", command_printfile },
    { "runscript", command_runscript },
    { "time", command_time },
    { "date", command_date },
    { "datetime", command_datetime },
    { "diskinfo", command_diskinfo },
    { "reloadconfig", command_reloadconfig },
    { "viewimage", command_viewimage },
    { "playaudio", command_playaudio },
    { "listpci", command_listpci },
    { "meminfo", command_meminfo },
    { "desktop", command_desktop },
    { "exit", command_exit },
};

int command_handle(char *command, int printprompt) {
    int exit = 0;
    int ran_command = 0;

    uint32_t old_location = file_current;

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

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (!strcmp(cmd->value, commands[i].name)) {
            exit = commands[i].func(argc, argv);
            ran_command = 1;
        }
    }

    if (!ran_command) {
        int found_script = 0;

        if (!config_has("/system/config/system.cfg", "disable_user_scripts")) {
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
        if (!term_input_buffer && printprompt) {
            if (old_location != file_current)
                term_update_path();

            term_draw_prompt();
        }

        term_prompt = term_x;
    }

    string_free(cmd);
    for (int i = 0; i < argc; i++)
        string_free(args[i]);

    return exit;
}
