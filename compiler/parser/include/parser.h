#ifndef NOVA_PARSER_H
#define NOVA_PARSER_H

/*
 * NOVA Parser
 *
 * Recursive-descent parser. Consumes a token stream from the lexer and
 * produces a NovAstNode* tree rooted at AST_PROGRAM.
 *
 * The parser drives the lexer directly — it does not pre-tokenise.
 * It owns no memory except the error list; AST nodes are heap-allocated
 * and owned by the caller (free with nova_ast_free).
 */

#include "ast.h"
#include "../../lexer/include/lexer.h"

#define NOVA_PARSER_MAX_ERRORS 64

typedef struct {
    uint32_t line;
    uint32_t col;
    char     message[256];
} NovParseError;

typedef struct {
    NovaLexer     *lex;
    NovaToken      current;     /* current (already consumed) token           */
    NovaToken      next;        /* one-token lookahead                        */
    NovParseError  errors[NOVA_PARSER_MAX_ERRORS];
    int            error_count;
    int            panic_mode;  /* set during error recovery                  */
} NovParser;


/* ── Lifecycle ────────────────────────────────────────────────────────────── */

void nova_parser_init(NovParser *p, NovaLexer *lex);

/* Parse the entire source. Returns AST_PROGRAM root. Caller frees. */
NovAstNode *nova_parse(NovParser *p);


/* ── Error reporting ──────────────────────────────────────────────────────── */

int  nova_parser_had_errors(const NovParser *p);
void nova_parser_print_errors(const NovParser *p);

#endif /* NOVA_PARSER_H */
