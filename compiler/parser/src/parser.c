/*
 * NOVA Parser — parser.c
 *
 * Recursive-descent parser for NOVA (.nv) source.
 * Consumes the token stream produced by the lexer.
 * Produces a NovAstNode* tree rooted at AST_PROGRAM.
 *
 * Grammar overview (simplified, see docs/grammar.ebnf for full spec):
 *
 *   program       := top_level*  EOF
 *   top_level     := mission_decl | constellation_decl | absorb
 *                  | let_decl | var_decl | struct_decl | enum_decl
 *                  | trait_decl | interface_decl | model_decl | unit_decl
 *   mission_decl  := ['parallel'] 'mission' IDENT '(' params ')' '->' type block
 *   block         := '{' stmt* '}'
 *   stmt          := let_decl | var_decl | return_stmt | if_stmt
 *                  | for_stmt | while_stmt | loop_stmt | break | continue
 *                  | autodiff_stmt | on_device_stmt | expr_stmt
 *   expr          := assignment
 *   assignment    := pipe ['=' assignment]
 *   pipe          := range ('|>' (pipeline_expr | expr))*
 *   range         := or ('..' or)?
 *   or            := and ('||' and)*
 *   and           := equality ('&&' equality)*
 *   equality      := comparison (('==' | '!=') comparison)*
 *   comparison    := addition (('<'|'>'|'<='|'>=') addition)*
 *   addition      := multiply (('+' | '-') multiply)*
 *   multiply      := power (('*' | '/' | '%') power)*
 *   power         := unary ('^' unary)*
 *   unary         := ('!' | '-') unary | postfix
 *   postfix       := primary ('.' IDENT | '(' args ')' | '[' expr ']'
 *                            | '?' | 'as' type)*
 *   primary       := literal | IDENT | 'transmit' '(' expr ')'
 *                  | 'panic' '(' expr ')'
 *                  | 'match' expr '{' arms '}'
 *                  | 'gradient' '(' expr ',' 'wrt' expr ')'
 *                  | '(' expr ')' | '[' elements ']'
 */

#include "../include/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal helpers
 * ───────────────────────────────────────────────────────────────────────────*/

/* Consume and return the current token, advance to the next. */
static NovaToken advance_tok(NovParser *p) {
    NovaToken prev = p->current;
    p->current = p->next;
    p->next    = nova_lexer_next(p->lex);
    /* Skip insignificant newlines inside expressions (handled contextually) */
    return prev;
}

/* Look at the current token type without consuming. */
static inline NovaTokenType cur_type(const NovParser *p) {
    return p->current.type;
}

/* Look at the next (lookahead-1) token type without consuming. */
static inline NovaTokenType peek_type(const NovParser *p) {
    return p->next.type;
}

/* Return 1 if the current token is of the given type. */
static inline int check(const NovParser *p, NovaTokenType t) {
    return p->current.type == t;
}

/* Advance if current token matches t. Returns 1 on match. */
static int match_tok(NovParser *p, NovaTokenType t) {
    if (check(p, t)) { advance_tok(p); return 1; }
    return 0;
}

/* Skip all TOK_NEWLINE tokens. */
static void skip_newlines(NovParser *p) {
    while (check(p, TOK_NEWLINE)) advance_tok(p);
}

/* Record a parse error and enter panic mode for recovery. */
static void parse_error(NovParser *p, const char *msg) {
    if (p->panic_mode) return; /* suppress cascading errors */
    p->panic_mode = 1;
    if (p->error_count >= NOVA_PARSER_MAX_ERRORS) return;
    NovParseError *e = &p->errors[p->error_count++];
    e->line = p->current.line;
    e->col  = p->current.col;
    snprintf(e->message, sizeof(e->message), "%s (got '%s')",
             msg, nova_token_type_name(p->current.type));
}

/* Consume the expected token or record an error. Returns the consumed token. */
static NovaToken expect(NovParser *p, NovaTokenType t, const char *ctx) {
    if (check(p, t)) return advance_tok(p);
    char msg[256];
    snprintf(msg, sizeof(msg), "expected %s in %s",
             nova_token_type_name(t), ctx);
    parse_error(p, msg);
    return p->current; /* return current without advancing */
}

/* Panic-mode recovery: advance until we find a synchronisation token. */
static void synchronise(NovParser *p) {
    p->panic_mode = 0;
    while (!check(p, TOK_EOF)) {
        if (check(p, TOK_NEWLINE) || check(p, TOK_RBRACE)) {
            advance_tok(p);
            return;
        }
        switch (cur_type(p)) {
            case TOK_MISSION: case TOK_LET: case TOK_VAR:
            case TOK_RETURN:  case TOK_IF:  case TOK_FOR:
            case TOK_WHILE:   case TOK_LOOP:
                return;
            default: break;
        }
        advance_tok(p);
    }
}

