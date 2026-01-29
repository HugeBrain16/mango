#include "script.h"
#include "heap.h"
#include "fio.h"
#include "string.h"
#include "terminal.h"
#include "color.h"
#include "command.h"

static script_runtime *runtime_init() {
    script_runtime *runtime = heap_alloc(sizeof(script_runtime));
    runtime->main_token = heap_alloc(sizeof(script_token_t));
    script_token_t *token = runtime->main_token;
    token->value = NULL;
    token->next = NULL;

    runtime->main_node = heap_alloc(sizeof(script_node_t));
    script_node_t *node = runtime->main_node;
    node->type = SCRIPT_FUNC;
    node->name = (char*) heap_alloc(SCRIPT_MAX_TOKENLENGTH);
    strcpy(node->name, "main");
    node->parent = NULL;
    node->head = NULL;
    node->next = NULL;
    node->value = NULL;
    return runtime;
}

static void runtime_free(script_runtime *runtime) {
    if (!runtime) return;

    script_node_t *node = runtime->main_node->head;
    while (node) {
        script_node_t *current = node;
        node = current->next;
        if (current->name)
            heap_free(current->name);
        heap_free(current);
    }

    if (runtime->main_node) {
        if (runtime->main_node->name)
            heap_free(runtime->main_node->name);
        heap_free(runtime->main_node);
    }

    script_token_t *token = runtime->main_token;
    while (token) {
        script_token_t *current = token;
        token = current->next;
        if (current->value)
            heap_free(current->value);
        heap_free(current);
    }

    heap_free(runtime);
}

static script_token_t *create_token(uint8_t type, size_t lineno) {
    script_token_t *token = heap_alloc(sizeof(script_token_t));
    token->next = NULL;
    token->value = (char*) heap_alloc(SCRIPT_MAX_TOKENLENGTH);
    token->value[0] = '\0';
    token->type = type;
    token->lineno = lineno;
    return token;
}

