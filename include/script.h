#ifndef SCRIPT_H
#define SCRIPT_H

#include <stdint.h>
#include <stddef.h>

#define SCRIPT_SIZE_TOKEN 4

#define SCRIPT_TOKEN_IDENTIFIER 0 // [a-zA-Z_]+
#define SCRIPT_TOKEN_NUMBER     1 // [0-9]+
#define SCRIPT_TOKEN_EQUAL      2 // =
#define SCRIPT_TOKEN_LPAREN     3 // (
#define SCRIPT_TOKEN_RPAREN     4 // )
#define SCRIPT_TOKEN_END        5 // ;
#define SCRIPT_TOKEN_SKIP       6 // #
#define SCRIPT_TOKEN_LET        7 // let
#define SCRIPT_TOKEN_STRING     8 // ".*"
#define SCRIPT_TOKEN_PLUS       12 // +
#define SCRIPT_TOKEN_MINUS      13 // -
#define SCRIPT_TOKEN_DIVIDE     14 // /
#define SCRIPT_TOKEN_TIMES      15 // *
#define SCRIPT_TOKEN_MODULO     16 // %
#define SCRIPT_TOKEN_COMMA      17 // ,
#define SCRIPT_TOKEN_DOT        18 // .
#define SCRIPT_TOKEN_FLOAT      19 // [0-9]+\.[0-9]+
#define SCRIPT_TOKEN_FUNC       20 // func
#define SCRIPT_TOKEN_LBRAC      21 // {
#define SCRIPT_TOKEN_RBRAC      22 // }

#define SCRIPT_AST_BINOP 0
#define SCRIPT_AST_LITERAL 1
#define SCRIPT_AST_CALL 2
#define SCRIPT_AST_STATEMENT 3

#define SCRIPT_STMT_EXPR 0
#define SCRIPT_STMT_ASSIGN 1
#define SCRIPT_STMT_DECLARE 2
#define SCRIPT_STMT_DEFINE 3
#define SCRIPT_STMT_FUNC 4
#define SCRIPT_STMT_BLOCK 5

#define SCRIPT_FUNC     0
#define SCRIPT_NULL     1
#define SCRIPT_INT      2
#define SCRIPT_STR      3
#define SCRIPT_FLOAT    4
#define SCRIPT_ID       5

typedef struct script_var script_var_t;
typedef struct script_env script_env_t;
typedef struct script_node script_node_t;
typedef struct script_stmt script_stmt_t;

typedef struct script_token {
    char *value;
    uint8_t type;
    size_t lineno;
    size_t size;
    struct script_token *next;
} script_token_t;

typedef struct script_env {
    script_var_t *var_head;
} script_env_t;

typedef struct script_var {
    char *name;
    uint8_t type;

    union {
        struct {
            char *str_value;
            size_t str_size;
        };
        int int_value;
        double float_value;
        script_stmt_t *func;
    };

    struct script_var *next;
} script_var_t;

typedef struct script_node {
    uint8_t node_type;
    uint8_t value_type;
    size_t lineno;

    struct script_node *parent;
    struct script_node *head;
    struct script_node *next;

    union {
        struct {
            uint8_t op;
            struct script_node *left;
            struct script_node *right;
        } binop;

        struct {
            char *str_value;
            size_t str_size;
            int int_value;
            double float_value;
        } literal;

        struct {
            struct script_node *func;
            struct script_node **argv;
            size_t argc;
        } call;
    };
} script_node_t;

typedef struct script_stmt {
    uint8_t type;
    size_t lineno;

    struct script_stmt *parent;
    struct script_stmt *child;
    struct script_stmt *next;

    union {
        struct {
            script_node_t *node;
        } expr;

        struct {
            char *name;
            script_node_t *value;
        } var;

        struct {
            script_env_t *env;
        } block;

        struct {
            script_node_t *name;
            script_node_t **params;
            size_t params_count;

            struct script_stmt *block;
        } func;
    };
} script_stmt_t;

typedef struct {
    script_stmt_t *main;
} script_runtime_t;

void script_run(const char *path);

#endif
