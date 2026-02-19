#include "script.h"
#include "heap.h"
#include "fio.h"
#include "string.h"
#include "terminal.h"
#include "color.h"
#include "command.h"

static void free_token(script_token_t *token);
static void free_node(script_node_t *node);
static void free_stmt(script_stmt_t *stmt);
static void free_var(script_var_t *var);
static void free_env(script_env_t *env);

static void block_add_statement(script_stmt_t *block, script_stmt_t *stmt);

static script_stmt_t *parse_statement(script_token_t **token);
static script_node_t *parse_expr(script_token_t **token);
static script_node_t *parse_comparison(script_token_t **token);
static script_node_t *parse_addsub(script_token_t **token);
static script_node_t *parse_term(script_token_t **token);
static script_node_t *parse_call(script_token_t **token);
static script_node_t *parse_factor(script_token_t **token);

static script_node_t *eval_binop(script_stmt_t *block, script_node_t *binop);
static script_node_t *eval_call(script_stmt_t *block, script_node_t *call);
static script_node_t *eval_expr(script_stmt_t *block, script_node_t *expr);

static script_eval_t *eval_block(script_stmt_t *block, script_stmt_t *stmt);
static script_eval_t *eval_if(script_stmt_t *block, script_stmt_t *stmt);
static script_eval_t *eval_statement(script_stmt_t *block, script_stmt_t *stmt);

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

    if (*c == '-') {
        token->value[i++] = *c;
        *c = fio_getc(file);
    }

    do {
        if (i == token->size * SCRIPT_SIZE_TOKEN - 1) {
            token->size++;
            token->value = heap_realloc(token->value, token->size * SCRIPT_SIZE_TOKEN);
        }

        token->value[i++] = *c;

        if (*c == '.')
            is_float++;

        *c = fio_getc(file);
    } while (isdigit(*c) || (*c == '.' && is_float < 2));

    token->value[i] = '\0';

    if (isalpha(*c) || is_float == 2) {
        char msg[64];
        strfmt(msg, "Error: Unexpected char \"%c\" (line: %d)\n", *lineno, *c);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
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

    return token;
}

