#ifndef NOVA_AST_H
#define NOVA_AST_H

/*
 * NOVA AST Node Definitions
 *
 * The AST is a tagged union tree. Every node is a NovAstNode with a type tag
 * and a union of payload structs. Nodes are heap-allocated; the tree is freed
 * recursively with nova_ast_free().
 *
 * Design:
 *   - All string data (identifiers, literals) is a NovString: a (ptr, len)
 *     slice into the original source buffer, plus a heap copy when needed.
 *   - Child nodes are pointers; lists of children use NovNodeList.
 *   - Every node carries source location (line, col) for error messages.
 */

#include "../../lexer/include/token.h"
#include <stddef.h>
#include <stdint.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * String slice — points into source buffer (not null-terminated)
 * ───────────────────────────────────────────────────────────────────────────*/
typedef struct {
    const char *ptr;
    size_t      len;
} NovString;

/* ─────────────────────────────────────────────────────────────────────────────
 * Dynamic list of AST node pointers
 * ───────────────────────────────────────────────────────────────────────────*/
struct NovAstNode;
typedef struct {
    struct NovAstNode **items;
    size_t              count;
    size_t              capacity;
} NovNodeList;

NovNodeList  nova_nodelist_new(void);
void         nova_nodelist_push(NovNodeList *list, struct NovAstNode *node);
void         nova_nodelist_free(NovNodeList *list);  /* does NOT free the nodes */

/* ─────────────────────────────────────────────────────────────────────────────
 * AST Node Types
 * ───────────────────────────────────────────────────────────────────────────*/
typedef enum {

    /* ── Program root ──────────────────────────────────────────────────────── */
    AST_PROGRAM,            /* root node: list of top-level items             */

    /* ── Declarations ──────────────────────────────────────────────────────── */
    AST_MISSION_DECL,       /* mission foo(params) -> ret_type { body }       */
    AST_PARAM,              /* name: type  (function parameter)               */
    AST_CONSTELLATION_DECL, /* constellation Name { items }                   */
    AST_ABSORB,             /* absorb path.{ name1, name2 }                  */
    AST_LET_DECL,           /* let name [: type] = expr                       */
    AST_VAR_DECL,           /* var name [: type] = expr                       */
    AST_STRUCT_DECL,        /* struct Name [extends Base] [implements Iface]  */
    AST_FIELD,              /* field name: type [= default]                   */
    AST_ENUM_DECL,          /* enum Name { Variant(T), ... }                  */
    AST_ENUM_VARIANT,       /* Variant  or  Variant(T, ...)                   */
    AST_TRAIT_DECL,         /* trait Name { mission sigs... }                 */
    AST_INTERFACE_DECL,     /* interface Name { mission sigs... }             */
    AST_UNIT_DECL,          /* unit parsec = 3.086e16[m]                      */
    AST_MODEL_DECL,         /* model Name { layer ... }                       */
    AST_LAYER_DECL,         /* layer kind(args)                               */

    /* ── Statements ────────────────────────────────────────────────────────── */
    AST_BLOCK,              /* { stmt* }                                       */
    AST_RETURN_STMT,        /* return expr?                                    */
    AST_IF_STMT,            /* if cond { then } [else { alt }]                */
    AST_FOR_STMT,           /* for ident in expr { body }                     */
    AST_WHILE_STMT,         /* while cond { body }                            */
    AST_LOOP_STMT,          /* loop { body }                                  */
    AST_BREAK_STMT,         /* break                                           */
    AST_CONTINUE_STMT,      /* continue                                        */
    AST_EXPR_STMT,          /* expression used as a statement                 */
    AST_AUTODIFF_STMT,      /* autodiff(expr) { update }                      */
    AST_ON_DEVICE_STMT,     /* on device(gpu) { body }                        */

    /* ── Expressions ───────────────────────────────────────────────────────── */
    AST_BINARY_EXPR,        /* lhs op rhs                                      */
    AST_UNARY_EXPR,         /* op expr  (prefix)                               */
    AST_CALL_EXPR,          /* callee(args)                                    */
    AST_NAMED_ARG,          /* name = expr  (named argument)                  */
    AST_INDEX_EXPR,         /* expr[index]                                     */
    AST_FIELD_EXPR,         /* expr.field                                      */
    AST_PIPELINE_EXPR,      /* expr |> pipeline [ steps ]                     */
    AST_PIPE_EXPR,          /* lhs |> rhs  (simple pipe without pipeline kw)  */
    AST_MATCH_EXPR,         /* match expr { arm => body, ... }                */
    AST_MATCH_ARM,          /* pattern [if guard] => body                     */
    AST_LAMBDA_EXPR,        /* params => expr                                 */
    AST_GRADIENT_EXPR,      /* gradient(expr, wrt target)                     */
    AST_AS_EXPR,            /* expr as [unit]  or  expr as Type               */
    AST_QUESTION_EXPR,      /* expr?  (propagate Option/Result)               */
    AST_RANGE_EXPR,         /* lo..hi                                          */
    AST_STRUCT_LITERAL,     /* Name { field: expr, ... }                      */
    AST_ARRAY_LITERAL,      /* [ expr, ... ]                                  */
    AST_TRANSMIT_EXPR,      /* transmit(expr)                                  */
    AST_PANIC_EXPR,         /* panic(expr)                                     */

    /* ── Literals ──────────────────────────────────────────────────────────── */
    AST_INT_LIT,            /* integer literal                                 */
    AST_FLOAT_LIT,          /* float literal                                   */
    AST_UNIT_LIT,           /* unit literal: value + unit string               */
    AST_STRING_LIT,         /* string literal (may contain interp segments)   */
    AST_BOOL_LIT,           /* true | false                                    */

    /* ── Types ─────────────────────────────────────────────────────────────── */
    AST_NAMED_TYPE,         /* Float  Int  Str  CosmoTransformer  ...         */
    AST_UNIT_TYPE,          /* Float[m/s]  (named type + unit annotation)     */
    AST_TENSOR_TYPE,        /* Tensor[Float[eV], 1024]                        */
    AST_GENERIC_TYPE,       /* Option[T]  Result[T, E]  Array[T]              */
    AST_VOID_TYPE,          /* Void                                            */
    AST_NEVER_TYPE,         /* Never                                           */

    /* ── Identifiers ───────────────────────────────────────────────────────── */
    AST_IDENT,              /* a bare identifier reference                    */
    AST_PATH,               /* cosmos.stats.pearson  (dot-separated path)     */

    AST_NODE_COUNT

} NovAstNodeType;


