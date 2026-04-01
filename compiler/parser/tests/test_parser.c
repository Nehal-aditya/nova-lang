/*
 * NOVA Parser — test_parser.c
 *
 * Build:
 *   gcc -std=c11 -Wall \
 *     -I../include -I../../lexer/include \
 *     ../../lexer/src/lexer.c ../../lexer/src/token.c \
 *     ../src/ast.c ../src/parser.c \
 *     test_parser.c -o test_parser
 *   ./test_parser
 */

#include "../include/parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; } \
    else { tests_failed++; fprintf(stderr, "  FAIL [line %d]: %s\n", __LINE__, msg); } \
} while(0)

/* Parse src and return AST root. Caller must nova_ast_free(). */
static NovAstNode *parse_src(const char *src) {
    NovaLexer lex;
    nova_lexer_init(&lex, src, strlen(src), "<test>");
    NovParser p;
    nova_parser_init(&p, &lex);
    return nova_parse(&p);
}

static NovAstNode *parse_src_checked(const char *src, NovParser *out_p) {
    NovaLexer *lex = calloc(1, sizeof(NovaLexer));
    nova_lexer_init(lex, src, strlen(src), "<test>");
    nova_parser_init(out_p, lex);
    return nova_parse(out_p);
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_hello_universe(void) {
    printf("  hello_universe ...\n");
    const char *src =
        "mission main() -> Void {\n"
        "    transmit(\"Hello, universe!\")\n"
        "}\n";
    NovAstNode *ast = parse_src(src);
    ASSERT(ast != NULL,                              "AST not NULL");
    ASSERT(ast->type == AST_PROGRAM,                 "root is PROGRAM");
    ASSERT(ast->as.program.items.count == 1,         "one top-level item");
    NovAstNode *m = ast->as.program.items.items[0];
    ASSERT(m->type == AST_MISSION_DECL,              "mission declaration");
    ASSERT(m->as.mission_decl.name.len == 4,         "mission named 'main'");
    ASSERT(m->as.mission_decl.is_parallel == 0,      "not parallel");
    ASSERT(m->as.mission_decl.ret_type != NULL,      "has return type");
    ASSERT(m->as.mission_decl.ret_type->type == AST_VOID_TYPE, "returns Void");
    ASSERT(m->as.mission_decl.body != NULL,          "has body");
    ASSERT(m->as.mission_decl.body->type == AST_BLOCK, "body is block");
    nova_ast_free(ast);
}

static void test_let_decl(void) {
    printf("  let declaration ...\n");
    const char *src = "let mass = 5.972e24[kg]\n";
    NovAstNode *ast = parse_src(src);
    ASSERT(ast->as.program.items.count == 1,         "one item");
    NovAstNode *ld = ast->as.program.items.items[0];
    ASSERT(ld->type == AST_LET_DECL,                 "let declaration");
    ASSERT(ld->as.let_decl.name.len == 4,            "name is 'mass'");
    ASSERT(ld->as.let_decl.init != NULL,             "has initialiser");
    ASSERT(ld->as.let_decl.init->type == AST_UNIT_LIT, "init is unit literal");
    nova_ast_free(ast);
}

static void test_delta_v(void) {
    printf("  delta_v mission ...\n");
    const char *src =
        "mission delta_v(isp: Float, m_wet: Float, m_dry: Float) -> Float {\n"
        "    let g0 = 9.80665[m/s]\n"
        "    return isp * g0\n"
        "}\n";
    NovAstNode *ast = parse_src(src);
    NovAstNode *m   = ast->as.program.items.items[0];
    ASSERT(m->type == AST_MISSION_DECL,          "mission decl");
    ASSERT(m->as.mission_decl.params.count == 3, "3 parameters");
    ASSERT(m->as.mission_decl.ret_type != NULL,  "has return type");
    NovAstNode *body = m->as.mission_decl.body;
    ASSERT(body->as.block.stmts.count == 2,      "2 statements in body");
    NovAstNode *ret = body->as.block.stmts.items[1];
    ASSERT(ret->type == AST_RETURN_STMT,         "second stmt is return");
    ASSERT(ret->as.return_stmt.value != NULL,    "return has value");
    ASSERT(ret->as.return_stmt.value->type == AST_BINARY_EXPR, "return is binary expr");
    nova_ast_free(ast);
}

static void test_if_stmt(void) {
    printf("  if statement ...\n");
    const char *src =
        "mission check(x: Int) -> Void {\n"
        "    if x > 0 {\n"
        "        transmit(\"positive\")\n"
        "    } else {\n"
        "        transmit(\"non-positive\")\n"
        "    }\n"
        "}\n";
    NovAstNode *ast  = parse_src(src);
    NovAstNode *m    = ast->as.program.items.items[0];
    NovAstNode *body = m->as.mission_decl.body;
    NovAstNode *stmt = body->as.block.stmts.items[0];
    ASSERT(stmt->type == AST_IF_STMT,             "if statement");
    ASSERT(stmt->as.if_stmt.cond != NULL,         "has condition");
    ASSERT(stmt->as.if_stmt.then_block != NULL,   "has then block");
    ASSERT(stmt->as.if_stmt.else_block != NULL,   "has else block");
    nova_ast_free(ast);
}

static void test_for_loop(void) {
    printf("  for loop ...\n");
    const char *src =
        "mission count() -> Void {\n"
        "    for i in 0..10 {\n"
        "        transmit(\"x\")\n"
        "    }\n"
        "}\n";
    NovAstNode *ast  = parse_src(src);
    NovAstNode *body = ast->as.program.items.items[0]->as.mission_decl.body;
    NovAstNode *stmt = body->as.block.stmts.items[0];
    ASSERT(stmt->type == AST_FOR_STMT,           "for statement");
    ASSERT(stmt->as.for_stmt.iter != NULL,       "has iterator");
    ASSERT(stmt->as.for_stmt.iter->type == AST_RANGE_EXPR, "iter is range");
    nova_ast_free(ast);
}

static void test_absorb(void) {
    printf("  absorb ...\n");
    const char *src = "absorb cosmos.stats.{ pearson, linear_fit }\n";
    NovAstNode *ast  = parse_src(src);
    NovAstNode *ab   = ast->as.program.items.items[0];
    ASSERT(ab->type == AST_ABSORB,                     "absorb node");
    ASSERT(ab->as.absorb.path_parts.count >= 2,        "has path parts");
    ASSERT(ab->as.absorb.names.count == 2,             "2 imported names");
    nova_ast_free(ast);
}

static void test_parallel_mission(void) {
    printf("  parallel mission ...\n");
    const char *src =
        "parallel mission process(data: Array) -> Array {\n"
        "    return data\n"
        "}\n";
    NovAstNode *ast = parse_src(src);
    NovAstNode *m   = ast->as.program.items.items[0];
    ASSERT(m->type == AST_MISSION_DECL,         "mission decl");
    ASSERT(m->as.mission_decl.is_parallel == 1, "is parallel");
    nova_ast_free(ast);
}

static void test_named_args(void) {
    printf("  named arguments ...\n");
    const char *src =
        "mission main() -> Void {\n"
        "    let dv = delta_v(isp=311.0[s], m_wet=549054.0[kg])\n"
        "}\n";
    NovAstNode *ast  = parse_src(src);
    NovAstNode *body = ast->as.program.items.items[0]->as.mission_decl.body;
    NovAstNode *ld   = body->as.block.stmts.items[0];
    ASSERT(ld->type == AST_LET_DECL,                          "let decl");
    NovAstNode *call = ld->as.let_decl.init;
    ASSERT(call->type == AST_CALL_EXPR,                       "call expr");
    ASSERT(call->as.call_expr.args.count == 2,                "2 args");
    ASSERT(call->as.call_expr.args.items[0]->type == AST_NAMED_ARG, "first is named arg");
    nova_ast_free(ast);
}

static void test_autodiff(void) {
    printf("  autodiff statement ...\n");
    const char *src =
        "mission train(net: Net, data: Dataset) -> Net {\n"
        "    let loss = cross_entropy(net.forward(data.x), data.y)\n"
        "    autodiff(loss) {\n"
        "        net.update(adam)\n"
        "    }\n"
        "    return net\n"
        "}\n";
    NovAstNode *ast  = parse_src(src);
    NovAstNode *body = ast->as.program.items.items[0]->as.mission_decl.body;
    NovAstNode *ad   = body->as.block.stmts.items[1];
    ASSERT(ad->type == AST_AUTODIFF_STMT,               "autodiff stmt");
    ASSERT(ad->as.autodiff_stmt.loss_expr != NULL,      "has loss expr");
    ASSERT(ad->as.autodiff_stmt.update_block != NULL,   "has update block");
    nova_ast_free(ast);
}

static void test_unit_decl(void) {
    printf("  unit declaration ...\n");
    const char *src = "unit parsec = 3.086e16[m]\n";
    NovAstNode *ast = parse_src(src);
    NovAstNode *ud  = ast->as.program.items.items[0];
    ASSERT(ud->type == AST_UNIT_DECL,          "unit decl");
    ASSERT(ud->as.unit_decl.name.len == 6,     "name 'parsec'");
    ASSERT(ud->as.unit_decl.value != NULL,     "has value");
    nova_ast_free(ast);
}

static void test_struct_decl(void) {
    printf("  struct declaration ...\n");
    const char *src =
        "struct Star {\n"
        "    mass: Float\n"
        "    luminosity: Float\n"
        "}\n";
    NovAstNode *ast = parse_src(src);
    NovAstNode *sd  = ast->as.program.items.items[0];
    ASSERT(sd->type == AST_STRUCT_DECL,          "struct decl");
    ASSERT(sd->as.struct_decl.fields.count == 2, "2 fields");
    nova_ast_free(ast);
}

static void test_match_expr(void) {
    printf("  match expression ...\n");
    const char *src =
        "mission main() -> Void {\n"
        "    match result {\n"
        "        true  => transmit(\"yes\")\n"
        "        false => transmit(\"no\")\n"
        "    }\n"
        "}\n";
    NovAstNode *ast  = parse_src(src);
    NovAstNode *body = ast->as.program.items.items[0]->as.mission_decl.body;
    NovAstNode *stmt = body->as.block.stmts.items[0];
    NovAstNode *me   = stmt->as.expr_stmt.expr;
    ASSERT(me->type == AST_MATCH_EXPR,       "match expr");
    ASSERT(me->as.match_expr.arms.count == 2,"2 match arms");
    nova_ast_free(ast);
}

static void test_no_errors_on_valid(void) {
    printf("  no errors on valid source ...\n");
    const char *src =
        "absorb cosmos.stats.{ pearson }\n"
        "mission main() -> Void {\n"
        "    let x = 42\n"
        "    let y = 3.14[m/s]\n"
        "    transmit(\"done\")\n"
        "}\n";
    NovParser p; memset(&p, 0, sizeof(p));
    NovAstNode *ast = parse_src_checked(src, &p);
    ASSERT(!nova_parser_had_errors(&p), "no parse errors");
    nova_ast_free(ast);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
int main(void) {
    printf("\n✦ NOVA Parser Test Suite\n");
    printf("═══════════════════════════════════════\n\n");

    test_hello_universe();
    test_let_decl();
    test_delta_v();
    test_if_stmt();
    test_for_loop();
    test_absorb();
    test_parallel_mission();
    test_named_args();
    test_autodiff();
    test_unit_decl();
    test_struct_decl();
    test_match_expr();
    test_no_errors_on_valid();

    printf("\n═══════════════════════════════════════\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed == 0)
        printf("  ✦ all passed\n\n");
    else
        printf("  ✗ %d failed\n\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
