/*
 * NOVA Lexer — test_lexer.c
 *
 * Lightweight self-contained test runner.
 * No external test framework required: just compile and run.
 *
 * Build:
 *   gcc -std=c11 -Wall -I../include ../src/lexer.c ../src/token.c test_lexer.c -o test_lexer
 *   ./test_lexer
 */

#include "../include/lexer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Minimal test harness ─────────────────────────────────────────────────── */
static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; } \
    else { \
        tests_failed++; \
        fprintf(stderr, "  FAIL [line %d]: %s\n", __LINE__, msg); \
    } \
} while(0)

/* Tokenise a string and return heap-allocated token array. Caller frees. */
static NovaToken *lex(const char *src, size_t *out_count) {
    NovaLexer l;
    nova_lexer_init(&l, src, strlen(src), "<test>");
    return nova_lexer_tokenise_all(&l, out_count);
}

/* Find first token of a given type in the array. Returns -1 if not found. */
static int find_tok(const NovaToken *toks, size_t n, NovaTokenType t) {
    for (size_t i = 0; i < n; i++)
        if (toks[i].type == t) return (int)i;
    return -1;
}


/* ── Test groups ──────────────────────────────────────────────────────────── */

static void test_hello_universe(void) {
    printf("  hello_universe.nv ...\n");
    const char *src =
        "mission main() -> Void {\n"
        "    transmit(\"Hello, universe!\")\n"
        "}\n";
    size_t n; NovaToken *t = lex(src, &n);

    ASSERT(t[0].type == TOK_MISSION,    "first token is 'mission'");
    ASSERT(t[1].type == TOK_IDENT,      "second token is ident 'main'");
    ASSERT(t[2].type == TOK_LPAREN,     "third token is '('");
    ASSERT(t[3].type == TOK_RPAREN,     "')'");
    ASSERT(t[4].type == TOK_ARROW,      "'->'");
    ASSERT(t[5].type == TOK_VOID,       "'Void'");
    ASSERT(t[6].type == TOK_LBRACE,     "'{'");
    ASSERT(find_tok(t, n, TOK_TRANSMIT) >= 0, "contains 'transmit'");
    ASSERT(find_tok(t, n, TOK_STRING_LIT) >= 0, "contains string literal");

    free(t);
}

static void test_keywords(void) {
    printf("  keywords ...\n");
    const char *src =
        "let var if else for in while loop break continue return match\n"
        "mission parallel constellation absorb model layer struct enum\n"
        "trait interface implements extends export extern unit\n"
        "autodiff gradient wrt pipeline on device wave\n"
        "transmit panic unsafe raw as Void Never self\n"
        "true false\n";
    size_t n; NovaToken *t = lex(src, &n);

    /* spot-check a few */
    ASSERT(find_tok(t, n, TOK_LET)        >= 0, "'let' keyword");
    ASSERT(find_tok(t, n, TOK_MISSION)    >= 0, "'mission' keyword");
    ASSERT(find_tok(t, n, TOK_AUTODIFF)   >= 0, "'autodiff' keyword");
    ASSERT(find_tok(t, n, TOK_TRANSMIT)   >= 0, "'transmit' keyword");
    ASSERT(find_tok(t, n, TOK_VOID)       >= 0, "'Void' keyword");
    ASSERT(find_tok(t, n, TOK_BOOL_LIT)   >= 0, "'true' -> BOOL_LIT");

    free(t);
}

static void test_unit_literals(void) {
    printf("  unit literals ...\n");
    struct { const char *src; } cases[] = {
        { "9.8[m/s]" },
        { "845000.0[N]" },
        { "311.0[s]" },
        { "1.0[kg]" },
        { "6.674e-11[N*m^2/kg^2]" },
        { "1.989e30[kg]" },
        { NULL }
    };
    for (int i = 0; cases[i].src; i++) {
        size_t n; NovaToken *t = lex(cases[i].src, &n);
        ASSERT(t[0].type == TOK_UNIT_LIT, cases[i].src);
        /* lexeme should span the whole "value[unit]" */
        ASSERT(t[0].lexeme_len == strlen(cases[i].src), "full lexeme length");
        free(t);
    }
}

static void test_operators(void) {
    printf("  operators ...\n");
    const char *src = "-> => |> .. ? + - * / % ^ @ == != < > <= >= && || ! = += -= *= /=";
    size_t n; NovaToken *t = lex(src, &n);

    ASSERT(find_tok(t, n, TOK_ARROW)    >= 0, "'->'");
    ASSERT(find_tok(t, n, TOK_FAT_ARROW)>= 0, "'=>'");
    ASSERT(find_tok(t, n, TOK_PIPE_GT)  >= 0, "'|>'");
    ASSERT(find_tok(t, n, TOK_DOT_DOT)  >= 0, "'..'");
    ASSERT(find_tok(t, n, TOK_QUESTION) >= 0, "'?'");
    ASSERT(find_tok(t, n, TOK_EQ_EQ)    >= 0, "'=='");
    ASSERT(find_tok(t, n, TOK_BANG_EQ)  >= 0, "'!='");
    ASSERT(find_tok(t, n, TOK_LT_EQ)    >= 0, "'<='");
    ASSERT(find_tok(t, n, TOK_GT_EQ)    >= 0, "'>='");
    ASSERT(find_tok(t, n, TOK_AND)      >= 0, "'&&'");
    ASSERT(find_tok(t, n, TOK_OR)       >= 0, "'||'");
    ASSERT(find_tok(t, n, TOK_PLUS_EQ)  >= 0, "'+='");

    free(t);
}