/* Helper: make NovString from a NovaToken's lexeme. */
static NovString str_from_tok(const NovaToken *tok) {
    NovString s = { tok->lexeme, tok->lexeme_len };
    return s;
}

/* Helper: allocate a new node at the current token's position. */
static NovAstNode *new_at_cur(NovParser *p, NovAstNodeType type) {
    return nova_ast_node_new(type, p->current.line, p->current.col);
}

/* Forward declarations */
static NovAstNode *parse_expr(NovParser *p);
static NovAstNode *parse_block(NovParser *p);
static NovAstNode *parse_type(NovParser *p);
static NovAstNode *parse_stmt(NovParser *p);
static NovAstNode *parse_top_level(NovParser *p);


/* ─────────────────────────────────────────────────────────────────────────────
 * Type parsing
 * ───────────────────────────────────────────────────────────────────────────*/

static NovAstNode *parse_type(NovParser *p) {
    /* Void / Never */
    if (check(p, TOK_VOID)) {
        NovAstNode *n = new_at_cur(p, AST_VOID_TYPE);
        advance_tok(p); return n;
    }
    if (check(p, TOK_NEVER)) {
        NovAstNode *n = new_at_cur(p, AST_NEVER_TYPE);
        advance_tok(p); return n;
    }

    /* Named type: Float, Int, Str, CosmoTransformer, ... */
    if (check(p, TOK_IDENT)) {
        NovAstNode *n = new_at_cur(p, AST_NAMED_TYPE);
        n->as.named_type.name = str_from_tok(&p->current);
        NovaToken name_tok = advance_tok(p);
        (void)name_tok;

        /* Check for [ — either unit annotation or generic */
        if (check(p, TOK_LBRACKET)) {
            advance_tok(p); /* consume [ */
            skip_newlines(p);

            /* Tensor[elem_type, shape] */
            if (n->as.named_type.name.len == 6 &&
                memcmp(n->as.named_type.name.ptr, "Tensor", 6) == 0) {
                NovAstNode *tn = nova_ast_node_new(AST_TENSOR_TYPE, n->line, n->col);
                tn->as.tensor_type.elem_type = parse_type(p);
                match_tok(p, TOK_COMMA);
                skip_newlines(p);
                tn->as.tensor_type.shape = parse_expr(p);
                skip_newlines(p);
                expect(p, TOK_RBRACKET, "Tensor type");
                nova_ast_free(n);
                return tn;
            }

            /* Float[unit] — unit type annotation */
            /* Peek: if content looks like a unit (letters, / * ^), treat as unit */
            NovAstNode *ut = nova_ast_node_new(AST_UNIT_TYPE, n->line, n->col);
            ut->as.unit_type.base = n;
            /* Collect unit tokens until ] */
            const char *unit_start = p->current.lexeme;
            size_t unit_len = 0;
            while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) {
                unit_len = (size_t)(p->current.lexeme - unit_start) + p->current.lexeme_len;
                advance_tok(p);
            }
            ut->as.unit_type.unit_str.ptr = unit_start;
            ut->as.unit_type.unit_str.len = unit_len;
            expect(p, TOK_RBRACKET, "unit type");
            return ut;
        }

        /* Option[T], Result[T,E], Array[T], Dataset[T,U], Wave */
        if (check(p, TOK_LBRACKET)) {
            NovAstNode *gt = nova_ast_node_new(AST_GENERIC_TYPE, n->line, n->col);
            gt->as.generic_type.name     = n->as.named_type.name;
            gt->as.generic_type.type_args = nova_nodelist_new();
            nova_ast_free(n);
            advance_tok(p); /* consume [ */
            skip_newlines(p);
            while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) {
                nova_nodelist_push(&gt->as.generic_type.type_args, parse_type(p));
                if (!match_tok(p, TOK_COMMA)) break;
                skip_newlines(p);
            }
            expect(p, TOK_RBRACKET, "generic type");
            return gt;
        }

        return n;
    }

    parse_error(p, "expected a type");
    return NULL;
}


/* ─────────────────────────────────────────────────────────────────────────────
 * Expression parsing — Pratt / recursive descent
 * ───────────────────────────────────────────────────────────────────────────*/

/* Parse a comma-separated argument list until ')'. Handles named args. */
static void parse_arg_list(NovParser *p, NovNodeList *args) {
    *args = nova_nodelist_new();
    skip_newlines(p);
    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        /* Named arg: name = expr */
        if (check(p, TOK_IDENT) && peek_type(p) == TOK_EQ) {
            NovAstNode *na = new_at_cur(p, AST_NAMED_ARG);
            na->as.named_arg.name  = str_from_tok(&p->current);
            advance_tok(p); /* consume ident */
            advance_tok(p); /* consume = */
            na->as.named_arg.value = parse_expr(p);
            nova_nodelist_push(args, na);
        } else {
            nova_nodelist_push(args, parse_expr(p));
        }
        skip_newlines(p);
        if (!match_tok(p, TOK_COMMA)) break;
        skip_newlines(p);
    }
}