static script_token_t *lex_string(fio_t *file, char *c, size_t *lineno) {
    script_token_t *token = create_token(SCRIPT_TOKEN_STRING, *lineno);
    size_t i = 0;

    *c = fio_getc(file);
    while (*c != '"' && *c != '\0') {
        if (i == token->size * SCRIPT_SIZE_TOKEN - 1) {
            token->size++;
            token->value = heap_realloc(token->value, token->size * SCRIPT_SIZE_TOKEN);
        }

        token->value[i++] = *c;
        *c = fio_getc(file);
    }
    token->value[i] = '\0';

    if (*c != '"') {
        char msg[32];
        strfmt(msg, "Error: Unclosed string (line: %d)\n", *lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
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
        default: {
                     char msg[32];
                     strfmt(msg, "Error: Illegal token (line: %d): \"%c\"\n", *lineno, c);
                     term_write(msg, COLOR_WHITE, COLOR_BLACK);
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
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            free_token(token);
            return NULL;
        }
    }

    *c = fio_getc(file);
    *c = fio_getc(file);

    return token;
}

static script_token_t *tokenize(fio_t *file) {
    char c = fio_getc(file);
    size_t lineno = 1;
    script_token_t *head = NULL;
    script_token_t *current = NULL;

    while (c != '\0') {
        if (c == '\n') {
            lineno++;
            c = fio_getc(file);
            continue;
        }

        if (c == ' ' || c == '\t') {
            c = fio_getc(file);
            continue;
        }

        script_token_t *token = NULL;

        if (isdigit(c) || (c == '-' && isdigit(fio_peek(file)))) {
            token = lex_number(file, &c, &lineno);
        } else if (isalpha(c)) {
            token = lex_identifier(file, &c, &lineno);
        } else if (c == '"') {
            token = lex_string(file, &c, &lineno);
            if (token) c = fio_getc(file);
        } else if (c == '=' && fio_peek(file) == '=') {
            token = lex_equaloperator(file, &c, &lineno);
        } else if (c == '!' && fio_peek(file) == '=') {
            token = lex_equaloperator(file, &c, &lineno);
        } else if (c == '>' && fio_peek(file) == '=') {
            token = lex_equaloperator(file, &c, &lineno);
        } else if (c == '<' && fio_peek(file) == '=') {
            token = lex_equaloperator(file, &c, &lineno);
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
    switch (var->type) {
        case SCRIPT_STR:
            heap_free(var->str_value);
            break;
        case SCRIPT_FUNC:
            free_stmt(var->func);
            break;
    }

    heap_free(var);
}

static void free_env(script_env_t *env) {
    script_var_t *var = env->var_head;
    while (var) {
        script_var_t *next = var->next;
        free_var(var);
        var = next;
    }

    heap_free(env);
}

static script_var_t *env_find_var(script_stmt_t *block, const char *name) {
    script_env_t *env = block->block.env;

    script_var_t *var = env->var_head;
    while (var) {
        if (!strcmp(var->name, name))
            return var;
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

static script_node_t *env_nodeify_var(script_stmt_t *block, script_node_t *node) {
    script_var_t *var = env_unscoped_find_var(block, node->literal.str_value);
    if (!var) return NULL;

    script_node_t *nodeified = heap_alloc(sizeof(script_node_t));
    nodeified->node_type = SCRIPT_AST_LITERAL;
    nodeified->lineno = node->lineno;
    nodeified->value_type = var->type;

    switch (var->type) {
        case SCRIPT_INT:
            nodeified->literal.int_value = var->int_value;
            break;
        case SCRIPT_FLOAT:
            nodeified->literal.float_value = var->float_value;
            break;
        case SCRIPT_STR:
            nodeified->literal.str_value = heap_alloc(var->str_size);
            nodeified->literal.str_size = var->str_size;
            memcpy(nodeified->literal.str_value, var->str_value, var->str_size);
            break;
    };
    return nodeified;
}

static void env_append_var(script_stmt_t *block, script_var_t *var) {
    script_env_t *env = block->block.env;

    if (!env->var_head)
        env->var_head = var;
    else {
        script_var_t *current = env->var_head;
        while (current->next)
            current = current->next;
        current->next = var;
    }
}

static script_var_t *env_new_var(const char *name) {
    script_var_t *var = heap_alloc(sizeof(script_var_t));
    var->name = heap_alloc(strlen(name) + 1);
    strcpy(var->name, name);
    var->next = NULL;

    return var;
}

static void env_set_var(script_stmt_t *block, const char *name, script_node_t *value) {
    if (value->node_type != SCRIPT_AST_LITERAL) return;

    script_var_t *var = env_find_var(block, name);
    if (!var) {
        var = env_new_var(name);
        env_append_var(block, var);
    } else {
        if (var->type == SCRIPT_STR)
            heap_free(var->str_value);
    }

    var->type = value->value_type;
    switch (value->value_type) {
        case SCRIPT_INT:
            var->int_value = value->literal.int_value;
            break;
        case SCRIPT_FLOAT:
            var->float_value = value->literal.float_value;
            break;
        case SCRIPT_STR:
            var->str_value = heap_alloc(value->literal.str_size);
            var->str_size = value->literal.str_size;
            memcpy(var->str_value, value->literal.str_value, value->literal.str_size);
            break;
    }
}

static void env_set_var_from_stmt(script_stmt_t *block, const char *name, script_stmt_t *value) {
    script_var_t *var = env_find_var(block, name);
    if (!var) {
        var = env_new_var(name);
        env_append_var(block, var);
    } else {
        if (var->type == SCRIPT_STR)
            heap_free(var->str_value);
    }

    switch (value->type) {
        case SCRIPT_STMT_FUNC:
            var->type = SCRIPT_FUNC;
            var->func = value;
            break;
    }
}

static script_node_t *node_null() {
    script_node_t *node = heap_alloc(sizeof(script_node_t));
    node->node_type = SCRIPT_AST_LITERAL;
    node->lineno = 0;
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

    node = heap_alloc(sizeof(script_node_t));
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

        node->literal.str_size = size;
        *value = heap_alloc(size);
        memcpy(*value, token->value, size);
    }

    return node;
}

static script_node_t *node_binop(uint8_t op, script_node_t *left, script_node_t *right) {
    script_node_t *node = heap_alloc(sizeof(script_node_t));
    node->node_type = SCRIPT_AST_BINOP;
    node->value_type = SCRIPT_NULL;
    node->lineno = left->lineno;

    node->binop.op = op;
    node->binop.left = left;
    node->binop.right = right;

    return node;
}

static script_node_t *node_call(script_node_t *func, script_node_t **argv, size_t argc) {
    script_node_t *node = heap_alloc(sizeof(script_node_t));
    node->node_type = SCRIPT_AST_CALL;
    node->value_type = SCRIPT_NULL;
    node->lineno = func->lineno;

    node->call.func = func;
    node->call.argv = argv;
    node->call.argc = argc;

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

static void free_node(script_node_t *node) {
    if (!node) return;

    switch (node->node_type) {
        case SCRIPT_AST_LITERAL:
            if (node->value_type == SCRIPT_STR)
                heap_free(node->literal.str_value);
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
    }

    heap_free(node);
}

static void free_stmt(script_stmt_t *stmt) {
    switch (stmt->type) {
        case SCRIPT_STMT_DEFINE:
        case SCRIPT_STMT_DECLARE:
            heap_free(stmt->var.name);
            free_node(stmt->var.value);
            break;
        case SCRIPT_STMT_EXPR:
            free_node(stmt->expr.node);
            break;
        case SCRIPT_STMT_BLOCK:
            free_env(stmt->block.env);
            break;
        case SCRIPT_STMT_FUNC:
            free_node(stmt->func.name);
            for (size_t i = 0; i < stmt->func.params_count; i++)
                free_node(stmt->func.params[i]);
            heap_free(stmt->func.params);

            if (stmt->func.block)
                free_stmt(stmt->func.block);
            break;
        case SCRIPT_STMT_RETURN:
            free_node(stmt->expr.node);
            break;
        case SCRIPT_STMT_IF:
            free_node(stmt->if_stmt.expr);
            free_stmt(stmt->if_stmt.then_stmt);
            if (stmt->if_stmt.else_stmt)
                free_stmt(stmt->if_stmt.else_stmt);
    }
}

static script_node_t *parse_factor(script_token_t **token) {
    if (!*token) return NULL;

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
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            free_node(node);
            return NULL;
        }

        *token = (*token)->next;
        return node;
    }

    char msg[64];
    strfmt(msg, "Error: expected value, got \"%s\" (line: %d)\n", (*token)->value, (*token)->lineno);
    term_write(msg, COLOR_WHITE, COLOR_BLACK);
    return NULL;
}

static script_node_t *parse_call(script_token_t **token) {
    script_node_t *node = parse_factor(token);
    if (!node || !*token) return node;

    if ((*token)->type != SCRIPT_TOKEN_LPAREN)
        return node;

    *token = (*token)->next;

    size_t argc = 0;
    script_node_t **argv = NULL;

    if ((*token)->type != SCRIPT_TOKEN_RPAREN) {
        while (1) {
            script_node_t *arg = parse_expr(token);
            if (!arg) {
                for (size_t i = 0; i < argc; i++)
                    free_node(argv[i]);
                heap_free(argv);
                free_node(node);
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
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return NULL;
    }

    *token = (*token)->next;

    return node_call(node, argv, argc);
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
            free_node(node);
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
            free_node(node);
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
            free_node(node);
            return NULL;
        }

        node = node_binop(op, node, right);
    }

    return node;
}

static script_node_t *parse_expr(script_token_t **token) {
    return parse_comparison(token);
}

static script_stmt_t *parse_declare(script_token_t **token) {
    if (!*token) return NULL;

    script_node_t *name = parse_factor(token);
    if (!name) return NULL;

    if (name->value_type != SCRIPT_ID) {
        char msg[64];
        strfmt(msg, "Error: expected identifier (line: %d)\n", name->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(name);
        return NULL;
    }

    if (!*token) {
        term_write("Error: unexpected eof\n", COLOR_WHITE, COLOR_BLACK);
        free_node(name);
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
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
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
    return stmt_return(parse_expr(token));
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
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
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
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(name);
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
                    term_write(msg, COLOR_WHITE, COLOR_BLACK);

                    for (size_t i = 0; i < params_count; i++)
                        free_node(params[i]);
                    heap_free(params);
                    free_node(name);
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
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(name);
        return NULL;
    }

    if (!*token || (*token)->type != SCRIPT_TOKEN_RPAREN) {
        char msg[64];
        strfmt(msg, "Error: expected ')' (line: %d)\n", *token ? (*token)->lineno : 0);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return NULL;
    }
    *token = (*token)->next;

    if (!*token || (*token)->type != SCRIPT_TOKEN_LBRAC) {
        char msg[64];
        strfmt(msg, "Error: expected '{' (line: %d)\n", *token ? (*token)->lineno : 0);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return NULL;
    }
    *token = (*token)->next;

    return stmt_func(name, parse_block(token), params, params_count);
}

static script_stmt_t *parse_if(script_token_t **token) {
    if (!*token) return NULL;

    if ((*token)->type != SCRIPT_TOKEN_LPAREN) {
        char msg[64];
        strfmt(msg, "Error: expected '(' (line: %d)\n", (*token)->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return NULL;
    }
    *token = (*token)->next;

    script_node_t *expr = parse_expr(token);
    if (!expr)
        return NULL;

    if (!*token || (*token)->type != SCRIPT_TOKEN_RPAREN) {
        char msg[64];
        strfmt(msg, "Error: expected ')' (line: %d)\n", *token ? (*token)->lineno : 0);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
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

static script_stmt_t *parse_statement(script_token_t **token) {
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
    } else {
        script_node_t *node = parse_expr(token);
        if (!node) return NULL;

        stmt = stmt_expr(node);
    }

    if (!stmt)
        return NULL;

    if (stmt->type != SCRIPT_STMT_FUNC &&
        stmt->type != SCRIPT_STMT_BLOCK &&
        stmt->type != SCRIPT_STMT_IF) {
        
        if (!*token || (*token)->type != SCRIPT_TOKEN_END) {
            char msg[64];
            strfmt(msg, "Error: expected ';' (line: %d)\n", *token ? (*token)->lineno : stmt->lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            free_stmt(stmt);
            return NULL;
        }

        *token = (*token)->next;
    }

    return stmt;
}

/* ==== built-ins ==== */

static script_node_t *call_exec(script_node_t *node) {
    size_t argc = node->call.argc;
    script_node_t **argv = node->call.argv;

    if (argc > 1) {
        char msg[64];
        strfmt(msg, "Error: Function exec() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(node);
        return NULL;
    }

    if (argv[0]->value_type == SCRIPT_STR)
        command_handle(argv[0]->literal.str_value, 0);
    else {
        char msg[64];
        strfmt(msg, "Error: Function exec() expects string argument (line: %d)\n", node->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(node);
        return NULL;
    }

    return node_null();
}

static script_node_t *call_print(script_node_t *node) {
    for (size_t i = 0; i < node->call.argc; i++) {
        switch(node->call.argv[i]->value_type) {
            case SCRIPT_INT:
                {
                    char buffer[12];
                    strint(buffer, node->call.argv[i]->literal.int_value);
                    term_write(buffer, COLOR_WHITE, COLOR_BLACK);
                    break;
                }
            case SCRIPT_FLOAT:
                {
                    char buffer[16];
                    strdouble(buffer, node->call.argv[i]->literal.float_value, 6);
                    term_write(buffer, COLOR_WHITE, COLOR_BLACK);
                    break;
                }
            case SCRIPT_STR:
                {
                    term_write(node->call.argv[i]->literal.str_value, COLOR_WHITE, COLOR_BLACK);
                    break;
                }
            case SCRIPT_NULL:
                {
                    term_write("null", COLOR_WHITE, COLOR_BLACK);
                    break;
                }
            case SCRIPT_BOOL:
                {
                    if (node->call.argv[i]->literal.int_value)
                        term_write("true", COLOR_WHITE, COLOR_BLACK);
                    else
                        term_write("false", COLOR_WHITE, COLOR_BLACK);
                    break;
                }
        }
    }

    return node_null();
}

static script_node_t *call_println(script_node_t *node) {
    script_node_t *ret = call_print(node);
    term_write("\n", COLOR_WHITE, COLOR_BLACK);
    return ret;
}

static script_node_t *call_as_str(script_node_t *node) {
    size_t argc = node->call.argc;

    if (argc > 1) {
        char msg[64];
        strfmt(msg, "Error: Function as_str() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(node);
        return NULL;
    }

    script_node_t *arg = node->call.argv[0];

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_STR;
    value->lineno = arg->lineno;

    switch (arg->value_type) {
        case SCRIPT_INT:
            {
                char buff[12];
                strint(buff, arg->literal.int_value);

                size_t size = strlen(buff) + 1;
                value->literal.str_size = size;
                value->literal.str_value = heap_alloc(size);
                strcpy(value->literal.str_value, buff);
                break;
            }
        case SCRIPT_FLOAT:
            {
                char buff[16];
                strdouble(buff, arg->literal.float_value, 6);

                size_t size = strlen(buff) + 1;
                value->literal.str_size = size;
                value->literal.str_value = heap_alloc(size);
                strcpy(value->literal.str_value, buff);
                break;
            }
        case SCRIPT_NULL:
            {
                char *buff = "null";
                size_t size = strlen(buff) + 1;
                value->literal.str_size = size;
                value->literal.str_value = heap_alloc(size);
                strcpy(value->literal.str_value, buff);
                break;
            }
        case SCRIPT_BOOL:
            {
                char *buff;
                if (arg->literal.int_value)
                    buff = "true";
                else
                    buff = "false";

                size_t size = strlen(buff) + 1;
                value->literal.str_size = size;
                value->literal.str_value = heap_alloc(size);
                strcpy(value->literal.str_value, buff);
                break;
            }
        case SCRIPT_STR:
            {
                free_node(value);
                return arg;
            }
        default:
            {
                char msg[64];
                strfmt(msg, "Error: Unsupported type (line: %d)\n", value->lineno);
                term_write(msg, COLOR_WHITE, COLOR_BLACK);
                free_node(value);
                return NULL;
            }
    }

    return value;
}

static script_node_t *call_as_int(script_node_t *node) {
    size_t argc = node->call.argc;

    if (argc > 1) {
        char msg[64];
        strfmt(msg, "Error: Function as_int() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(node);
        return NULL;
    }

    script_node_t *arg = node->call.argv[0];

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_INT;
    value->lineno = arg->lineno;

    switch (arg->value_type) {
        case SCRIPT_INT:
            {
                free_node(value);
                return arg;
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
                term_write(msg, COLOR_WHITE, COLOR_BLACK);
                free_node(value);
                return NULL;
            }
    }

    return value;
}

static script_node_t *call_as_float(script_node_t *node) {
    size_t argc = node->call.argc;

    if (argc > 1) {
        char msg[64];
        strfmt(msg, "Error: Function as_float() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(node);
        return NULL;
    }

    script_node_t *arg = node->call.argv[0];

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_FLOAT;
    value->lineno = arg->lineno;

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
                free_node(value);
                return arg;
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
                term_write(msg, COLOR_WHITE, COLOR_BLACK);
                free_node(value);
                return NULL;
            }
    }

    return value;
}

static script_node_t *call_type_name(script_node_t *node) {
    size_t argc = node->call.argc;

    if (argc > 1) {
        char msg[64];
        strfmt(msg, "Error: Function type_name() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(node);
        return NULL;
    }

    script_node_t *arg = node->call.argv[0];

    script_node_t *value = node_null();
    value->node_type = SCRIPT_AST_LITERAL;
    value->value_type = SCRIPT_STR;
    value->lineno = arg->lineno;

    switch (arg->value_type) {
        case SCRIPT_STR:
            value->literal.str_value = "str";
            break;
        case SCRIPT_INT:
            value->literal.str_value = "int";
            break;
        case SCRIPT_FLOAT:
            value->literal.str_value = "float";
            break;
        case SCRIPT_BOOL:
            value->literal.str_value = "bool";
            break;
        case SCRIPT_NULL:
            value->literal.str_value = "null";
            break;
        case SCRIPT_FUNC:
            value->literal.str_value = "function";
            break;
        case SCRIPT_FILE:
            value->literal.str_value = "file";
            break;
    }

    value->literal.str_size = strlen(value->literal.str_value);
    return value;
}

/* ================== */

static script_node_t *eval_binop(script_stmt_t *block, script_node_t *binop) {
    script_node_t *left = binop->binop.left;
    script_node_t *right = binop->binop.right;

    if (left->node_type == SCRIPT_AST_BINOP)
        left = eval_binop(block, left);
    if (right->node_type == SCRIPT_AST_BINOP)
        right = eval_binop(block, right);

    if (left->node_type != SCRIPT_AST_LITERAL)
        left = eval_expr(block, left);
    if (right->node_type != SCRIPT_AST_LITERAL)
        right = eval_expr(block, right);

    if (left->value_type == SCRIPT_ID) {
        char *name = left->literal.str_value;
        left = env_nodeify_var(block, left);
        if (!left) {
            char msg[64];
            strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n", name, binop->lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            free_node(binop);
            return NULL;
        }
    }
    if (right->value_type == SCRIPT_ID) {
        char *name = right->literal.str_value;
        right = env_nodeify_var(block, right);
        if (!right) {
            char msg[64];
            strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n", name, binop->lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            free_node(binop);
            return NULL;
        }
    }

    uint8_t op = binop->binop.op;
    if (op == SCRIPT_TOKEN_ISEQUAL) {
        if (left->value_type == SCRIPT_NULL || right->value_type == SCRIPT_NULL) {
            if (left->value_type == right->value_type)
                return node_true();
        }

        if (left->value_type == SCRIPT_STR && right->value_type == SCRIPT_STR) {
            if (!strcmp(left->literal.str_value, right->literal.str_value))
                return node_true();
        }

        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l == r)
                return node_true();
        }

        return node_false();
    } else if (op == SCRIPT_TOKEN_ISNTEQUAL) {
        if (left->value_type == SCRIPT_NULL || right->value_type == SCRIPT_NULL) {
            if (left->value_type != right->value_type)
                return node_true();
        }

        if (left->value_type == SCRIPT_STR && right->value_type == SCRIPT_STR) {
            if (strcmp(left->literal.str_value, right->literal.str_value))
                return node_true();
        }

        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l != r)
                return node_true();
        }

        return node_false();
    } else if (op == SCRIPT_TOKEN_LESSTHAN) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l < r)
                return node_true();
        
            return node_false();
        }
    } else if (op == SCRIPT_TOKEN_MORETHAN) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l > r)
                return node_true();
        
            return node_false();
        }
    } else if (op == SCRIPT_TOKEN_LESSEQUAL) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l <= r)
                return node_true();
        
            return node_false();
        }
    } else if (op == SCRIPT_TOKEN_MOREEQUAL) {
        if ((left->value_type == SCRIPT_INT || left->value_type == SCRIPT_FLOAT) &&
                (right->value_type == SCRIPT_INT || right->value_type == SCRIPT_FLOAT)) {

            double l = (left->value_type == SCRIPT_FLOAT) ?
                left->literal.float_value : left->literal.int_value;
            double r = (right->value_type == SCRIPT_FLOAT) ?
                right->literal.float_value : right->literal.int_value;

            if (l >= r)
                return node_true();
        
            return node_false();
        }
    }

    script_node_t *node = heap_alloc(sizeof(script_node_t));
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
                strfmt(msg, "Error: Zero division (line: %d)\n", node->lineno);
                term_write(msg, COLOR_WHITE, COLOR_BLACK);
                free_node(node);
                return NULL;
            }

            node->literal.float_value = l / r;

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
                term_write(msg, COLOR_WHITE, COLOR_BLACK);
                free_node(node);
                return NULL;
            }

            node->value_type = SCRIPT_INT;
            node->literal.int_value = l % r;

            return node;
        }
    } else if (op == SCRIPT_TOKEN_TIMES) {
        if (left->value_type == SCRIPT_INT) {
            if (right->value_type == SCRIPT_INT) {
                node->value_type = SCRIPT_INT;
                node->literal.int_value = left->literal.int_value * right->literal.int_value;

                return node;
            } else if (right->value_type == SCRIPT_STR) {
                node->value_type = SCRIPT_STR;

                int v = left->literal.int_value;
                if (v < 0)
                    v = 0;

                size_t i = (size_t) v;
                size_t size = right->literal.str_size * i;
                char **value = &node->literal.str_value;
                *value = heap_alloc(size + 1);
                memset(*value, 0, size + 1);

                while (i > 0) {
                    strcat(*value, right->literal.str_value);
                    i--;
                }

                return node;
            } 
        } else if (left->value_type == SCRIPT_STR) {
            if (right->value_type == SCRIPT_INT) {
                node->value_type = SCRIPT_STR;

                int v = right->literal.int_value;
                if (v < 0)
                    v = 0;

                size_t i = (size_t) v;
                size_t size = left->literal.str_size * i;
                char **value = &node->literal.str_value;
                *value = heap_alloc(size + 1);
                memset(*value, 0, size + 1);

                while (i > 0) {
                    strcat(*value, left->literal.str_value);
                    i--;
                }

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

            return node;
        }
    }

    char msg[64];
    strfmt(msg, "Error: Unsupported operation (line: %d)\n", binop->lineno);
    term_write(msg, COLOR_WHITE, COLOR_BLACK);
    free_node(binop);
    return NULL;
}

