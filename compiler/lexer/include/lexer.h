#ifndef NOVA_LEXER_H
#define NOVA_LEXER_H

#include "token.h"
#include <stddef.h>
#include <stdint.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * NovaLexer
 *
 * A zero-copy, hand-written finite-automaton lexer for NOVA source files.
 *
 * The lexer does NOT own the source buffer. The caller must keep the source
 * buffer alive for the entire lifetime of the lexer (and any tokens it
 * produces, since tokens point into the source).
 *
 * Usage:
 *   NovaLexer lex;
 *   nova_lexer_init(&lex, source_ptr, source_len, "main.nv");
 *
 *   NovaToken tok;
 *   while ((tok = nova_lexer_next(&lex)).type != TOK_EOF) {
 *       // use tok
 *   }
 *
 *   nova_lexer_free(&lex);   // frees error list only; does not free source
 * ───────────────────────────────────────────────────────────────────────────*/

/* Maximum number of lexer errors before we stop reporting (not stop lexing). */
#define NOVA_LEXER_MAX_ERRORS 64

/* A lexer error record. */
typedef struct {
    uint32_t    line;
    uint32_t    col;
    char        message[128];
} NovaLexError;

/* The lexer state. Treat as opaque — use the API below. */
typedef struct {
    /* Source buffer (not owned) */
    const char  *src;
    size_t       src_len;
    const char  *filename;

    /* Cursor */
    size_t       pos;           /* byte index of next char to consume         */
    uint32_t     line;          /* current 1-based line                       */
    uint32_t     col;           /* current 1-based column                     */

    /* Saved position for token start */
    size_t       tok_start;
    uint32_t     tok_line;
    uint32_t     tok_col;

    /* Error list */
    NovaLexError errors[NOVA_LEXER_MAX_ERRORS];
    int          error_count;

    /* Flags */
    int          had_newline;   /* set when last non-ws was a newline         */
} NovaLexer;


/* ── Lifecycle ────────────────────────────────────────────────────────────── */

/*
 * Initialise the lexer.
 * `src`      — pointer to the source bytes (UTF-8, not null-terminated OK)
 * `src_len`  — byte length of src
 * `filename` — used only in error messages; may be NULL
 */
void nova_lexer_init(NovaLexer *lex,
                     const char *src, size_t src_len,
                     const char *filename);

/*
 * Free any resources owned by the lexer.
 * (Currently only bookkeeping — the error list is stack-allocated.)
 * Does NOT free `src` or `filename`.
 */
void nova_lexer_free(NovaLexer *lex);


/* ── Token production ─────────────────────────────────────────────────────── */

/*
 * Scan and return the next token.
 * Skips whitespace and comments automatically.
 * Returns TOK_EOF once all input is consumed.
 * On an unrecognised character, returns TOK_ERROR and records the error.
 */
NovaToken nova_lexer_next(NovaLexer *lex);

/*
 * Peek at the next token without consuming it.
 * Equivalent to calling next() then rewinding (cheap: saves/restores pos).
 */
NovaToken nova_lexer_peek(NovaLexer *lex);

/*
 * Tokenise the entire source into a heap-allocated array.
 * The caller owns the returned array and must free() it.
 * *out_count is set to the number of tokens (including the final TOK_EOF).
 * Returns NULL on allocation failure.
 */
NovaToken *nova_lexer_tokenise_all(NovaLexer *lex, size_t *out_count);


/* ── Error reporting ──────────────────────────────────────────────────────── */

/* Return 1 if any errors were recorded during lexing. */
int nova_lexer_had_errors(const NovaLexer *lex);

/* Print all recorded errors to stderr in a human-readable format. */
void nova_lexer_print_errors(const NovaLexer *lex);


/* ── Debug ────────────────────────────────────────────────────────────────── */

/* Print a single token to stdout (for debugging). */
void nova_token_print(const NovaToken *tok);

/* Print all tokens in an array to stdout. */
void nova_tokens_print_all(const NovaToken *tokens, size_t count);

#endif /* NOVA_LEXER_H */
