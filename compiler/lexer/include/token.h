#ifndef NOVA_TOKEN_H
#define NOVA_TOKEN_H

#include <stddef.h>
#include <stdint.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * NOVA Token Types
 * Every token the lexer can produce. Grouped by category.
 * ───────────────────────────────────────────────────────────────────────────*/
typedef enum {

    /* ── Literals ────────────────────────────────────────────────────────── */
    TOK_INT_LIT,        /* 42  1000  0xFF  0b1010                             */
    TOK_FLOAT_LIT,      /* 3.14  1.0e10  6.674e-11                            */
    TOK_UNIT_LIT,       /* 9.8[m/s]  845000.0[N]  311.0[s]                   */
    TOK_STRING_LIT,     /* "hello universe"  "r = {r:.4}"                     */
    TOK_BOOL_LIT,       /* true  false                                         */

    /* ── Identifiers ─────────────────────────────────────────────────────── */
    TOK_IDENT,          /* any user-defined name: mass, delta_v, stars        */

    /* ── Keywords: declarations ──────────────────────────────────────────── */
    TOK_MISSION,        /* mission                                             */
    TOK_PARALLEL,       /* parallel                                            */
    TOK_CONSTELLATION,  /* constellation                                       */
    TOK_ABSORB,         /* absorb                                              */
    TOK_MODEL,          /* model                                               */
    TOK_LAYER,          /* layer                                               */
    TOK_STRUCT,         /* struct                                              */
    TOK_ENUM,           /* enum                                                */
    TOK_TRAIT,          /* trait                                               */
    TOK_INTERFACE,      /* interface                                           */
    TOK_IMPLEMENTS,     /* implements                                          */
    TOK_EXTENDS,        /* extends                                             */
    TOK_EXPORT,         /* export                                              */
    TOK_EXTERN,         /* extern                                              */
    TOK_UNIT_KW,        /* unit  (custom unit declaration)                    */

    /* ── Keywords: bindings ──────────────────────────────────────────────── */
    TOK_LET,            /* let                                                 */
    TOK_VAR,            /* var                                                 */

    /* ── Keywords: control flow ──────────────────────────────────────────── */
    TOK_IF,             /* if                                                  */
    TOK_ELSE,           /* else                                                */
    TOK_FOR,            /* for                                                 */
    TOK_IN,             /* in                                                  */
    TOK_WHILE,          /* while                                               */
    TOK_LOOP,           /* loop                                                */
    TOK_BREAK,          /* break                                               */
    TOK_CONTINUE,       /* continue                                            */
    TOK_RETURN,         /* return                                              */
    TOK_MATCH,          /* match                                               */

    /* ── Keywords: AI / scientific ───────────────────────────────────────── */
    TOK_AUTODIFF,       /* autodiff                                            */
    TOK_GRADIENT,       /* gradient                                            */
    TOK_WRT,            /* wrt                                                 */
    TOK_PIPELINE,       /* pipeline                                            */
    TOK_ON,             /* on                                                  */
    TOK_DEVICE,         /* device                                              */
    TOK_WAVE,           /* wave                                                */

    /* ── Keywords: output / safety ───────────────────────────────────────── */
    TOK_TRANSMIT,       /* transmit                                            */
    TOK_PANIC,          /* panic                                               */
    TOK_UNSAFE,         /* unsafe                                              */
    TOK_RAW,            /* raw                                                 */
    TOK_AS,             /* as                                                  */

    /* ── Keywords: types ─────────────────────────────────────────────────── */
    TOK_VOID,           /* Void                                                */
    TOK_NEVER,          /* Never                                               */
    TOK_SELF_KW,        /* self                                                */

    /* ── Operators: arithmetic ───────────────────────────────────────────── */
    TOK_PLUS,           /* +                                                   */
    TOK_MINUS,          /* -                                                   */
    TOK_STAR,           /* *                                                   */
    TOK_SLASH,          /* /                                                   */
    TOK_PERCENT,        /* %                                                   */
    TOK_CARET,          /* ^  (power, unit-aware)                             */
    TOK_AT,             /* @  (tensor matmul)                                 */

    /* ── Operators: comparison ───────────────────────────────────────────── */
    TOK_EQ_EQ,          /* ==                                                  */
    TOK_BANG_EQ,        /* !=                                                  */
    TOK_LT,             /* <                                                   */
    TOK_GT,             /* >                                                   */
    TOK_LT_EQ,          /* <=                                                  */
    TOK_GT_EQ,          /* >=                                                  */

    /* ── Operators: logical ──────────────────────────────────────────────── */
    TOK_AND,            /* &&                                                  */
    TOK_OR,             /* ||                                                  */
    TOK_BANG,           /* !                                                   */

    /* ── Operators: assignment ───────────────────────────────────────────── */
    TOK_EQ,             /* =                                                   */
    TOK_PLUS_EQ,        /* +=                                                  */
    TOK_MINUS_EQ,       /* -=                                                  */
    TOK_STAR_EQ,        /* *=                                                  */
    TOK_SLASH_EQ,       /* /=                                                  */

    /* ── Operators: NOVA-specific ────────────────────────────────────────── */
    TOK_ARROW,          /* ->  or  Unicode arrow                              */
    TOK_FAT_ARROW,      /* =>                                                  */
    TOK_PIPE_GT,        /* |>                                                  */
    TOK_DOT_DOT,        /* ..                                                  */
    TOK_QUESTION,       /* ?                                                   */
    TOK_AMP,            /* &                                                   */
    TOK_PIPE,           /* |  (enum variant separator / borrow)               */

    /* ── Delimiters ──────────────────────────────────────────────────────── */
    TOK_LPAREN,         /* (                                                   */
    TOK_RPAREN,         /* )                                                   */
    TOK_LBRACE,         /* {                                                   */
    TOK_RBRACE,         /* }                                                   */
    TOK_LBRACKET,       /* [                                                   */
    TOK_RBRACKET,       /* ]                                                   */
    TOK_COMMA,          /* ,                                                   */
    TOK_SEMICOLON,      /* ;                                                   */
    TOK_COLON,          /* :                                                   */
    TOK_COLON_COLON,    /* ::                                                  */
    TOK_DOT,            /* .                                                   */
    TOK_HASH,           /* #                                                   */

    /* ── Special ─────────────────────────────────────────────────────────── */
    TOK_NEWLINE,        /* significant newline                                 */
    TOK_EOF,            /* end of input                                        */
    TOK_ERROR,          /* invalid character                                   */

    TOK_COUNT

} NovaTokenType;


/* ─────────────────────────────────────────────────────────────────────────────
 * Token struct
 * `lexeme` points into the source buffer — NOT null-terminated.
 * ───────────────────────────────────────────────────────────────────────────*/
typedef struct {
    NovaTokenType  type;
    const char    *lexeme;      /* pointer into source (not owned, not null-terminated) */
    size_t         lexeme_len;
    uint32_t       line;        /* 1-based */
    uint32_t       col;         /* 1-based */
} NovaToken;


/* ── Token helpers ────────────────────────────────────────────────────────── */
const char *nova_token_type_name(NovaTokenType type);
int         nova_token_is_keyword(NovaTokenType type);
int         nova_token_is_literal(NovaTokenType type);
int         nova_token_is_binop(NovaTokenType type);

#endif /* NOVA_TOKEN_H */