/* primary := literal | ident | '(' expr ')' | special forms */
static NovAstNode *parse_primary(NovParser *p) {
    uint32_t line = p->current.line, col = p->current.col;

    /* Integer literal */
    if (check(p, TOK_INT_LIT)) {
        NovAstNode *n = nova_ast_node_new(AST_INT_LIT, line, col);
        /* Parse the lexeme as int64 */
        char buf[64]; size_t len = p->current.lexeme_len;
        if (len >= sizeof(buf)) len = sizeof(buf)-1;
        memcpy(buf, p->current.lexeme, len); buf[len] = '\0';
        n->as.int_lit.value = (int64_t)strtoll(buf, NULL, 0);
        advance_tok(p);
        return n;
    }

    /* Float literal */
    if (check(p, TOK_FLOAT_LIT)) {
        NovAstNode *n = nova_ast_node_new(AST_FLOAT_LIT, line, col);
        char buf[64]; size_t len = p->current.lexeme_len;
        if (len >= sizeof(buf)) len = sizeof(buf)-1;
        memcpy(buf, p->current.lexeme, len); buf[len] = '\0';
        n->as.float_lit.value = strtod(buf, NULL);
        advance_tok(p);
        return n;
    }

    /* Unit literal: "9.8[m/s]" — lexeme spans the whole thing */
    if (check(p, TOK_UNIT_LIT)) {
        NovAstNode *n = nova_ast_node_new(AST_UNIT_LIT, line, col);
        const char *lex = p->current.lexeme;
        size_t      len = p->current.lexeme_len;
        /* Split at '[' */
        size_t bracket = 0;
        for (size_t i = 0; i < len; i++) {
            if (lex[i] == '[') { bracket = i; break; }
        }
        char numbuf[64];
        size_t nlen = bracket < sizeof(numbuf) ? bracket : sizeof(numbuf)-1;
        memcpy(numbuf, lex, nlen); numbuf[nlen] = '\0';
        n->as.unit_lit.value    = strtod(numbuf, NULL);
        /* unit_str points to the content between [ and ] */
        n->as.unit_lit.unit_str.ptr = lex + bracket + 1;
        n->as.unit_lit.unit_str.len = len - bracket - 2; /* exclude [ and ] */
        advance_tok(p);
        return n;
    }

    /* String literal */
    if (check(p, TOK_STRING_LIT)) {
        NovAstNode *n = nova_ast_node_new(AST_STRING_LIT, line, col);
        n->as.string_lit.value = str_from_tok(&p->current);
        advance_tok(p);
        return n;
    }

    /* Bool literal */
    if (check(p, TOK_BOOL_LIT)) {
        NovAstNode *n = nova_ast_node_new(AST_BOOL_LIT, line, col);
        n->as.bool_lit.value = (p->current.lexeme[0] == 't') ? 1 : 0;
        advance_tok(p);
        return n;
    }

    /* transmit(expr) */
    if (check(p, TOK_TRANSMIT)) {
        NovAstNode *n = nova_ast_node_new(AST_TRANSMIT_EXPR, line, col);
        advance_tok(p);
        expect(p, TOK_LPAREN, "transmit");
        n->as.transmit_expr.arg = parse_expr(p);
        expect(p, TOK_RPAREN, "transmit");
        return n;
    }

    /* panic(expr) */
    if (check(p, TOK_PANIC)) {
        NovAstNode *n = nova_ast_node_new(AST_PANIC_EXPR, line, col);
        advance_tok(p);
        expect(p, TOK_LPAREN, "panic");
        n->as.panic_expr.arg = parse_expr(p);
        expect(p, TOK_RPAREN, "panic");
        return n;
    }

    /* match expr { arm => body, ... } */
    if (check(p, TOK_MATCH)) {
        NovAstNode *n = nova_ast_node_new(AST_MATCH_EXPR, line, col);
        advance_tok(p);
        n->as.match_expr.subject = parse_expr(p);
        n->as.match_expr.arms    = nova_nodelist_new();
        expect(p, TOK_LBRACE, "match");
        skip_newlines(p);
        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            NovAstNode *arm = nova_ast_node_new(AST_MATCH_ARM, p->current.line, p->current.col);
            arm->as.match_arm.pattern = parse_expr(p);
            /* Optional guard: if cond */
            if (check(p, TOK_IF)) {
                advance_tok(p);
                arm->as.match_arm.guard = parse_expr(p);
            }
            expect(p, TOK_FAT_ARROW, "match arm");
            arm->as.match_arm.body = parse_expr(p);
            nova_nodelist_push(&n->as.match_expr.arms, arm);
            skip_newlines(p);
            match_tok(p, TOK_COMMA);
            skip_newlines(p);
        }
        expect(p, TOK_RBRACE, "match");
        return n;
    }

    /* gradient(expr, wrt target) */
    if (check(p, TOK_GRADIENT)) {
        NovAstNode *n = nova_ast_node_new(AST_GRADIENT_EXPR, line, col);
        advance_tok(p);
        expect(p, TOK_LPAREN, "gradient");
        n->as.gradient_expr.expr = parse_expr(p);
        expect(p, TOK_COMMA, "gradient");
        expect(p, TOK_WRT, "gradient wrt");
        n->as.gradient_expr.target = parse_expr(p);
        expect(p, TOK_RPAREN, "gradient");
        return n;
    }

    /* Identifier or path */
    if (check(p, TOK_IDENT)) {
        NovAstNode *n = nova_ast_node_new(AST_IDENT, line, col);
        n->as.ident.name = str_from_tok(&p->current);
        advance_tok(p);
        return n;
    }

    /* Grouped expression: ( expr ) */
    if (check(p, TOK_LPAREN)) {
        advance_tok(p);
        NovAstNode *n = parse_expr(p);
        expect(p, TOK_RPAREN, "grouped expression");
        return n;
    }

    /* Array literal: [ elem, ... ] */
    if (check(p, TOK_LBRACKET)) {
        NovAstNode *n = nova_ast_node_new(AST_ARRAY_LITERAL, line, col);
        n->as.array_literal.elements = nova_nodelist_new();
        advance_tok(p);
        skip_newlines(p);
        while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) {
            nova_nodelist_push(&n->as.array_literal.elements, parse_expr(p));
            skip_newlines(p);
            if (!match_tok(p, TOK_COMMA)) break;
            skip_newlines(p);
        }
        expect(p, TOK_RBRACKET, "array literal");
        return n;
    }

    parse_error(p, "expected an expression");
    return NULL;
}

