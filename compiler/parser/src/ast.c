#include "../include/ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ── NovNodeList ──────────────────────────────────────────────────────────── */
NovNodeList nova_nodelist_new(void) {
    NovNodeList l = { NULL, 0, 0 };
    return l;
}

void nova_nodelist_push(NovNodeList *list, NovAstNode *node) {
    if (list->count >= list->capacity) {
        size_t cap = list->capacity == 0 ? 4 : list->capacity * 2;
        NovAstNode **tmp = realloc(list->items, cap * sizeof(NovAstNode *));
        if (!tmp) return; /* OOM: silently drop — caller checks errors */
        list->items    = tmp;
        list->capacity = cap;
    }
    list->items[list->count++] = node;
}

void nova_nodelist_free(NovNodeList *list) {
    free(list->items);
    list->items    = NULL;
    list->count    = 0;
    list->capacity = 0;
}

/* ── Node allocation ──────────────────────────────────────────────────────── */
NovAstNode *nova_ast_node_new(NovAstNodeType type, uint32_t line, uint32_t col) {
    NovAstNode *n = calloc(1, sizeof(NovAstNode));
    if (!n) return NULL;
    n->type = type;
    n->line = line;
    n->col  = col;
    return n;
}

/* ── Recursive free ───────────────────────────────────────────────────────── */
static void free_nodelist_and_nodes(NovNodeList *list) {
    for (size_t i = 0; i < list->count; i++)
        nova_ast_free(list->items[i]);
    nova_nodelist_free(list);
}

