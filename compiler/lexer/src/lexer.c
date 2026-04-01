/*
 * NOVA Lexer — lexer.c
 *
 * Hand-written finite automaton lexer for NOVA (.nv) source files.
 *
 * Design notes:
 *   - Zero-copy: tokens are (pointer, length) slices into the source buffer.
 *   - UTF-8 aware: Unicode arrows (→) and unit superscripts (m²) are handled.
 *   - Comments: -- starts a line comment; everything to end-of-line is skipped.
 *   - Unit literals: a float or int immediately followed by '[' scans the
 *     unit expression until the matching ']', producing TOK_UNIT_LIT whose
 *     lexeme spans the entire "9.8[m/s]" sequence.
 *   - String literals: support {expr} interpolation placeholders (the lexer
 *     does not parse the expression — it just includes the braces in the
 *     string token so the parser can handle them).
 */

#include "../include/lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal helpers
 * ───────────────────────────────────────────────────────────────────────────*/

/* Remaining bytes from current position. */
static inline size_t remaining(const NovaLexer *lex) {
    return lex->src_len - lex->pos;
}

/* Current byte (does not advance). Returns 0 if at end. */
static inline char cur(const NovaLexer *lex) {
    return (lex->pos < lex->src_len) ? lex->src[lex->pos] : '\0';
}

/* Next byte (lookahead-1, does not advance). Returns 0 if past end. */
static inline char peek1(const NovaLexer *lex) {
    return (lex->pos + 1 < lex->src_len) ? lex->src[lex->pos + 1] : '\0';
}

/* Advance by one byte, updating line/col. Returns the consumed byte. */
static char advance(NovaLexer *lex) {
    char c = lex->src[lex->pos++];
    if (c == '\n') {
        lex->line++;
        lex->col = 1;
    } else {
        lex->col++;
    }
    return c;
}

/* Advance if current byte matches `c`. Returns 1 on match, 0 otherwise. */
static int match(NovaLexer *lex, char c) {
    if (cur(lex) == c) { advance(lex); return 1; }
    return 0;
}

/* Record an error. Stops recording after NOVA_LEXER_MAX_ERRORS. */
static void lex_error(NovaLexer *lex, const char *msg) {
    if (lex->error_count >= NOVA_LEXER_MAX_ERRORS) return;
    NovaLexError *e = &lex->errors[lex->error_count++];
    e->line = lex->tok_line;
    e->col  = lex->tok_col;
    snprintf(e->message, sizeof(e->message), "%s", msg);
}

/* Build a token from the saved start position to the current position. */
static NovaToken make_token(const NovaLexer *lex, NovaTokenType type) {
    NovaToken tok;
    tok.type       = type;
    tok.lexeme     = lex->src + lex->tok_start;
    tok.lexeme_len = lex->pos - lex->tok_start;
    tok.line       = lex->tok_line;
    tok.col        = lex->tok_col;
    return tok;
}

/* Save current position as the start of the next token. */
static void mark_start(NovaLexer *lex) {
    lex->tok_start = lex->pos;
    lex->tok_line  = lex->line;
    lex->tok_col   = lex->col;
}


/* ─────────────────────────────────────────────────────────────────────────────
 * Unicode helpers
 *
 * NOVA source is UTF-8. We need to handle:
 *   →  (U+2192, E2 86 92) — return type arrow (alias for ->)
 *   ²  (U+00B2, C2 B2)    — superscript 2, valid in unit expressions
 *   ³  (U+00B3, C2 B3)    — superscript 3, valid in unit expressions
 *   ·  (U+00B7, C2 B7)    — middle dot, unit multiplication in display
 *   ☉  (U+2609, E2 98 89) — solar symbol, valid in unit identifiers
 * ───────────────────────────────────────────────────────────────────────────*/

/* Return the number of bytes in the UTF-8 sequence starting at src[pos].    */
static int utf8_seq_len(unsigned char lead) {
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1; /* invalid — treat as single byte */
}

/*
 * Check if the bytes at lex->src[lex->pos] match the given UTF-8 sequence.
 * `seq` is the raw byte sequence, `seq_len` is its byte length.
 */
static int cur_is_utf8(const NovaLexer *lex, const unsigned char *seq, int seq_len) {
    if ((size_t)(lex->pos + seq_len) > lex->src_len) return 0;
    return memcmp(lex->src + lex->pos, seq, seq_len) == 0;
}