/* postfix: handle . field, ( args ), [ index ], ?, as type */
static NovAstNode *parse_postfix(NovParser *p) {
    NovAstNode *node = parse_primary(p);
    if (!node) return NULL;

    for (;;) {
        uint32_t line = p->current.line, col = p->current.col;

        /* Field access: expr.field */
        if (check(p, TOK_DOT)) {
            advance_tok(p);
            NovAstNode *fe = nova_ast_node_new(AST_FIELD_EXPR, line, col);
            fe->as.field_expr.object = node;
            if (check(p, TOK_IDENT)) {
                fe->as.field_expr.field = str_from_tok(&p->current);
                advance_tok(p);
            } else {
                parse_error(p, "expected field name after '.'");
            }
            node = fe;
            continue;
        }

        /* Call: expr( args ) */
        if (check(p, TOK_LPAREN)) {
            advance_tok(p);
            NovAstNode *call = nova_ast_node_new(AST_CALL_EXPR, line, col);
            call->as.call_expr.callee = node;
            parse_arg_list(p, &call->as.call_expr.args);
            expect(p, TOK_RPAREN, "function call");
            node = call;
            continue;
        }

        /* Index: expr[index] */
        if (check(p, TOK_LBRACKET)) {
            advance_tok(p);
            NovAstNode *idx = nova_ast_node_new(AST_INDEX_EXPR, line, col);
            idx->as.index_expr.object = node;
            idx->as.index_expr.index  = parse_expr(p);
            expect(p, TOK_RBRACKET, "index expression");
            node = idx;
            continue;
        }

        /* Propagation: expr? */
        if (check(p, TOK_QUESTION)) {
            advance_tok(p);
            NovAstNode *q = nova_ast_node_new(AST_QUESTION_EXPR, line, col);
            q->as.question_expr.expr = node;
            node = q;
            continue;
        }

        /* Cast / unit conversion: expr as [unit] or expr as Type */
        if (check(p, TOK_AS)) {
            advance_tok(p);
            NovAstNode *as_n = nova_ast_node_new(AST_AS_EXPR, line, col);
            as_n->as.as_expr.expr        = node;
            as_n->as.as_expr.target_type = parse_type(p);
            node = as_n;
            continue;
        }

        break;
    }
    return node;
}

/* Unary: ! expr  or  -expr */
static NovAstNode *parse_unary(NovParser *p) {
    if (check(p, TOK_BANG) || check(p, TOK_MINUS)) {
        NovAstNode *n = new_at_cur(p, AST_UNARY_EXPR);
        n->as.unary_expr.op = cur_type(p);
        advance_tok(p);
        n->as.unary_expr.operand = parse_unary(p);
        return n;
    }
    return parse_postfix(p);
}

