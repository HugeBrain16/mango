#include "script.h"
#include "heap.h"
#include "fio.h"
#include "string.h"
#include "terminal.h"
#include "color.h"
#include "command.h"
#include "file.h"
#include "config.h"
#include "pit.h"
#include "rand.h"
#include "kernel.h"
#include "screen.h"
#include "cpu.h"
#include "unit.h"
#include "keyboard.h"

int script_exit = 0;

static int script_should_exit = 0;
static int script_argc = 0;
static char **script_argv = NULL;
static int script_printfg = COLOR_WHITE;
static int script_printbg = COLOR_BLACK;
static uint32_t *script_screen_buffer = NULL;
static size_t script_screen_buffer_size = 0;
static list_t *script_modules = NULL;
static list_t *script_module_paths = NULL;
static string_t *script_printcap = NULL;
static int script_printcap_print = 1;

static void free_token(script_token_t *token);
static void free_node(script_node_t *node);
static void free_stmt(script_stmt_t *stmt);
static void free_var(script_var_t *var);
static void free_env(script_env_t *env);
static void free_eval(script_eval_t *eval);

static void block_add_statement(script_stmt_t *block, script_stmt_t *stmt);
static int get_tokens(const char *path, script_token_t **tokens);
static script_runtime_t *get_runtime();
static int load_runtime(script_stmt_t *block, script_token_t *tokens);
static void free_runtime(script_runtime_t *runtime);

static script_node_t *ref_node(script_node_t *node);
static void unref_node(script_node_t *node);

static script_node_t *node_new();
static script_node_t *node_null();
static script_node_t *node_true();
static script_node_t *node_false();
static script_node_t *node_type_name(script_node_t *node);

static script_stmt_t *parse_statement(script_token_t **token);
static script_stmt_t *parse_statement_inner(script_token_t **token);
static script_node_t *parse_expr(script_token_t **token);
static script_node_t *parse_assignop(script_token_t **token);
static script_node_t *parse_logic(script_token_t **token);
static script_node_t *parse_comparison(script_token_t **token);
static script_node_t *parse_addsub(script_token_t **token);
static script_node_t *parse_term(script_token_t **token);
static script_node_t *parse_call(script_token_t **token);
static script_node_t *parse_factor(script_token_t **token);

#define DEF_CALL(n) \
    static script_node_t *call_##n(script_stmt_t *block, script_node_t *node)

DEF_CALL(print);
DEF_CALL(println);
DEF_CALL(exec);
DEF_CALL(as_str);
DEF_CALL(as_int);
DEF_CALL(as_float);
DEF_CALL(type_name);
DEF_CALL(file_open);
DEF_CALL(file_close);
DEF_CALL(file_getc);
DEF_CALL(file_peek);
DEF_CALL(file_read);
DEF_CALL(file_write);
DEF_CALL(file_isfile);
DEF_CALL(file_isfolder);
DEF_CALL(file_list);
DEF_CALL(char_at);
DEF_CALL(sizeof);
DEF_CALL(input);
DEF_CALL(config_has);
DEF_CALL(config_get);
DEF_CALL(list_init);
DEF_CALL(list_clear);
DEF_CALL(list_push);
DEF_CALL(list_get);
DEF_CALL(list_pop);
DEF_CALL(list_remove);
DEF_CALL(list_str);
DEF_CALL(list_has);
DEF_CALL(sleep);
DEF_CALL(sys_ticks);
DEF_CALL(sys_tps);
DEF_CALL(sys_log);
DEF_CALL(sys_perf);
DEF_CALL(argc);
DEF_CALL(argv);
DEF_CALL(rand);
DEF_CALL(randrange);
DEF_CALL(color);
DEF_CALL(color_rgb);
DEF_CALL(color_setfg);
DEF_CALL(color_setbg);
DEF_CALL(color_reset);
DEF_CALL(exit);
DEF_CALL(ata_slot);
DEF_CALL(ata_serial);
DEF_CALL(ata_rev);
DEF_CALL(ata_model);
DEF_CALL(cpu_name);
DEF_CALL(cpu_vendor);
DEF_CALL(cpu_family);
DEF_CALL(cpu_model);
DEF_CALL(screen_init);
DEF_CALL(screen_width);
DEF_CALL(screen_height);
DEF_CALL(screen_pitch);
DEF_CALL(screen_scale);
DEF_CALL(screen_clear);
DEF_CALL(screen_draw);
DEF_CALL(screen_flush);
DEF_CALL(printcap_init);
DEF_CALL(printcap_close);
DEF_CALL(printcap_get);
DEF_CALL(printcap_print);
DEF_CALL(internal_getvars);
DEF_CALL(internal_getname);
DEF_CALL(internal_getvalue);
DEF_CALL(internal_getrefcount);

static script_node_t *eval_binop(script_stmt_t *block, script_node_t *binop);
static script_node_t *eval_call(script_stmt_t *block, script_node_t *call);
static script_node_t *eval_index(script_stmt_t *block, script_node_t *var);
static script_node_t *eval_expr(script_stmt_t *block, script_node_t *expr);

static script_eval_t *eval_block(script_stmt_t *block, script_stmt_t *stmt);
static script_eval_t *eval_if(script_stmt_t *block, script_stmt_t *stmt);
static script_eval_t *eval_while(script_stmt_t *block, script_stmt_t *stmt);
static script_eval_t *eval_for(script_stmt_t *block, script_stmt_t *stmt);
static script_eval_t *eval_statement(script_stmt_t *block, script_stmt_t *stmt);

typedef enum call {
    CALL_PRINT,
    CALL_PRINTLN,
    CALL_EXEC,
    CALL_AS_STR,
    CALL_AS_INT,
    CALL_AS_FLOAT,
    CALL_TYPE_NAME,
    CALL_FILE_OPEN,
    CALL_FILE_CLOSE,
    CALL_FILE_GETC,
    CALL_FILE_PEEK,
    CALL_FILE_READ,
    CALL_FILE_WRITE,
    CALL_FILE_ISFILE,
    CALL_FILE_ISFOLDER,
    CALL_FILE_LIST,
    CALL_CHAR_AT,
    CALL_SIZEOF,
    CALL_INPUT,
    CALL_CONFIG_HAS,
    CALL_CONFIG_GET,
    CALL_LIST_INIT,
    CALL_LIST_CLEAR,
    CALL_LIST_PUSH,
    CALL_LIST_GET,
    CALL_LIST_POP,
    CALL_LIST_REMOVE,
    CALL_LIST_STR,
    CALL_LIST_HAS,
    CALL_SLEEP,
    CALL_SYS_TICKS,
    CALL_SYS_TPS,
    CALL_SYS_LOG,
    CALL_SYS_PERF,
    CALL_ARGC,
    CALL_ARGV,
    CALL_RAND,
    CALL_RANDRANGE,
    CALL_COLOR,
    CALL_COLOR_RGB,
    CALL_COLOR_SETFG,
    CALL_COLOR_SETBG,
    CALL_COLOR_RESET,
    CALL_EXIT,
    CALL_ATA_SLOT,
    CALL_ATA_SERIAL,
    CALL_ATA_REV,
    CALL_ATA_MODEL,
    CALL_CPU_NAME,
    CALL_CPU_VENDOR,
    CALL_CPU_FAMILY,
    CALL_CPU_MODEL,
    CALL_SCREEN_INIT,
    CALL_SCREEN_WIDTH,
    CALL_SCREEN_HEIGHT,
    CALL_SCREEN_PITCH,
    CALL_SCREEN_SCALE,
    CALL_SCREEN_CLEAR,
    CALL_SCREEN_DRAW,
    CALL_SCREEN_FLUSH,
    CALL_PRINTCAP_INIT,
    CALL_PRINTCAP_GET,
    CALL_PRINTCAP_CLOSE,
    CALL_PRINTCAP_PRINT,
    CALL_INTERNAL_GETVARS,
    CALL_INTERNAL_GETNAME,
    CALL_INTERNAL_GETVALUE,
    CALL_INTERNAL_GETREFCOUNT,

    CALL_E_COUNT,
} call_e;

static const script_builtin_entry_t builtins[CALL_E_COUNT] = {
    [CALL_PRINT]             = { "print",        call_print },
    [CALL_PRINTLN]           = { "println",      call_println },
    [CALL_EXEC]              = { "exec",         call_exec },
    [CALL_AS_STR]            = { "as_str",       call_as_str },
    [CALL_AS_INT]            = { "as_int",       call_as_int },
    [CALL_AS_FLOAT]          = { "as_float",     call_as_float },
    [CALL_TYPE_NAME]         = { "type_name",    call_type_name },
    [CALL_FILE_OPEN]         = { "file_open",    call_file_open },
    [CALL_FILE_CLOSE]        = { "file_close",   call_file_close },
    [CALL_FILE_GETC]         = { "file_getc",    call_file_getc },
    [CALL_FILE_PEEK]         = { "file_peek",    call_file_peek },
    [CALL_FILE_READ]         = { "file_read",    call_file_read },
    [CALL_FILE_WRITE]        = { "file_write",   call_file_write },
    [CALL_FILE_ISFILE]       = { "file_isfile",  call_file_isfile },
    [CALL_FILE_ISFOLDER]     = { "file_isfolder",call_file_isfolder },
    [CALL_FILE_LIST]         = { "file_list",    call_file_list },
    [CALL_CHAR_AT]           = { "char_at",      call_char_at },
    [CALL_SIZEOF]            = { "sizeof",       call_sizeof },
    [CALL_INPUT]             = { "input",        call_input },
    [CALL_CONFIG_HAS]        = { "config_has",   call_config_has },
    [CALL_CONFIG_GET]        = { "config_get",   call_config_get },
    [CALL_LIST_INIT]         = { "list_init",    call_list_init },
    [CALL_LIST_CLEAR]        = { "list_clear",   call_list_clear },
    [CALL_LIST_PUSH]         = { "list_push",    call_list_push },
    [CALL_LIST_GET]          = { "list_get",     call_list_get },
    [CALL_LIST_POP]          = { "list_pop",     call_list_pop },
    [CALL_LIST_REMOVE]       = { "list_remove",  call_list_remove },
    [CALL_LIST_STR]          = { "list_str",     call_list_str },
    [CALL_LIST_HAS]          = { "list_has",     call_list_has },
    [CALL_SLEEP]             = { "sleep",        call_sleep },
    [CALL_SYS_TICKS]         = { "sys_ticks",    call_sys_ticks },
    [CALL_SYS_TPS]           = { "sys_tps",      call_sys_tps },
    [CALL_SYS_LOG]           = { "sys_log",      call_sys_log },
    [CALL_SYS_PERF]          = { "sys_perf",     call_sys_perf },
    [CALL_ARGC]              = { "argc",         call_argc },
    [CALL_ARGV]              = { "argv",         call_argv },
    [CALL_RAND]              = { "rand",         call_rand },
    [CALL_RANDRANGE]         = { "randrange",    call_randrange },
    [CALL_COLOR]             = { "color",        call_color },
    [CALL_COLOR_RGB]         = { "color_rgb",    call_color_rgb },
    [CALL_COLOR_SETFG]       = { "color_setfg",  call_color_setfg },
    [CALL_COLOR_SETBG]       = { "color_setbg",  call_color_setbg },
    [CALL_COLOR_RESET]       = { "color_reset",  call_color_reset },
    [CALL_EXIT]              = { "exit",         call_exit },
    [CALL_ATA_SLOT]          = { "ata_slot",     call_ata_slot },
    [CALL_ATA_SERIAL]        = { "ata_serial",   call_ata_serial },
    [CALL_ATA_REV]           = { "ata_rev",      call_ata_rev },
    [CALL_ATA_MODEL]         = { "ata_model",    call_ata_model },
    [CALL_CPU_NAME]          = { "cpu_name",     call_cpu_name },
    [CALL_CPU_VENDOR]        = { "cpu_vendor",   call_cpu_vendor },
    [CALL_CPU_FAMILY]        = { "cpu_family",   call_cpu_family },
    [CALL_CPU_MODEL]         = { "cpu_model",    call_cpu_model },
    [CALL_SCREEN_INIT]       = { "screen_init",   call_screen_init },
    [CALL_SCREEN_WIDTH]      = { "screen_width",  call_screen_width },
    [CALL_SCREEN_HEIGHT]     = { "screen_height", call_screen_height },
    [CALL_SCREEN_PITCH]      = { "screen_pitch",  call_screen_pitch },
    [CALL_SCREEN_SCALE]      = { "screen_scale",  call_screen_scale },
    [CALL_SCREEN_CLEAR]      = { "screen_clear",  call_screen_clear },
    [CALL_SCREEN_DRAW]       = { "screen_draw",   call_screen_draw },
    [CALL_SCREEN_FLUSH]      = { "screen_flush",  call_screen_flush },
    [CALL_PRINTCAP_INIT]     = { "printcap_init",  call_printcap_init },
    [CALL_PRINTCAP_GET]      = { "printcap_get",   call_printcap_get },
    [CALL_PRINTCAP_CLOSE]    = { "printcap_close", call_printcap_close },
    [CALL_PRINTCAP_PRINT]    = { "printcap_print", call_printcap_print },
    [CALL_INTERNAL_GETVARS]  = { "internal_getvars",  call_internal_getvars },
    [CALL_INTERNAL_GETNAME]  = { "internal_getname",  call_internal_getname },
    [CALL_INTERNAL_GETVALUE] = { "internal_getvalue", call_internal_getvalue },
    [CALL_INTERNAL_GETREFCOUNT] = { "internal_getrefcount", call_internal_getrefcount },
};

static script_node_t *g_null = NULL;
static script_node_t *g_true = NULL;
static script_node_t *g_false = NULL;

static script_node_t *node_cmp(script_node_t *n1, script_node_t *n2) {
    int cmp = 0;

    if (n1->value_type == n2->value_type) {
        switch (n1->value_type) {
            case SCRIPT_LIST:
                cmp = n1->literal.list == n2->literal.list;
                break;
            case SCRIPT_STR:
                cmp = !strcmp(n1->literal.str_value, n2->literal.str_value);
                break;
            case SCRIPT_INT:
            case SCRIPT_BOOL:
                cmp = n1->literal.int_value == n2->literal.int_value;
                break;
            case SCRIPT_FLOAT:
                cmp = n1->literal.float_value == n2->literal.float_value;
                break;
            case SCRIPT_FILE:
                cmp = n1->literal.file == n2->literal.file;
                break;
            case SCRIPT_FUNC:
                cmp = n1->literal.func == n2->literal.func;
                break;
            case SCRIPT_NULL:
                cmp = 1;
                break;
        }
    }

    if (cmp)
        return g_true;

    return g_false;
}

static char *node_repr(script_node_t *node) {
    char *buffer = NULL;

    switch(node->value_type) {
        case SCRIPT_INT:
            buffer = heap_alloc(12);
            strint(buffer, node->literal.int_value);
            break;
        case SCRIPT_FLOAT:
            buffer = heap_alloc(16);
            strdouble(buffer, node->literal.float_value, 6);
            break;
        case SCRIPT_STR:
            {
                size_t size = node->literal.str_size;
                buffer = heap_alloc(size);

                memcpy(buffer, node->literal.str_value, size);
                break;
            }
        case SCRIPT_NULL:
            buffer = heap_alloc(5);
            strcpy(buffer, "null");
            break;
        case SCRIPT_BOOL:
            buffer = heap_alloc(6);

            if (node->literal.int_value)
                strcpy(buffer, "true");
            else
                strcpy(buffer, "false");
            break;
        case SCRIPT_FILE:
            {
                fio_t *fio_file = node->literal.file;
                buffer = heap_alloc(128);

                if (fio_file) {
                    file_node_t file;
                    file_node(fio_file->file, &file);

                    strfmt(buffer, "[(0x%x) FILE=%d NAME=%s MODE=%d SEEK=%d ]",
                        node,
                        fio_file->file,
                        file.name,
                        fio_file->mode,
                        fio_file->seek);
                }
            }
            break;
        case SCRIPT_LIST:
        case SCRIPT_VARLIST:
            {
                list_t *list = node->literal.list;
                buffer = heap_alloc(128);

                if (list) {
                    script_node_t *type_name = node_type_name(node);
                    strfmt(buffer, "[(0x%x) TYPE=%s LIST=0x%x SIZE=%d ]",
                        node, type_name->literal.str_value, list, list->size);
                    unref_node(type_name);
                }
            }
            break;
        case SCRIPT_VAR:
            {
                buffer = heap_alloc(strlen(node->var.name) + 64);
                script_node_t *type_name = node_type_name(node->var.value);
                strfmt(buffer, "[(0x%x) TYPE=%s NAME=%s ]",
                    node, type_name->literal.str_value, node->var.name);
                unref_node(type_name);
            }
            break;
        case SCRIPT_FUNC:
            {
                script_stmt_t *func = node->literal.func;
                script_node_t *name = func->func.name;
                int params_count = func->func.params_count;

                size_t size = 32 + name->literal.str_size;
                for (int i = 0; i < params_count; i++)
                    size += func->func.params[i]->literal.str_size;

                buffer = heap_alloc(size);

                strfmt(buffer, "[(0x%x) FUNC=%s ", node, name->literal.str_value);
                for (int i = 0; i < params_count; i++) {
                    if (i == 0)
                        strcat(buffer, "PARAMS=(");

                    script_node_t *param = func->func.params[i];
                    strcat(buffer, param->literal.str_value);

                    if (i < params_count - 1)
                        strcat(buffer, ", ");
                }

                if (params_count > 0)
                    strcat(buffer, ")");

                strcat(buffer, " ]");
            }
            break;
    }

    return buffer;
}