/* UTF-8 encoding of U+2192 RIGHTWARDS ARROW (→) */
static const unsigned char UTF8_ARROW[]     = { 0xE2, 0x86, 0x92 };
/* UTF-8 encoding of U+00B2 SUPERSCRIPT TWO (²) */
static const unsigned char UTF8_SUP2[] __attribute__((unused))      = { 0xC2, 0xB2 };
/* UTF-8 encoding of U+00B3 SUPERSCRIPT THREE (³) */
static const unsigned char UTF8_SUP3[] __attribute__((unused))      = { 0xC2, 0xB3 };
/* UTF-8 encoding of U+00B7 MIDDLE DOT (·) */
static const unsigned char UTF8_MIDDOT[] __attribute__((unused))    = { 0xC2, 0xB7 };

/* Advance past `n` bytes (for multi-byte UTF-8 sequences). */
static void advance_bytes(NovaLexer *lex, int n) {
    for (int i = 0; i < n; i++) advance(lex);
}


/* ─────────────────────────────────────────────────────────────────────────────
 * Keyword table
 * Linear search is fine for the small keyword set; a trie can replace this
 * later if profiling shows it matters.
 * ───────────────────────────────────────────────────────────────────────────*/
typedef struct { const char *word; NovaTokenType type; } Keyword;

static const Keyword KEYWORDS[] = {
    { "mission",        TOK_MISSION       },
    { "parallel",       TOK_PARALLEL      },
    { "constellation",  TOK_CONSTELLATION },
    { "absorb",         TOK_ABSORB        },
    { "model",          TOK_MODEL         },
    { "layer",          TOK_LAYER         },
    { "struct",         TOK_STRUCT        },
    { "enum",           TOK_ENUM          },
    { "trait",          TOK_TRAIT         },
    { "interface",      TOK_INTERFACE     },
    { "implements",     TOK_IMPLEMENTS    },
    { "extends",        TOK_EXTENDS       },
    { "export",         TOK_EXPORT        },
    { "extern",         TOK_EXTERN        },
    { "unit",           TOK_UNIT_KW       },
    { "let",            TOK_LET           },
    { "var",            TOK_VAR           },
    { "if",             TOK_IF            },
    { "else",           TOK_ELSE          },
    { "for",            TOK_FOR           },
    { "in",             TOK_IN            },
    { "while",          TOK_WHILE         },
    { "loop",           TOK_LOOP          },
    { "break",          TOK_BREAK         },
    { "continue",       TOK_CONTINUE      },
    { "return",         TOK_RETURN        },
    { "match",          TOK_MATCH         },
    { "autodiff",       TOK_AUTODIFF      },
    { "gradient",       TOK_GRADIENT      },
    { "wrt",            TOK_WRT           },
    { "pipeline",       TOK_PIPELINE      },
    { "on",             TOK_ON            },
    { "device",         TOK_DEVICE        },
    { "wave",           TOK_WAVE          },
    { "transmit",       TOK_TRANSMIT      },
    { "panic",          TOK_PANIC         },
    { "unsafe",         TOK_UNSAFE        },
    { "raw",            TOK_RAW           },
    { "as",             TOK_AS            },
    { "Void",           TOK_VOID          },
    { "Never",          TOK_NEVER         },
    { "self",           TOK_SELF_KW       },
    { "true",           TOK_BOOL_LIT      },
    { "false",          TOK_BOOL_LIT      },
    { NULL,             TOK_ERROR         },  /* sentinel */
};

/* Match lexeme[0..len) against the keyword table. Returns TOK_IDENT on miss. */
static NovaTokenType lookup_keyword(const char *lexeme, size_t len) {
    for (int i = 0; KEYWORDS[i].word != NULL; i++) {
        size_t klen = strlen(KEYWORDS[i].word);
        if (klen == len && memcmp(KEYWORDS[i].word, lexeme, len) == 0)
            return KEYWORDS[i].type;
    }
    return TOK_IDENT;
}


/* ─────────────────────────────────────────────────────────────────────────────
 * Individual scanner functions
 * ───────────────────────────────────────────────────────────────────────────*/

/* Skip a line comment (-- to end of line). Does not consume the newline. */
static void skip_line_comment(NovaLexer *lex) {
    /* Already consumed '--'. Advance to end of line. */
    while (lex->pos < lex->src_len && cur(lex) != '\n')
        advance(lex);
}

