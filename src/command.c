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
#include "serial.h"

static void command_scaleup(int argc, char *argv[]) {
    unused(argc); unused(argv);

    term_scale++;
}

static void command_scaledown(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (term_scale > 1)
        term_scale--;
}

static void command_clear(int argc, char *argv[]) {
    unused(argc); unused(argv);

    screen_clear(COLOR_BLACK);
    term_x = 0;
    term_y = 0;
}

static void command_exit(int argc, char *argv[]) {
    unused(argc); unused(argv);

    term_write("Halting...\n", COLOR_WHITE, COLOR_BLACK);
    abort();
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
    strfmt(buff, "Memory: %d MB\n", ((heap_end - heap_start) >> 20) + 2);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
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
    term_write("\n", COLOR_BLACK, COLOR_BLACK);
}

static void command_echo(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        term_write(argv[i], COLOR_WHITE, COLOR_BLACK);
        term_write(" ", COLOR_BLACK, COLOR_BLACK);
    }
    term_write("\n", COLOR_BLACK, COLOR_BLACK);
}

static void command_list(int argc, char *argv[]) {
    static char buff[FILE_MAX_NAME + 16];
    file_node_t *parent = NULL;
    if (argc > 0)
        parent = file_get_node(argv[0]);
    else
        parent = file_parent;

    if (parent->child_head && parent->type == FILE_FOLDER) {
        term_write("\nList of files:\n", COLOR_WHITE, COLOR_BLACK);
        file_node_t *current = parent->child_head;
        while (current) {
            if (current->type == FILE_DATA)
                strfmt(buff, "- %s\n", current->name);
            else if (current->type == FILE_FOLDER)
                strfmt(buff, "-> %s\n", current->name);
            term_write(buff, COLOR_WHITE, COLOR_BLACK);
            current = current->child_next;
        }
    } else if (!parent || parent->type != FILE_FOLDER)
        term_write("Not a folder.\n", COLOR_WHITE, COLOR_BLACK);
    else
        term_write("Empty folder.\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_newfile(int argc, char *argv[]) {
    if (argc > 0) {
        file_node_t *parent = NULL;
        char *path_parent = heap_alloc(FILE_MAX_NAME * 2);
        char *path_basename = heap_alloc(FILE_MAX_NAME);

        if (file_split_path(argv[0], path_parent, path_basename)) {
            if (path_parent[0] == '\0')
                parent = file_parent;
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
    if (argc > 0) {
        file_node_t *target = file_get_node(argv[0]);

        if (!file_delete(target->parent, target->name))
            term_write("Failed deleting file!\n", COLOR_WHITE, COLOR_BLACK);
    } else
        term_write("Usage: delfile <path>\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_editfile(int argc, char *argv[]) {
    if (argc > 0) {
        file_node_t *parent = NULL;
        char *path_parent = heap_alloc(FILE_MAX_NAME * 2);
        char *path_basename = heap_alloc(FILE_MAX_NAME);

        if (file_split_path(argv[0], path_parent, path_basename)) {
            if (path_parent[0] == '\0')
                parent = file_parent;
            else
                parent = file_get_node(path_parent);

            if (parent) {
                if (file_exists(parent, path_basename)) {
                    term_mode = TERM_MODE_EDIT;
                    command_clear(argc, argv);
                } else
                    term_write("File does not exist!\n", COLOR_WHITE, COLOR_BLACK);
            } else
                term_write("Invalid path!\n", COLOR_WHITE, COLOR_BLACK);
        } else
            term_write("Invalid path!\n", COLOR_WHITE, COLOR_BLACK);

        heap_free(path_parent);
        heap_free(path_basename);
    } else
        term_write("Usage: editfile <path>\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_newfolder(int argc, char *argv[]) {
    if (argc > 0) {
        file_node_t *parent = NULL;
        char *path_parent = heap_alloc(FILE_MAX_NAME * 2);
        char *path_basename = heap_alloc(FILE_MAX_NAME);

        if (file_split_path(argv[0], path_parent, path_basename)) {
            if (path_parent[0] == '\0')
                parent = file_parent;
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
    if (argc > 0) {
        file_node_t *target = file_get_node(argv[0]);

        if (target != file_root) {
            if (!folder_delete(target->parent, target->name))
                term_write("Failed deleting folder!\n", COLOR_WHITE, COLOR_BLACK);
        } else
            term_write("Cannot delete root folder!\n", COLOR_WHITE, COLOR_BLACK);
    } else
        term_write("Usage: delfolder <path>\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_goto(int argc, char *argv[]) {
    if (argc > 0) {
        file_node_t *target = file_get_node(argv[0]);

        if (target && target->type == FILE_FOLDER)
            file_parent = target;
        else
            term_write("Not a folder.\n", COLOR_WHITE, COLOR_BLACK);
    } else
        term_write("Usage: goto <path>\n", COLOR_WHITE, COLOR_BLACK);
}

static void command_goup(int argc, char *argv[]) {
    unused(argc); unused(argv);

    if (file_parent->parent)
        file_parent = file_parent->parent;
    else
        term_write("Already at topmost folder!\n", COLOR_WHITE, COLOR_BLACK);
}

void command_handle(const char *command) {
    char cmd[128] = {0};
    static char args[32][512] = {0};
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
    else if (!strcmp(cmd, "exit")) command_exit(argc, argv);
    else if (!strcmp(cmd, "fetch")) command_fetch(argc, argv);
    else if (!strcmp(cmd, "echo")) command_echo(argc, argv);
    else if (!strcmp(cmd, "list")) command_list(argc, argv);
    else if (!strcmp(cmd, "newfile")) command_newfile(argc, argv);
    else if (!strcmp(cmd, "delfile")) command_delfile(argc, argv);
    else if (!strcmp(cmd, "editfile")) command_editfile(argc, argv);
    else if (!strcmp(cmd, "newfolder")) command_newfolder(argc, argv);
    else if (!strcmp(cmd, "delfolder")) command_delfolder(argc, argv);
    else if (!strcmp(cmd, "goto")) command_goto(argc, argv);
    else if (!strcmp(cmd, "goup")) command_goup(argc, argv);
    else term_write("Unknown command\n", COLOR_WHITE, COLOR_BLACK);
}
