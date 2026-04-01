#include "../include/token.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Token type name table
 * Index must match the NovaTokenType enum exactly.
 * ───────────────────────────────────────────────────────────────────────────*/
static const char *TOKEN_NAMES[TOK_COUNT] = {
    /* Literals */
    "INT_LIT", "FLOAT_LIT", "UNIT_LIT", "STRING_LIT", "BOOL_LIT",

    /* Identifier */
    "IDENT",

    /* Keywords: declarations */
    "mission", "parallel", "constellation", "absorb", "model", "layer",
    "struct", "enum", "trait", "interface", "implements", "extends",
    "export", "extern", "unit",

    /* Keywords: bindings */
    "let", "var",

    /* Keywords: control flow */
    "if", "else", "for", "in", "while", "loop",
    "break", "continue", "return", "match",

    /* Keywords: AI / scientific */
    "autodiff", "gradient", "wrt", "pipeline", "on", "device", "wave",

    /* Keywords: output / safety */
    "transmit", "panic", "unsafe", "raw", "as",

    /* Keywords: types */
    "Void", "Never", "self",

    /* Operators: arithmetic */
    "+", "-", "*", "/", "%", "^", "@",

    /* Operators: comparison */
    "==", "!=", "<", ">", "<=", ">=",

    /* Operators: logical */
    "&&", "||", "!",

    /* Operators: assignment */
    "=", "+=", "-=", "*=", "/=",

    /* Operators: NOVA-specific */
    "->", "=>", "|>", "..", "?", "&", "|",

    /* Delimiters */
    "(", ")", "{", "}", "[", "]",
    ",", ";", ":", "::", ".", "#",

    /* Special */
    "NEWLINE", "EOF", "ERROR",
};

const char *nova_token_type_name(NovaTokenType type) {
    if (type < 0 || type >= TOK_COUNT) return "<unknown>";
    return TOKEN_NAMES[type];
}

int nova_token_is_keyword(NovaTokenType type) {
    return type >= TOK_MISSION && type <= TOK_SELF_KW;
}

int nova_token_is_literal(NovaTokenType type) {
    return type >= TOK_INT_LIT && type <= TOK_BOOL_LIT;
}

int nova_token_is_binop(NovaTokenType type) {
    switch (type) {
        case TOK_PLUS: case TOK_MINUS: case TOK_STAR: case TOK_SLASH:
        case TOK_PERCENT: case TOK_CARET: case TOK_AT:
        case TOK_EQ_EQ: case TOK_BANG_EQ:
        case TOK_LT: case TOK_GT: case TOK_LT_EQ: case TOK_GT_EQ:
        case TOK_AND: case TOK_OR:
        case TOK_PIPE_GT: case TOK_DOT_DOT:
            return 1;
        default:
            return 0;
    }
}