/* Skip whitespace (spaces, tabs, carriage returns). NOT newlines. */
static void skip_spaces(NovaLexer *lex) {
    while (lex->pos < lex->src_len) {
        char c = cur(lex);
        if (c == ' ' || c == '\t' || c == '\r')
            advance(lex);
        else
            break;
    }
}

/*
 * Scan a string literal. Opening '"' already consumed.
 * Handles escape sequences: \n \t \\ \"
 * Handles {expr} interpolation placeholders (included verbatim in lexeme).
 * Returns TOK_STRING_LIT.
 */
static NovaToken scan_string(NovaLexer *lex) {
    int depth = 0; /* tracks nested {} inside interpolations */
    while (lex->pos < lex->src_len) {
        char c = cur(lex);
        if (c == '\\') {
            advance(lex); /* skip backslash */
            if (lex->pos < lex->src_len) advance(lex); /* skip escaped char */
            continue;
        }
        if (c == '{') { depth++; advance(lex); continue; }
        if (c == '}') {
            if (depth > 0) depth--;
            advance(lex);
            continue;
        }
        if (depth == 0 && c == '"') {
            advance(lex); /* consume closing '"' */
            return make_token(lex, TOK_STRING_LIT);
        }
        advance(lex);
    }
    lex_error(lex, "unterminated string literal");
    return make_token(lex, TOK_ERROR);
}

/*
 * Scan a numeric literal (integer or float), then check for a unit suffix.
 *
 * Grammar (simplified):
 *   integer  := [0-9_]+ | '0x'[0-9a-fA-F_]+ | '0b'[01_]+
 *   float    := [0-9_]+ '.' [0-9_]* exponent?
 *             | [0-9_]+ exponent
 *   exponent := ('e'|'E') ('+'|'-')? [0-9]+
 *   unit_lit := (integer | float) '[' unit_expr ']'
 *
 * The lexeme for a unit literal spans from the first digit to the ']'.
 */
static NovaToken scan_number(NovaLexer *lex) {
    int is_float = 0;
    char c = cur(lex);

    /* Hex literal */
    if (c == '0' && (peek1(lex) == 'x' || peek1(lex) == 'X')) {
        advance(lex); advance(lex);
        while (isxdigit(cur(lex)) || cur(lex) == '_') advance(lex);
        goto check_unit;
    }

    /* Binary literal */
    if (c == '0' && (peek1(lex) == 'b' || peek1(lex) == 'B')) {
        advance(lex); advance(lex);
        while (cur(lex) == '0' || cur(lex) == '1' || cur(lex) == '_') advance(lex);
        goto check_unit;
    }

    /* Decimal integer or float */
    while (isdigit(cur(lex)) || cur(lex) == '_') advance(lex);

    if (cur(lex) == '.' && isdigit(peek1(lex))) {
        is_float = 1;
        advance(lex); /* consume '.' */
        while (isdigit(cur(lex)) || cur(lex) == '_') advance(lex);
    }

    if (cur(lex) == 'e' || cur(lex) == 'E') {
        is_float = 1;
        advance(lex);
        if (cur(lex) == '+' || cur(lex) == '-') advance(lex);
        if (!isdigit(cur(lex))) {
            lex_error(lex, "expected digit after exponent");
            return make_token(lex, TOK_ERROR);
        }
        while (isdigit(cur(lex))) advance(lex);
    }

check_unit:
    /* Unit suffix: '[' unit_expr ']'
     * unit_expr may contain letters, digits, /, *, ^, ·, Unicode superscripts,
     * spaces, and the special symbols defined above. */
    if (cur(lex) == '[') {
        advance(lex); /* consume '[' */
        int bracket_depth = 1;
        while (lex->pos < lex->src_len && bracket_depth > 0) {
            unsigned char lead = (unsigned char)cur(lex);
            /* Handle multi-byte UTF-8 inside unit expressions */
            int seq = utf8_seq_len(lead);
            if (seq > 1) {
                advance_bytes(lex, seq);
                continue;
            }
            if (cur(lex) == '[') bracket_depth++;
            else if (cur(lex) == ']') { bracket_depth--; if (bracket_depth == 0) break; }
            advance(lex);
        }
        if (cur(lex) != ']') {
            lex_error(lex, "unterminated unit literal (missing ']')");
            return make_token(lex, TOK_ERROR);
        }
        advance(lex); /* consume ']' */
        return make_token(lex, TOK_UNIT_LIT);
    }

    return make_token(lex, is_float ? TOK_FLOAT_LIT : TOK_INT_LIT);
}