void nova_ast_free(NovAstNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_PROGRAM:
            free_nodelist_and_nodes(&node->as.program.items); break;
        case AST_MISSION_DECL:
            free_nodelist_and_nodes(&node->as.mission_decl.params);
            nova_ast_free(node->as.mission_decl.ret_type);
            nova_ast_free(node->as.mission_decl.body); break;
        case AST_CONSTELLATION_DECL:
            free_nodelist_and_nodes(&node->as.constellation_decl.items); break;
        case AST_ABSORB:
            free_nodelist_and_nodes(&node->as.absorb.path_parts);
            free_nodelist_and_nodes(&node->as.absorb.names); break;
        case AST_LET_DECL:
            nova_ast_free(node->as.let_decl.type_node);
            nova_ast_free(node->as.let_decl.init); break;
        case AST_VAR_DECL:
            nova_ast_free(node->as.var_decl.type_node);
            nova_ast_free(node->as.var_decl.init); break;
        case AST_STRUCT_DECL:
            free_nodelist_and_nodes(&node->as.struct_decl.fields); break;
        case AST_ENUM_DECL:
            free_nodelist_and_nodes(&node->as.enum_decl.variants); break;
        case AST_ENUM_VARIANT:
            free_nodelist_and_nodes(&node->as.enum_variant.payload_types); break;
        case AST_TRAIT_DECL:
            free_nodelist_and_nodes(&node->as.trait_decl.items); break;
        case AST_INTERFACE_DECL:
            free_nodelist_and_nodes(&node->as.interface_decl.items); break;
        case AST_UNIT_DECL:
            nova_ast_free(node->as.unit_decl.value); break;
        case AST_MODEL_DECL:
            free_nodelist_and_nodes(&node->as.model_decl.layers); break;
        case AST_LAYER_DECL:
            free_nodelist_and_nodes(&node->as.layer_decl.args); break;
        case AST_BLOCK:
            free_nodelist_and_nodes(&node->as.block.stmts); break;
        case AST_RETURN_STMT:
            nova_ast_free(node->as.return_stmt.value); break;
        case AST_IF_STMT:
            nova_ast_free(node->as.if_stmt.cond);
            nova_ast_free(node->as.if_stmt.then_block);
            nova_ast_free(node->as.if_stmt.else_block); break;
        case AST_FOR_STMT:
            nova_ast_free(node->as.for_stmt.iter);
            nova_ast_free(node->as.for_stmt.body); break;
        case AST_WHILE_STMT:
            nova_ast_free(node->as.while_stmt.cond);
            nova_ast_free(node->as.while_stmt.body); break;
        case AST_LOOP_STMT:
            nova_ast_free(node->as.loop_stmt.body); break;
        case AST_EXPR_STMT:
            nova_ast_free(node->as.expr_stmt.expr); break;
        case AST_AUTODIFF_STMT:
            nova_ast_free(node->as.autodiff_stmt.loss_expr);
            nova_ast_free(node->as.autodiff_stmt.update_block); break;
        case AST_ON_DEVICE_STMT:
            nova_ast_free(node->as.on_device_stmt.body); break;
        case AST_BINARY_EXPR:
            nova_ast_free(node->as.binary_expr.lhs);
            nova_ast_free(node->as.binary_expr.rhs); break;
        case AST_UNARY_EXPR:
            nova_ast_free(node->as.unary_expr.operand); break;
        case AST_CALL_EXPR:
            nova_ast_free(node->as.call_expr.callee);
            free_nodelist_and_nodes(&node->as.call_expr.args); break;
        case AST_NAMED_ARG:
            nova_ast_free(node->as.named_arg.value); break;
        case AST_INDEX_EXPR:
            nova_ast_free(node->as.index_expr.object);
            nova_ast_free(node->as.index_expr.index); break;
        case AST_FIELD_EXPR:
            nova_ast_free(node->as.field_expr.object); break;
        case AST_PIPELINE_EXPR:
            nova_ast_free(node->as.pipeline_expr.source);
            free_nodelist_and_nodes(&node->as.pipeline_expr.steps); break;
        case AST_PIPE_EXPR:
            nova_ast_free(node->as.pipe_expr.lhs);
            nova_ast_free(node->as.pipe_expr.rhs); break;
        case AST_MATCH_EXPR:
            nova_ast_free(node->as.match_expr.subject);
            free_nodelist_and_nodes(&node->as.match_expr.arms); break;
        case AST_MATCH_ARM:
            nova_ast_free(node->as.match_arm.pattern);
            nova_ast_free(node->as.match_arm.guard);
            nova_ast_free(node->as.match_arm.body); break;
        case AST_LAMBDA_EXPR:
            free_nodelist_and_nodes(&node->as.lambda_expr.params);
            nova_ast_free(node->as.lambda_expr.body); break;
        case AST_GRADIENT_EXPR:
            nova_ast_free(node->as.gradient_expr.expr);
            nova_ast_free(node->as.gradient_expr.target); break;
        case AST_AS_EXPR:
            nova_ast_free(node->as.as_expr.expr);
            nova_ast_free(node->as.as_expr.target_type); break;
        case AST_QUESTION_EXPR:
            nova_ast_free(node->as.question_expr.expr); break;
        case AST_RANGE_EXPR:
            nova_ast_free(node->as.range_expr.lo);
            nova_ast_free(node->as.range_expr.hi); break;
        case AST_STRUCT_LITERAL:
            free_nodelist_and_nodes(&node->as.struct_literal.fields); break;
        case AST_ARRAY_LITERAL:
            free_nodelist_and_nodes(&node->as.array_literal.elements); break;
        case AST_TRANSMIT_EXPR:
            nova_ast_free(node->as.transmit_expr.arg); break;
        case AST_PANIC_EXPR:
            nova_ast_free(node->as.panic_expr.arg); break;
        case AST_UNIT_TYPE:
            nova_ast_free(node->as.unit_type.base); break;
        case AST_TENSOR_TYPE:
            nova_ast_free(node->as.tensor_type.elem_type);
            nova_ast_free(node->as.tensor_type.shape); break;
        case AST_GENERIC_TYPE:
            free_nodelist_and_nodes(&node->as.generic_type.type_args); break;
        case AST_PATH:
            free_nodelist_and_nodes(&node->as.path.parts); break;
        case AST_FIELD:
            nova_ast_free(node->as.field.type_node);
            nova_ast_free(node->as.field.default_val); break;
        case AST_PARAM:
            nova_ast_free(node->as.param.type_node); break;
        default: break;
    }
    free(node);
}

/* ── Node type name ───────────────────────────────────────────────────────── */
static const char *NODE_NAMES[AST_NODE_COUNT] = {
    "PROGRAM",
    "MISSION_DECL","PARAM","CONSTELLATION_DECL","ABSORB",
    "LET_DECL","VAR_DECL","STRUCT_DECL","FIELD","ENUM_DECL","ENUM_VARIANT",
    "TRAIT_DECL","INTERFACE_DECL","UNIT_DECL","MODEL_DECL","LAYER_DECL",
    "BLOCK","RETURN_STMT","IF_STMT","FOR_STMT","WHILE_STMT","LOOP_STMT",
    "BREAK_STMT","CONTINUE_STMT","EXPR_STMT","AUTODIFF_STMT","ON_DEVICE_STMT",
    "BINARY_EXPR","UNARY_EXPR","CALL_EXPR","NAMED_ARG","INDEX_EXPR","FIELD_EXPR",
    "PIPELINE_EXPR","PIPE_EXPR","MATCH_EXPR","MATCH_ARM","LAMBDA_EXPR",
    "GRADIENT_EXPR","AS_EXPR","QUESTION_EXPR","RANGE_EXPR",
    "STRUCT_LITERAL","ARRAY_LITERAL","TRANSMIT_EXPR","PANIC_EXPR",
    "INT_LIT","FLOAT_LIT","UNIT_LIT","STRING_LIT","BOOL_LIT",
    "NAMED_TYPE","UNIT_TYPE","TENSOR_TYPE","GENERIC_TYPE","VOID_TYPE","NEVER_TYPE",
    "IDENT","PATH",
};