/* Power: unary (^ unary)* — right-associative */
static NovAstNode *parse_power(NovParser *p) {
    NovAstNode *lhs = parse_unary(p);
    if (check(p, TOK_CARET)) {
        uint32_t line = p->current.line, col = p->current.col;
        advance_tok(p);
        NovAstNode *n = nova_ast_node_new(AST_BINARY_EXPR, line, col);
        n->as.binary_expr.op  = TOK_CARET;
        n->as.binary_expr.lhs = lhs;
        n->as.binary_expr.rhs = parse_power(p); /* right-assoc */
        return n;
    }
    return lhs;
}

/* Helper: parse left-assoc binary operators */
#define PARSE_BINOP(name, next, ...)                                       \
static NovAstNode *name(NovParser *p) {                                    \
    NovAstNode *lhs = next(p);                                             \
    NovaTokenType ops[] = { __VA_ARGS__, TOK_COUNT };                      \
    for (;;) {                                                             \
        int matched = 0;                                                   \
        for (int i = 0; ops[i] != TOK_COUNT; i++) {                        \
            if (!check(p, ops[i])) continue;                               \
            uint32_t ln = p->current.line, cl = p->current.col;           \
            NovaTokenType op = cur_type(p); advance_tok(p);               \
            NovAstNode *n = nova_ast_node_new(AST_BINARY_EXPR, ln, cl);   \
            n->as.binary_expr.op = op;                                     \
            n->as.binary_expr.lhs = lhs;                                  \
            n->as.binary_expr.rhs = next(p);                              \
            lhs = n; matched = 1; break;                                   \
        }                                                                  \
        if (!matched) break;                                               \
    }                                                                      \
    return lhs;                                                            \
}

PARSE_BINOP(parse_multiply, parse_power, TOK_STAR, TOK_SLASH, TOK_PERCENT, TOK_AT)
PARSE_BINOP(parse_addition, parse_multiply, TOK_PLUS, TOK_MINUS)
PARSE_BINOP(parse_comparison, parse_addition, TOK_LT, TOK_GT, TOK_LT_EQ, TOK_GT_EQ)
PARSE_BINOP(parse_equality, parse_comparison, TOK_EQ_EQ, TOK_BANG_EQ)
PARSE_BINOP(parse_and, parse_equality, TOK_AND)
PARSE_BINOP(parse_or, parse_and, TOK_OR)

/* Range: or '..' or */
static NovAstNode *parse_range(NovParser *p) {
    NovAstNode *lo = parse_or(p);
    if (check(p, TOK_DOT_DOT)) {
        uint32_t line = p->current.line, col = p->current.col;
        advance_tok(p);
        NovAstNode *n = nova_ast_node_new(AST_RANGE_EXPR, line, col);
        n->as.range_expr.lo = lo;
        n->as.range_expr.hi = parse_or(p);
        return n;
    }
    return lo;
}

/* Pipe: range ('|>' ...)* */
static NovAstNode *parse_pipe(NovParser *p) {
    NovAstNode *lhs = parse_range(p);
    while (check(p, TOK_PIPE_GT)) {
        uint32_t line = p->current.line, col = p->current.col;
        advance_tok(p);
        skip_newlines(p);

        /* pipeline [ steps ] */
        if (check(p, TOK_PIPELINE)) {
            advance_tok(p);
            NovAstNode *pl = nova_ast_node_new(AST_PIPELINE_EXPR, line, col);
            pl->as.pipeline_expr.source = lhs;
            pl->as.pipeline_expr.steps  = nova_nodelist_new();
            expect(p, TOK_LBRACKET, "pipeline");
            skip_newlines(p);
            while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) {
                nova_nodelist_push(&pl->as.pipeline_expr.steps, parse_expr(p));
                skip_newlines(p);
                match_tok(p, TOK_COMMA);
                skip_newlines(p);
            }
            expect(p, TOK_RBRACKET, "pipeline");
            lhs = pl;
        } else {
            /* simple pipe: lhs |> rhs */
            NovAstNode *rhs  = parse_range(p);
            NovAstNode *pipe = nova_ast_node_new(AST_PIPE_EXPR, line, col);
            pipe->as.pipe_expr.lhs = lhs;
            pipe->as.pipe_expr.rhs = rhs;
            lhs = pipe;
        }
    }
    return lhs;
}