/*
 * Scan an identifier or keyword.
 * Identifiers: start with [a-zA-Z_], continue with [a-zA-Z0-9_].
 * Unicode letters (e.g. Greek) are allowed in identifiers; we scan them as
 * raw bytes (multi-byte UTF-8) so they round-trip correctly.
 */
static NovaToken scan_ident_or_keyword(NovaLexer *lex) {
    while (lex->pos < lex->src_len) {
        unsigned char lead = (unsigned char)cur(lex);
        if (isalnum(lead) || lead == '_') {
            advance(lex);
        } else if (lead >= 0x80) {
            /* Multi-byte UTF-8: include in identifier */
            int seq = utf8_seq_len(lead);
            advance_bytes(lex, seq);
        } else {
            break;
        }
    }
    NovaToken tok = make_token(lex, TOK_IDENT);
    tok.type = lookup_keyword(tok.lexeme, tok.lexeme_len);
    return tok;
}


/* ─────────────────────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────────────────────*/

void nova_lexer_init(NovaLexer *lex,
                     const char *src, size_t src_len,
                     const char *filename) {
    memset(lex, 0, sizeof(*lex));
    lex->src      = src;
    lex->src_len  = src_len;
    lex->filename = filename ? filename : "<input>";
    lex->line     = 1;
    lex->col      = 1;
}

void nova_lexer_free(NovaLexer *lex) {
    /* Nothing heap-allocated currently. Reset to catch use-after-free. */
    memset(lex, 0, sizeof(*lex));
}

NovaToken nova_lexer_next(NovaLexer *lex) {
    for (;;) {
        /* Skip spaces (but not newlines — they can be significant). */
        skip_spaces(lex);

        if (lex->pos >= lex->src_len) {
            mark_start(lex);
            return make_token(lex, TOK_EOF);
        }

        mark_start(lex);
        char c = cur(lex);

        /* ── Newline ────────────────────────────────────────────────────── */
        if (c == '\n') {
            advance(lex);
            return make_token(lex, TOK_NEWLINE);
        }

        /* ── Line comment: -- ───────────────────────────────────────────── */
        if (c == '-' && peek1(lex) == '-') {
            advance(lex); advance(lex);
            skip_line_comment(lex);
            continue; /* re-enter loop after comment */
        }

        /* ── Unicode arrow → (U+2192) ────────────────────────────────────  */
        if (cur_is_utf8(lex, UTF8_ARROW, 3)) {
            advance_bytes(lex, 3);
            return make_token(lex, TOK_ARROW);
        }

        /* ── String literal ─────────────────────────────────────────────── */
        if (c == '"') {
            advance(lex);
            return scan_string(lex);
        }

        /* ── Numeric literal ─────────────────────────────────────────────  */
        if (isdigit(c)) {
            return scan_number(lex);
        }

        /* ── Identifier / keyword ────────────────────────────────────────── */
        if (isalpha(c) || c == '_' || (unsigned char)c >= 0x80) {
            return scan_ident_or_keyword(lex);
        }

        /* ── Single/double-char operators and delimiters ─────────────────── */
        advance(lex); /* consume `c` */

        switch (c) {

            /* Arithmetic */
            case '+':
                if (match(lex, '=')) return make_token(lex, TOK_PLUS_EQ);
                return make_token(lex, TOK_PLUS);
            case '*':
                if (match(lex, '=')) return make_token(lex, TOK_STAR_EQ);
                return make_token(lex, TOK_STAR);
            case '/':
                if (match(lex, '=')) return make_token(lex, TOK_SLASH_EQ);
                return make_token(lex, TOK_SLASH);
            case '%': return make_token(lex, TOK_PERCENT);
            case '^': return make_token(lex, TOK_CARET);
            case '@': return make_token(lex, TOK_AT);

            /* Minus: could be -= or -> */
            case '-':
                if (match(lex, '=')) return make_token(lex, TOK_MINUS_EQ);
                if (match(lex, '>')) return make_token(lex, TOK_ARROW);
                return make_token(lex, TOK_MINUS);

            /* Comparison / assignment */
            case '=':
                if (match(lex, '=')) return make_token(lex, TOK_EQ_EQ);
                if (match(lex, '>')) return make_token(lex, TOK_FAT_ARROW);
                return make_token(lex, TOK_EQ);
            case '!':
                if (match(lex, '=')) return make_token(lex, TOK_BANG_EQ);
                return make_token(lex, TOK_BANG);
            case '<':
                if (match(lex, '=')) return make_token(lex, TOK_LT_EQ);
                return make_token(lex, TOK_LT);
            case '>':
                if (match(lex, '=')) return make_token(lex, TOK_GT_EQ);
                return make_token(lex, TOK_GT);

            /* Logical */
            case '&':
                if (match(lex, '&')) return make_token(lex, TOK_AND);
                return make_token(lex, TOK_AMP);
            case '|':
                if (match(lex, '|')) return make_token(lex, TOK_OR);
                if (match(lex, '>')) return make_token(lex, TOK_PIPE_GT);
                return make_token(lex, TOK_PIPE);

            /* Dots */
            case '.':
                if (match(lex, '.')) return make_token(lex, TOK_DOT_DOT);
                return make_token(lex, TOK_DOT);

            /* Colon */
            case ':':
                if (match(lex, ':')) return make_token(lex, TOK_COLON_COLON);
                return make_token(lex, TOK_COLON);

            /* Delimiters */
            case '(': return make_token(lex, TOK_LPAREN);
            case ')': return make_token(lex, TOK_RPAREN);
            case '{': return make_token(lex, TOK_LBRACE);
            case '}': return make_token(lex, TOK_RBRACE);
            case '[': return make_token(lex, TOK_LBRACKET);
            case ']': return make_token(lex, TOK_RBRACKET);
            case ',': return make_token(lex, TOK_COMMA);
            case ';': return make_token(lex, TOK_SEMICOLON);
            case '#': return make_token(lex, TOK_HASH);
            case '?': return make_token(lex, TOK_QUESTION);

            default: {
                char msg[64];
                snprintf(msg, sizeof(msg),
                         "unexpected character '%c' (0x%02X)", c, (unsigned char)c);
                lex_error(lex, msg);
                return make_token(lex, TOK_ERROR);
            }
        }
    }
}