const char *nova_ast_node_type_name(NovAstNodeType type) {
    if (type < 0 || type >= AST_NODE_COUNT) return "<unknown>";
    return NODE_NAMES[type];
}

/* ── Pretty printer ───────────────────────────────────────────────────────── */
static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

static void print_str(NovString s) {
    printf("%.*s", (int)s.len, s.ptr);
}

void nova_ast_print(const NovAstNode *node, int indent) {
    if (!node) { print_indent(indent); printf("<null>\n"); return; }
    print_indent(indent);
    printf("[%u:%u] %s", node->line, node->col, nova_ast_node_type_name(node->type));

    switch (node->type) {
        case AST_IDENT:
            printf("  '"); print_str(node->as.ident.name); printf("'"); break;
        case AST_INT_LIT:
            printf("  %lld", (long long)node->as.int_lit.value); break;
        case AST_FLOAT_LIT:
            printf("  %g", node->as.float_lit.value); break;
        case AST_UNIT_LIT:
            printf("  %g[", node->as.unit_lit.value);
            print_str(node->as.unit_lit.unit_str); printf("]"); break;
        case AST_STRING_LIT:
            printf("  \""); print_str(node->as.string_lit.value); printf("\""); break;
        case AST_BOOL_LIT:
            printf("  %s", node->as.bool_lit.value ? "true" : "false"); break;
        case AST_MISSION_DECL:
            printf("  '"); print_str(node->as.mission_decl.name);
            printf("'%s", node->as.mission_decl.is_parallel ? " [parallel]" : ""); break;
        case AST_LET_DECL: case AST_VAR_DECL:
            printf("  '"); print_str(node->as.let_decl.name); printf("'"); break;
        case AST_BINARY_EXPR:
            printf("  op=%s", nova_token_type_name(node->as.binary_expr.op)); break;
        case AST_UNARY_EXPR:
            printf("  op=%s", nova_token_type_name(node->as.unary_expr.op)); break;
        case AST_NAMED_TYPE:
            printf("  '"); print_str(node->as.named_type.name); printf("'"); break;
        default: break;
    }
    printf("\n");

    /* Recurse into children */
    switch (node->type) {
        case AST_PROGRAM:
            for (size_t i = 0; i < node->as.program.items.count; i++)
                nova_ast_print(node->as.program.items.items[i], indent+1);
            break;
        case AST_MISSION_DECL:
            for (size_t i = 0; i < node->as.mission_decl.params.count; i++)
                nova_ast_print(node->as.mission_decl.params.items[i], indent+1);
            nova_ast_print(node->as.mission_decl.ret_type, indent+1);
            nova_ast_print(node->as.mission_decl.body, indent+1); break;
        case AST_BLOCK:
            for (size_t i = 0; i < node->as.block.stmts.count; i++)
                nova_ast_print(node->as.block.stmts.items[i], indent+1);
            break;
        case AST_LET_DECL:
            nova_ast_print(node->as.let_decl.type_node, indent+1);
            nova_ast_print(node->as.let_decl.init, indent+1); break;
        case AST_RETURN_STMT:
            nova_ast_print(node->as.return_stmt.value, indent+1); break;
        case AST_EXPR_STMT:
            nova_ast_print(node->as.expr_stmt.expr, indent+1); break;
        case AST_BINARY_EXPR:
            nova_ast_print(node->as.binary_expr.lhs, indent+1);
            nova_ast_print(node->as.binary_expr.rhs, indent+1); break;
        case AST_UNARY_EXPR:
            nova_ast_print(node->as.unary_expr.operand, indent+1); break;
        case AST_CALL_EXPR:
            nova_ast_print(node->as.call_expr.callee, indent+1);
            for (size_t i = 0; i < node->as.call_expr.args.count; i++)
                nova_ast_print(node->as.call_expr.args.items[i], indent+1);
            break;
        case AST_IF_STMT:
            nova_ast_print(node->as.if_stmt.cond, indent+1);
            nova_ast_print(node->as.if_stmt.then_block, indent+1);
            nova_ast_print(node->as.if_stmt.else_block, indent+1); break;
        case AST_FOR_STMT:
            nova_ast_print(node->as.for_stmt.iter, indent+1);
            nova_ast_print(node->as.for_stmt.body, indent+1); break;
        case AST_WHILE_STMT:
            nova_ast_print(node->as.while_stmt.cond, indent+1);
            nova_ast_print(node->as.while_stmt.body, indent+1); break;
        default: break;
    }
}