/* ─────────────────────────────────────────────────────────────────────────────
 * Payload structs — one per major node category
 * ───────────────────────────────────────────────────────────────────────────*/

typedef struct { NovNodeList items;                                  } AstProgram;
typedef struct { NovString name; NovNodeList params;
                 struct NovAstNode *ret_type; struct NovAstNode *body;
                 int is_parallel; int is_exported;                   } AstMissionDecl;
typedef struct { NovString name; struct NovAstNode *type_node;       } AstParam;
typedef struct { NovString name; NovNodeList items;                  } AstConstellationDecl;
typedef struct { NovNodeList path_parts; NovNodeList names;          } AstAbsorb;
typedef struct { NovString name; struct NovAstNode *type_node;
                 struct NovAstNode *init; int is_exported;           } AstLetDecl;
typedef struct { NovString name; struct NovAstNode *type_node;
                 struct NovAstNode *init; int is_exported;           } AstVarDecl;
typedef struct { NovString name; NovString base; NovString iface;
                 NovNodeList fields;                                  } AstStructDecl;
typedef struct { NovString name; struct NovAstNode *type_node;
                 struct NovAstNode *default_val;                     } AstField;
typedef struct { NovString name; NovNodeList variants;               } AstEnumDecl;
typedef struct { NovString name; NovNodeList payload_types;          } AstEnumVariant;
typedef struct { NovString name; NovNodeList items;                  } AstTraitDecl;
typedef struct { NovString name; NovNodeList items;                  } AstInterfaceDecl;
typedef struct { NovString name; struct NovAstNode *value;           } AstUnitDecl;
typedef struct { NovString name; NovNodeList layers;                 } AstModelDecl;
typedef struct { NovString kind; NovNodeList args;                   } AstLayerDecl;

typedef struct { NovNodeList stmts;                                  } AstBlock;
typedef struct { struct NovAstNode *value;                           } AstReturnStmt;
typedef struct { struct NovAstNode *cond;
                 struct NovAstNode *then_block;
                 struct NovAstNode *else_block;                      } AstIfStmt;
typedef struct { NovString var; struct NovAstNode *iter;
                 struct NovAstNode *body;                            } AstForStmt;
typedef struct { struct NovAstNode *cond; struct NovAstNode *body;   } AstWhileStmt;
typedef struct { struct NovAstNode *body;                            } AstLoopStmt;
typedef struct { struct NovAstNode *expr;                            } AstExprStmt;
typedef struct { struct NovAstNode *loss_expr;
                 struct NovAstNode *update_block;                    } AstAutodiffStmt;
typedef struct { NovString device_name;
                 struct NovAstNode *body;                            } AstOnDeviceStmt;

typedef struct { NovaTokenType op;
                 struct NovAstNode *lhs; struct NovAstNode *rhs;     } AstBinaryExpr;
typedef struct { NovaTokenType op; struct NovAstNode *operand;       } AstUnaryExpr;
typedef struct { struct NovAstNode *callee; NovNodeList args;        } AstCallExpr;
typedef struct { NovString name; struct NovAstNode *value;           } AstNamedArg;
typedef struct { struct NovAstNode *object;
                 struct NovAstNode *index;                           } AstIndexExpr;
typedef struct { struct NovAstNode *object; NovString field;         } AstFieldExpr;
typedef struct { struct NovAstNode *source; NovNodeList steps;       } AstPipelineExpr;
typedef struct { struct NovAstNode *lhs; struct NovAstNode *rhs;     } AstPipeExpr;
typedef struct { struct NovAstNode *subject; NovNodeList arms;       } AstMatchExpr;
typedef struct { struct NovAstNode *pattern;
                 struct NovAstNode *guard;
                 struct NovAstNode *body;                            } AstMatchArm;