static script_node_t *node_clone(script_node_t *node) {
    script_node_t *cloned = node_null();
    cloned->node_type = SCRIPT_AST_LITERAL;
    cloned->value_type = node->value_type;
    cloned->lineno = node->lineno;

    switch (node->value_type) {
        case SCRIPT_INT:
        case SCRIPT_BOOL:
            cloned->literal.int_value = node->literal.int_value;
            break;
        case SCRIPT_FLOAT:
            cloned->literal.float_value = node->literal.float_value;
            break;
        case SCRIPT_STR:
            {
                size_t size = node->literal.str_size;
                char *value = heap_alloc(size);
                memcpy(value, node->literal.str_value, size);

                cloned->literal.str_size = size;
                cloned->literal.str_value = value;
                break;
            }
        case SCRIPT_FUNC:
            cloned->literal.func = node->literal.func;
            break;
        case SCRIPT_FILE:
            cloned->literal.file = node->literal.file;
            break;
        case SCRIPT_LIST:
        case SCRIPT_VARLIST:
            cloned->literal.list = node->literal.list;
            break;
        case SCRIPT_VAR:
            {
                size_t size = strlen(node->var.name) + 1;
                cloned->var.name = heap_alloc(size);
                memcpy(cloned->var.name, node->var.name, size);
                cloned->var.value = node->var.value;
                break;
            }
    }

    return cloned;
}

static void free_eval(script_eval_t *eval) {
    if (!eval) return;

    if (eval->node) unref_node(eval->node);
    heap_free(eval);
}

static script_token_t *create_token(uint8_t type, size_t lineno) {
    script_token_t *token = heap_alloc(sizeof(script_token_t));
    token->next = NULL;
    token->value = (char*) heap_alloc(SCRIPT_SIZE_TOKEN);
    token->value[0] = '\0';
    token->type = type;
    token->size = 1;
    token->lineno = lineno;
    return token;
}

static void free_token(script_token_t *token) {
    if (!token) return;
    heap_free(token->value);
    heap_free(token);
}

static script_token_t *lex_number(fio_t *file, char *c, size_t *lineno) {
    script_token_t *token = create_token(SCRIPT_TOKEN_NUMBER, *lineno);
    size_t i = 0;
    int is_float = 0;
    char prev = '\0';

    if (*c == '-') {
        token->value[i++] = *c;
        *c = fio_getc(file);
    }

    do {
        if (*c == '_') {
            goto skip;
        }

        if (*c == '.' && prev == '_') {
            char msg[64];
            strfmt(msg, "Error: Unexpected number literal (line: %d)\n", *lineno);
            term_write(msg);
            free_token(token);
            return NULL;
        }

        if (i == token->size * SCRIPT_SIZE_TOKEN - 1) {
            token->size *= 2;
            token->value = heap_realloc(token->value, token->size * SCRIPT_SIZE_TOKEN);
        }

        token->value[i++] = *c;

        if (*c == '.')
            is_float++;

        skip:
            prev = *c;
            *c = fio_getc(file);
    } while (isdigit(*c) || *c == '_' || (*c == '.' && is_float < 2));

    if (prev == '_') {
        char msg[64];
        strfmt(msg, "Error: Unexpected number literal (line: %d)\n", *lineno);
        term_write(msg);
        free_token(token);
        return NULL;
    }

    token->value[i] = '\0';

    if (isalpha(*c) || is_float == 2) {
        char msg[64];
        strfmt(msg, "Error: Unexpected char \"%c\" (line: %d)\n", *c, *lineno);
        term_write(msg);
        free_token(token);
        return NULL;
    }

    if (is_float)
        token->type = SCRIPT_TOKEN_FLOAT;

    return token;
}

static script_token_t *lex_identifier(fio_t *file, char *c, size_t *lineno) {
    script_token_t *token = create_token(SCRIPT_TOKEN_IDENTIFIER, *lineno);
    size_t i = 0;

    do {
        if (i == token->size * SCRIPT_SIZE_TOKEN - 1) {
            token->size++;
            token->value = heap_realloc(token->value, token->size * SCRIPT_SIZE_TOKEN);
        }

        token->value[i++] = *c;
        *c = fio_getc(file);
    } while (isalpha(*c) || isdigit(*c) || *c == '_');

    token->value[i] = '\0';

    if (!strcmp(token->value, "let")) token->type = SCRIPT_TOKEN_LET;
    else if (!strcmp(token->value, "func")) token->type = SCRIPT_TOKEN_FUNC;
    else if (!strcmp(token->value, "null")) token->type = SCRIPT_TOKEN_NULL;
    else if (!strcmp(token->value, "return")) token->type = SCRIPT_TOKEN_RETURN;
    else if (!strcmp(token->value, "if")) token->type = SCRIPT_TOKEN_IF;
    else if (!strcmp(token->value, "else")) token->type = SCRIPT_TOKEN_ELSE;
    else if (!strcmp(token->value, "true")) token->type = SCRIPT_TOKEN_TRUE;
    else if (!strcmp(token->value, "false")) token->type = SCRIPT_TOKEN_FALSE;
    else if (!strcmp(token->value, "while")) token->type = SCRIPT_TOKEN_WHILE;
    else if (!strcmp(token->value, "for")) token->type = SCRIPT_TOKEN_FOR;
    else if (!strcmp(token->value, "break")) token->type = SCRIPT_TOKEN_BREAK;
    else if (!strcmp(token->value, "continue")) token->type = SCRIPT_TOKEN_CONTINUE;
    else if (!strcmp(token->value, "include")) token->type = SCRIPT_TOKEN_INCLUDE;
    else if (!strcmp(token->value, "delete")) token->type = SCRIPT_TOKEN_DELETE;

    return token;
}

static script_token_t *lex_string(fio_t *file, char *c, size_t *lineno) {
    script_token_t *token = create_token(SCRIPT_TOKEN_STRING, *lineno);
    size_t i = 0;

    *c = fio_getc(file);

    int is_escaped = 0;
    while ((*c != '"' || is_escaped) && *c != '\0' && *c != FIO_EOF) {
        if (i == token->size * SCRIPT_SIZE_TOKEN - 1) {
            token->size++;
            token->value = heap_realloc(token->value, token->size * SCRIPT_SIZE_TOKEN);
        }

        token->value[i++] = *c;

        if (is_escaped)
            is_escaped = 0;
        else if (*c == '\\')
            is_escaped = 1;

        *c = fio_getc(file);
    }
    token->value[i] = '\0';

    if (*c != '"') {
        char msg[32];
        strfmt(msg, "Error: Unclosed string (line: %d)\n", *lineno);
        term_write(msg);
        free_token(token);
        return NULL;
    }

    return token;
}

static script_token_t *lex_operator(char c, size_t *lineno) {
    script_token_t *token = create_token(SCRIPT_TOKEN_END, *lineno);
    token->value[0] = c;
    token->value[1] = '\0';

    switch (c) {
        case '(': token->type = SCRIPT_TOKEN_LPAREN; break;
        case ')': token->type = SCRIPT_TOKEN_RPAREN; break;
        case '=': token->type = SCRIPT_TOKEN_EQUAL; break;
        case '+': token->type = SCRIPT_TOKEN_PLUS; break;
        case '-': token->type = SCRIPT_TOKEN_MINUS; break;
        case '/': token->type = SCRIPT_TOKEN_DIVIDE; break;
        case '*': token->type = SCRIPT_TOKEN_TIMES; break;
        case '%': token->type = SCRIPT_TOKEN_MODULO; break;
        case ';': token->type = SCRIPT_TOKEN_END; break;
        case ',': token->type = SCRIPT_TOKEN_COMMA; break;
        case '.': token->type = SCRIPT_TOKEN_DOT; break;
        case '{': token->type = SCRIPT_TOKEN_LBRAC; break;
        case '}': token->type = SCRIPT_TOKEN_RBRAC; break;
        case '<': token->type = SCRIPT_TOKEN_LESSTHAN; break;
        case '>': token->type = SCRIPT_TOKEN_MORETHAN; break;
        case '[': token->type = SCRIPT_TOKEN_LSBRAC; break;
        case ']': token->type = SCRIPT_TOKEN_RSBRAC; break;
        default: {
                     char msg[32];
                     strfmt(msg, "Error: Illegal token (line: %d): \"%c\"\n", *lineno, c);
                     term_write(msg);
                     free_token(token);
                     return NULL;
                 }
    }

    return token;
}

static script_token_t *lex_equaloperator(fio_t *file, char *c, size_t *lineno) {
    script_token_t *token = create_token(SCRIPT_TOKEN_ISEQUAL, *lineno);
    token->value[0] = *c;
    token->value[1] = '=';
    token->value[2] = '\0';

    switch (*c) {
        case '=': token->type = SCRIPT_TOKEN_ISEQUAL; break;
        case '!': token->type = SCRIPT_TOKEN_ISNTEQUAL; break;
        case '>': token->type = SCRIPT_TOKEN_MOREEQUAL; break;
        case '<': token->type = SCRIPT_TOKEN_LESSEQUAL; break;
        default: {
            char msg[64];
            strfmt(msg, "Error: Unexpected '%c' for equal operator (line: %d)\n", *c, *lineno);
            term_write(msg);
            free_token(token);
            return NULL;
        }
    }

    *c = fio_getc(file);
    *c = fio_getc(file);

    return token;
}

static script_token_t *lex_logicoperator(fio_t *file, char *c, size_t *lineno) {
    script_token_t *token = create_token(SCRIPT_TOKEN_AND, *lineno);
    token->value[0] = *c;
    token->value[1] = *c;
    token->value[2] = '\0';

    switch (*c) {
        case '&': token->type = SCRIPT_TOKEN_AND; break;
        case '|': token->type = SCRIPT_TOKEN_OR; break;
        default: {
            char msg[64];
            strfmt(msg, "Error: Unexpected '%c' for logic operator (line: %d)\n", *c, *lineno);
            term_write(msg);
            free_token(token);
            return NULL;
        }
    }

    *c = fio_getc(file);
    *c = fio_getc(file);

    return token;
}

static script_token_t *lex_assignoperator(fio_t *file, char *c, size_t *lineno) {
    script_token_t *token = create_token(SCRIPT_TOKEN_ADDASSIGN, *lineno);
    token->value[0] = *c;
    token->value[1] = '=';
    token->value[2] = '\0';

    switch (*c) {
        case '+': token->type = SCRIPT_TOKEN_ADDASSIGN; break;
        case '-': token->type = SCRIPT_TOKEN_SUBASSIGN; break;
        case '*': token->type = SCRIPT_TOKEN_MULASSIGN; break;
        case '/': token->type = SCRIPT_TOKEN_DIVASSIGN; break;
        default: {
            char msg[64];
            strfmt(msg, "Error: Unexpected '%c' for assign operator (line: %d)\n", *c, *lineno);
            term_write(msg);
            free_token(token);
            return NULL;
        }
    }

    *c = fio_getc(file);
    *c = fio_getc(file);

    return token;
}

static script_token_t *lex_hex(fio_t *file, char *c, size_t *lineno) {
    fio_getc(file);

    *c = fio_getc(file);
    if (!isbase16(*c)) {
        char msg[64];
        strfmt(msg, "Error: Unexpected hex value '%c' (line: %d)\n", *c, *lineno);
        term_write(msg);
        return NULL;
    }

    size_t idx = 0;
    size_t size = 0;
    char *value = heap_alloc(SCRIPT_SIZE_TOKEN);
    while (isbase16(*c)) {
        if (idx == size * SCRIPT_SIZE_TOKEN - 1) {
            size++;
            value = heap_realloc(value, size * SCRIPT_SIZE_TOKEN);
        }

        value[idx++] = *c;
        *c = fio_getc(file);
    }

    if (idx >= size)
        value = heap_realloc(value, size + 1);
    value[idx++] = '\0';

    char conv[size + 1];
    strint(conv, hexstr(value));

    memcpy(value, conv, strlen(conv) + 1);

    size = strlen(value) + 1;
    value = heap_realloc(value, size);

    script_token_t *token = create_token(SCRIPT_TOKEN_NUMBER, *lineno);
    heap_free(token->value);
    token->value = value;
    token->size = size;

    return token;
}

static script_token_t *tokenize(fio_t *file) {
    char c = fio_getc(file);
    size_t lineno = 1;
    script_token_t *head = NULL;
    script_token_t *current = NULL;

    int in_comment = 0;

    while (c != '\0' && c != FIO_EOF) {
        if (c == '#' && !in_comment) {
            if (fio_peek(file) == '-') {
                in_comment = 2;

                fio_getc(file);
                c = fio_getc(file);
                continue;
            }

            in_comment = 1;
        }

        if (c == '-' && in_comment == 2 && fio_peek(file) == '#') {
            in_comment = 0;

            fio_getc(file);
            c = fio_getc(file);
            continue;
        }

        if (c == '\n') {
            if (in_comment == 1)
                in_comment = 0;

            lineno++;
            c = fio_getc(file);
            continue;
        }

        if (c == ' ' || c == '\t' || in_comment) {
            c = fio_getc(file);
            continue;
        }

        script_token_t *token = NULL;

        char next = fio_peek(file);
        if (c == '0' && next == 'x') {
            token = lex_hex(file, &c, &lineno);
        } else if (isdigit(c) || (c == '-' && isdigit(next))) {
            token = lex_number(file, &c, &lineno);
        } else if (isalpha(c) || c == '_') {
            token = lex_identifier(file, &c, &lineno);
        } else if (c == '"') {
            token = lex_string(file, &c, &lineno);
            if (token) c = fio_getc(file);
        } else if (c == '=' && next == '=') {
            token = lex_equaloperator(file, &c, &lineno);
        } else if (c == '!' && next == '=') {
            token = lex_equaloperator(file, &c, &lineno);
        } else if (c == '>' && next == '=') {
            token = lex_equaloperator(file, &c, &lineno);
        } else if (c == '<' && next == '=') {
            token = lex_equaloperator(file, &c, &lineno);
        } else if (c == '&' && next == '&') {
            token = lex_logicoperator(file, &c, &lineno);
        } else if (c == '|' && next == '|') {
            token = lex_logicoperator(file, &c, &lineno);
        } else if (c == '+' && next == '=') {
            token = lex_assignoperator(file, &c, &lineno);
        } else if (c == '-' && next == '=') {
            token = lex_assignoperator(file, &c, &lineno);
        } else if (c == '*' && next == '=') {
            token = lex_assignoperator(file, &c, &lineno);
        } else if (c == '/' && next == '=') {
            token = lex_assignoperator(file, &c, &lineno);
        } else if (c == '!') {
            token = create_token(SCRIPT_TOKEN_NEG, lineno);
            token->value[0] = '!';
            token->value[1] = '\0';
            token->size = 2;
            if (token) c = fio_getc(file);
        } else {
            token = lex_operator(c, &lineno);
            if (token) c = fio_getc(file);
        }

        if (!token)
            goto fail;

        if (!head) {
            head = token;
            current = token;
        } else {
            current->next = token;
            current = token;
        }
    }

    return head;

fail:
    while (head) {
        script_token_t *next = head->next;
        free_token(head);
        head = next;
    }
    return NULL;
}

static void free_var(script_var_t *var) {
    if (!var) return;

    heap_free(var->name);
    unref_node(var->value);
    heap_free(var);
}

static void free_env(script_env_t *env) {
    if (!env) return;    

    script_var_t *var = env->var_head;
    while (var) {
        script_var_t *next = var->next;
        free_var(var);
        var = next;
    }

    heap_free(env);
}

static void env_reset(script_env_t *env) {
    script_var_t *var = env->var_head;
    while (var) {
        script_var_t *next = var->next;
        free_var(var);
        var = next;
    }
    env->var_head = NULL;
    env->var_tail = NULL;
}

static script_var_t *env_find_var(script_stmt_t *block, const char *name) {
    script_env_t *env = block->block.env;

    script_var_t *var = env->var_head;
    while (var) {
        if (!strcmp(var->name, name)) {
            if (var != env->var_head) {
                var->prev->next = var->next;

                if (var->next)
                    var->next->prev = var->prev;
                else
                    env->var_tail = var->prev;

                var->next = env->var_head;
                var->prev = NULL;

                if (env->var_head)
                    env->var_head->prev = var;

                env->var_head = var;
            }
            return var;
        }
        var = var->next;
    }

    return NULL;
}

static script_var_t *env_unscoped_find_var(script_stmt_t *block, const char *name) {
    script_stmt_t *parent = block;
    while (parent) {
        script_var_t *var = env_find_var(parent, name);
        if (var)
            return var;

        parent = parent->parent;
    }

    return NULL;
}

static script_stmt_t *env_find_block(script_stmt_t *block, const char *name) {
    script_stmt_t *parent = block;

    while (parent) {
        script_var_t *var = env_find_var(parent, name);
        if (var)
            return parent;
        parent = parent->parent;
    }

    return NULL;
}

static void env_append_var(script_stmt_t *block, script_var_t *var) {
    script_env_t *env = block->block.env;

    if (!env->var_tail) {
        env->var_head = var;
        env->var_tail = var;
    } else {
        var->prev = env->var_tail;
        env->var_tail->next = var;
        env->var_tail = var;
    }
}