/* Assignment (right-assoc): pipe ['=' assignment] */
static NovAstNode *parse_assignment(NovParser *p) {
    NovAstNode *lhs = parse_pipe(p);
    if (check(p, TOK_EQ) || check(p, TOK_PLUS_EQ) ||
        check(p, TOK_MINUS_EQ) || check(p, TOK_STAR_EQ) || check(p, TOK_SLASH_EQ)) {
        uint32_t line = p->current.line, col = p->current.col;
        NovaTokenType op = cur_type(p); advance_tok(p);
        NovAstNode *n = nova_ast_node_new(AST_BINARY_EXPR, line, col);
        n->as.binary_expr.op  = op;
        n->as.binary_expr.lhs = lhs;
        n->as.binary_expr.rhs = parse_assignment(p); /* right-assoc */
        return n;
    }
    return lhs;
}

static NovAstNode *parse_expr(NovParser *p) {
    return parse_assignment(p);
}


/* ─────────────────────────────────────────────────────────────────────────────
 * Statement parsing
 * ───────────────────────────────────────────────────────────────────────────*/

static NovAstNode *parse_block(NovParser *p) {
    NovAstNode *block = new_at_cur(p, AST_BLOCK);
    block->as.block.stmts = nova_nodelist_new();
    expect(p, TOK_LBRACE, "block");
    skip_newlines(p);
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        NovAstNode *stmt = parse_stmt(p);
        if (stmt) nova_nodelist_push(&block->as.block.stmts, stmt);
        skip_newlines(p);
        if (p->panic_mode) synchronise(p);
    }
    expect(p, TOK_RBRACE, "block");
    return block;
}

static NovAstNode *parse_let_or_var(NovParser *p) {
    int is_let = check(p, TOK_LET);
    NovAstNode *n = new_at_cur(p, is_let ? AST_LET_DECL : AST_VAR_DECL);
    advance_tok(p);

    if (check(p, TOK_IDENT)) {
        n->as.let_decl.name = str_from_tok(&p->current);
        advance_tok(p);
    } else {
        parse_error(p, "expected name after let/var");
    }

    /* Optional type annotation */
    if (match_tok(p, TOK_COLON))
        n->as.let_decl.type_node = parse_type(p);

    /* Initialiser */
    if (match_tok(p, TOK_EQ))
        n->as.let_decl.init = parse_expr(p);

    return n;
}

static NovAstNode *parse_stmt(NovParser *p) {
    uint32_t line = p->current.line, col = p->current.col;

    /* let / var */
    if (check(p, TOK_LET) || check(p, TOK_VAR))
        return parse_let_or_var(p);

    /* return */
    if (check(p, TOK_RETURN)) {
        NovAstNode *n = nova_ast_node_new(AST_RETURN_STMT, line, col);
        advance_tok(p);
        if (!check(p, TOK_NEWLINE) && !check(p, TOK_RBRACE) && !check(p, TOK_EOF))
            n->as.return_stmt.value = parse_expr(p);
        return n;
    }

    /* if */
    if (check(p, TOK_IF)) {
        NovAstNode *n = nova_ast_node_new(AST_IF_STMT, line, col);
        advance_tok(p);
        n->as.if_stmt.cond       = parse_expr(p);
        n->as.if_stmt.then_block = parse_block(p);
        if (check(p, TOK_ELSE)) {
            advance_tok(p);
            if (check(p, TOK_IF))
                n->as.if_stmt.else_block = parse_stmt(p);
            else
                n->as.if_stmt.else_block = parse_block(p);
        }
        return n;
    }

    /* for VAR in EXPR { body } */
    if (check(p, TOK_FOR)) {
        NovAstNode *n = nova_ast_node_new(AST_FOR_STMT, line, col);
        advance_tok(p);
        if (check(p, TOK_IDENT)) {
            n->as.for_stmt.var = str_from_tok(&p->current);
            advance_tok(p);
        }
        expect(p, TOK_IN, "for statement");
        n->as.for_stmt.iter = parse_expr(p);
        n->as.for_stmt.body = parse_block(p);
        return n;
    }

    /* while */
    if (check(p, TOK_WHILE)) {
        NovAstNode *n = nova_ast_node_new(AST_WHILE_STMT, line, col);
        advance_tok(p);
        n->as.while_stmt.cond = parse_expr(p);
        n->as.while_stmt.body = parse_block(p);
        return n;
    }

    /* loop */
    if (check(p, TOK_LOOP)) {
        NovAstNode *n = nova_ast_node_new(AST_LOOP_STMT, line, col);
        advance_tok(p);
        n->as.loop_stmt.body = parse_block(p);
        return n;
    }

    /* break */
    if (check(p, TOK_BREAK)) {
        NovAstNode *n = nova_ast_node_new(AST_BREAK_STMT, line, col);
        advance_tok(p); return n;
    }

    /* continue */
    if (check(p, TOK_CONTINUE)) {
        NovAstNode *n = nova_ast_node_new(AST_CONTINUE_STMT, line, col);
        advance_tok(p); return n;
    }

    /* autodiff(loss) { update } */
    if (check(p, TOK_AUTODIFF)) {
        NovAstNode *n = nova_ast_node_new(AST_AUTODIFF_STMT, line, col);
        advance_tok(p);
        expect(p, TOK_LPAREN, "autodiff");
        n->as.autodiff_stmt.loss_expr = parse_expr(p);
        expect(p, TOK_RPAREN, "autodiff");
        n->as.autodiff_stmt.update_block = parse_block(p);
        return n;
    }

    /* on device(gpu) { body } */
    if (check(p, TOK_ON)) {
        NovAstNode *n = nova_ast_node_new(AST_ON_DEVICE_STMT, line, col);
        advance_tok(p);
        expect(p, TOK_DEVICE, "on device");
        expect(p, TOK_LPAREN, "on device");
        if (check(p, TOK_IDENT)) {
            n->as.on_device_stmt.device_name = str_from_tok(&p->current);
            advance_tok(p);
        }
        expect(p, TOK_RPAREN, "on device");
        n->as.on_device_stmt.body = parse_block(p);
        return n;
    }

    /* Expression statement (includes transmit, panic, assignments) */
    NovAstNode *expr = parse_expr(p);
    if (!expr) return NULL;
    NovAstNode *stmt = nova_ast_node_new(AST_EXPR_STMT, line, col);
    stmt->as.expr_stmt.expr = expr;
    return stmt;
}


