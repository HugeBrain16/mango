#ifndef SCRIPT_H
#define SCRIPT_H

#include <stdint.h>
#include <stddef.h>

#define SCRIPT_MAX_TOKENLENGTH 4096

#define SCRIPT_TOKEN_IDENTIFIER 0 // [a-zA-Z_]+
#define SCRIPT_TOKEN_NUMBER     1 // [0-9]+
#define SCRIPT_TOKEN_EQUAL      2 // =
#define SCRIPT_TOKEN_LPAREN     3 // (
#define SCRIPT_TOKEN_RPAREN     4 // )
#define SCRIPT_TOKEN_END        5 // ;
#define SCRIPT_TOKEN_SKIP       6 // #
#define SCRIPT_TOKEN_LET        7 // let
#define SCRIPT_TOKEN_STRING     8 // ".*"
#define SCRIPT_TOKEN_PRINT      9 // print
#define SCRIPT_TOKEN_PRINTLN    10 // println
#define SCRIPT_TOKEN_EXEC       11 // exec

#define SCRIPT_FUNC 0
#define SCRIPT_NULL 1
#define SCRIPT_INT  2
#define SCRIPT_STR  3

typedef struct script_token {
    char *value;
    uint8_t type;
    size_t lineno;
    struct script_token *next;
} script_token_t;

typedef struct script_node {
    struct script_node *parent;
    struct script_node *head;
    struct script_node *next;
    char *name;
    uint8_t type;
    void *value;
} script_node_t;

typedef struct {
    uint32_t size;
    script_node_t *main_node;
    script_token_t *main_token;
} script_runtime;

void script_run(const char *path);

#endif