static script_node_t *eval_call(script_stmt_t *block, script_node_t *call) {
    script_node_t **eval_args = heap_alloc(sizeof(script_node_t*) * call->call.argc);

    for (size_t i = 0; i < call->call.argc; i++) {
        eval_args[i] = eval_expr(block, call->call.argv[i]);

        if (eval_args[i]->value_type == SCRIPT_ID) {
            script_node_t *var = env_nodeify_var(block, eval_args[i]);
            if (!var) {
                char msg[64];
                strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n",
                        eval_args[i]->literal.str_value,
                        eval_args[i]->lineno);
                term_write(msg, COLOR_WHITE, COLOR_BLACK);
                heap_free(eval_args);
                return NULL;
            }

            eval_args[i] = var;
        }

        if (!eval_args[i]) {
            for (size_t j = 0; j < i; j++)
                free_node(eval_args[j]);
            heap_free(eval_args);
            free_node(call);
            return NULL;
        }
    }

    char *name = call->call.func->literal.str_value;
    script_node_t *ret = node_null();

    script_node_t copy_call = *call;
    copy_call.call.argv = eval_args;

    if (!strcmp(name, "print")) ret = call_print(&copy_call);
    else if (!strcmp(name, "println")) ret = call_println(&copy_call);
    else if (!strcmp(name, "exec")) ret = call_exec(&copy_call);
    else if (!strcmp(name, "as_str")) ret = call_as_str(&copy_call);
    else if (!strcmp(name, "as_int")) ret = call_as_int(&copy_call);
    else if (!strcmp(name, "as_float")) ret = call_as_float(&copy_call);
    else if (!strcmp(name, "type_name")) ret = call_type_name(&copy_call);
    else {
        script_var_t *var = env_unscoped_find_var(block, name);
        if (var) {
            if (var->type != SCRIPT_FUNC) {
                char msg[64];
                strfmt(msg, "Error: Variable \"%s\" is not callable (line: %d)\n", name, call->lineno);
                term_write(msg, COLOR_WHITE, COLOR_BLACK);
                free_node(call);
                return NULL;    
            }

            script_stmt_t *func = var->func;
            script_stmt_t *call_block = stmt_block(block);

            if (call->call.argc < func->func.params_count) {
                char msg[64];
                strfmt(msg, "Error: Function \"%s\" takes %d argument(s), got %d (line: %d)\n",
                    name, func->func.params_count, call->call.argc, call->lineno);
                term_write(msg, COLOR_WHITE, COLOR_BLACK);
                free_node(call);
                return NULL;
            }

            for (size_t i = 0; i < func->func.params_count; i++) {
                script_node_t *param = func->func.params[i];
                script_node_t *arg = eval_args[i];

                env_set_var(call_block, param->literal.str_value, arg);
            }

            script_eval_t *eval = eval_block(call_block, func->func.block);
            if (eval->type == SCRIPT_EVAL_RETURN)
                ret = eval->node;

            free_stmt(call_block);
        } else {
            char msg[64];
            strfmt(msg, "Error: Undefined call \"%s\" (line: %d)\n", name, call->lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            free_node(call);
            return NULL;
        }
    }

    heap_free(eval_args);
    return ret;
}