/* ─────────────────────────────────────────────────────────────────────────────
 * Top-level declarations
 * ───────────────────────────────────────────────────────────────────────────*/

/* Parse function parameter list: (name: type, ...) */
static void parse_params(NovParser *p, NovNodeList *params) {
    *params = nova_nodelist_new();
    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        NovAstNode *param = new_at_cur(p, AST_PARAM);
        if (check(p, TOK_IDENT)) {
            param->as.param.name = str_from_tok(&p->current);
            advance_tok(p);
        }
        if (match_tok(p, TOK_COLON))
            param->as.param.type_node = parse_type(p);
        nova_nodelist_push(params, param);
        if (!match_tok(p, TOK_COMMA)) break;
        skip_newlines(p);
    }
}

static NovAstNode *parse_mission_decl(NovParser *p, int is_parallel) {
    NovAstNode *n = new_at_cur(p, AST_MISSION_DECL);
    n->as.mission_decl.is_parallel = is_parallel;
    advance_tok(p); /* consume 'mission' */

    if (check(p, TOK_IDENT)) {
        n->as.mission_decl.name = str_from_tok(&p->current);
        advance_tok(p);
    } else {
        parse_error(p, "expected mission name");
    }

    expect(p, TOK_LPAREN, "mission declaration");
    parse_params(p, &n->as.mission_decl.params);
    expect(p, TOK_RPAREN, "mission declaration");

    /* Return type: -> Type */
    if (match_tok(p, TOK_ARROW))
        n->as.mission_decl.ret_type = parse_type(p);

    skip_newlines(p);
    n->as.mission_decl.body = parse_block(p);
    return n;
}

static NovAstNode *parse_absorb(NovParser *p) {
    NovAstNode *n = new_at_cur(p, AST_ABSORB);
    n->as.absorb.path_parts = nova_nodelist_new();
    n->as.absorb.names      = nova_nodelist_new();
    advance_tok(p); /* consume 'absorb' */

    /* Parse path: cosmos.stats  or  cosmos.stats.{ names } */
    while (check(p, TOK_IDENT)) {
        NovAstNode *part = new_at_cur(p, AST_IDENT);
        part->as.ident.name = str_from_tok(&p->current);
        nova_nodelist_push(&n->as.absorb.path_parts, part);
        advance_tok(p);
        if (!match_tok(p, TOK_DOT)) break;
        /* Check for .{ names } */
        if (check(p, TOK_LBRACE)) break;
    }

    /* Optional { name1, name2, ... } */
    if (match_tok(p, TOK_LBRACE)) {
        skip_newlines(p);
        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            if (check(p, TOK_IDENT)) {
                NovAstNode *nm = new_at_cur(p, AST_IDENT);
                nm->as.ident.name = str_from_tok(&p->current);
                nova_nodelist_push(&n->as.absorb.names, nm);
                advance_tok(p);
            }
            if (!match_tok(p, TOK_COMMA)) break;
            skip_newlines(p);
        }
        expect(p, TOK_RBRACE, "absorb");
    }
    return n;
}