static void env_remove_var(script_stmt_t *block, script_var_t *var) {
    script_env_t *env = block->block.env;

    if (var) {
        if (var->prev)
            var->prev->next = var->next;
        else
            env->var_head = var->next;

        if (var->next)
            var->next->prev = var->prev;
        else
            env->var_tail = var->prev;
        free_var(var);
    }
}

static script_var_t *env_new_var(const char *name) {
    script_var_t *var = heap_alloc(sizeof(script_var_t));

    size_t length = strlen(name) + 1;
    var->name = heap_alloc(length);
    memcpy(var->name, name, length);
    var->next = NULL;
    var->prev = NULL;

    return var;
}

static void env_set_var(script_stmt_t *block, const char *name, script_node_t *value) {
    if (value->node_type != SCRIPT_AST_LITERAL) return;

    script_var_t *var = env_find_var(block, name);
    if (!var) {
        var = env_new_var(name);
        env_append_var(block, var);
    } else
        unref_node(var->value);

    var->value = value;
}

static void env_set_var_from_stmt(script_stmt_t *block, const char *name, script_stmt_t *value) {
    script_var_t *var = env_find_var(block, name);
    if (!var) {
        var = env_new_var(name);
        env_append_var(block, var);
    } else
        unref_node(var->value);

    script_node_t *node = node_null();
    node->node_type = SCRIPT_AST_LITERAL;
    node->lineno = value->lineno;

    switch (value->type) {
        case SCRIPT_STMT_FUNC:
            node->value_type = SCRIPT_FUNC;
            node->literal.func = value;
            break;
    }

    var->value = node;
}

static script_node_t *node_new() {
    script_node_t *node = heap_alloc(sizeof(script_node_t));
    node->lineno = 0;
    node->ref = 1;

    return node;
}

static script_node_t *node_null() {
    script_node_t *node = node_new();
    node->node_type = SCRIPT_AST_LITERAL;
    node->value_type = SCRIPT_NULL;

    return node;
}

static script_node_t *node_true() {
    script_node_t *node = node_null();
    node->value_type = SCRIPT_BOOL;
    node->literal.int_value = 1;

    return node;
}

static script_node_t *node_false() {
    script_node_t *node = node_true();
    node->literal.int_value = 0;

    return node;
}

static script_node_t *node_string(char *src) {
    script_node_t *node = node_null();
    node->node_type = SCRIPT_AST_LITERAL;
    node->value_type = SCRIPT_STR;
    node->lineno = 0;

    size_t length = strlen(src) + 1;
    node->literal.str_size = length;
    node->literal.str_value = heap_alloc(length);
    memcpy(node->literal.str_value, src, node->literal.str_size);
    return node;
}

static script_node_t *node_int(int n) {
    script_node_t *node = node_null();
    node->node_type = SCRIPT_AST_LITERAL;
    node->value_type = SCRIPT_INT;
    node->lineno = 0;
    node->literal.int_value = n;
    return node;
}

static script_node_t *node_float(double n) {
    script_node_t *node = node_null();
    node->node_type = SCRIPT_AST_LITERAL;
    node->value_type = SCRIPT_FLOAT;
    node->lineno = 0;
    node->literal.float_value = n;
    return node;
}

static script_node_t *node_var(char *name, script_node_t *value) {
    script_node_t *node = node_null();
    node->node_type = SCRIPT_AST_LITERAL;
    node->value_type = SCRIPT_VAR;
    node->lineno = 0;

    size_t size = strlen(name) + 1;
    node->var.name = heap_alloc(size);
    memcpy(node->var.name, name, size);

    node->var.value = ref_node(value);
    return node;
}

static int node_isprimitive(script_node_t *node) {
    if (!node) return 0;

    switch (node->value_type) {
        case SCRIPT_BOOL:
        case SCRIPT_STR:
        case SCRIPT_INT:
        case SCRIPT_FLOAT:
        case SCRIPT_NULL:
            return 1;
    }

    return 0;
}

static int node_istrue(script_node_t *node) {
    if (!node) return 0;

    switch (node->value_type) {
        case SCRIPT_BOOL:
            if (!node->literal.int_value)
                return 0;
            break;
        case SCRIPT_NULL:
            return 0;
        case SCRIPT_STR:
            if (strlen(node->literal.str_value) == 0)
                return 0;
            break;
        case SCRIPT_INT:
            if (node->literal.int_value <= 0)
                return 0;
            break;
        case SCRIPT_FLOAT:
            if (node->literal.float_value <= 0)
                return 0;
            break;
    }

    return 1;
}

static script_node_t *node_type_name(script_node_t *node) {
    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_STR;
    value->lineno = node->lineno;

    char *str_value = NULL;
    switch (node->value_type) {
        case SCRIPT_STR:
            str_value = "str";
            break;
        case SCRIPT_INT:
            str_value = "int";
            break;
        case SCRIPT_FLOAT:
            str_value = "float";
            break;
        case SCRIPT_BOOL:
            str_value = "bool";
            break;
        case SCRIPT_NULL:
            str_value = "null";
            break;
        case SCRIPT_FUNC:
            str_value = "function";
            break;
        case SCRIPT_FILE:
            str_value = "file";
            break;
        case SCRIPT_LIST:
            str_value = "list";
            break;
        case SCRIPT_VAR:
            str_value = "var";
            break;
        case SCRIPT_VARLIST:
            str_value = "varlist";
            break;
    }

    size_t str_length = strlen(str_value) + 1;
    value->literal.str_value = heap_alloc(str_length);
    memcpy(value->literal.str_value, str_value, str_length);
    value->literal.str_size = str_length;
    return value;
}

static script_node_t *node_literal(script_token_t *token) {
    script_node_t *node = NULL;

    if (token->type == SCRIPT_TOKEN_NULL)
        node = node_null();
    else if (token->type == SCRIPT_TOKEN_TRUE)
        node = node_true();
    else if (token->type == SCRIPT_TOKEN_FALSE)
        node = node_false();

    if (node) {
        node->lineno = token->lineno;
        return node;
    }

    node = node_new();
    node->node_type = SCRIPT_AST_LITERAL;
    node->lineno = token->lineno;

    if (token->type == SCRIPT_TOKEN_NUMBER) {
        node->value_type = SCRIPT_INT;
        node->literal.int_value = intstr(token->value);
    } else if (token->type == SCRIPT_TOKEN_FLOAT) {
        node->value_type = SCRIPT_FLOAT;
        node->literal.float_value = doublestr(token->value);
    } else if (token->type == SCRIPT_TOKEN_STRING || token->type == SCRIPT_TOKEN_IDENTIFIER) {
        if (token->type == SCRIPT_TOKEN_IDENTIFIER)
            node->value_type = SCRIPT_ID;
        else
            node->value_type = SCRIPT_STR;

        char **value = &node->literal.str_value;
        size_t size = strlen(token->value) + 1;

        *value = heap_alloc(size);
        unescape(*value, token->value, size);
        node->literal.str_size = strlen(*value) + 1;
    }

    return node;
}

static script_node_t *node_binop(uint8_t op, script_node_t *left, script_node_t *right) {
    script_node_t *node = node_new();
    node->node_type = SCRIPT_AST_BINOP;
    node->value_type = SCRIPT_NULL;
    node->lineno = left->lineno;

    node->binop.op = op;
    node->binop.left = left;
    node->binop.right = right;

    return node;
}

static script_node_t *node_call(script_node_t *func, script_node_t **argv, size_t argc) {
    script_node_t *node = node_new();
    node->node_type = SCRIPT_AST_CALL;
    node->value_type = SCRIPT_NULL;
    node->lineno = func->lineno;

    node->call.func = func;
    node->call.argv = argv;
    node->call.argc = argc;
    node->call.builtin = -1;
    for (int i = 0; i < CALL_E_COUNT; i++) {
        if (!strcmp(builtins[i].name, func->literal.str_value))
            node->call.builtin = i;
    }

    return node;
}

static script_node_t *node_index(script_node_t *var, script_node_t *index) {
    script_node_t *node = node_new();
    node->node_type = SCRIPT_AST_INDEX;
    node->value_type = SCRIPT_NULL;
    node->lineno = var->lineno;

    node->index.var = var;
    node->index.index = index;

    return node;
}

static script_stmt_t *stmt_var(script_node_t *name, script_node_t *value, uint8_t type) {
    script_stmt_t *stmt = heap_alloc(sizeof(script_stmt_t));

    stmt->type = type;
    stmt->lineno = name->lineno;
    stmt->parent = NULL;
    stmt->child = NULL;
    stmt->next = NULL;
    stmt->var.name = heap_alloc(name->literal.str_size);
    memcpy(stmt->var.name, name->literal.str_value, name->literal.str_size);
    stmt->var.value = value;

    return stmt;
}

static script_stmt_t *stmt_expr(script_node_t *expr) {
    script_stmt_t *stmt = heap_alloc(sizeof(script_stmt_t));
    stmt->type = SCRIPT_STMT_EXPR;
    stmt->lineno = expr->lineno;
    stmt->parent = NULL;
    stmt->child = NULL;
    stmt->next = NULL;
    stmt->expr.node = expr;

    return stmt;
}

static script_stmt_t *stmt_block(script_stmt_t *parent) {
    script_stmt_t *stmt = heap_alloc(sizeof(script_stmt_t));

    stmt->type = SCRIPT_STMT_BLOCK;
    stmt->lineno = parent ? parent->lineno : 0;
    stmt->parent = parent;
    stmt->child = NULL;
    stmt->next = NULL;

    stmt->block.env = heap_alloc(sizeof(script_env_t));
    stmt->block.env->var_head = NULL;
    stmt->block.env->var_tail = NULL;

    return stmt;
}

static script_stmt_t *stmt_func(script_node_t *name, script_stmt_t *block, script_node_t **params, size_t params_count) {
    script_stmt_t *stmt = heap_alloc(sizeof(script_stmt_t));
    stmt->type = SCRIPT_STMT_FUNC;
    stmt->lineno = name->lineno;
    stmt->parent = NULL;
    stmt->child = NULL;
    stmt->next = NULL;
    stmt->func.name = name;
    stmt->func.params = params;
    stmt->func.params_count = params_count;
    stmt->func.block = block;

    return stmt;
}

static script_stmt_t *stmt_return(script_node_t *expr) {
    script_stmt_t *stmt = stmt_expr(expr);
    stmt->type = SCRIPT_STMT_RETURN;

    return stmt;
}

static script_stmt_t *stmt_break(script_node_t *expr) {
    script_stmt_t *stmt = stmt_expr(expr);
    stmt->type = SCRIPT_STMT_BREAK;

    return stmt;
}

static script_stmt_t *stmt_continue(script_node_t *expr) {
    script_stmt_t *stmt = stmt_expr(expr);
    stmt->type = SCRIPT_STMT_CONTINUE;

    return stmt;
}

static script_stmt_t *stmt_if(script_node_t *expr, script_stmt_t *then_stmt, script_stmt_t *else_stmt) {
    script_stmt_t *stmt = heap_alloc(sizeof(script_stmt_t));
    stmt->type = SCRIPT_STMT_IF;
    stmt->lineno = expr->lineno;
    stmt->parent = NULL;
    stmt->child = NULL;
    stmt->next = NULL;
    stmt->if_stmt.expr = expr;
    stmt->if_stmt.then_stmt = then_stmt;
    stmt->if_stmt.else_stmt = else_stmt;

    return stmt;
}

static script_stmt_t *stmt_while(script_node_t *expr, script_stmt_t *body) {
    script_stmt_t *stmt = heap_alloc(sizeof(script_stmt_t));
    stmt->type = SCRIPT_STMT_WHILE;
    stmt->lineno = expr->lineno;
    stmt->parent = NULL;
    stmt->child = NULL;
    stmt->next = NULL;
    stmt->while_stmt.expr = expr;
    stmt->while_stmt.body = body;

    return stmt;
}

static script_stmt_t *stmt_for(script_stmt_t *init, script_node_t *expr, script_stmt_t *update, script_stmt_t *body) {
    script_stmt_t *stmt = heap_alloc(sizeof(script_stmt_t));
    stmt->type = SCRIPT_STMT_FOR;
    stmt->lineno = expr->lineno;
    stmt->parent = NULL;
    stmt->child = NULL;
    stmt->next = NULL;
    stmt->for_stmt.init = init;
    stmt->for_stmt.expr = expr;
    stmt->for_stmt.update = update;
    stmt->for_stmt.body = body;

    return stmt;
}

static script_stmt_t *stmt_include(script_node_t *path) {
    script_stmt_t *stmt = heap_alloc(sizeof(script_stmt_t));
    stmt->type = SCRIPT_STMT_INCLUDE;
    stmt->lineno = path->lineno;
    stmt->parent = NULL;
    stmt->child = NULL;
    stmt->next = NULL;
    stmt->include_stmt.path = path;

    return stmt;
}

static script_stmt_t *stmt_delete(script_node_t *name) {
    script_stmt_t *stmt = heap_alloc(sizeof(script_stmt_t));
    stmt->type = SCRIPT_STMT_DELETE;
    stmt->lineno = name->lineno;
    stmt->parent = NULL;
    stmt->child = NULL;
    stmt->next = NULL;
    stmt->delete_stmt.name = name;

    return stmt;
}

static script_node_t *ref_node(script_node_t *node) {
    if (!node) return NULL;

    if (node == g_null || node == g_true || node == g_false)
        return node;

    node->ref++;
    return node;
}

static void unref_node(script_node_t *node) {
    if (!node) return;

    if (node == g_null || node == g_true || node == g_false)
        return;

    if (node->ref > 0)
        node->ref--;

    if (node->ref == 0)
        free_node(node);
}

static void free_node(script_node_t *node) {
    if (!node) return;
    if (node == g_null || node == g_true || node == g_false) return;

    switch (node->node_type) {
        case SCRIPT_AST_LITERAL:
            switch (node->value_type) {
                case SCRIPT_STR:
                case SCRIPT_ID:
                    heap_free(node->literal.str_value);
                    break;
                case SCRIPT_VAR:
                    heap_free(node->var.name);
                    unref_node(node->var.value);
                    break;
            }
            break;
        case SCRIPT_AST_BINOP:
            free_node(node->binop.left);
            free_node(node->binop.right);
            break;
        case SCRIPT_AST_CALL:
            for (size_t i = 0; i < node->call.argc; i++)
                free_node(node->call.argv[i]);
            heap_free(node->call.argv);
            free_node(node->call.func);
            break;
        case SCRIPT_AST_INDEX:
            free_node(node->index.var);
            free_node(node->index.index);
            break;
    }

    heap_free(node);
}

static void free_stmt(script_stmt_t *stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case SCRIPT_STMT_DEFINE:
        case SCRIPT_STMT_DECLARE:
        case SCRIPT_STMT_ASSIGN:
            heap_free(stmt->var.name);
            unref_node(stmt->var.value);
            break;
        case SCRIPT_STMT_EXPR:
            unref_node(stmt->expr.node);
            break;
        case SCRIPT_STMT_BLOCK:
            free_env(stmt->block.env);
            script_stmt_t *child = stmt->child;
            while (child) {
                script_stmt_t *next = child->next;
                free_stmt(child);
                child = next;
            }
            break;
        case SCRIPT_STMT_FUNC:
            unref_node(stmt->func.name);
            for (size_t i = 0; i < stmt->func.params_count; i++)
                unref_node(stmt->func.params[i]);
            heap_free(stmt->func.params);

            if (stmt->func.block)
                free_stmt(stmt->func.block);
            break;
        case SCRIPT_STMT_RETURN:
        case SCRIPT_STMT_BREAK:
        case SCRIPT_STMT_CONTINUE:
            unref_node(stmt->expr.node);
            break;
        case SCRIPT_STMT_IF:
            unref_node(stmt->if_stmt.expr);
            free_stmt(stmt->if_stmt.then_stmt);
            if (stmt->if_stmt.else_stmt)
                free_stmt(stmt->if_stmt.else_stmt);
            break;
        case SCRIPT_STMT_WHILE:
            unref_node(stmt->while_stmt.expr);
            free_stmt(stmt->while_stmt.body);
            break;
        case SCRIPT_STMT_FOR:
            free_stmt(stmt->for_stmt.init);
            unref_node(stmt->for_stmt.expr);
            free_stmt(stmt->for_stmt.update);
            free_stmt(stmt->for_stmt.body);
            break;
        case SCRIPT_STMT_INCLUDE:
            unref_node(stmt->include_stmt.path);
            break;
        case SCRIPT_STMT_DELETE:
            unref_node(stmt->delete_stmt.name);
            break;
    }

    heap_free(stmt);
}

static script_node_t *parse_factor(script_token_t **token) {
    if (!*token) return NULL;

    if ((*token)->type == SCRIPT_TOKEN_NEG) {
        *token = (*token)->next;

        script_node_t *node = parse_call(token);
        if (!node) return NULL;

        return node_binop(SCRIPT_TOKEN_NEG, node, NULL);
    }

    if ((*token)->type == SCRIPT_TOKEN_NUMBER ||
            (*token)->type == SCRIPT_TOKEN_STRING ||
            (*token)->type == SCRIPT_TOKEN_FLOAT ||
            (*token)->type == SCRIPT_TOKEN_NULL ||
            (*token)->type == SCRIPT_TOKEN_TRUE ||
            (*token)->type == SCRIPT_TOKEN_FALSE ||
            (*token)->type == SCRIPT_TOKEN_IDENTIFIER) {

        script_node_t *node = node_literal(*token);

        *token = (*token)->next;
        return node;
    }

    if ((*token)->type == SCRIPT_TOKEN_LPAREN) {
        *token = (*token)->next;
        script_node_t *node = parse_expr(token);
        if (!node) return NULL;

        if (!*token || (*token)->type != SCRIPT_TOKEN_RPAREN) {
            char msg[64];
            strfmt(msg, "Error: expected ')' (line: %d)\n", *token ? (*token)->lineno : 0);
            term_write(msg);
            unref_node(node);
            return NULL;
        }

        *token = (*token)->next;
        return node;
    }

    char msg[64];
    strfmt(msg, "Error: expected value, got \"%s\" (line: %d)\n", (*token)->value, (*token)->lineno);
    term_write(msg);
    return NULL;
}