typedef struct { NovNodeList params; struct NovAstNode *body;        } AstLambdaExpr;
typedef struct { struct NovAstNode *expr;
                 struct NovAstNode *target;                          } AstGradientExpr;
typedef struct { struct NovAstNode *expr;
                 struct NovAstNode *target_type;                     } AstAsExpr;
typedef struct { struct NovAstNode *expr;                            } AstQuestionExpr;
typedef struct { struct NovAstNode *lo; struct NovAstNode *hi;       } AstRangeExpr;
typedef struct { NovString name; NovNodeList fields;                 } AstStructLiteral;
typedef struct { NovNodeList elements;                               } AstArrayLiteral;
typedef struct { struct NovAstNode *arg;                             } AstTransmitExpr;
typedef struct { struct NovAstNode *arg;                             } AstPanicExpr;

typedef struct { int64_t value;                                      } AstIntLit;
typedef struct { double  value;                                      } AstFloatLit;
typedef struct { double  value; NovString unit_str;                  } AstUnitLit;
typedef struct { NovString value;                                    } AstStringLit;
typedef struct { int     value;                                      } AstBoolLit;

typedef struct { NovString name;                                     } AstNamedType;
typedef struct { struct NovAstNode *base; NovString unit_str;        } AstUnitType;
typedef struct { struct NovAstNode *elem_type;
                 struct NovAstNode *shape;                           } AstTensorType;
typedef struct { NovString name; NovNodeList type_args;              } AstGenericType;

typedef struct { NovString name;                                     } AstIdent;
typedef struct { NovNodeList parts;                                  } AstPath;


/* ─────────────────────────────────────────────────────────────────────────────
 * NovAstNode — the tagged union
 * ───────────────────────────────────────────────────────────────────────────*/
typedef struct NovAstNode {
    NovAstNodeType type;
    uint32_t       line;
    uint32_t       col;
    union {
        AstProgram          program;
        AstMissionDecl      mission_decl;
        AstParam            param;
        AstConstellationDecl constellation_decl;
        AstAbsorb           absorb;
        AstLetDecl          let_decl;
        AstVarDecl          var_decl;
        AstStructDecl       struct_decl;
        AstField            field;
        AstEnumDecl         enum_decl;
        AstEnumVariant      enum_variant;
        AstTraitDecl        trait_decl;
        AstInterfaceDecl    interface_decl;
        AstUnitDecl         unit_decl;
        AstModelDecl        model_decl;
        AstLayerDecl        layer_decl;
        AstBlock            block;
        AstReturnStmt       return_stmt;
        AstIfStmt           if_stmt;
        AstForStmt          for_stmt;
        AstWhileStmt        while_stmt;
        AstLoopStmt         loop_stmt;
        AstExprStmt         expr_stmt;
        AstAutodiffStmt     autodiff_stmt;
        AstOnDeviceStmt     on_device_stmt;
        AstBinaryExpr       binary_expr;
        AstUnaryExpr        unary_expr;
        AstCallExpr         call_expr;
        AstNamedArg         named_arg;
        AstIndexExpr        index_expr;
        AstFieldExpr        field_expr;
        AstPipelineExpr     pipeline_expr;
        AstPipeExpr         pipe_expr;
        AstMatchExpr        match_expr;
        AstMatchArm         match_arm;
        AstLambdaExpr       lambda_expr;
        AstGradientExpr     gradient_expr;
        AstAsExpr           as_expr;
        AstQuestionExpr     question_expr;
        AstRangeExpr        range_expr;
        AstStructLiteral    struct_literal;
        AstArrayLiteral     array_literal;
        AstTransmitExpr     transmit_expr;
        AstPanicExpr        panic_expr;
        AstIntLit           int_lit;
        AstFloatLit         float_lit;
        AstUnitLit          unit_lit;
        AstStringLit        string_lit;
        AstBoolLit          bool_lit;
        AstNamedType        named_type;
        AstUnitType         unit_type;
        AstTensorType       tensor_type;
        AstGenericType      generic_type;
        AstIdent            ident;
        AstPath             path;
    } as;
} NovAstNode;


/* ─────────────────────────────────────────────────────────────────────────────
 * AST API
 * ───────────────────────────────────────────────────────────────────────────*/

/* Allocate a new node of the given type, zeroed. Sets line and col. */
NovAstNode *nova_ast_node_new(NovAstNodeType type, uint32_t line, uint32_t col);

/* Recursively free a node and all its children. Safe to call with NULL. */
void nova_ast_free(NovAstNode *node);

/* Pretty-print the AST tree to stdout (for debugging). */
void nova_ast_print(const NovAstNode *node, int indent);

/* Return a static string name for a node type. */
const char *nova_ast_node_type_name(NovAstNodeType type);

#endif /* NOVA_AST_H */