static NovAstNode *parse_struct_decl(NovParser *p) {
    NovAstNode *n = new_at_cur(p, AST_STRUCT_DECL);
    n->as.struct_decl.fields = nova_nodelist_new();
    advance_tok(p); /* consume 'struct' */
    if (check(p, TOK_IDENT)) {
        n->as.struct_decl.name = str_from_tok(&p->current);
        advance_tok(p);
    }
    if (check(p, TOK_EXTENDS)) {
        advance_tok(p);
        if (check(p, TOK_IDENT)) { n->as.struct_decl.base = str_from_tok(&p->current); advance_tok(p); }
    }
    if (check(p, TOK_IMPLEMENTS)) {
        advance_tok(p);
        if (check(p, TOK_IDENT)) { n->as.struct_decl.iface = str_from_tok(&p->current); advance_tok(p); }
    }
    skip_newlines(p);
    expect(p, TOK_LBRACE, "struct");
    skip_newlines(p);
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        NovAstNode *field = new_at_cur(p, AST_FIELD);
        if (check(p, TOK_IDENT)) {
            field->as.field.name = str_from_tok(&p->current); advance_tok(p);
        }
        if (match_tok(p, TOK_COLON)) field->as.field.type_node = parse_type(p);
        if (match_tok(p, TOK_EQ))    field->as.field.default_val = parse_expr(p);
        nova_nodelist_push(&n->as.struct_decl.fields, field);
        skip_newlines(p);
    }
    expect(p, TOK_RBRACE, "struct");
    return n;
}

static NovAstNode *parse_unit_decl(NovParser *p) {
    NovAstNode *n = new_at_cur(p, AST_UNIT_DECL);
    advance_tok(p); /* consume 'unit' */
    if (check(p, TOK_IDENT)) {
        n->as.unit_decl.name = str_from_tok(&p->current); advance_tok(p);
    }
    expect(p, TOK_EQ, "unit declaration");
    n->as.unit_decl.value = parse_expr(p);
    return n;
}

static NovAstNode *parse_constellation_decl(NovParser *p) {
    NovAstNode *n = new_at_cur(p, AST_CONSTELLATION_DECL);
    n->as.constellation_decl.items = nova_nodelist_new();
    advance_tok(p);
    if (check(p, TOK_IDENT)) {
        n->as.constellation_decl.name = str_from_tok(&p->current); advance_tok(p);
    }
    skip_newlines(p);
    expect(p, TOK_LBRACE, "constellation");
    skip_newlines(p);
    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        NovAstNode *item = parse_top_level(p);
        if (item) nova_nodelist_push(&n->as.constellation_decl.items, item);
        skip_newlines(p);
    }
    expect(p, TOK_RBRACE, "constellation");
    return n;
}

static NovAstNode *parse_top_level(NovParser *p) {
    skip_newlines(p);

    /* parallel mission */
    if (check(p, TOK_PARALLEL)) {
        advance_tok(p);
        if (check(p, TOK_MISSION)) return parse_mission_decl(p, 1);
        parse_error(p, "expected 'mission' after 'parallel'");
        return NULL;
    }

    /* export prefix */
    int is_exported = 0;
    if (check(p, TOK_EXPORT)) { is_exported = 1; advance_tok(p); skip_newlines(p); }
    (void)is_exported; /* will be used by semantic pass */

    switch (cur_type(p)) {
        case TOK_MISSION:       return parse_mission_decl(p, 0);
        case TOK_ABSORB:        return parse_absorb(p);
        case TOK_LET:
        case TOK_VAR:           return parse_let_or_var(p);
        case TOK_STRUCT:        return parse_struct_decl(p);
        case TOK_CONSTELLATION: return parse_constellation_decl(p);
        case TOK_UNIT_KW:       return parse_unit_decl(p);
        case TOK_EOF:           return NULL;
        default:
            /* Try parsing as an expression statement */
            return parse_stmt(p);
    }
}


/* ─────────────────────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────────────────────*/

void nova_parser_init(NovParser *p, NovaLexer *lex) {
    memset(p, 0, sizeof(*p));
    p->lex = lex;
    /* Prime the two-token lookahead */
    p->current = nova_lexer_next(lex);
    p->next    = nova_lexer_next(lex);
}

NovAstNode *nova_parse(NovParser *p) {
    NovAstNode *prog = nova_ast_node_new(AST_PROGRAM, 1, 1);
    prog->as.program.items = nova_nodelist_new();

    while (!check(p, TOK_EOF)) {
        skip_newlines(p);
        if (check(p, TOK_EOF)) break;
        NovAstNode *node = parse_top_level(p);
        if (node) nova_nodelist_push(&prog->as.program.items, node);
        if (p->panic_mode) synchronise(p);
    }
    return prog;
}

int nova_parser_had_errors(const NovParser *p) {
    return p->error_count > 0;
}

void nova_parser_print_errors(const NovParser *p) {
    for (int i = 0; i < p->error_count; i++) {
        const NovParseError *e = &p->errors[i];
        fprintf(stderr, "%u:%u: parse error: %s\n", e->line, e->col, e->message);
    }
}