static script_node_t *parse_call(script_token_t **token) {
    script_node_t *node = parse_factor(token);
    if (!node || !*token) return node;

    if ((*token)->type == SCRIPT_TOKEN_LPAREN) {
        *token = (*token)->next;

        size_t argc = 0;
        script_node_t **argv = NULL;

        if ((*token)->type != SCRIPT_TOKEN_RPAREN) {
            while (1) {
                script_node_t *arg = parse_expr(token);
                if (!arg) {
                    for (size_t i = 0; i < argc; i++)
                        unref_node(argv[i]);
                    heap_free(argv);
                    unref_node(node);
                    return NULL;
                }

                argv = heap_realloc(argv, (argc + 1) * sizeof(*argv));
                argv[argc++] = arg;

                if ((*token)->type == SCRIPT_TOKEN_COMMA) {
                    *token = (*token)->next;
                    continue;
                }

                break;
            }
        }

        if (!*token || (*token)->type != SCRIPT_TOKEN_RPAREN) {
            char msg[64];
            strfmt(msg, "Error: expected ')' (line: %d)\n", *token ? (*token)->lineno : 0);
            term_write(msg);
            return NULL;
        }
        *token = (*token)->next;

        return node_call(node, argv, argc);
    } else if ((*token)->type == SCRIPT_TOKEN_LSBRAC) {
        *token = (*token)->next;

        script_node_t *index = parse_expr(token);
        if (!node) return node;

        if (!*token || (*token)->type != SCRIPT_TOKEN_RSBRAC) {
            char msg[64];
            strfmt(msg, "Error: expected ']' (line: %d)\n", *token ? (*token)->lineno : 0);
            term_write(msg);
            return NULL;
        }
        *token = (*token)->next;

        return node_index(node, index);
    }

    return node;
}

static script_node_t *parse_term(script_token_t **token) {
    if (!*token) return NULL;

    script_node_t *node = parse_call(token);

    while (*token && ((*token)->type == SCRIPT_TOKEN_TIMES ||
                (*token)->type == SCRIPT_TOKEN_DIVIDE ||
                (*token)->type == SCRIPT_TOKEN_MODULO)) {

        uint8_t op = (*token)->type;
        *token = (*token)->next;

        script_node_t *right = parse_call(token);
        if (!right) {
            unref_node(node);
            return NULL;
        }

        node = node_binop(op, node, right);
    }

    return node;
}

static script_node_t *parse_addsub(script_token_t **token) {
    if (!*token) return NULL;

    script_node_t *node = parse_term(token);

    while (*token && ((*token)->type == SCRIPT_TOKEN_PLUS || (*token)->type == SCRIPT_TOKEN_MINUS)) {
        uint8_t op = (*token)->type;
        *token = (*token)->next;

        script_node_t *right = parse_term(token);
        if (!right) {
            unref_node(node);
            return NULL;
        }

        node = node_binop(op, node, right);
    }

    return node;
}

static script_node_t *parse_comparison(script_token_t **token) {
    if (!*token) return NULL;

    script_node_t *node = parse_addsub(token);

    while (*token && (
        (*token)->type == SCRIPT_TOKEN_ISEQUAL ||
        (*token)->type == SCRIPT_TOKEN_ISNTEQUAL ||
        (*token)->type == SCRIPT_TOKEN_LESSTHAN ||
        (*token)->type == SCRIPT_TOKEN_MORETHAN ||
        (*token)->type == SCRIPT_TOKEN_LESSEQUAL ||
        (*token)->type == SCRIPT_TOKEN_MOREEQUAL)) {

        uint8_t op = (*token)->type;
        *token = (*token)->next;

        script_node_t *right = parse_addsub(token);
        if (!right) {
            unref_node(node);
            return NULL;
        }

        node = node_binop(op, node, right);
    }

    return node;
}

static script_node_t *parse_logic(script_token_t **token) {
    if (!*token) return NULL;

    script_node_t *node = parse_comparison(token);

    while (*token && (
        (*token)->type == SCRIPT_TOKEN_AND ||
        (*token)->type == SCRIPT_TOKEN_OR)) {

        uint8_t op = (*token)->type;
        *token = (*token)->next;

        script_node_t *right = parse_comparison(token);
        if (!right) {
            unref_node(node);
            return NULL;
        }

        node = node_binop(op, node, right);
    }

    return node;
}

static script_node_t *parse_assignop(script_token_t **token) {
    if (!*token) return NULL;

    script_node_t *node = parse_logic(token);

    while (*token && (
        (*token)->type == SCRIPT_TOKEN_ADDASSIGN ||
        (*token)->type == SCRIPT_TOKEN_SUBASSIGN ||
        (*token)->type == SCRIPT_TOKEN_MULASSIGN ||
        (*token)->type == SCRIPT_TOKEN_DIVASSIGN)) {

        uint8_t op = (*token)->type;
        *token = (*token)->next;

        script_node_t *right = parse_logic(token);
        if (!right) {
            unref_node(node);
            return NULL;
        }

        node = node_binop(op, node, right);
    }

    return node;
}

static script_node_t *parse_expr(script_token_t **token) {
    /* this already advances the token */
    return parse_assignop(token);
}

static script_stmt_t *parse_declare(script_token_t **token) {
    if (!*token) return NULL;

    script_node_t *name = parse_factor(token);
    if (!name) return NULL;

    if (name->value_type != SCRIPT_ID) {
        char msg[64];
        strfmt(msg, "Error: expected identifier (line: %d)\n", name->lineno);
        term_write(msg);
        unref_node(name);
        return NULL;
    }

    if (!*token) {
        term_write("Error: unexpected eof\n");
        unref_node(name);
        return NULL;
    }

    if ((*token)->type == SCRIPT_TOKEN_END) {
        return stmt_var(name, NULL, SCRIPT_STMT_DECLARE);
    } else if ((*token)->type == SCRIPT_TOKEN_EQUAL) {
        *token = (*token)->next;
        return stmt_var(name, parse_expr(token), SCRIPT_STMT_DEFINE);
    } else {
        char msg[64];
        strfmt(msg, "Error: Unexpected \"%s\" (line: %d)\n",
            *token ? (*token)->value : "",
            *token ? (*token)->lineno : name->lineno);
        term_write(msg);
        return NULL;
    }
}

static script_stmt_t *parse_assign(script_token_t **token) {
    script_node_t *name = node_literal(*token);

    // skip var name and equal, already checked previously
    *token = (*token)->next;
    *token = (*token)->next;

    return stmt_var(name, parse_expr(token), SCRIPT_STMT_ASSIGN);
}

static script_stmt_t *parse_return(script_token_t **token) {
    if (!*token || (*token)->type == SCRIPT_TOKEN_END)
        return stmt_return(node_null());

    return stmt_return(parse_expr(token));
}

static script_stmt_t *parse_break() {
    return stmt_break(node_null());
}

static script_stmt_t *parse_continue() {
    return stmt_continue(node_null());
}

static script_stmt_t *parse_block(script_token_t **token) {
    if (!*token) return NULL;

    script_stmt_t *block = stmt_block(NULL);

    while (*token && (*token)->type != SCRIPT_TOKEN_RBRAC) {
        script_stmt_t *stmt = parse_statement(token);
        if (!stmt) {
            free_stmt(block);
            return NULL;
        }
        block_add_statement(block, stmt);
    }

    if (!*token || (*token)->type != SCRIPT_TOKEN_RBRAC) {
        char msg[64];
        strfmt(msg, "Error: expected '}' (line: %d)\n", block->lineno);
        term_write(msg);
        free_stmt(block);
        return NULL;
    }
    *token = (*token)->next;

    return block;
}

static script_stmt_t *parse_function(script_token_t **token) {
    if (!*token) return NULL;

    script_node_t *name = parse_factor(token);
    if (!name) return NULL;

    if (name->value_type != SCRIPT_ID) {
        char msg[64];
        strfmt(msg, "Error: expected identifier (line: %d)\n", name->lineno);
        term_write(msg);
        unref_node(name);
        return NULL;
    }

    size_t params_count = 0;
    script_node_t **params = NULL;

    if ((*token)->type == SCRIPT_TOKEN_LPAREN) {
        *token = (*token)->next;

        if ((*token)->type != SCRIPT_TOKEN_RPAREN) {
            while (1) {
                script_node_t *param = parse_factor(token);
                if (param->value_type != SCRIPT_ID) {
                    char msg[64];
                    strfmt(msg, "Error: expected parameter as identifier (line: %d)\n", (*token)->lineno);
                    term_write(msg);

                    for (size_t i = 0; i < params_count; i++)
                        unref_node(params[i]);
                    heap_free(params);
                    unref_node(name);
                    return NULL;
                }

                params = heap_realloc(params, (params_count + 1) * sizeof(*params));
                params[params_count++] = param;

                if ((*token)->type == SCRIPT_TOKEN_COMMA) {
                    *token = (*token)->next;
                    continue;
                }

                break;
            }
        }
    } else {
        char msg[64];
        strfmt(msg, "Error: expected '(' (line: %d)\n", *token ? (*token)->lineno : 0);
        term_write(msg);
        unref_node(name);
        return NULL;
    }

    if (!*token || (*token)->type != SCRIPT_TOKEN_RPAREN) {
        char msg[64];
        strfmt(msg, "Error: expected ')' (line: %d)\n", *token ? (*token)->lineno : 0);
        term_write(msg);
        return NULL;
    }
    *token = (*token)->next;

    if (!*token || (*token)->type != SCRIPT_TOKEN_LBRAC) {
        char msg[64];
        strfmt(msg, "Error: expected '{' (line: %d)\n", *token ? (*token)->lineno : 0);
        term_write(msg);
        return NULL;
    }
    *token = (*token)->next;

    return stmt_func(name, parse_block(token), params, params_count);
}

static script_stmt_t *parse_if(script_token_t **token) {
    if (!*token || (*token)->type != SCRIPT_TOKEN_LPAREN) {
        char msg[64];
        strfmt(msg, "Error: expected '(' (line: %d)\n", (*token)->lineno);
        term_write(msg);
        return NULL;
    }
    *token = (*token)->next;

    script_node_t *expr = parse_expr(token);
    if (!expr)
        return NULL;

    if (!*token || (*token)->type != SCRIPT_TOKEN_RPAREN) {
        char msg[64];
        strfmt(msg, "Error: expected ')' (line: %d)\n", *token ? (*token)->lineno : 0);
        term_write(msg);
        return NULL;
    }
    *token = (*token)->next;

    script_stmt_t *then_stmt = parse_statement(token);
    if (!then_stmt)
        return NULL;

    script_stmt_t *else_stmt = NULL;
    if (*token && (*token)->type == SCRIPT_TOKEN_ELSE) {
        *token = (*token)->next;

        else_stmt = parse_statement(token);
        if (!else_stmt)
            return NULL;
    }

    return stmt_if(expr, then_stmt, else_stmt);
}

static script_stmt_t *parse_while(script_token_t **token) {
    if (!*token || (*token)->type != SCRIPT_TOKEN_LPAREN) {
        char msg[64];
        strfmt(msg, "Error: expected '(' (line: %d)\n", (*token)->lineno);
        term_write(msg);
        return NULL;
    }
    *token = (*token)->next;

    script_node_t *expr = parse_expr(token);
    if (!expr) return NULL;

    if (!*token || (*token)->type != SCRIPT_TOKEN_RPAREN) {
        char msg[64];
        strfmt(msg, "Error: expected ')' (line: %d)\n", *token ? (*token)->lineno : 0);
        term_write(msg);
        return NULL;
    }
    *token = (*token)->next;

    script_stmt_t *body = parse_statement(token);
    if (!body) {
        unref_node(expr);
        return NULL;
    }

    return stmt_while(expr, body);
}

static script_stmt_t *parse_for(script_token_t **token) {
    if (!*token || (*token)->type != SCRIPT_TOKEN_LPAREN) {
        char msg[64];
        strfmt(msg, "Error: expected '(' (line: %d)\n", (*token)->lineno);
        term_write(msg);
        return NULL;
    }
    *token = (*token)->next;

    script_stmt_t *init = parse_statement(token);
    if (!init)
        return NULL;

    script_node_t *expr = parse_expr(token);
    if (!expr) {
        free_stmt(init);
        return NULL;
    }

    if (!*token || (*token)->type != SCRIPT_TOKEN_END) {
        char msg[64];
        strfmt(msg, "Error: expected ';' (line: %d)\n", (*token)->lineno);
        term_write(msg);

        free_stmt(init);
        unref_node(expr);
        return NULL;
    }
    *token = (*token)->next;

    script_stmt_t *update = parse_statement_inner(token);
    if (!update) {
        free_stmt(init);
        unref_node(expr);
        return NULL;
    }

    if (!*token || (*token)->type != SCRIPT_TOKEN_RPAREN) {
        char msg[64];
        strfmt(msg, "Error: expected ')' (line: %d)\n", (*token)->lineno);
        term_write(msg);

        free_stmt(init);
        unref_node(expr);
        free_stmt(update);
        return NULL;
    }
    *token = (*token)->next;

    script_stmt_t *body = parse_statement(token);
    if (!body) {
        free_stmt(init);
        unref_node(expr);
        free_stmt(update);
        return NULL;
    }

    return stmt_for(init, expr, update, body);
}

static script_stmt_t *parse_include(script_token_t **token) {
    if (!*token || (*token)->type != SCRIPT_TOKEN_STRING) {
        char msg[64];
        int lineno = *token ? (*token)->lineno : 0;
        strfmt(msg, "Error: expected string of path (line: %d)\n", lineno);
        term_write(msg);
        return NULL;
    }

    char *path = (*token)->value;
    size_t lineno = (*token)->lineno;
    *token = (*token)->next;

    script_node_t *node = node_string(path);
    node->lineno = lineno;
    return stmt_include(node);
}

static script_stmt_t *parse_delete(script_token_t **token) {
    if (!*token || (*token)->type != SCRIPT_TOKEN_IDENTIFIER) {
        char msg[64];
        int lineno = *token ? (*token)->lineno : 0;
        strfmt(msg, "Error: expected identifier (line: %d)\n", lineno);
        term_write(msg);
        return NULL;
    }

    char *name = (*token)->value;
    size_t lineno = (*token)->lineno;
    *token = (*token)->next;

    script_node_t *node = node_string(name);
    node->lineno = lineno;
    return stmt_delete(node);
}

static script_stmt_t *parse_statement_inner(script_token_t **token) {
    if (!*token) return NULL;

    script_stmt_t *stmt = NULL;
    if ((*token)->type == SCRIPT_TOKEN_LET) {
        *token = (*token)->next;
        stmt = parse_declare(token);
    } else if ((*token)->type == SCRIPT_TOKEN_IDENTIFIER &&
            ((*token)->next && (*token)->next->type == SCRIPT_TOKEN_EQUAL)) { 
        stmt = parse_assign(token);
    } else if ((*token)->type == SCRIPT_TOKEN_LBRAC) {
        *token = (*token)->next;
        stmt = parse_block(token);
    } else if ((*token)->type == SCRIPT_TOKEN_FUNC) {
        *token = (*token)->next;
        stmt = parse_function(token);
    } else if ((*token)->type == SCRIPT_TOKEN_IF) {
        *token = (*token)->next;
        stmt = parse_if(token);
    } else if ((*token)->type == SCRIPT_TOKEN_RETURN) {
        *token = (*token)->next;
        stmt = parse_return(token);
    } else if ((*token)->type == SCRIPT_TOKEN_BREAK) {
        *token = (*token)->next;
        stmt = parse_break();
    } else if ((*token)->type == SCRIPT_TOKEN_CONTINUE) {
        *token = (*token)->next;
        stmt = parse_continue();
    } else if ((*token)->type == SCRIPT_TOKEN_INCLUDE) {
        *token = (*token)->next;
        stmt = parse_include(token);
    } else if ((*token)->type == SCRIPT_TOKEN_DELETE) {
        *token = (*token)->next;
        stmt = parse_delete(token);
    } else if ((*token)->type == SCRIPT_TOKEN_WHILE) {
        *token = (*token)->next;
        stmt = parse_while(token);
    } else if ((*token)->type == SCRIPT_TOKEN_FOR) {
        *token = (*token)->next;
        stmt = parse_for(token);
    } else {
        script_node_t *node = parse_expr(token);
        if (!node) return NULL;

        stmt = stmt_expr(node);
    }

    return stmt;
}