static script_node_t *eval_expr(script_stmt_t *block, script_node_t *expr) {
    if (!expr) return NULL;

    switch (expr->node_type) {
        case SCRIPT_AST_LITERAL:
            if (expr->value_type == SCRIPT_ID) {
                script_node_t *var = env_nodeify_var(block, expr);
                if (!var) {
                    char msg[64];
                    strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n",
                        expr->literal.str_value, expr->lineno);
                    term_write(msg, COLOR_WHITE, COLOR_BLACK);
                    return NULL;
                }
                return var;
            }

            return expr;
        case SCRIPT_AST_BINOP:
            return eval_binop(block, expr);
        case SCRIPT_AST_CALL:
            return eval_call(block, expr);
    }

    char msg[64];
    strfmt(msg, "Error: Unsupported operation (line: %d)\n", expr->lineno);
    term_write(msg, COLOR_WHITE, COLOR_BLACK);
    free_node(expr);
    return NULL;
}

static script_node_t *eval_declare(script_stmt_t *block, script_stmt_t *stmt) {
    if (!stmt) return NULL;

    script_var_t *var = env_find_var(block, stmt->var.name);
    if (var) {
        char msg[128];
        if (var->type == SCRIPT_FUNC)
            strfmt(msg, "Error: Function with the same name already defined in this scope (line: %d)\n", stmt->lineno);
        else
            strfmt(msg, "Error: Variable already defined in this scope (line: %d)\n", stmt->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_stmt(stmt);
        return NULL;
    }

    script_node_t *value = node_null();
    env_set_var(block, stmt->var.name, value);

    return node_null();
}

static script_node_t *eval_define(script_stmt_t *block, script_stmt_t *stmt) {
    if (!stmt) return NULL;

    script_var_t *var = env_find_var(block, stmt->var.name);
    if (var) {
        char msg[128];
        if (var->type == SCRIPT_FUNC)
            strfmt(msg, "Error: Function with the same name already defined in this scope (line: %d)\n", stmt->lineno);
        else
            strfmt(msg, "Error: Variable already defined in this scope (line: %d)\n", stmt->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_stmt(stmt);
        return NULL;
    }
    script_node_t *value = eval_expr(block, stmt->var.value);
    if (!value)
        return NULL;

    env_set_var(block, stmt->var.name, value);

    return node_null();
}

static script_node_t *eval_assign(script_stmt_t *block, script_stmt_t *stmt) {
    if (!stmt) return NULL;

    script_var_t *var = env_unscoped_find_var(block, stmt->var.name);
    if (!var) {
        char msg[64];
        strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n", stmt->var.name, stmt->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_stmt(stmt);
        return NULL;
    }

    if (var->type == SCRIPT_FUNC) {
        char msg[64];
        strfmt(msg, "Error: Cannot assign values to a function (line: %d)\n", stmt->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_stmt(stmt);
        return NULL;
    }

    script_node_t *value = eval_expr(block, stmt->var.value);
    if (!value)
        return NULL;

    env_set_var(block, stmt->var.name, value);

    return node_null();
}

static script_node_t *eval_func(script_stmt_t *block, script_stmt_t *stmt) {
    if (!stmt) return NULL;

    char *name = stmt->func.name->literal.str_value;

    script_var_t *var = env_find_var(block, name);
    if (var) {
        char msg[128];
        if (var->type == SCRIPT_FUNC)
            strfmt(msg, "Error: Function with the same name already defined in this scope (line: %d)\n", stmt->lineno);
        else
            strfmt(msg, "Error: Variable already defined in this scope (line: %d)\n", stmt->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_stmt(stmt);
        return NULL;
    }
    script_stmt_t *func_block = stmt->func.block;
    func_block->parent = block;

    env_set_var_from_stmt(block, name, stmt);

    return node_null();
}

static script_eval_t *eval_block(script_stmt_t *block, script_stmt_t *stmt) {
    script_eval_t *eval = NULL;

    script_stmt_t *current = stmt->child;
    while (current) {
        eval = eval_statement(block, current);
        if (eval->type == SCRIPT_EVAL_RETURN)
            return eval;

        heap_free(eval);
        current = current->next;
    }

    if (!eval) {
        eval = heap_alloc(sizeof(script_eval_t));
        eval->type = SCRIPT_EVAL_NONE;
        eval->node = node_null();
    }

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

    if (!eval) {
        eval = heap_alloc(sizeof(script_eval_t));
        eval->type = SCRIPT_EVAL_NONE;
        eval->node = node_null();
    }

    free_node(expr);
    return eval;
}

static script_eval_t *eval_statement(script_stmt_t *block, script_stmt_t *stmt) {
    if (!stmt) return NULL;

    script_eval_t *eval = heap_alloc(sizeof(script_eval_t));
    eval->type = SCRIPT_EVAL_NONE;

    switch (stmt->type) {
        case SCRIPT_STMT_EXPR:
            eval->node = eval_expr(block, stmt->expr.node);
            break;
        case SCRIPT_STMT_RETURN:
            eval->type = SCRIPT_EVAL_RETURN;
            eval->node = eval_expr(block, stmt->expr.node);
            break;
        case SCRIPT_STMT_DECLARE:
            eval->node = eval_declare(block, stmt);
            break;
        case SCRIPT_STMT_DEFINE:
            eval->node = eval_define(block, stmt);
            break;
        case SCRIPT_STMT_ASSIGN:
            eval->node = eval_assign(block, stmt);
            break;
        case SCRIPT_STMT_FUNC:
            eval->node = eval_func(block, stmt);
            break;
        case SCRIPT_STMT_BLOCK:
            return eval_block(block, stmt);
        case SCRIPT_STMT_IF:
            return eval_if(block, stmt);
    }

    return eval;
}

static void block_add_statement(script_stmt_t *block, script_stmt_t* stmt) {
    if (block->type != SCRIPT_STMT_BLOCK)
        return;

    if (!block->child) {
        block->child = stmt;
        stmt->parent = block;
    } else {
        script_stmt_t *current = block->child;
        while (current) {
            if (!current->next) {
                stmt->parent = block;
                current->next = stmt;
                break;
            }

            current = current->next;
        }
    }
}

static script_runtime_t *get_runtime() {
    script_runtime_t *rt = heap_alloc(sizeof(script_runtime_t));
    rt->main = stmt_block(NULL);

    return rt;
}

static void free_runtime(script_runtime_t *rt) {
    free_stmt(rt->main);
    heap_free(rt);
}

void script_run(const char *path) {
    fio_t *file = fio_open(path, FIO_READ);
    if (!file) return;

    script_token_t *token_head = tokenize(file);
    if (!token_head) {
        fio_close(file);
        return;
    }
    fio_close(file);

    script_token_t *tokens = token_head;
    script_runtime_t *rt = get_runtime();

    while (token_head) {
        script_stmt_t *stmt = parse_statement(&token_head);
        if (!stmt)
            goto cleanup;

        block_add_statement(rt->main, stmt);
    }
    eval_block(rt->main, rt->main);

cleanup:
    while (tokens) {
        script_token_t *next = tokens->next;
        free_token(tokens);
        tokens = next;
    }

    free_runtime(rt);
}