static void test_unicode_arrow(void) {
    printf("  unicode arrow ...\n");
    /* UTF-8 for → is E2 86 92 */
    const char *src = "mission foo() \xE2\x86\x92 Void { }";
    size_t n; NovaToken *t = lex(src, &n);
    ASSERT(find_tok(t, n, TOK_ARROW) >= 0, "Unicode → recognized as TOK_ARROW");
    free(t);
}

static void test_comments(void) {
    printf("  comments ...\n");
    const char *src =
        "-- this whole line is a comment\n"
        "let x = 42  -- inline comment\n"
        "let y = 10\n";
    size_t n; NovaToken *t = lex(src, &n);
    /* Should have: NEWLINE LET IDENT EQ INT_LIT NEWLINE LET IDENT EQ INT_LIT NEWLINE EOF */
    ASSERT(find_tok(t, n, TOK_LET) >= 0, "let after comment");
    /* Count INT_LIT tokens — should be exactly 2 */
    int int_count = 0;
    for (size_t i = 0; i < n; i++)
        if (t[i].type == TOK_INT_LIT) int_count++;
    ASSERT(int_count == 2, "exactly 2 int literals (comments ignored)");
    free(t);
}

static void test_string_interpolation(void) {
    printf("  string interpolation ...\n");
    const char *src = "\"r = {r:.4}  v = {v as [km/s]:.2} km/s\"";
    size_t n; NovaToken *t = lex(src, &n);
    ASSERT(n == 2, "exactly 2 tokens: STRING_LIT + EOF");
    ASSERT(t[0].type == TOK_STRING_LIT, "string literal with interpolation");
    free(t);
}

static void test_delta_v(void) {
    printf("  delta_v.nv snippet ...\n");
    const char *src =
        "mission delta_v(isp: Float[s], m_wet: Float[kg], m_dry: Float[kg]) -> Float[m/s] {\n"
        "    let g0 = 9.80665[m/s]\n"
        "    return isp * g0 * ln(m_wet / m_dry)\n"
        "}\n";
    size_t n; NovaToken *t = lex(src, &n);

    ASSERT(t[0].type == TOK_MISSION,  "starts with mission");
    ASSERT(find_tok(t, n, TOK_UNIT_LIT) >= 0, "contains unit literals");
    ASSERT(find_tok(t, n, TOK_ARROW)    >= 0, "contains ->");
    ASSERT(find_tok(t, n, TOK_RETURN)   >= 0, "contains return");
    ASSERT(find_tok(t, n, TOK_STAR)     >= 0, "contains *");
    ASSERT(find_tok(t, n, TOK_SLASH)    >= 0, "contains /");
    ASSERT(!nova_lexer_had_errors(&(NovaLexer){0}), "no lex errors expected");

    free(t);
}

static void test_line_numbers(void) {
    printf("  line numbers ...\n");
    const char *src = "let a = 1\nlet b = 2\nlet c = 3\n";
    size_t n; NovaToken *t = lex(src, &n);
    /* Find the three 'let' tokens and check their line numbers */
    int let_lines[3] = {0}; int li = 0;
    for (size_t i = 0; i < n && li < 3; i++)
        if (t[i].type == TOK_LET) let_lines[li++] = t[i].line;
    ASSERT(let_lines[0] == 1, "first let on line 1");
    ASSERT(let_lines[1] == 2, "second let on line 2");
    ASSERT(let_lines[2] == 3, "third let on line 3");
    free(t);
}

static void test_error_token(void) {
    printf("  error token ...\n");
    const char *src = "let x = $invalid";
    size_t n; NovaToken *t = lex(src, &n);
    ASSERT(find_tok(t, n, TOK_ERROR) >= 0, "'$' produces TOK_ERROR");
    free(t);
}


/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    printf("\n✦ NOVA Lexer Test Suite\n");
    printf("═══════════════════════════════════════\n\n");

    test_hello_universe();
    test_keywords();
    test_unit_literals();
    test_operators();
    test_unicode_arrow();
    test_comments();
    test_string_interpolation();
    test_delta_v();
    test_line_numbers();
    test_error_token();

    printf("\n═══════════════════════════════════════\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed == 0)
        printf("  ✦ all passed\n\n");
    else
        printf("  ✗ %d failed\n\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