static int lex_number(fio_t *file, char *c, size_t *lineno, script_token_t **current_token) {
    script_token_t *token = create_token(SCRIPT_TOKEN_NUMBER, *lineno);
    size_t i = 0;

    if (*c == '-') {
        token->value[i++] = *c;
        *c = fio_getc(file);
        if (!isdigit(*c)) {
            char msg[64];
            strfmt(msg, "Error: Expected digit after '-' (line: %d)\n", *lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            heap_free(token->value);
            heap_free(token);
            return 0;
        }
    }

    do {
        if (i == SCRIPT_MAX_TOKENLENGTH - 1) {
            char msg[64];
            strfmt(msg, "Error: Token is too long (line: %d)\n", *lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            heap_free(token->value);
            heap_free(token);
            return 0;
        }
        token->value[i++] = *c;
        *c = fio_getc(file);
    } while (isdigit(*c));

    token->value[i] = '\0';

    if (isalpha(*c)) {
        char msg[64];
        strfmt(msg, "Error: Unexpected char (line: %d): \"%c\"\n", *lineno, *c);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        heap_free(token->value);
        heap_free(token);
        return 0;
    }

    (*current_token)->next = token;
    *current_token = token;
    return 1;
}

static int lex_identifier(fio_t *file, char *c, size_t *lineno, script_token_t **current_token) {
    script_token_t *token = create_token(SCRIPT_TOKEN_IDENTIFIER, *lineno);
    size_t i = 0;

    do {
        if (i == SCRIPT_MAX_TOKENLENGTH - 1) {
            char msg[64];
            strfmt(msg, "Error: Token is too long (line: %d)\n", *lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            heap_free(token->value);
            heap_free(token);
            return 0;
        }
        token->value[i++] = *c;
        *c = fio_getc(file);
    } while (isalpha(*c) || isdigit(*c) || *c == '_');

    token->value[i] = '\0';

    if (!strcmp(token->value, "let")) 
        token->type = SCRIPT_TOKEN_LET;
    else if (!strcmp(token->value, "print"))
        token->type = SCRIPT_TOKEN_PRINT;
    else if (!strcmp(token->value, "println"))
        token->type = SCRIPT_TOKEN_PRINTLN;
    else if (!strcmp(token->value, "exec"))
        token->type = SCRIPT_TOKEN_EXEC;

    (*current_token)->next = token;
    *current_token = token;
    return 1;
}

static int lex_string(fio_t *file, char *c, size_t *lineno, script_token_t **current_token) {
    script_token_t *token = create_token(SCRIPT_TOKEN_STRING, *lineno);
    size_t i = 0;

    *c = fio_getc(file);
    while (*c != '"' && *c != '\0') {
        if (i == SCRIPT_MAX_TOKENLENGTH - 1) {
            char msg[64];
            strfmt(msg, "Error: Token is too long (line: %d)\n", *lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            heap_free(token->value);
            heap_free(token);
            return 0;
        }
        token->value[i++] = *c;
        *c = fio_getc(file);
    }
    token->value[i] = '\0';

    if (*c != '"') {
        char msg[64];
        strfmt(msg, "Error: Unclosed string (line: %d)\n", *lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        heap_free(token->value);
        heap_free(token);
        return 0;
    }

    (*current_token)->next = token;
    *current_token = token;
    return 1;
}

static int lex_operator(char c, size_t *lineno, script_token_t **current_token) {
    script_token_t *token = create_token(SCRIPT_TOKEN_END, *lineno);

    switch (c) {
        case '(':
            token->type = SCRIPT_TOKEN_LPAREN;
            break;
        case ')':
            token->type = SCRIPT_TOKEN_RPAREN;
            break;
        case '=':
            token->type = SCRIPT_TOKEN_EQUAL;
            break;
        case ';':
            token->type = SCRIPT_TOKEN_END;
            break;
        default: {
                     char msg[64];
                     strfmt(msg, "Error: Illegal token (line: %d): \"%c\"\n", *lineno, c);
                     term_write(msg, COLOR_WHITE, COLOR_BLACK);
                     heap_free(token->value);
                     heap_free(token);
                     return 0;
                 }
    }

    (*current_token)->next = token;
    *current_token = token;
    return 1;
}

static int tokenize(fio_t *file, script_runtime *runtime) {
    char c = fio_getc(file);
    size_t lineno = 1;
    script_token_t *current_token = runtime->main_token;

    while (c != '\0') {
        if (c == '\n') {
            lineno++;
            c = fio_getc(file);
            continue;
        }

        if (c == ' ') {
            c = fio_getc(file);
            continue;
        }

        if (isdigit(c) || c == '-') {
            if (!lex_number(file, &c, &lineno, &current_token))
                return 0;
        } else if (isalpha(c)) {
            if (!lex_identifier(file, &c, &lineno, &current_token))
                return 0;
        } else if (c == '"') {
            if (!lex_string(file, &c, &lineno, &current_token))
                return 0;
            c = fio_getc(file);
        } else {
            if (!lex_operator(c, &lineno, &current_token))
                return 0;
            c = fio_getc(file);
        }
    }

    return 1;
}

static script_node_t *create_node(const char *name, script_node_t *parent) {
    script_node_t *node = heap_alloc(sizeof(script_node_t));
    node->name = (char*) heap_alloc(SCRIPT_MAX_TOKENLENGTH);
    strcpy(node->name, name);
    node->parent = parent;
    node->head = NULL;
    node->next = NULL;
    node->value = NULL;
    return node;
}

static void add_node_to_scope(script_node_t *scope, script_node_t *node) {
    if (scope->head == NULL) {
        scope->head = node;
    } else {
        script_node_t *current = scope->head;
        while (current->next)
            current = current->next;
        current->next = node;
    }
}

static script_node_t *find_node_in_scope(script_node_t *scope, const char *name) {
    script_node_t *current = scope->head;
    while (current) {
        if (!strcmp(current->name, name))
            return current;
        current = current->next;
    }
    return NULL;
}

static int parse_let_statement(script_token_t **token, script_node_t *scope) {
    size_t lineno = (*token)->lineno;
    script_token_t *var = (*token)->next;
    if (!var || var->type != SCRIPT_TOKEN_IDENTIFIER) {
        char msg[64];
        strfmt(msg, "Error: Expected variable name (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    script_token_t *equal = var->next;
    if (equal->type == SCRIPT_TOKEN_END) {
        script_node_t *ref = find_node_in_scope(scope, var->value);
        if (ref) {
            ref->type = SCRIPT_NULL;
            ref->value = NULL;
        } else {
            script_node_t *new_node = create_node(var->value, scope);
            new_node->type = SCRIPT_NULL;
            new_node->value = NULL;
            add_node_to_scope(scope, new_node);
        }

        *token = equal->next;
        return 1;
    }

    if (!equal || equal->type != SCRIPT_TOKEN_EQUAL) {
        char msg[64];
        strfmt(msg, "Error: Expected '=' (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    script_token_t *value = equal->next;
    if (!value || (value->type != SCRIPT_TOKEN_STRING && 
                value->type != SCRIPT_TOKEN_NUMBER && 
                value->type != SCRIPT_TOKEN_IDENTIFIER)) {
        char msg[64];
        strfmt(msg, "Error: Expected value (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    script_node_t *new_node = create_node(var->value, scope);

    if (value->type == SCRIPT_TOKEN_STRING) {
        new_node->type = SCRIPT_STR;
        new_node->value = (void *) value->value;
    } else if (value->type == SCRIPT_TOKEN_NUMBER) {
        new_node->type = SCRIPT_INT;
        new_node->value = (void *) intstr(value->value);
    } else if (value->type == SCRIPT_TOKEN_IDENTIFIER) {
        script_node_t *ref = find_node_in_scope(scope, value->value);
        if (!ref) {
            char msg[64];
            strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n", value->value, lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            heap_free(new_node->name);
            heap_free(new_node);
            return 0;
        }
        new_node->type = ref->type;
        new_node->value = ref->value;
    }

    add_node_to_scope(scope, new_node);

    script_token_t *end = value->next;
    if (!end || end->type != SCRIPT_TOKEN_END) {
        char msg[64];
        strfmt(msg, "Error: Expected ';' (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    *token = end->next;
    return 1;
}

static int parse_assignment(script_token_t **token, script_node_t *scope) {
    script_token_t *id_token = *token;
    size_t lineno = id_token->lineno;
    script_token_t *equal = id_token->next;

    if (!equal || equal->type != SCRIPT_TOKEN_EQUAL) {
        char msg[64];
        strfmt(msg, "Error: Invalid syntax (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    script_token_t *value = equal->next;
    if (!value || (value->type != SCRIPT_TOKEN_STRING && 
                value->type != SCRIPT_TOKEN_NUMBER && 
                value->type != SCRIPT_TOKEN_IDENTIFIER)) {
        char msg[64];
        strfmt(msg, "Error: Expected value (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    script_node_t *node = find_node_in_scope(scope, id_token->value);
    if (!node) {
        char msg[64];
        strfmt(msg, "Error: Assignee undefined \"%s\" (line: %d)\n", id_token->value, lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    if (value->type == SCRIPT_TOKEN_STRING) {
        node->type = SCRIPT_STR;
        node->value = (void *) value->value;
    } else if (value->type == SCRIPT_TOKEN_NUMBER) {
        node->type = SCRIPT_INT;
        node->value = (void *) intstr(value->value);
    } else if (value->type == SCRIPT_TOKEN_IDENTIFIER) {
        script_node_t *ref = find_node_in_scope(scope, value->value);
        if (!ref) {
            char msg[64];
            strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n", value->value, lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            return 0;
        }
        node->type = ref->type;
        node->value = ref->value;
    }

    script_token_t *end = value->next;
    if (!end || end->type != SCRIPT_TOKEN_END) {
        char msg[64];
        strfmt(msg, "Error: Expected ';' (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    *token = end->next;
    return 1;
}

static int parse_print_statement(script_token_t **token, script_node_t *scope) {
    size_t lineno = (*token)->lineno;

    script_token_t *value = (*token)->next;
    if (!value || (value->type != SCRIPT_TOKEN_STRING && 
                value->type != SCRIPT_TOKEN_NUMBER && 
                value->type != SCRIPT_TOKEN_IDENTIFIER)) {
        char msg[64];
        strfmt(msg, "Error: Expected value (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    if (value->type == SCRIPT_TOKEN_STRING || value->type == SCRIPT_TOKEN_NUMBER) {
        term_write(value->value, COLOR_WHITE, COLOR_BLACK);
    } else if (value->type == SCRIPT_TOKEN_IDENTIFIER) {
        script_node_t *ref = find_node_in_scope(scope, value->value);
        if (!ref) {
            char msg[64];
            strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n", value->value, lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            return 0;
        }

        if (ref->type == SCRIPT_STR)
            term_write(ref->value, COLOR_WHITE, COLOR_BLACK);
        else if (ref->type == SCRIPT_INT) {
            char buff[16];
            strint(buff, (int) ref->value);
            term_write(buff, COLOR_WHITE, COLOR_BLACK);
        }
    }

    script_token_t *end = value->next;
    if (!end || end->type != SCRIPT_TOKEN_END) {
        char msg[64];
        strfmt(msg, "Error: Expected ';' (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    *token = end->next;
    return 1;
}

static int parse_println_statement(script_token_t **token, script_node_t *scope) {
    size_t lineno = (*token)->lineno;

    script_token_t *value = (*token)->next;
    if (!value || (value->type != SCRIPT_TOKEN_STRING && 
                value->type != SCRIPT_TOKEN_NUMBER && 
                value->type != SCRIPT_TOKEN_IDENTIFIER)) {
        char msg[64];
        strfmt(msg, "Error: Expected value (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    if (value->type == SCRIPT_TOKEN_STRING || value->type == SCRIPT_TOKEN_NUMBER) {
        term_write(value->value, COLOR_WHITE, COLOR_BLACK);
    } else if (value->type == SCRIPT_TOKEN_IDENTIFIER) {
        script_node_t *ref = find_node_in_scope(scope, value->value);
        if (!ref) {
            char msg[64];
            strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n", value->value, lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            return 0;
        }

        if (ref->type == SCRIPT_STR)
            term_write(ref->value, COLOR_WHITE, COLOR_BLACK);
        else if (ref->type == SCRIPT_INT) {
            char buff[16];
            strint(buff, (int) ref->value);
            term_write(buff, COLOR_WHITE, COLOR_BLACK);
        }
    }

    term_write("\n", COLOR_WHITE, COLOR_BLACK);

    script_token_t *end = value->next;
    if (!end || end->type != SCRIPT_TOKEN_END) {
        char msg[64];
        strfmt(msg, "Error: Expected ';' (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    *token = end->next;
    return 1;
}

static int parse_exec_statement(script_token_t **token, script_node_t *scope) {
    size_t lineno = (*token)->lineno;

    script_token_t *value = (*token)->next;
    if (!value || (value->type != SCRIPT_TOKEN_STRING && 
                value->type != SCRIPT_TOKEN_NUMBER && 
                value->type != SCRIPT_TOKEN_IDENTIFIER)) {
        char msg[64];
        strfmt(msg, "Error: Expected value (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    if (value->type == SCRIPT_TOKEN_STRING) {
        command_handle(value->value, 0);
    } else if (value->type == SCRIPT_TOKEN_IDENTIFIER) {
        script_node_t *ref = find_node_in_scope(scope, value->value);
        if (!ref) {
            char msg[64];
            strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n", value->value, lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            return 0;
        }

        if (ref->type == SCRIPT_STR)
            command_handle(ref->value, 0);
        else if (ref->type == SCRIPT_INT) {
            char msg[64];
            strfmt(msg, "Error: Expected string value (line: %d)\n", lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            return 0;
        }
    } else {
        char msg[64];
        strfmt(msg, "Error: Expected string value (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    script_token_t *end = value->next;
    if (!end || end->type != SCRIPT_TOKEN_END) {
        char msg[64];
        strfmt(msg, "Error: Expected ';' (line: %d)\n", lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return 0;
    }

    *token = end->next;
    return 1;
}

static int parse(script_runtime *runtime) {
    script_node_t *current_node = runtime->main_node;
    script_token_t *token = runtime->main_token->next;

    while (token) {
        if (token->type == SCRIPT_TOKEN_LET) {
            if (!parse_let_statement(&token, current_node))
                return 0;
        } else if (token->type == SCRIPT_TOKEN_IDENTIFIER) {
            if (!parse_assignment(&token, current_node))
                return 0;
        } else if (token->type == SCRIPT_TOKEN_PRINT) {
            if (!parse_print_statement(&token, current_node))
                return 0;
        } else if (token->type == SCRIPT_TOKEN_PRINTLN) {
            if (!parse_println_statement(&token, current_node))
                return 0;
        } else if (token->type == SCRIPT_TOKEN_EXEC) {
            if (!parse_exec_statement(&token, current_node))
                return 0;
        } else {
            token = token->next;
        }
    }

    return 1;
}

void script_run(const char *path) {
    fio_t *file = fio_open(path, FIO_READ);
    if (!file) return;

    script_runtime *runtime = runtime_init();

    if (!tokenize(file, runtime)) {
        fio_close(file);
        runtime_free(runtime);
        return;
    }

    fio_close(file);

    if (!parse(runtime)) {
        runtime_free(runtime);
        return;
    }

    runtime_free(runtime);
}
