#include "script.h"
#include "heap.h"
#include "fio.h"
#include "string.h"
#include "terminal.h"
#include "color.h"
#include "command.h"

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
        char msg[32];
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

static script_var_t *env_find_var(script_env_t *env, const char *name) {
    script_var_t *var = env->var_head;
    while (var) {
        if (!strcmp(var->name, name))
            return var;
        var = var->next;
    }

    return NULL;
}

static script_node_t *env_nodeify_var(script_env_t *env, script_node_t *node) {
    script_node_t *nodeified = heap_alloc(sizeof(script_node_t));
    memcpy(nodeified, node, sizeof(script_node_t));
    heap_free(nodeified->literal.str_value);

    script_var_t *var = env_find_var(env, node->literal.str_value);
    if (!var) return NULL;
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

static void env_set_var(script_env_t *env, const char *name, script_node_t *value) {
    if (value->node_type != SCRIPT_AST_LITERAL) return;

    script_var_t *var = env_find_var(env, name);
    if (!var) {
        var = heap_alloc(sizeof(script_var_t));
        var->name = heap_alloc(strlen(name) + 1);
        strcpy(var->name, name);
        var->next = NULL;

        if (!env->var_head)
            env->var_head = var;
        else {
            script_var_t *current = env->var_head;
            while (current->next)
                current = current->next;
            current->next = var;
        }
    } else if (var->type == SCRIPT_STR)
        heap_free(var->str_value);

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

static void free_env(script_env_t *env) {
    script_var_t *var = env->var_head;
    while (var) {
        script_var_t *next = var->next;

        if (var->type == SCRIPT_STR)
            heap_free(var->str_value);
        heap_free(var);

        var = next;
    }

    heap_free(env);
}

static script_node_t *node_null() {
    script_node_t *node = heap_alloc(sizeof(script_node_t));
    node->node_type = SCRIPT_AST_LITERAL;
    node->lineno = 0;
    node->value_type = SCRIPT_NULL;

    return node;
}

static script_node_t *node_literal(script_token_t *token) {
    script_node_t *node = heap_alloc(sizeof(script_node_t));
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

static script_node_t *node_var(script_node_t *name, script_node_t *value, uint8_t stmt_type) {
    script_node_t *node = heap_alloc(sizeof(script_node_t));
    node->node_type = SCRIPT_AST_STATEMENT;
    node->lineno = name->lineno;

    node->stmt.type = stmt_type;
    node->stmt.var.name = heap_alloc(name->literal.str_size);
    memcpy(node->stmt.var.name, name->literal.str_value, name->literal.str_size);
    node->stmt.var.value = value;

    return node;
}

static script_node_t *node_expr(script_node_t *expr) {
    script_node_t *node = heap_alloc(sizeof(script_node_t));
    node->node_type = SCRIPT_AST_STATEMENT;
    node->lineno = expr->lineno;
    node->stmt.type = SCRIPT_STMT_EXPR;
    node->stmt.expr.node = expr;

    return node;
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
        case SCRIPT_AST_STATEMENT:
            switch (node->stmt.type) {
                case SCRIPT_STMT_DEFINE:
                case SCRIPT_STMT_DECLARE:
                    heap_free(node->stmt.var.name);
                    free_node(node->stmt.var.value);
                    break;
                case SCRIPT_STMT_EXPR:
                    free_node(node->stmt.expr.node);
                    break;
                case SCRIPT_STMT_BLOCK:
                    if (node->stmt.block.parent)
                        free_node(node->stmt.block.parent);

                    free_env(node->stmt.block.env);

                    script_stmts_t *current_stmt = node->stmt.block.stmts;
                    while (current_stmt && current_stmt->node) {
                        free_node(current_stmt->node);
                        current_stmt = current_stmt->next;
                    }
                    heap_free(node->stmt.block.stmts);

                    script_blocks_t *current_block = node->stmt.block.child;
                    while (current_block && current_block->node) {
                        free_node(current_block->node);
                        current_block = current_block->next;
                    }
                    heap_free(node->stmt.block.child);
            }
    }

    heap_free(node);
}

static script_node_t *parse_expr(script_token_t **token);
static script_node_t *parse_term(script_token_t **token);
static script_node_t *parse_call(script_token_t **token);
static script_node_t *parse_factor(script_token_t **token);

static script_node_t *parse_factor(script_token_t **token) {
    if (!*token) return NULL;

    if ((*token)->type == SCRIPT_TOKEN_NUMBER ||
            (*token)->type == SCRIPT_TOKEN_STRING ||
            (*token)->type == SCRIPT_TOKEN_FLOAT ||
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
            strfmt(msg, "Error: expected ')' (line: %d)\n", (*token)->lineno);
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

    if ((*token)->type != SCRIPT_TOKEN_RPAREN) {
        char msg[64];
        strfmt(msg, "Error: expected ')' (line: %d)\n", (*token)->lineno);
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

static script_node_t *parse_expr(script_token_t **token) {
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

static script_node_t *parse_declare(script_token_t **token) {
    if (!*token) return NULL;

    script_node_t *name = parse_factor(token);
    if (!name) return NULL;

    if (name->value_type != SCRIPT_ID) {
        char msg[64];
        strfmt(msg, "Error: expected identifier (line: %d)\n", (*token)->lineno);
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
        return node_var(name, NULL, SCRIPT_STMT_DECLARE);
    } else if ((*token)->type == SCRIPT_TOKEN_EQUAL) {
        *token = (*token)->next;
        return node_var(name, parse_expr(token), SCRIPT_STMT_DEFINE);
    } else {
        char msg[64];
        strfmt(msg, "Error: Unexpected \"%s\" (line: %d)\n", (*token)->value, (*token)->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return NULL;
    }
}

static script_node_t *parse_assign(script_token_t **token) {
    script_node_t *name = node_literal(*token);

    // skip var name and equal, already checked previously
    *token = (*token)->next;
    *token = (*token)->next;

    return node_var(name, parse_expr(token), SCRIPT_STMT_ASSIGN);
}

static script_node_t *parse_statement(script_token_t **token) {
    if (!*token) return NULL;

    script_node_t *stmt = NULL;
    if ((*token)->type == SCRIPT_TOKEN_LET) {
        *token = (*token)->next;
        stmt = parse_declare(token);
    } else if ((*token)->type == SCRIPT_TOKEN_IDENTIFIER &&
            ((*token)->next && (*token)->next->type == SCRIPT_TOKEN_EQUAL)) { 
        stmt = parse_assign(token);
    } else if ((*token)->type == SCRIPT_TOKEN_FUNC) {
        char msg[64];
        strfmt(msg, "Error: Not implemented (line: %d)\n", *token ? (*token)->lineno : stmt->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        return NULL;
    } else {
        script_node_t *node = parse_expr(token);
        if (!node) return NULL;

        stmt = node_expr(node);
    }

    if (!*token || (*token)->type != SCRIPT_TOKEN_END) {
        char msg[64];
        strfmt(msg, "Error: expected ';' (line: %d)\n", *token ? (*token)->lineno : stmt->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(stmt);
        return NULL;
    }

    *token = (*token)->next;
    return stmt;
}

/* ==== built-ins ==== */

static script_node_t *call_exec(script_node_t *node) {
    size_t argc = node->call.argc;
    script_node_t **argv = node->call.argv;

    if (argc > 1) {
        char msg[32];
        strfmt(msg, "Error: Function exec() takes 1 argument, got %d (line: %d)\n", argc, node->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(node);
        return NULL;
    }

    if (argv[0]->value_type == SCRIPT_STR)
        command_handle(argv[0]->literal.str_value, 0);
    else {
        char msg[32];
        strfmt(msg, "Error: Expected string argument (line: %d)\n", node->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(node);
        return NULL;
    }

    script_node_t *ret = heap_alloc(sizeof(script_node_t));
    ret->node_type = SCRIPT_AST_LITERAL;
    ret->value_type = SCRIPT_NULL;
    return ret;
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
        }
    }

    script_node_t *ret = heap_alloc(sizeof(script_node_t));
    ret->node_type = SCRIPT_AST_LITERAL;
    ret->value_type = SCRIPT_NULL;
    return ret;
}

static script_node_t *call_println(script_node_t *node) {
    script_node_t *ret = call_print(node);
    term_write("\n", COLOR_WHITE, COLOR_BLACK);
    return ret;
}

/* ================== */

static script_node_t *eval_binop(script_node_t *block, script_node_t *binop);
static script_node_t *eval_call(script_node_t *block, script_node_t *call);
static script_node_t *eval_expr(script_node_t *block, script_node_t *expr);

static script_node_t *eval_binop(script_node_t *block, script_node_t *binop) {
    script_node_t *left = binop->binop.left;
    script_node_t *right = binop->binop.right;

    if (left->node_type == SCRIPT_AST_BINOP)
        left = eval_binop(block, left);
    if (right->node_type == SCRIPT_AST_BINOP)
        right = eval_binop(block, right);

    if (left->value_type == SCRIPT_ID) {
        char *name = left->literal.str_value;
        left = env_nodeify_var(block->stmt.block.env, left);
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
        right = env_nodeify_var(block->stmt.block.env, right);
        if (!right) {
            char msg[64];
            strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n", name, binop->lineno);
            term_write(msg, COLOR_WHITE, COLOR_BLACK);
            free_node(binop);
            return NULL;
        }
    }

    script_node_t *node = heap_alloc(sizeof(script_node_t));
    node->node_type = SCRIPT_AST_LITERAL;

    uint8_t op = binop->binop.op;
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

            size_t size = left->literal.str_size + right->literal.str_size;
            char **value = &node->literal.str_value;
            *value = heap_alloc(size);
            memset(*value, 0, size);
            strcat(*value, left->literal.str_value);
            strcat(*value, right->literal.str_value);

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

static script_node_t *eval_call(script_node_t *block, script_node_t *call) {
    for (size_t i = 0; i < call->call.argc; i++) {
        call->call.argv[i] = eval_expr(block, call->call.argv[i]);

        if (call->call.argv[i]->value_type == SCRIPT_ID) {
            script_node_t *var = env_nodeify_var(block->stmt.block.env, call->call.argv[i]);
            if (!var) {
                char msg[64];
                strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n",
                        call->call.argv[i]->literal.str_value,
                        call->call.argv[i]->lineno);
                term_write(msg, COLOR_WHITE, COLOR_BLACK);
                free_node(call->call.argv[i]);
                return NULL;
            }

            call->call.argv[i] = var;
        }

        if (!call->call.argv[i]) {
            free_node(call);
            return NULL;
        }
    }

    char *name = call->call.func->literal.str_value;
    if (!strcmp(name, "print")) return call_print(call);
    else if (!strcmp(name, "println")) return call_println(call);
    else if (!strcmp(name, "exec")) return call_exec(call);
    else {
        char msg[64];
        strfmt(msg, "Error: Undefined function \"%s\" (line: %d)\n", name, call->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(call);
        return NULL;
    }

    return call;
}

static script_node_t *eval_expr(script_node_t *block, script_node_t *expr) {
    if (!expr) return NULL;

    switch (expr->node_type) {
        case SCRIPT_AST_LITERAL:
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

static script_node_t *eval_declare(script_node_t *block, script_node_t *stmt) {
    if (!stmt) return NULL;

    script_var_t *var = env_find_var(block->stmt.block.env, stmt->stmt.var.name);
    if (var) {
        char msg[64];
        strfmt(msg, "Error: Variable already defined in this scope (line: %d)\n", stmt->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(stmt);
        return NULL;
    }

    script_node_t *value = node_null();
    env_set_var(block->stmt.block.env, stmt->stmt.var.name, value);

    return stmt;
}

static script_node_t *eval_define(script_node_t *block, script_node_t *stmt) {
    if (!stmt) return NULL;

    script_var_t *var = env_find_var(block->stmt.block.env, stmt->stmt.var.name);
    if (var) {
        char msg[64];
        strfmt(msg, "Error: Variable already defined in this scope (line: %d)\n", stmt->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(stmt);
        return NULL;
    }
    env_set_var(block->stmt.block.env, stmt->stmt.var.name, stmt->stmt.var.value);

    return stmt;
}

static script_node_t *eval_assign(script_node_t *block, script_node_t *stmt) {
    if (!stmt) return NULL;

    script_var_t *var = env_find_var(block->stmt.block.env, stmt->stmt.var.name);
    if (!var) {
        char msg[64];
        strfmt(msg, "Error: Undefined \"%s\" (line: %d)\n", stmt->stmt.var.name, stmt->lineno);
        term_write(msg, COLOR_WHITE, COLOR_BLACK);
        free_node(stmt);
        return NULL;
    }
    env_set_var(block->stmt.block.env, stmt->stmt.var.name, stmt->stmt.var.value);

    return stmt;
}

static script_node_t *eval_statement(script_node_t *block, script_node_t *stmt) {
    if (!stmt) return NULL;

    switch (stmt->stmt.type) {
        case SCRIPT_STMT_EXPR:
            return eval_expr(block, stmt->stmt.expr.node);
        case SCRIPT_STMT_DECLARE:
            return eval_declare(block, stmt);
        case SCRIPT_STMT_DEFINE:
            return eval_define(block, stmt);
        case SCRIPT_STMT_ASSIGN:
            return eval_assign(block, stmt);
    }

    return NULL;
}

static void block_add_statement(script_node_t *block, script_node_t* stmt) {
    if (block->node_type != SCRIPT_AST_STATEMENT && block->stmt.type != SCRIPT_STMT_BLOCK)
        return;

    script_stmts_t *current = block->stmt.block.stmts;
    while (current) {
        if (!current->node) {
            current->node = stmt;
            current->next = heap_alloc(sizeof(script_stmts_t));
            current->next->node = NULL;
            current->next->next = NULL;
            break;
        }

        current = current->next;
    }
}

static void block_run(script_node_t *block) {
    script_stmts_t *current = block->stmt.block.stmts;
    while (current && current->node) {
        eval_statement(block, current->node);
        current = current->next;
    }
}

static script_runtime_t *get_runtime() {
    script_runtime_t *rt = heap_alloc(sizeof(script_runtime_t));
    rt->main = heap_alloc(sizeof(script_node_t));

    script_node_t *block = rt->main;
    block->node_type = SCRIPT_AST_STATEMENT;
    block->value_type = SCRIPT_NULL;

    block->stmt.type = SCRIPT_STMT_BLOCK;
    block->stmt.block.parent = NULL;

    block->stmt.block.env = heap_alloc(sizeof(script_env_t));
    block->stmt.block.env->var_head = NULL;

    block->stmt.block.stmts = heap_alloc(sizeof(script_stmts_t));
    block->stmt.block.stmts->node = NULL;
    block->stmt.block.stmts->next = NULL;

    block->stmt.block.child = heap_alloc(sizeof(script_blocks_t));
    block->stmt.block.child->node = NULL;
    block->stmt.block.child->next = NULL;

    return rt;
}

static void free_runtime(script_runtime_t *rt) {
    free_node(rt->main);
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
        script_node_t *stmt = parse_statement(&token_head);
        if (!stmt)
            break;

        block_add_statement(rt->main, stmt);
    }
    block_run(rt->main);

    while (tokens) {
        script_token_t *next = tokens->next;
        free_token(tokens);
        tokens = next;
    }

    free_runtime(rt);
}