NovaToken nova_lexer_peek(NovaLexer *lex) {
    /* Save full lexer state, call next(), restore. */
    size_t   saved_pos      = lex->pos;
    uint32_t saved_line     = lex->line;
    uint32_t saved_col      = lex->col;
    size_t   saved_start    = lex->tok_start;
    uint32_t saved_tok_line = lex->tok_line;
    uint32_t saved_tok_col  = lex->tok_col;
    int      saved_errcnt   = lex->error_count;

    NovaToken tok = nova_lexer_next(lex);

    lex->pos         = saved_pos;
    lex->line        = saved_line;
    lex->col         = saved_col;
    lex->tok_start   = saved_start;
    lex->tok_line    = saved_tok_line;
    lex->tok_col     = saved_tok_col;
    lex->error_count = saved_errcnt;

    return tok;
}

NovaToken *nova_lexer_tokenise_all(NovaLexer *lex, size_t *out_count) {
    size_t capacity = 256;
    size_t count    = 0;
    NovaToken *tokens = malloc(capacity * sizeof(NovaToken));
    if (!tokens) return NULL;

    NovaToken tok;
    do {
        tok = nova_lexer_next(lex);
        if (count >= capacity) {
            capacity *= 2;
            NovaToken *tmp = realloc(tokens, capacity * sizeof(NovaToken));
            if (!tmp) { free(tokens); return NULL; }
            tokens = tmp;
        }
        tokens[count++] = tok;
    } while (tok.type != TOK_EOF);

    *out_count = count;
    return tokens;
}

int nova_lexer_had_errors(const NovaLexer *lex) {
    return lex->error_count > 0;
}

void nova_lexer_print_errors(const NovaLexer *lex) {
    for (int i = 0; i < lex->error_count; i++) {
        const NovaLexError *e = &lex->errors[i];
        fprintf(stderr, "%s:%u:%u: error: %s\n",
                lex->filename, e->line, e->col, e->message);
    }
}

void nova_token_print(const NovaToken *tok) {
    printf("%-14s  %3u:%-3u  [%.*s]\n",
           nova_token_type_name(tok->type),
           tok->line, tok->col,
           (int)tok->lexeme_len, tok->lexeme);
}

void nova_tokens_print_all(const NovaToken *tokens, size_t count) {
    printf("%-14s  %s     %s\n", "TYPE", "LINE:COL", "LEXEME");
    printf("-----------------------------------------------\n");
    for (size_t i = 0; i < count; i++)
        nova_token_print(&tokens[i]);
}