static script_stmt_t *parse_statement(script_token_t **token) {
    script_stmt_t *stmt = parse_statement_inner(token);
    if (!stmt) return NULL;

    if (stmt->type != SCRIPT_STMT_FUNC &&
        stmt->type != SCRIPT_STMT_BLOCK &&
        stmt->type != SCRIPT_STMT_IF &&
        stmt->type != SCRIPT_STMT_WHILE &&
        stmt->type != SCRIPT_STMT_FOR) {
        
        if (!*token || (*token)->type != SCRIPT_TOKEN_END) {
            char msg[64];
            strfmt(msg, "Error: expected ';' (line: %d)\n", *token ? (*token)->lineno : stmt->lineno);
            term_write(msg);
            free_stmt(stmt);
            return NULL;
        }

        *token = (*token)->next;
    }

    return stmt;
}

/* ==== builtins ==== */

static script_node_t *call_cpu_name(script_stmt_t *block, script_node_t *node) {
    unused(block);

    script_node_t *value = node_string(cpu_name);
    value->lineno = node->lineno;
    return value;
}

static script_node_t *call_cpu_vendor(script_stmt_t *block, script_node_t *node) {
    unused(block);

    script_node_t *value = node_string(cpu_vendor);
    value->lineno = node->lineno;
    return value;
}

static script_node_t *call_cpu_family(script_stmt_t *block, script_node_t *node) {
    unused(block);

    script_node_t *value = node_int(cpu_family);
    value->lineno = node->lineno;
    return value;
}

static script_node_t *call_cpu_model(script_stmt_t *block, script_node_t *node) {
    unused(block);

    script_node_t *value = node_int(cpu_model);
    value->lineno = node->lineno;
    return value;
}

static script_node_t *call_ata_slot(script_stmt_t *block, script_node_t *node) {
    unused(block);

    script_node_t *value = node_int(file_drive_slot());
    value->lineno = node->lineno;
    return value;
}

static script_node_t *call_ata_serial(script_stmt_t *block, script_node_t *node) {
    unused(block);

    drive_t drive;
    file_drive_spec(&drive);

    script_node_t *value = node_string(drive.serial);
    value->lineno = node->lineno;
    return value;
}

static script_node_t *call_ata_rev(script_stmt_t *block, script_node_t *node) {
    unused(block);

    drive_t drive;
    file_drive_spec(&drive);

    script_node_t *value = node_string(drive.rev);
    value->lineno = node->lineno;
    return value;
}

static script_node_t *call_ata_model(script_stmt_t *block, script_node_t *node) {
    unused(block);

    drive_t drive;
    file_drive_spec(&drive);

    script_node_t *value = node_string(drive.model);
    value->lineno = node->lineno;
    return value;
}

static script_node_t *call_exit(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;
    script_node_t **argv = node->call.argv;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function exit() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    if (argv[0]->value_type == SCRIPT_INT) {
        script_exit = argv[0]->literal.int_value;
        script_should_exit = 1;
        return g_null;
    } else {
        char msg[64];
        script_node_t *name = node_type_name(argv[0]);
        strfmt(msg, "Error: Function exit() expects string argument, got %s (line: %d)\n", name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(name);
        return NULL;
    }
}

static script_node_t *call_exec(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;
    script_node_t **argv = node->call.argv;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function exec() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    if (argv[0]->value_type == SCRIPT_STR) {
        int exit = command_handle(argv[0]->literal.str_value, 0);

        script_node_t *value = node_null();
        value->node_type = SCRIPT_AST_LITERAL;
        value->value_type = SCRIPT_INT;
        value->lineno = node->lineno;
        value->literal.int_value = exit;

        return value;
    } else {
        char msg[64];
        script_node_t *name = node_type_name(argv[0]);
        strfmt(msg, "Error: Function exec() expects string argument, got %s (line: %d)\n", name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(name);
        return NULL;
    }
}

static script_node_t *call_print(script_stmt_t *block, script_node_t *node) {
    unused(block);

    for (size_t i = 0; i < node->call.argc; i++) {
        char *repr = node_repr(node->call.argv[i]);

        if (repr) {
            int fg = term_fg;
            int bg = term_bg;
            term_fg = script_printfg;
            term_bg = script_printbg;

            if (script_printcap_print)
                term_write(repr);
            if (script_printcap)
                string_puts(script_printcap, repr);

            term_fg = fg;
            term_bg = bg;
            heap_free(repr);
            screen_flush();
        }
    }

    return g_null;
}

static script_node_t *call_sys_log(script_stmt_t *block, script_node_t *node) {
    unused(block);

    for (size_t i = 0; i < node->call.argc; i++) {
        char *repr = node_repr(node->call.argv[i]);

        if (repr) {
            log(repr);
            heap_free(repr);
        }
    }

    return g_null;
}

static script_node_t *call_println(script_stmt_t *block, script_node_t *node) {
    unused(block);

    script_node_t *ret = call_print(block, node);

    if (script_printcap_print)
        term_write("\n");
    if (script_printcap)
        string_putc(script_printcap, '\n');

    return ret;
}

static script_node_t *call_as_str(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function as_str() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *arg = node->call.argv[0];
    if (arg->value_type == SCRIPT_LIST)
        return call_list_str(block, node);
    else if (arg->value_type == SCRIPT_FUNC)
        return node_string(arg->literal.func->func.name->literal.str_value);

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_STR;
    value->lineno = node->lineno;

    char *repr = node_repr(arg);
    if (repr) {
        value->literal.str_size = strlen(repr) + 1;
        value->literal.str_value = repr;
    } else {
        char msg[64];
        strfmt(msg, "Error: Unsupported type (line: %d)\n", value->lineno);
        term_write(msg);
        unref_node(value);
        return NULL;
    }

    return value;
}

static script_node_t *call_as_int(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function as_int() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *arg = node->call.argv[0];

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_INT;
    value->lineno = node->lineno;

    switch (arg->value_type) {
        case SCRIPT_INT:
            {
                unref_node(value);

                script_node_t *copy = node_null();
                copy->value_type = SCRIPT_INT;
                copy->literal.int_value = arg->literal.int_value;
                return copy;
            }
        case SCRIPT_BOOL:
            {
                value->literal.int_value = arg->literal.int_value;
                break;
            }
        case SCRIPT_FLOAT:
            {
                value->literal.int_value = (int) arg->literal.float_value;
                break;
            }
        case SCRIPT_NULL:
            {
                value->literal.int_value = 0;
                break;
            }
        case SCRIPT_STR:
            {
                value->literal.int_value = (int) doublestr(arg->literal.str_value);
                break;
            }
        default:
            {
                char msg[64];
                strfmt(msg, "Error: Unsupported type (line: %d)\n", value->lineno);
                term_write(msg);
                unref_node(value);
                return NULL;
            }
    }

    return value;
}

static script_node_t *call_as_float(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function as_float() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *arg = node->call.argv[0];

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_FLOAT;
    value->lineno = node->lineno;

    switch (arg->value_type) {
        case SCRIPT_INT:
            {
                value->literal.float_value = (double) arg->literal.int_value;
                break;
            }
        case SCRIPT_BOOL:
            {
                value->literal.float_value = (double) arg->literal.int_value;
                break;
            }
        case SCRIPT_FLOAT:
            {
                unref_node(value);

                script_node_t *copy = node_null();
                copy->value_type = SCRIPT_FLOAT;
                copy->literal.float_value = arg->literal.float_value;
                return copy;
            }
        case SCRIPT_NULL:
            {
                value->literal.float_value = 0.0;
                break;
            }
        case SCRIPT_STR:
            {
                value->literal.float_value = doublestr(arg->literal.str_value);
                break;
            }
        default:
            {
                char msg[64];
                strfmt(msg, "Error: Unsupported type (line: %d)\n", value->lineno);
                term_write(msg);
                unref_node(value);
                return NULL;
            }
    }

    return value;
}

static script_node_t *call_type_name(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function type_name() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    return node_type_name(node->call.argv[0]);
}

static script_node_t *call_file_open(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 2) {
        char msg[64];
        strfmt(msg, "Error: Function file_open() takes 2 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *filename = node->call.argv[0];
    script_node_t *mode = node->call.argv[1];

    if (filename->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(filename);
        strfmt(msg, "Error: Function file_open() arg 1 expects string argument, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (mode->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(mode);
        strfmt(msg, "Error: Function file_open() arg 2 expects string argument, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    script_node_t *value = node_null();
    value->lineno = node->lineno;

    fio_t *file = fio_open(filename->literal.str_value, mode->literal.str_value[0]);
    if (!file)
        return value;

    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_FILE;
    value->literal.file = file;

    return value;
}

static script_node_t *call_file_close(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function file_close() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *file = node->call.argv[0];

    if (file->value_type != SCRIPT_FILE) {
        char msg[128];
        script_node_t *type_name = node_type_name(file);
        strfmt(msg, "Error: Function file_open() expects file, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (file->literal.file)
        fio_close(file->literal.file);

    return g_null;
}

static script_node_t *call_file_getc(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function file_getc() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *file = node->call.argv[0];

    if (file->value_type != SCRIPT_FILE) {
        char msg[128];
        script_node_t *type_name = node_type_name(file);
        strfmt(msg, "Error: Function file_getc() expects file, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (file->literal.file) {
        char c = fio_getc(file->literal.file);

        script_node_t *value = node_null();
        value->node_type = SCRIPT_AST_LITERAL;
        value->value_type = SCRIPT_STR;
        value->lineno = file->lineno;
        value->literal.str_value = heap_alloc(2);
        value->literal.str_value[0] = c;
        value->literal.str_value[1] = '\0';
        value->literal.str_size = 2;

        return value;
    }

    return g_null;
}

static script_node_t *call_file_peek(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function file_peek() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *file = node->call.argv[0];

    if (file->value_type != SCRIPT_FILE) {
        char msg[128];
        script_node_t *type_name = node_type_name(file);
        strfmt(msg, "Error: Function file_peek() expects file, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (file->literal.file) {
        char c = fio_peek(file->literal.file);

        script_node_t *value = node_null();
        value->node_type = SCRIPT_AST_LITERAL;
        value->value_type = SCRIPT_STR;
        value->lineno = file->lineno;
        value->literal.str_value = heap_alloc(2);
        value->literal.str_value[0] = c;
        value->literal.str_value[1] = '\0';
        value->literal.str_size = 2;

        return value;
    }

    return g_null;
}

static script_node_t *call_file_read(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function file_read() requires at least 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *file = node->call.argv[0];
    script_node_t *length = NULL;
    if (argc > 1)
        length = node->call.argv[1];

    if (file->value_type != SCRIPT_FILE) {
        char msg[128];
        script_node_t *type_name = node_type_name(file);
        strfmt(msg, "Error: Function file_read() arg 1 expects file, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (length && length->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(length);
        strfmt(msg, "Error: Function file_read() arg 2 expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (file->literal.file) {
        size_t len;
        if (!length) {
            uint32_t target = file->literal.file->file;
            file_node_t target_node;
            file_node(target, &target_node);
            len = target_node.size - file->literal.file->seek;
        } else len = length->literal.int_value;

        script_node_t *value = node_null();
        value->node_type = SCRIPT_AST_LITERAL;
        value->value_type = SCRIPT_STR;
        value->lineno = file->lineno;
        value->literal.str_value = heap_alloc(len);
        value->literal.str_size = len;
        fio_read(file->literal.file, value->literal.str_value, len);

        return value;
    }

    return g_null;
}

static script_node_t *call_file_write(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 2) {
        char msg[64];
        strfmt(msg, "Error: Function file_write() takes 2 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *file = node->call.argv[0];
    script_node_t *string = node->call.argv[1];

    if (file->value_type != SCRIPT_FILE) {
        char msg[128];
        script_node_t *type_name = node_type_name(file);
        strfmt(msg, "Error: Function file_write() arg 1 expects file, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (string->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(string);
        strfmt(msg, "Error: Function file_write() arg 2 expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (file->literal.file)
        fio_write(file->literal.file, string->literal.str_value, string->literal.str_size - 1);

    return g_null;
}

static script_node_t *call_file_isfile(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc < 1) {
        char msg[64];
        strfmt(msg, "Error: Function file_isfile() takes 1 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *path = node->call.argv[0];

    if (path->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(path);
        strfmt(msg, "Error: Function file_isfile() expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (file_path_isfile(path->literal.str_value))
        return g_true;

    return g_false;
}

static script_node_t *call_file_isfolder(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc < 1) {
        char msg[64];
        strfmt(msg, "Error: Function file_isfolder() takes 1 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *path = node->call.argv[0];

    if (path->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(path);
        strfmt(msg, "Error: Function file_isfolder() expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (file_path_isfolder(path->literal.str_value))
        return g_true;

    return g_false;
}

static script_node_t *call_file_list(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    script_node_t *path = NULL;
    if (argc == 1) {
        path = node->call.argv[0];

        if (path->value_type != SCRIPT_STR) {
            char msg[128];
            script_node_t *type_name = node_type_name(path);
            strfmt(msg, "Error: Function file_list() expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
            term_write(msg);
            unref_node(type_name);
            return NULL;
        }

        if (!file_path_isfolder(path->literal.str_value))
            return g_null;
    }

    script_node_t *list = call_list_init(block, node);

    file_node_t target_node;
    uint32_t target = path ? file_get_node(path->literal.str_value) : file_current;
    file_node(target, &target_node);

    if (target_node.child_head) {
        uint32_t current = target_node.child_head;
        file_node_t current_node;

        while (current) {
            file_node(current, &current_node);
            list_push(list->literal.list, (void*)node_string(current_node.name));

            current = current_node.child_next;
        }
    }

    return list;
}

static script_node_t *call_char_at(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 2) {
        char msg[64];
        strfmt(msg, "Error: Function char_at() takes 2 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *string = node->call.argv[0];
    script_node_t *index = node->call.argv[1];

    if (string->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(string);
        strfmt(msg, "Error: Function char_at() arg 1 expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (index->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(index);
        strfmt(msg, "Error: Function char_at() arg 2 expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (index->literal.int_value >= 0 && index->literal.int_value < (int) string->literal.str_size - 1) {
        script_node_t *value = node_null();
        value->node_type = SCRIPT_AST_LITERAL;
        value->value_type = SCRIPT_STR;
        value->lineno = node->lineno;
        value->literal.str_value = heap_alloc(2);
        value->literal.str_size = 2;
        value->literal.str_value[0] = string->literal.str_value[index->literal.int_value];
        value->literal.str_value[1] = '\0';

        return value;
    }

    return g_null;
}

static script_node_t *call_sizeof(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function sizeof() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *arg = node->call.argv[0];

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_INT;
    value->lineno = node->lineno;

    switch (arg->value_type) {
        case SCRIPT_INT:
            {
                value->literal.int_value = 1;
                break;
            }
        case SCRIPT_BOOL:
            {
                value->literal.int_value = 1;
                break;
            }
        case SCRIPT_FLOAT:
            {
                value->literal.int_value = 1;
                break;
            }
        case SCRIPT_STR:
            {
                value->literal.int_value = arg->literal.str_size - 1;
                break;
            }
        case SCRIPT_FILE:
            {
                file_node_t file;
                file_node(arg->literal.file->file, &file);

                value->literal.int_value = (int) FIO_FS_BLOCKSIZE * file.size;
                break;
            }
        case SCRIPT_LIST:
        case SCRIPT_VARLIST:
            {
                value->literal.int_value = (int) arg->literal.list->size;
                break;
            }
        default:
            {
                value->literal.int_value = 0;
                break;
            }
    }

    return value;
}

static script_node_t *call_input(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    char input[TERM_INPUT_SIZE];
    char *prompt = NULL;
    if (argc == 1) {
        script_node_t *arg = node->call.argv[0];

        if (arg->value_type != SCRIPT_STR) {
            char msg[128];
            script_node_t *type_name = node_type_name(arg);
            strfmt(msg, "Error: Function input() expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
            term_write(msg);
            unref_node(type_name);
            return NULL;
        }

        prompt = heap_alloc(arg->literal.str_size);
        memcpy(prompt, arg->literal.str_value, arg->literal.str_size);
    } else if (argc > 1) {
        char msg[64];
        strfmt(msg, "Error: Function input() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    int fg = term_fg;
    int bg = term_bg;
    term_fg = script_printfg;
    term_bg = script_printbg;

    keyboard_mode = KEYBOARD_MODE_TERM;
    term_get_input(prompt == NULL ? "" : prompt, input, sizeof(input));
    keyboard_mode = KEYBOARD_MODE_SCRIPT;

    term_fg = fg;
    term_bg = bg;
    heap_free(prompt);

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_STR;
    value->lineno = node->lineno;
    value->literal.str_size = strlen(input) + 1;
    value->literal.str_value = heap_alloc(value->literal.str_size);
    memcpy(value->literal.str_value, input, value->literal.str_size);

    return value;
}

static script_node_t *call_config_has(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 2) {
        char msg[64];
        strfmt(msg, "Error: Function config_has() takes 2 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *path = node->call.argv[0];
    script_node_t *name = node->call.argv[1];

    if (path->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(path);
        strfmt(msg, "Error: Function config_has() arg 1 expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (name->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(name);
        strfmt(msg, "Error: Function config_has() arg 2 expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (config_has(path->literal.str_value, name->literal.str_value))
        return g_true;

    return g_false;
}

static script_node_t *call_config_get(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 2) {
        char msg[64];
        strfmt(msg, "Error: Function config_has() takes 2 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *path = node->call.argv[0];
    script_node_t *name = node->call.argv[1];

    if (path->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(path);
        strfmt(msg, "Error: Function config_has() arg 1 expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (name->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(name);
        strfmt(msg, "Error: Function config_has() arg 2 expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    char *value = config_get(path->literal.str_value, name->literal.str_value);

    script_node_t *ret = node_null();
    ret->node_type = SCRIPT_AST_LITERAL;
    ret->value_type = SCRIPT_STR;
    ret->lineno = node->lineno;

    int length = strlen(value) + 1;
    ret->literal.str_size = length;
    ret->literal.str_value = heap_alloc(length);
    memcpy(ret->literal.str_value, value, length);
    heap_free(value);

    return ret;
}

static script_node_t *call_list_init(script_stmt_t *block, script_node_t *node) {
    unused(block);

    list_t *list = heap_alloc(sizeof(list_t));
    list_init(list);

    script_node_t *ret = node_null();
    ret->node_type = SCRIPT_AST_LITERAL;
    ret->value_type = SCRIPT_LIST;
    ret->lineno = node->lineno;
    ret->literal.list = list;

    return ret;
}

static script_node_t *call_list_clear(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function list_clear() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *list = node->call.argv[0];

    if (list->value_type != SCRIPT_LIST) {
        char msg[128];
        script_node_t *type_name = node_type_name(list);
        strfmt(msg, "Error: Function list_clear() expects list, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (list->literal.list) {
        list_node_t *current = list->literal.list->head;
        while (current) {
            list_node_t *next = current->next;
            unref_node((script_node_t*)current->data);
            heap_free(current);
            current = next;
        }
    }

    return g_null;
}

static script_node_t *call_list_pop(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function list_pop() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *list = node->call.argv[0];

    if (list->value_type != SCRIPT_LIST) {
        char msg[128];
        script_node_t *type_name = node_type_name(list);
        strfmt(msg, "Error: Function list_pop() expects list, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (list->literal.list) {
        script_node_t *value = (script_node_t*)list_pop(list->literal.list);
        if (value)
            return value;
    }

    return g_null;
}

static script_node_t *call_list_push(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 2) {
        char msg[64];
        strfmt(msg, "Error: Function list_push() takes 2 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *list = node->call.argv[0];
    script_node_t *value = node->call.argv[1];

    if (list->value_type != SCRIPT_LIST) {
        char msg[128];
        script_node_t *type_name = node_type_name(list);
        strfmt(msg, "Error: Function list_push() arg 1 expects list, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (list->literal.list)
        list_push(list->literal.list, (void*)ref_node(value));

    return g_null;
}

static script_node_t *call_list_get(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 2) {
        char msg[64];
        strfmt(msg, "Error: Function list_get() takes 2 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *list = node->call.argv[0];
    script_node_t *index = node->call.argv[1];

    if (list->value_type != SCRIPT_LIST) {
        char msg[128];
        script_node_t *type_name = node_type_name(list);
        strfmt(msg, "Error: Function list_get() arg 1 expects list, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (index->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(index);
        strfmt(msg, "Error: Function list_get() arg 2 expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (list->literal.list && index->literal.int_value >= 0) {
        script_node_t *value = (script_node_t*)list_get(list->literal.list, (size_t)index->literal.int_value);
        if (value)
            return ref_node(value);
    }

    return g_null;
}

static script_node_t *call_list_remove(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 2) {
        char msg[64];
        strfmt(msg, "Error: Function list_remove() takes 2 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *list = node->call.argv[0];
    script_node_t *index = node->call.argv[1];

    if (list->value_type != SCRIPT_LIST) {
        char msg[128];
        script_node_t *type_name = node_type_name(list);
        strfmt(msg, "Error: Function list_remove() arg 1 expects list, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (index->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(index);
        strfmt(msg, "Error: Function list_remove() arg 2 expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (list->literal.list && index->literal.int_value >= 0)
        list_remove(list->literal.list, (size_t)index->literal.int_value);

    return g_null;
}

static script_node_t *call_list_str(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function list_str() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *list = node->call.argv[0];

    if (list->value_type != SCRIPT_LIST) {
        char msg[128];
        script_node_t *type_name = node_type_name(list);
        strfmt(msg, "Error: Function list_str() arg 1 expects list, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (list->literal.list) {
        string_t *str = string_init();
        string_putc(str, '[');

        int first = 1;
        list_t *l = list->literal.list;
        list_node_t *current = l->head;
        while (current) {
            script_node_t *li = (script_node_t*)current->data;
            char *val = node_repr(li);
            if (val) {
                if (!first)
                    string_puts(str, ", ");

                char sym = '\0';
                if (li->value_type == SCRIPT_STR) {
                    if (strhasc(val, '\''))
                        sym = '"';
                    else
                        sym = '\'';
                }

                if (sym)
                    string_putc(str, sym);
                string_puts(str, val);
                if (sym)
                    string_putc(str, sym);
                heap_free(val);

                first = 0;
            }
            current = current->next;
        }
        string_putc(str, ']');

        script_node_t *res = node_string(str->value);
        res->lineno = node->lineno;
        string_free(str);
        return res;
    }

    return g_null;
}

static script_node_t *call_list_has(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 2) {
        char msg[64];
        strfmt(msg, "Error: Function list_has() takes 2 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *list = node->call.argv[0];
    script_node_t *item = node->call.argv[1];

    if (list->value_type != SCRIPT_LIST) {
        char msg[128];
        script_node_t *type_name = node_type_name(list);
        strfmt(msg, "Error: Function list_has() arg 1 expects list, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (list->literal.list && list->literal.list->size > 0) {
        list_t *l = list->literal.list;

        list_node_t *current = l->head;
        while (current) {
            script_node_t *li = (script_node_t*)current->data;

            script_node_t *cmp = node_cmp(li, item);
            if (node_istrue(cmp))
                return cmp;

            current = current->next;
        }
    }

    return g_false;
}

static script_node_t *call_sleep(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function sleep() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *interval = node->call.argv[0];

    if (interval->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(interval);
        strfmt(msg, "Error: Function sleep() expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    uint32_t start = pit_ticks;
    uint32_t wait = ms_to_ticks(interval->literal.int_value);

    __asm__ volatile("sti");
    while (pit_ticks - start < wait)
        __asm__ volatile("hlt");

    return g_null;
}

static script_node_t *call_sys_ticks(script_stmt_t *block, script_node_t *node) {
    unused(block);

    __asm__ volatile("sti");
    int ticks = (int)pit_ticks;

    script_node_t *value = node_int(ticks);
    value->lineno = node->lineno;
    return value;
}

static script_node_t *call_sys_tps(script_stmt_t *block, script_node_t *node) {
    unused(block);

    __asm__ volatile("sti");
    int tps = (int)pit_hz;

    script_node_t *value = node_int(tps);
    value->lineno = node->lineno;
    return value;
}

static script_node_t *call_sys_perf(script_stmt_t *block, script_node_t *node) {
    unused(block);

    __asm__ volatile("sti");
    int ticks = (int)pit_ticks;
    int tps = (int)pit_hz;

    script_node_t *value = node_float((double)ticks / tps);
    value->lineno = node->lineno;
    return value;
}

static script_node_t *call_argc(script_stmt_t *block, script_node_t *node) {
    unused(block);

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_INT;
    value->lineno = node->lineno;
    value->literal.int_value = script_argc;

    return value;
}

static script_node_t *call_argv(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function argv() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *index = node->call.argv[0];

    if (index->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(index);
        strfmt(msg, "Error: Function argv() expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    int idx = index->literal.int_value;
    if (idx >= script_argc || idx < 0)
        return g_null;

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_STR;
    value->lineno = node->lineno;

    int length = strlen(script_argv[idx]) + 1;
    value->literal.str_size = length;
    value->literal.str_value = heap_alloc(length);
    memcpy(value->literal.str_value, script_argv[idx], length);

    return value;
}

static script_node_t *call_rand(script_stmt_t *block, script_node_t *node) {
    unused(block);

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_INT;
    value->lineno = node->lineno;
    value->literal.int_value = rand();

    return value;
}

static script_node_t *call_randrange(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 2) {
        char msg[64];
        strfmt(msg, "Error: Function randrange() takes 2 arguments, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *min = node->call.argv[0];
    script_node_t *max = node->call.argv[1];

    if (min->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(min);
        strfmt(msg, "Error: Function randrange() arg 1 expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (max->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(max);
        strfmt(msg, "Error: Function randrange() arg 2 expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_INT;
    value->lineno = node->lineno;
    value->literal.int_value = randrange(min->literal.int_value, max->literal.int_value);

    return value;
}

static script_node_t *call_color_setfg(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function color_setfg() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *name = node->call.argv[0];

    if (name->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(name);
        strfmt(msg, "Error: Function color_setfg() expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    int col = color(name->literal.str_value);
    if (col != COLOR_INVALID)
        script_printfg = col;
    else
        script_printfg = term_fg;

    return g_null;
}

static script_node_t *call_color_setbg(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function color_setbg() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *name = node->call.argv[0];

    if (name->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(name);
        strfmt(msg, "Error: Function color_setbg() expects str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    int col = color(name->literal.str_value);
    if (col != COLOR_INVALID)
        script_printbg = col;
    else
        script_printbg = term_bg;

    return g_null;
}

static script_node_t *call_color_reset(script_stmt_t *block, script_node_t *node) {
    unused(block); unused(node);

    script_printfg = term_fg;
    script_printbg = term_bg;
    return g_null;
}

static script_node_t *call_color(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function color() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *name = node->call.argv[0];

    if (name->value_type != SCRIPT_STR) {
        char msg[128];
        script_node_t *type_name = node_type_name(name);
        strfmt(msg, "Error: Function color() expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    return node_int(color(name->literal.str_value));
}

static script_node_t *call_color_rgb(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 3) {
        char msg[64];
        strfmt(msg, "Error: Function color_rgb() takes 3 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    for (size_t i = 0; i < argc; i++) {
        script_node_t *arg = node->call.argv[i];

        if (arg->value_type != SCRIPT_INT) {
            char msg[128];
            script_node_t *type_name = node_type_name(arg);
            strfmt(msg, "Error: Function color_rgba() arg %d expects int, got %s (line: %d)\n", i + 1, type_name->literal.str_value, node->lineno);
            term_write(msg);
            unref_node(type_name);
            return NULL;
        }
    }

    script_node_t *r = node->call.argv[0];
    script_node_t *g = node->call.argv[1];
    script_node_t *b = node->call.argv[2];

    return node_int((int)COLOR_RGB(
        r->literal.int_value,
        g->literal.int_value,
        b->literal.int_value));
}

static script_node_t *call_screen_init(script_stmt_t *block, script_node_t *node) {
    unused(block); unused(node);

    if (!script_screen_buffer) {
        script_screen_buffer_size = (screen_pitch / sizeof(uint32_t)) * screen_height;
        script_screen_buffer = heap_alloc(script_screen_buffer_size * sizeof(uint32_t));
        memcpy(script_screen_buffer, screen_buffer, script_screen_buffer_size * sizeof(uint32_t));
    } else {
        char msg[128];
        strfmt(msg, "Warning: Screen is already initialized. (line: %d)\n", node->lineno);
        term_write(msg);
    }

    return g_null;
}

static script_node_t *call_screen_width(script_stmt_t *block, script_node_t *node) {
    unused(block); unused(node);

    return node_int(screen_width);
}

static script_node_t *call_screen_height(script_stmt_t *block, script_node_t *node) {
    unused(block); unused(node);

    return node_int(screen_height);
}

static script_node_t *call_screen_pitch(script_stmt_t *block, script_node_t *node) {
    unused(block); unused(node);

    return node_int(screen_pitch);
}

static script_node_t *call_screen_scale(script_stmt_t *block, script_node_t *node) {
    unused(block); unused(node);

    return node_float(screen_scale);
}

static script_node_t *call_screen_draw(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 3) {
        char msg[64];
        strfmt(msg, "Error: Function screen_draw() takes 3 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *nx = node->call.argv[0];
    script_node_t *ny = node->call.argv[1];
    script_node_t *nc = node->call.argv[2];

    if (nx->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(nx);
        strfmt(msg, "Error: Function screen_draw() x expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (ny->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(ny);
        strfmt(msg, "Error: Function screen_draw() y expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (nc->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(nc);
        strfmt(msg, "Error: Function screen_draw() color expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (!script_screen_buffer) {
        char msg[128];
        strfmt(msg, "Error: Screen is not initialized. (line: %d)\n", node->lineno);
        term_write(msg);
        return NULL;
    }

    int x = nx->literal.int_value;
    int y = ny->literal.int_value;
    int c = nc->literal.int_value;

    if (x >= 0 && x < screen_width && y >= 0 && y < screen_height)
        script_screen_buffer[y * (screen_pitch / sizeof(uint32_t)) + x] = c;

    return g_null;
}

static script_node_t *call_screen_clear(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function screen_clear() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *col = node->call.argv[0];

    if (col->value_type != SCRIPT_INT) {
        char msg[128];
        script_node_t *type_name = node_type_name(col);
        strfmt(msg, "Error: Function screen_clear() expects int, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    if (!script_screen_buffer) {
        char msg[128];
        strfmt(msg, "Error: Screen is not initialized. (line: %d)\n", node->lineno);
        term_write(msg);
        return NULL;
    }

    size_t pixels = (screen_pitch / sizeof(uint32_t)) * screen_height;
    for (size_t i = 0; i < pixels; i++)
        script_screen_buffer[i] = col->literal.int_value;
    return g_null;
}

static script_node_t *call_screen_flush(script_stmt_t *block, script_node_t *node) {
    unused(block); unused(node);

    if (!script_screen_buffer) {
        char msg[128];
        strfmt(msg, "Error: Screen is not initialized. (line: %d)\n", node->lineno);
        term_write(msg);
        return NULL;
    }

    memcpy(back_buffer, script_screen_buffer, script_screen_buffer_size * sizeof(uint32_t));
    return g_null;
}

static script_node_t *call_printcap_close(script_stmt_t *block, script_node_t *node) {
    unused(block); unused(node);

    if (script_printcap)
        string_free(script_printcap);
    script_printcap = NULL;

    return g_null;
}

static script_node_t *call_printcap_init(script_stmt_t *block, script_node_t *node) {
    unused(block);

    call_printcap_close(block, node);
    script_printcap = string_init();

    return g_null;
}

static script_node_t *call_printcap_get(script_stmt_t *block, script_node_t *node) {
    unused(block); unused(node);

    if (script_printcap)
        return node_string(script_printcap->value);

    return g_null;
}

static script_node_t *call_printcap_print(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function printcap_print() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *print = node->call.argv[0];

    if (print->value_type != SCRIPT_BOOL) {
        char msg[128];
        script_node_t *type_name = node_type_name(print);
        strfmt(msg, "Error: Function printcap_print() expects bool, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    script_printcap_print = print->literal.int_value;

    return g_null;
}

static script_node_t *call_internal_getvars(script_stmt_t *block, script_node_t *node) {
    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function internal_getvars() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *type = node->call.argv[0];

    if (type->value_type != SCRIPT_STR && type->value_type != SCRIPT_NULL) {
        char msg[128];
        script_node_t *type_name = node_type_name(type);
        strfmt(msg, "Error: Function internal_getvars() expects null or str, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    script_node_t *list = call_list_init(block, node);
    list->value_type = SCRIPT_VARLIST;

    script_stmt_t *parent = block;
    while (parent) {
        script_var_t *var = parent->block.env->var_head;
        while (var) {
            script_node_t *type_name = node_type_name(var->value);
            if (type->value_type == SCRIPT_NULL || (type->value_type == SCRIPT_STR && node_istrue(node_cmp(type, type_name))))
                list_push(list->literal.list, node_var(var->name, var->value));
            var = var->next;
            unref_node(type_name);
        }
        parent = parent->parent;
    }

    return list;
}

static script_node_t *call_internal_getname(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function internal_getname() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *var = node->call.argv[0];

    if (var->value_type != SCRIPT_VAR) {
        char msg[128];
        script_node_t *type_name = node_type_name(var);
        strfmt(msg, "Error: Function internal_getname() expects var, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    return node_string(var->var.name);
}

static script_node_t *call_internal_getvalue(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function internal_getvalue() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *var = node->call.argv[0];

    if (var->value_type != SCRIPT_VAR) {
        char msg[128];
        script_node_t *type_name = node_type_name(var);
        strfmt(msg, "Error: Function internal_getvalue() expects var, got %s (line: %d)\n", type_name->literal.str_value, node->lineno);
        term_write(msg);
        unref_node(type_name);
        return NULL;
    }

    return ref_node(var->var.value);
}

static script_node_t *call_internal_getrefcount(script_stmt_t *block, script_node_t *node) {
    unused(block);

    size_t argc = node->call.argc;

    if (argc != 1) {
        char msg[64];
        strfmt(msg, "Error: Function internal_getrefcount() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg);
        return NULL;
    }

    return node_int(node->call.argv[0]->ref);
}


/* ================== */

static script_node_t *eval_binop(script_stmt_t *block, script_node_t *binop) {
    uint8_t op = binop->binop.op;
    script_node_t *left = binop->binop.left;
    script_node_t *right = binop->binop.right;

    int free_left = 0;
    int free_right = 0;

    if (op == SCRIPT_TOKEN_NEG) {
        script_node_t *val = binop->binop.left;

        int free = 1;
        if (val->node_type == SCRIPT_AST_BINOP)
            val = eval_binop(block, val);
        else if (val->node_type != SCRIPT_AST_LITERAL)
            val = eval_expr(block, val);
        else
            free = 0;

        if (!val)
            return NULL;

        if (val->value_type == SCRIPT_ID) {
            char *name = val->literal.str_value;
            script_var_t *var = env_unscoped_find_var(block, name);
            if (free)
                unref_node(val);

            if (!var) {
                char msg[64];
                strfmt(msg, "Error: Undeclared \"%s\" (line: %d)\n", name, binop->lineno);
                term_write(msg);
                return NULL;
            }

            val = var->value;
            free = 0;
        }

        script_node_t *node = node_istrue(val) ? g_false : g_true;
        if (free)
            unref_node(val);
        return node;
    }

    if (left->node_type == SCRIPT_AST_BINOP) {
        left = eval_binop(block, left);
        free_left = 1;
    }
    if (right->node_type == SCRIPT_AST_BINOP) {
        right = eval_binop(block, right);
        free_right = 1;
    }

    if (left->node_type != SCRIPT_AST_LITERAL) {
        left = eval_expr(block, left);
        free_left = 1;
        
        if (!left)
            return NULL;
    }
    if (right->node_type != SCRIPT_AST_LITERAL) {
        right = eval_expr(block, right);
        free_right = 1;

        if (!right) {
            if (free_left)
                unref_node(left);

            return NULL;
        }
    }

    if (right->value_type == SCRIPT_ID) {
        char *rname = right->literal.str_value;
        script_var_t *rvar = env_unscoped_find_var(block, rname);
        if (free_right) unref_node(right);

        if (!rvar) {
            char msg[64];
            strfmt(msg, "Error: Undeclared \"%s\" (line: %d)\n", rname, binop->lineno);
            term_write(msg);
            return NULL;
        }

        right = rvar->value;
        free_right = 0;
    }

    if (left->value_type == SCRIPT_ID) {
        char *name = left->literal.str_value;
        script_var_t *var = env_unscoped_find_var(block, name);

        if (!var) {
            char msg[64];
            strfmt(msg, "Error: Undeclared \"%s\" (line: %d)\n", name, binop->lineno);
            term_write(msg);
            return NULL;
        }

        script_node_t *val = var->value;
        int ltype = val->value_type;
        int rtype = right->value_type;

        if (ltype == SCRIPT_INT && rtype == SCRIPT_INT) {
            int l = val->literal.int_value;
            int r = right->literal.int_value;

            int oped = 1;
            if (op == SCRIPT_TOKEN_ADDASSIGN)
                val->literal.int_value = l + r;
            else if (op == SCRIPT_TOKEN_SUBASSIGN)
                val->literal.int_value = l - r;
            else if (op == SCRIPT_TOKEN_MULASSIGN)
                val->literal.int_value = l * r;
            else if (op == SCRIPT_TOKEN_DIVASSIGN)
                val->literal.int_value = l / r;
            else
                oped = 0;

            if (oped) {
                val->value_type = SCRIPT_INT;

                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return ref_node(val);
            }
        } else if (ltype == SCRIPT_STR) {
            val->value_type = SCRIPT_STR;

            if (rtype == SCRIPT_STR && op == SCRIPT_TOKEN_ADDASSIGN) {
                size_t left_len = strlen(val->literal.str_value);
                size_t right_len = strlen(right->literal.str_value);
                size_t size = left_len + right_len + 1;

                char *old = val->literal.str_value;
                val->literal.str_value = heap_alloc(size);
                memcpy(val->literal.str_value, old, left_len);
                memcpy(val->literal.str_value + left_len,
                    right->literal.str_value, right_len + 1);
                val->literal.str_size = size;
                heap_free(old);

                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return ref_node(val);
            } else if (rtype == SCRIPT_INT && op == SCRIPT_TOKEN_MULASSIGN) {
                val->value_type = SCRIPT_STR;

                int v = right->literal.int_value;
                if (v < 0)
                    v = 0;

                size_t i = (size_t) v;
                size_t size = (val->literal.str_size - 1) * i;
                char *old = val->literal.str_value;
                char **value = &val->literal.str_value;
                *value = heap_alloc(size + 1);
                memset(*value, 0, size + 1);

                while (i > 0) {
                    strcat(*value, old);
                    i--;
                }
                val->literal.str_size = size + 1;
                heap_free(old);

                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return ref_node(val);
            }
        } else if ((ltype == SCRIPT_INT || ltype == SCRIPT_FLOAT) &&
                   (rtype == SCRIPT_INT || rtype == SCRIPT_FLOAT)) {
            double l = (val->value_type == SCRIPT_FLOAT) ?
                val->literal.float_value : val->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            int oped = 1;
            if (op == SCRIPT_TOKEN_ADDASSIGN)
                val->literal.float_value = l + r;
            else if (op == SCRIPT_TOKEN_SUBASSIGN)
                val->literal.float_value = l - r;
            else if (op == SCRIPT_TOKEN_MULASSIGN)
                val->literal.float_value = l * r;
            else if (op == SCRIPT_TOKEN_DIVASSIGN)
                val->literal.float_value = l / r;
            else
                oped = 0;

            if (oped) {
                val->value_type = SCRIPT_FLOAT;

                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return ref_node(val);
            }
        }

        if (free_left) unref_node(left);
        left = var->value;
    }

    if (op == SCRIPT_TOKEN_AND) {
        if (node_istrue(left) && node_istrue(right)) {
            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_true;
        }

        if (free_left) unref_node(left);
        if (free_right) unref_node(right);
        return g_false;
    } else if (op == SCRIPT_TOKEN_OR) {
        if (node_istrue(left) || node_istrue(right)) {
            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_true;
        }

        if (free_left) unref_node(left);
        if (free_right) unref_node(right);
        return g_false;
    } else if (op == SCRIPT_TOKEN_ISEQUAL) {
        if (left->value_type == SCRIPT_BOOL || right->value_type == SCRIPT_BOOL) {
            if (left->literal.int_value == right->literal.int_value) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }

        if (left->value_type == SCRIPT_NULL || right->value_type == SCRIPT_NULL) {
            if (left->value_type == right->value_type) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }

        if (left->value_type == SCRIPT_STR && right->value_type == SCRIPT_STR) {
            if (!strcmp(left->literal.str_value, right->literal.str_value)) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }

        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l == r) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }
    } else if (op == SCRIPT_TOKEN_ISNTEQUAL) {
        if (left->value_type == SCRIPT_BOOL || right->value_type == SCRIPT_BOOL) {
            if (left->literal.int_value != right->literal.int_value) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }

        if (left->value_type == SCRIPT_NULL || right->value_type == SCRIPT_NULL) {
            if (left->value_type != right->value_type) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }

        if (left->value_type == SCRIPT_STR && right->value_type == SCRIPT_STR) {
            if (strcmp(left->literal.str_value, right->literal.str_value)) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }

        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l != r) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }
    } else if (op == SCRIPT_TOKEN_LESSTHAN) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l < r) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }
    } else if (op == SCRIPT_TOKEN_MORETHAN) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l > r) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }
        
            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }
    } else if (op == SCRIPT_TOKEN_LESSEQUAL) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l <= r) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }
        
            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }
    } else if (op == SCRIPT_TOKEN_MOREEQUAL) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l >= r) {
                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return g_true;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return g_false;
        }
    }

    script_node_t *node = node_new();
    node->node_type = SCRIPT_AST_LITERAL;

    if (op == SCRIPT_TOKEN_PLUS) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {
            if (left->value_type == SCRIPT_FLOAT || right->value_type == SCRIPT_FLOAT) {
                node->value_type = SCRIPT_FLOAT;

                double l = (left->value_type == SCRIPT_FLOAT) ?
                    left->literal.float_value : left->literal.int_value;
                double r = (right->value_type == SCRIPT_FLOAT) ?
                    right->literal.float_value : right->literal.int_value;

                node->literal.float_value = l + r;
            } else {
                node->value_type = SCRIPT_INT;
                node->literal.int_value = left->literal.int_value + right->literal.int_value;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return node;
        } else if (left->value_type == SCRIPT_STR && right->value_type == SCRIPT_STR) {
            node->value_type = SCRIPT_STR;

            size_t left_len = strlen(left->literal.str_value);
            size_t right_len = strlen(right->literal.str_value);
            size_t size = left_len + right_len + 1;

            node->literal.str_value = heap_alloc(size);
            memcpy(node->literal.str_value,
                left->literal.str_value, left_len);
            memcpy(node->literal.str_value + left_len,
                right->literal.str_value, right_len + 1);
            node->literal.str_size = size;

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return node;
        }
    } else if (op == SCRIPT_TOKEN_MINUS) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {
            if (left->value_type == SCRIPT_FLOAT || right->value_type == SCRIPT_FLOAT) {
                node->value_type = SCRIPT_FLOAT;

                double l = (left->value_type == SCRIPT_FLOAT) ?
                    left->literal.float_value : left->literal.int_value;
                double r = (right->value_type == SCRIPT_FLOAT) ?
                    right->literal.float_value : right->literal.int_value;

                node->literal.float_value = l - r;
            } else {
                node->value_type = SCRIPT_INT;
                node->literal.int_value = left->literal.int_value - right->literal.int_value;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return node;
        }
    } else if (op == SCRIPT_TOKEN_DIVIDE) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {
            node->value_type = SCRIPT_FLOAT;

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (r == 0) {
                char msg[64];
                strfmt(msg, "Error: Zero division (line: %d)\n", binop->lineno);
                term_write(msg);
                unref_node(node);
                return NULL;
            }

            node->literal.float_value = l / r;

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return node;
        }
    } else if (op == SCRIPT_TOKEN_MODULO) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            int l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            int r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (r == 0) {
                char msg[64];
                strfmt(msg, "Error: Modulo by zero (line: %d)\n", binop->lineno);
                term_write(msg);
                unref_node(node);
                return NULL;
            }

            node->value_type = SCRIPT_INT;
            node->literal.int_value = l % r;

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return node;
        }
    } else if (op == SCRIPT_TOKEN_TIMES) {
        if (left->value_type == SCRIPT_INT) {
            if (right->value_type == SCRIPT_INT) {
                node->value_type = SCRIPT_INT;
                node->literal.int_value = left->literal.int_value * right->literal.int_value;

                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return node;
            } else if (right->value_type == SCRIPT_STR) {
                node->value_type = SCRIPT_STR;

                int v = left->literal.int_value;
                if (v < 0)
                    v = 0;

                size_t i = (size_t) v;
                size_t size = (right->literal.str_size - 1) * i;
                char **value = &node->literal.str_value;
                *value = heap_alloc(size + 1);
                memset(*value, 0, size + 1);

                while (i > 0) {
                    strcat(*value, right->literal.str_value);
                    i--;
                }
                node->literal.str_size = size + 1;

                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return node;
            } 
        } else if (left->value_type == SCRIPT_STR) {
            if (right->value_type == SCRIPT_INT) {
                node->value_type = SCRIPT_STR;

                int v = right->literal.int_value;
                if (v < 0)
                    v = 0;

                size_t i = (size_t) v;
                size_t size = (left->literal.str_size - 1) * i;
                char **value = &node->literal.str_value;
                *value = heap_alloc(size + 1);
                memset(*value, 0, size + 1);

                while (i > 0) {
                    strcat(*value, left->literal.str_value);
                    i--;
                }
                node->literal.str_size = size + 1;

                if (free_left) unref_node(left);
                if (free_right) unref_node(right);
                return node;
            }
        } else if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {
            if (left->value_type == SCRIPT_FLOAT || right->value_type == SCRIPT_FLOAT) {
                node->value_type = SCRIPT_FLOAT;

                double l = (left->value_type == SCRIPT_FLOAT) ?
                    left->literal.float_value : left->literal.int_value;
                double r = (right->value_type == SCRIPT_FLOAT) ?
                    right->literal.float_value : right->literal.int_value;

                node->literal.float_value = l * r;
            } else {
                node->value_type = SCRIPT_INT;
                node->literal.int_value =
                    left->literal.int_value * right->literal.int_value;
            }

            if (free_left) unref_node(left);
            if (free_right) unref_node(right);
            return node;
        }
    }

    char msg[64];
    strfmt(msg, "Error: Unsupported operation (line: %d)\n", binop->lineno);
    term_write(msg);
    if (free_left) unref_node(left);
    if (free_right) unref_node(right);
    unref_node(node);
    return NULL;
}

static script_node_t *eval_call(script_stmt_t *block, script_node_t *call) {
    script_node_t **eval_args = heap_alloc(sizeof(script_node_t*) * call->call.argc);

    for (size_t i = 0; i < call->call.argc; i++) {
        eval_args[i] = eval_expr(block, call->call.argv[i]);

        if (!eval_args[i]) {
            for (size_t j = 0; j < i; j++)
                unref_node(eval_args[j]);
            heap_free(eval_args);
            return NULL;
        }
    }

    char *name = call->call.func->literal.str_value;
    script_node_t *ret = NULL;

    script_node_t copy_call = *call;
    copy_call.call.argv = eval_args;

    if (call->call.builtin >= 0 && call->call.builtin < CALL_E_COUNT) {
        ret = builtins[call->call.builtin].func(block, &copy_call);
        if (!ret) {
            script_exit = 1;
            script_should_exit = 1;
        }
    } else {
        script_var_t *var = env_unscoped_find_var(block, name);
        if (var) {
            script_node_t *vv = var->value;

            if (vv->value_type != SCRIPT_FUNC) {
                char msg[64];
                strfmt(msg, "Error: Variable \"%s\" is not callable (line: %d)\n", name, call->lineno);
                term_write(msg);
                return NULL;
            }

            script_stmt_t *func = vv->literal.func;
            script_stmt_t *call_block = stmt_block(func->func.block->parent);

            if (call->call.argc < func->func.params_count) {
                char msg[64];
                strfmt(msg, "Error: Function \"%s\" takes %d argument(s), got %d (line: %d)\n",
                    name, func->func.params_count, call->call.argc, call->lineno);
                term_write(msg);
                return NULL;
            }

            for (size_t i = 0; i < func->func.params_count; i++) {
                script_node_t *param = func->func.params[i];
                script_node_t *arg = eval_args[i];

                env_set_var(call_block, param->literal.str_value, arg);
                eval_args[i] = NULL;
            }

            script_eval_t *eval = eval_block(call_block, func->func.block);
            if (eval->type == SCRIPT_EVAL_RETURN) {
                ret = eval->node;
                heap_free(eval);
            } else
                free_eval(eval);

            free_stmt(call_block);
        } else {
            char msg[64];
            strfmt(msg, "Error: Undefined call \"%s\" (line: %d)\n", name, call->lineno);
            term_write(msg);
            return NULL;
        }
    }

    for (size_t i = 0; i < call->call.argc; i++)
        unref_node(eval_args[i]);
    heap_free(eval_args);
    return ret ? ret : g_null;
}

static script_node_t *eval_index(script_stmt_t *block, script_node_t *index) {
    char *varname = index->index.var->literal.str_value;
    script_var_t *var = env_unscoped_find_var(block, varname);
    if (!var) {
        char msg[64];
        strfmt(msg, "Error: Undeclared \"%s\" (line: %d)\n", varname, index->lineno);
        term_write(msg);
        return NULL;
    }

    script_node_t *idx = eval_expr(block, index->index.index);
    if (!idx)
        return NULL;

    switch (var->value->value_type) {
        case SCRIPT_STR:
            {
                script_node_t *string = var->value;
                if (idx->value_type != SCRIPT_INT) {
                    char msg[64];
                    strfmt(msg, "Error: Index expects 'int' type (line: %d)\n", index->lineno);
                    term_write(msg);
                    unref_node(idx);
                    return NULL;
                }

                if (idx->literal.int_value >= 0 && idx->literal.int_value < (int) string->literal.str_size - 1) {
                    script_node_t *value = node_null();
                    value->node_type = SCRIPT_AST_LITERAL;
                    value->value_type = SCRIPT_STR;
                    value->lineno = index->lineno;
                    value->literal.str_value = heap_alloc(2);
                    value->literal.str_size = 2;
                    value->literal.str_value[0] = string->literal.str_value[idx->literal.int_value];
                    value->literal.str_value[1] = '\0';

                    unref_node(idx);
                    return value;
                } else {
                    unref_node(idx);
                    return g_null;
                }

                break;
            }
        case SCRIPT_LIST:
        case SCRIPT_VARLIST:
            {
                list_t *list = var->value->literal.list;
                if (!list) {
                    char msg[64];
                    strfmt(msg, "Error: List is not initialized (line: %d)\n", index->lineno);
                    term_write(msg);
                    unref_node(idx);
                    return NULL;
                }

                if (idx->value_type != SCRIPT_INT) {
                    char msg[64];
                    strfmt(msg, "Error: Index expects 'int' type (line: %d)\n", index->lineno);
                    term_write(msg);
                    unref_node(idx);
                    return NULL;
                }

                if (idx->literal.int_value >= 0 && idx->literal.int_value < (int)list->size) {
                    script_node_t *value = (script_node_t*)list_get(list, (size_t)idx->literal.int_value);
                    if (value) {
                        unref_node(idx);
                        return ref_node(value);
                    }
                } else {
                    unref_node(idx);
                    return g_null;
                }

                break;
            }
    }

    char msg[128];
    script_node_t *type = node_type_name(var->value);
    strfmt(msg, "Error: Cannot index a '%s' type (line: %d)\n", type->literal.str_value, index->lineno);
    term_write(msg);
    unref_node(type);
    return NULL;
}

static script_node_t *eval_expr(script_stmt_t *block, script_node_t *expr) {
    if (!expr) return NULL;

    switch (expr->node_type) {
        case SCRIPT_AST_LITERAL:
            if (expr->value_type == SCRIPT_ID) {
                char *name = expr->literal.str_value;

                script_var_t *var = env_unscoped_find_var(block, name);
                if (!var) {
                    char msg[64];
                    strfmt(msg, "Error: Undeclared \"%s\" (line: %d)\n", name, expr->lineno);
                    term_write(msg);
                    return NULL;
                }

                if (node_isprimitive(var->value))
                    return node_clone(var->value);

                return ref_node(var->value);
            }

            return node_clone(expr);
        case SCRIPT_AST_BINOP:
            return eval_binop(block, expr);
        case SCRIPT_AST_CALL:
            return eval_call(block, expr);
        case SCRIPT_AST_INDEX:
            return eval_index(block, expr);
    }

    char msg[64];
    strfmt(msg, "Error: Unsupported operation (line: %d)\n", expr->lineno);
    term_write(msg);
    return NULL;
}

static script_node_t *eval_declare(script_stmt_t *block, script_stmt_t *stmt) {
    if (!stmt) return NULL;

    script_var_t *var = env_find_var(block, stmt->var.name);
    if (var) {
        char msg[128];
        if (var->value->value_type == SCRIPT_FUNC)
            strfmt(msg, "Error: Function with the same name already defined in this scope (line: %d)\n", stmt->lineno);
        else
            strfmt(msg, "Error: Variable already defined in this scope (line: %d)\n", stmt->lineno);
        term_write(msg);
        free_stmt(stmt);
        return NULL;
    }

    script_node_t *value = node_null();
    env_set_var(block, stmt->var.name, value);

    return g_null;
}

static script_node_t *eval_define(script_stmt_t *block, script_stmt_t *stmt) {
    if (!stmt) return NULL;

    script_var_t *var = env_find_var(block, stmt->var.name);
    if (var) {
        char msg[128];
        if (var->value->value_type == SCRIPT_FUNC)
            strfmt(msg, "Error: Function with the same name already defined in this scope (line: %d)\n", stmt->lineno);
        else
            strfmt(msg, "Error: Variable already defined in this scope (line: %d)\n", stmt->lineno);
        term_write(msg);
        free_stmt(stmt);
        return NULL;
    }
    script_node_t *value = eval_expr(block, stmt->var.value);
    if (!value)
        return NULL;

    env_set_var(block, stmt->var.name, value);

    return g_null;
}

static script_node_t *eval_assign(script_stmt_t *block, script_stmt_t *stmt) {
    if (!stmt) return NULL;

    script_var_t *var = env_unscoped_find_var(block, stmt->var.name);
    if (!var) {
        char msg[64];
        strfmt(msg, "Error: Undeclared \"%s\" (line: %d)\n", stmt->var.name, stmt->lineno);
        term_write(msg);
        free_stmt(stmt);
        return NULL;
    }

    if (var->value->value_type == SCRIPT_FUNC) {
        char msg[64];
        strfmt(msg, "Error: Cannot assign values to a function (line: %d)\n", stmt->lineno);
        term_write(msg);
        free_stmt(stmt);
        return NULL;
    }

    script_node_t *value = eval_expr(block, stmt->var.value);
    if (!value)
        return NULL;

    script_stmt_t *scope = env_find_block(block, stmt->var.name);
    env_set_var(scope, stmt->var.name, value);

    return g_null;
}

static script_node_t *eval_func(script_stmt_t *block, script_stmt_t *stmt) {
    if (!stmt) return NULL;

    char *name = stmt->func.name->literal.str_value;

    script_var_t *var = env_find_var(block, name);
    if (var) {
        char msg[128];
        if (var->value->value_type == SCRIPT_FUNC)
            strfmt(msg, "Error: Function with the same name already defined in this scope (line: %d)\n", stmt->lineno);
        else
            strfmt(msg, "Error: Variable already defined in this scope (line: %d)\n", stmt->lineno);
        term_write(msg);
        free_stmt(stmt);
        return NULL;
    }
    script_stmt_t *func_block = stmt->func.block;
    func_block->parent = block;

    env_set_var_from_stmt(block, name, stmt);

    return g_null;
}

static script_eval_t *eval_block(script_stmt_t *block, script_stmt_t *stmt) {
    script_eval_t *eval = NULL;

    script_stmt_t *current = stmt->child;
    while (current) {
        eval = eval_statement(block, current);
        if (!eval)
            return NULL;

        if (eval->type == SCRIPT_EVAL_RETURN ||
            eval->type == SCRIPT_EVAL_BREAK ||
            eval->type == SCRIPT_EVAL_CONTINUE ||
            script_should_exit)
            return eval;

        free_eval(eval);
        eval = NULL;
        current = current->next;
    }

    eval = heap_alloc(sizeof(script_eval_t));
    eval->type = SCRIPT_EVAL_NONE;
    eval->node = NULL;

    return eval;
}

static script_eval_t *eval_if(script_stmt_t *block, script_stmt_t *stmt) {
    script_eval_t *eval = NULL;
    script_node_t *expr = eval_expr(block, stmt->if_stmt.expr);

    if (node_istrue(expr))
        eval = eval_statement(block, stmt->if_stmt.then_stmt);
    else if (stmt->if_stmt.else_stmt) {
        eval = eval_statement(block, stmt->if_stmt.else_stmt);
    }
    unref_node(expr);

    if (!eval) {
        eval = heap_alloc(sizeof(script_eval_t));
        eval->type = SCRIPT_EVAL_NONE;
        eval->node = NULL;
    }

    return eval;
}

static script_eval_t *eval_while(script_stmt_t *block, script_stmt_t *stmt) {
    script_eval_t *eval = NULL;
    script_stmt_t *scope = stmt_block(block);

    while (1) {
        script_node_t *expr = eval_expr(block, stmt->while_stmt.expr);
        int is_true = node_istrue(expr);
        unref_node(expr);

        if (!is_true)
            break;

        eval = eval_statement(scope, stmt->while_stmt.body);
        if (!eval) {
            free_stmt(scope);
            return NULL;
        }

        env_reset(scope->block.env);

        if (eval->type == SCRIPT_EVAL_RETURN || script_should_exit) {
            free_stmt(scope);
            return eval;
        }

        if (eval->type == SCRIPT_EVAL_BREAK) {
            free_eval(eval);
            eval = NULL;
            break;
        }

        if (eval->type == SCRIPT_EVAL_CONTINUE) {
            free_eval(eval);
            eval = NULL;
            continue;
        }

        free_eval(eval);
        eval = NULL;
    }

    free_stmt(scope);

    if (!eval) {
        eval = heap_alloc(sizeof(script_eval_t));
        eval->type = SCRIPT_EVAL_NONE;
        eval->node = NULL;
    }

    return eval;
}

static script_eval_t *eval_for(script_stmt_t *block, script_stmt_t *stmt) {
    script_eval_t *eval = NULL;
    script_stmt_t *scope = stmt_block(block);
    script_stmt_t *scopescope = stmt_block(scope);

    script_eval_t *env = eval_statement(scope, stmt->for_stmt.init);
    free_eval(env);

    while (1) {
        script_node_t *expr = eval_expr(scope, stmt->for_stmt.expr);
        int is_true = node_istrue(expr);
        unref_node(expr);

        if (!is_true)
            break;

        eval = eval_statement(scopescope, stmt->for_stmt.body);
        if (!eval) {
            free_stmt(scope);
            free_stmt(scopescope);
            return NULL;
        }
        env_reset(scopescope->block.env);

        if (eval->type == SCRIPT_EVAL_RETURN || script_should_exit) {
            free_stmt(scope);
            free_stmt(scopescope);
            return eval;
        }

        if (eval->type == SCRIPT_EVAL_BREAK) {
            free_eval(eval);
            eval = NULL;
            break;
        }

        if (eval->type == SCRIPT_EVAL_CONTINUE) {
            free_eval(eval);
            eval = NULL;
        }

        free_eval(eval);
        eval = NULL;

        env = eval_statement(scope, stmt->for_stmt.update);
        free_eval(env);
    }

    free_stmt(scope);
    free_stmt(scopescope);

    if (!eval) {
        eval = heap_alloc(sizeof(script_eval_t));
        eval->type = SCRIPT_EVAL_NONE;
        eval->node = NULL;
    }

    return eval;
}

static script_node_t *eval_include(script_stmt_t *block, script_stmt_t *stmt) {
    script_stmt_t *root = block;
    while (root->parent)
        root = root->parent;

    script_token_t *tokens = NULL;
    script_node_t *npath = stmt->include_stmt.path;

    char *path = heap_alloc(FILE_MAX_PATH + FILE_MAX_NAME);
    memcpy(path, npath->literal.str_value, npath->literal.str_size);

    if (!file_path_isfile(path))
        strfmt(path, "/system/modules/%s", npath->literal.str_value);

    for (size_t i = 0; i < script_module_paths->size; i++) {
        char *p = (char*)list_get(script_module_paths, i);

        if (!strcmp(p, path)) {
            heap_free(path);
            return g_null;
        }
    }

    int token_status = get_tokens(path, &tokens);
    if (token_status == 1) {
        char msg[128];
        strfmt(msg, "Error: File not found (line: %d)\n", stmt->lineno);
        term_write(msg);
        return NULL;
    }

    if (!tokens) {
        if (token_status == 2)
            return g_null;

        return NULL;
    }

    script_stmt_t *module = stmt_block(NULL);
    if(!load_runtime(module, tokens)) {
        while (tokens) {
            script_token_t *next = tokens->next;
            free_token(tokens);
            tokens = next;
        }
        free_stmt(module);
        return NULL;
    }

    free_eval(eval_block(root, module));

    while (tokens) {
        script_token_t *next = tokens->next;
        free_token(tokens);
        tokens = next;
    }

    list_push(script_modules, module);
    list_push(script_module_paths, path);
    return g_null;
}

static script_node_t *eval_delete(script_stmt_t *block, script_stmt_t *stmt) {
    char *name = stmt->delete_stmt.name->literal.str_value;

    script_var_t *var = env_unscoped_find_var(block, name);
    if (var)
        env_remove_var(block, var);
    else {
        char msg[128];
        strfmt(msg, "Error: Undeclared \"%s\" (line: %d)\n", name, stmt->lineno);
        term_write(msg);
        return NULL;
    }
    return g_null;
}

static script_eval_t *eval_statement(script_stmt_t *block, script_stmt_t *stmt) {
    if (!stmt) return NULL;

    script_eval_t *eval = NULL;
    switch (stmt->type) {
        case SCRIPT_STMT_EXPR:
            {
                script_node_t *node = eval_expr(block, stmt->expr.node);
                if (!node)
                    break;

                eval = heap_alloc(sizeof(script_eval_t));
                eval->type = SCRIPT_EVAL_NONE;
                eval->node = node;
                break;
            }
        case SCRIPT_STMT_RETURN:
            {
                script_node_t *node = eval_expr(block, stmt->expr.node);
                if (!node)
                    break;

                eval = heap_alloc(sizeof(script_eval_t));
                eval->type = SCRIPT_EVAL_RETURN;
                eval->node = node;
                break;
            }
        case SCRIPT_STMT_BREAK:
            eval = heap_alloc(sizeof(script_eval_t));
            eval->type = SCRIPT_EVAL_BREAK;
            eval->node = NULL;
            break;
        case SCRIPT_STMT_CONTINUE:
            eval = heap_alloc(sizeof(script_eval_t));
            eval->type = SCRIPT_EVAL_CONTINUE;
            eval->node = NULL;
            break;
        case SCRIPT_STMT_DECLARE:
            {
                script_node_t *node = eval_declare(block, stmt);
                if (!node)
                    return NULL;

                eval = heap_alloc(sizeof(script_eval_t));
                eval->type = SCRIPT_EVAL_NONE;
                eval->node = node;
                break;
            }
        case SCRIPT_STMT_DEFINE:
            {
                script_node_t *node = eval_define(block, stmt);
                if (!node)
                    return NULL;

                eval = heap_alloc(sizeof(script_eval_t));
                eval->type = SCRIPT_EVAL_NONE;
                eval->node = node;
                break;
            }
        case SCRIPT_STMT_ASSIGN:
            {
                script_node_t *node = eval_assign(block, stmt);
                if (!node)
                    return NULL;

                eval = heap_alloc(sizeof(script_eval_t));
                eval->type = SCRIPT_EVAL_NONE;
                eval->node = node;
                break;
            }
        case SCRIPT_STMT_FUNC:
            {
                script_node_t *node = eval_func(block, stmt);
                if (!node)
                    return NULL;

                eval = heap_alloc(sizeof(script_eval_t));
                eval->type = SCRIPT_EVAL_NONE;
                eval->node = node;
                break;
            }
        case SCRIPT_STMT_INCLUDE:
            {
                script_node_t *node = eval_include(block, stmt);
                if (!node)
                    return NULL;

                eval = heap_alloc(sizeof(script_eval_t));
                eval->type = SCRIPT_EVAL_NONE;
                eval->node = node;
                break;
            }
        case SCRIPT_STMT_DELETE:
            {
                script_node_t *node = eval_delete(block, stmt);
                if (!node)
                    return NULL;

                eval = heap_alloc(sizeof(script_eval_t));
                eval->type = SCRIPT_EVAL_NONE;
                eval->node = node;
                break;
            }
        case SCRIPT_STMT_BLOCK:
            return eval_block(block, stmt);
        case SCRIPT_STMT_IF:
            return eval_if(block, stmt);
        case SCRIPT_STMT_WHILE:
            return eval_while(block, stmt);
        case SCRIPT_STMT_FOR:
            return eval_for(block, stmt);
    }

    return eval;
}

static void block_add_statement(script_stmt_t *block, script_stmt_t *stmt) {
    if (block->type != SCRIPT_STMT_BLOCK)
        return;

    stmt->parent = block;
    stmt->next = NULL;

    if (!block->child) {
        block->child = stmt;
        block->tail = stmt;
    } else {
        block->tail->next = stmt;
        block->tail = stmt;
    }
}

static script_runtime_t *get_runtime() {
    script_runtime_t *rt = heap_alloc(sizeof(script_runtime_t));
    rt->main = stmt_block(NULL);

    if (!g_null)
        g_null = node_null();
    if (!g_true)
        g_true = node_true();
    if (!g_false)
        g_false = node_false();

    return rt;
}

static int load_runtime(script_stmt_t *block, script_token_t *tokens) {
    while (tokens) {
        script_stmt_t *stmt = parse_statement(&tokens);
        if (!stmt)
            return 0;

        block_add_statement(block, stmt);
    }

    return 1;
}

static void free_runtime(script_runtime_t *rt) {
    free_stmt(rt->main);
    heap_free(rt);

    if (g_null) {
        heap_free(g_null);
        g_null = NULL;
    }
    if (g_true) {
        heap_free(g_true);
        g_true = NULL;
    }
    if (g_false) {
        heap_free(g_false);
        g_false = NULL;
    }
}

static int get_tokens(const char *path, script_token_t **tokens) {
    fio_t *file = fio_open(path, FIO_READ);
    if (!file)
        return 1;

    file_node_t node;
    file_node(file->file, &node);

    if (node.size == 0)
        return 2;

    script_token_t *token_head = tokenize(file);
    if (!token_head) {
        fio_close(file);
        return 3;
    }
    fio_close(file);

    *tokens = token_head;
    return 0;
}

void script_handle_type(uint8_t scancode) {
    if (keyboard_ctrl) {
        char c = scancode_to_char(scancode);

        if (!keyboard_shift) {
            if (c == 'c') {
                script_should_exit = 1;

                if (term_input_buffer) {
                    term_input_buffer = NULL;

                    term_input_cursor = 0;
                    term_input_pos = 0;
                    term_input[0] = '\0';
                }
            }
        }
    }
}

void script_run(const char *path, int argc, char *argv[]) {
    int kmode = keyboard_mode;
    keyboard_mode = KEYBOARD_MODE_SCRIPT;

    script_exit = 0;
    script_should_exit = 0;

    script_argc = argc;
    script_argv = argv;

    script_printfg = term_fg;
    script_printbg = term_bg;

    script_screen_buffer = NULL;
    script_screen_buffer_size = 0;

    script_modules = NULL;
    script_module_paths = NULL;
    script_modules = heap_alloc(sizeof(list_t));
    script_module_paths = heap_alloc(sizeof(list_t));
    list_init(script_modules);
    list_init(script_module_paths);

    script_token_t *token_head = NULL;
    script_token_t *tokens = NULL;
    script_runtime_t *rt = NULL;

    if (get_tokens(path, &token_head)) {
        script_exit = 1;
        goto cleanup;
    }

    tokens = token_head;
    rt = get_runtime();
    int status = load_runtime(rt->main, token_head);
    if (!status)
        goto cleanup;
    free_eval(eval_block(rt->main, rt->main));

cleanup:
    while (tokens) {
        script_token_t *next = tokens->next;
        free_token(tokens);
        tokens = next;
    }

    if (script_screen_buffer) {
        heap_free(script_screen_buffer);
        script_screen_buffer = NULL;
        script_screen_buffer_size = 0;
    }

    while (script_modules->size)
        free_stmt((script_stmt_t*)list_pop(script_modules));
    list_free(script_modules);

    while (script_module_paths->size)
        heap_free((char*)list_pop(script_module_paths));
    list_free(script_module_paths);

    if (rt)
        free_runtime(rt);

    keyboard_mode = kmode;
}