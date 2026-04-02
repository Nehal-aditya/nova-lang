use nova_semantic::{SemanticAnalyser, DeclKind, SemanticError};

#[test]
fn undefined_variable_reference() {
    let mut analyser = SemanticAnalyser::new();
    analyser.resolve_name(0, "undefined_var", 5, 10);
    assert!(!analyser.is_ok());
    assert_eq!(analyser.errors.len(), 1);
    match &analyser.errors[0] {
        SemanticError::UndefinedVariable { name, line, col } => {
            assert_eq!(name, "undefined_var");
            assert_eq!(*line, 5);
            assert_eq!(*col, 10);
        }
        _ => panic!("expected UndefinedVariable error"),
    }
}

#[test]
fn variable_used_before_declaration() {
    let mut analyser = SemanticAnalyser::new();
    // Try to resolve 'x' before declaring it
    analyser.resolve_name(0, "x", 1, 5);
    assert!(!analyser.is_ok());
    // Then declare it
    analyser
        .declare_in_scope(0, "x", DeclKind::Let, 2, 5)
        .expect("should declare");
    // Should still have the error
    assert_eq!(analyser.errors.len(), 1);
}

#[test]
fn shadowing_in_nested_scope_ok() {
    let mut analyser = SemanticAnalyser::new();
    // Declare global
    analyser
        .declare_in_scope(0, "value", DeclKind::Let, 1, 1)
        .expect("global");
    // Create nested block
    let nested = analyser.scope_tree.push_scope(0);
    // Shadow in nested scope
    analyser
        .declare_in_scope(nested, "value", DeclKind::Var, 2, 1)
        .expect("nested (shadowing)");
    // Should resolve to the nested one
    let resolved = analyser.resolve_name(nested, "value", 3, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Var); // The shadowed one
    assert!(analyser.is_ok());
}

#[test]
fn double_declaration_in_same_scope_error() {
    let mut analyser = SemanticAnalyser::new();
    analyser
        .declare_in_scope(0, "x", DeclKind::Let, 1, 5)
        .expect("first");
    let result = analyser.declare_in_scope(0, "x", DeclKind::Var, 2, 5);
    assert!(result.is_err());
    match result.unwrap_err() {
        SemanticError::DuplicateDeclaration { name, .. } => {
            assert_eq!(name, "x");
        }
        _ => panic!("expected DuplicateDeclaration"),
    }
}

#[test]
fn unit_declaration_registers() {
    let mut analyser = SemanticAnalyser::new();
    // Declare a custom unit
    analyser
        .declare_in_scope(0, "parsec", DeclKind::Unit, 10, 1)
        .expect("declare unit");
    // Verify it's in the module scope
    let resolved = analyser.resolve_name(0, "parsec", 15, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Unit);
    assert!(analyser.is_ok());
}

#[test]
fn mission_declaration() {
    let mut analyser = SemanticAnalyser::new();
    analyser
        .declare_in_scope(0, "delta_v", DeclKind::Mission, 1, 1)
        .expect("declare mission");
    let resolved = analyser.resolve_name(0, "delta_v", 5, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Mission);
    assert!(analyser.is_ok());
}

#[test]
fn struct_declaration() {
    let mut analyser = SemanticAnalyser::new();
    analyser
        .declare_in_scope(0, "Vector3", DeclKind::Struct, 1, 1)
        .expect("declare struct");
    let resolved = analyser.resolve_name(0, "Vector3", 10, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Struct);
}

#[test]
fn enum_declaration() {
    let mut analyser = SemanticAnalyser::new();
    analyser
        .declare_in_scope(0, "Result", DeclKind::Enum, 1, 1)
        .expect("declare enum");
    let resolved = analyser.resolve_name(0, "Result", 5, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Enum);
}

#[test]
fn trait_declaration() {
    let mut analyser = SemanticAnalyser::new();
    analyser
        .declare_in_scope(0, "Numeric", DeclKind::Trait, 1, 1)
        .expect("declare trait");
    let resolved = analyser.resolve_name(0, "Numeric", 5, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Trait);
}

#[test]
fn interface_declaration() {
    let mut analyser = SemanticAnalyser::new();
    analyser
        .declare_in_scope(0, "Drawable", DeclKind::Interface, 1, 1)
        .expect("declare interface");
    let resolved = analyser.resolve_name(0, "Drawable", 5, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Interface);
}

#[test]
fn model_declaration() {
    let mut analyser = SemanticAnalyser::new();
    analyser
        .declare_in_scope(0, "TransformerNet", DeclKind::Model, 1, 1)
        .expect("declare model");
    let resolved = analyser.resolve_name(0, "TransformerNet", 10, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Model);
}

#[test]
fn import_declaration() {
    let mut analyser = SemanticAnalyser::new();
    analyser
        .declare_in_scope(0, "pearson", DeclKind::Import, 1, 1)
        .expect("declare import");
    let resolved = analyser.resolve_name(0, "pearson", 5, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Import);
}

#[test]
fn parameter_declaration() {
    let mut analyser = SemanticAnalyser::new();
    analyser
        .declare_in_scope(0, "x", DeclKind::Param, 1, 5)
        .expect("declare param");
    let resolved = analyser.resolve_name(0, "x", 2, 10).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Param);
}

#[test]
fn multiple_errors_accumulated() {
    let mut analyser = SemanticAnalyser::new();
    analyser.resolve_name(0, "undefined1", 5, 1);
    analyser.resolve_name(0, "undefined2", 10, 1);
    analyser.resolve_name(0, "undefined3", 15, 1);
    assert_eq!(analyser.errors.len(), 3);
    assert!(!analyser.is_ok());
}

#[test]
fn scope_resolution_chain() {
    let mut analyser = SemanticAnalyser::new();
    // Set up nested scopes: 0 -> 1 -> 2 -> 3
    analyser
        .declare_in_scope(0, "global", DeclKind::Let, 1, 1)
        .expect("global");
    let s1 = analyser.scope_tree.push_scope(0);
    analyser
        .declare_in_scope(s1, "level1", DeclKind::Let, 2, 1)
        .expect("level1");
    let s2 = analyser.scope_tree.push_scope(s1);
    analyser
        .declare_in_scope(s2, "level2", DeclKind::Let, 3, 1)
        .expect("level2");
    let s3 = analyser.scope_tree.push_scope(s2);
    analyser
        .declare_in_scope(s3, "level3", DeclKind::Let, 4, 1)
        .expect("level3");

    // From s3, resolve all names
    assert!(analyser.resolve_name(s3, "global", 5, 1).is_some());
    assert!(analyser.resolve_name(s3, "level1", 5, 1).is_some());
    assert!(analyser.resolve_name(s3, "level2", 5, 1).is_some());
    assert!(analyser.resolve_name(s3, "level3", 5, 1).is_some());
    assert!(analyser.is_ok());
}

#[test]
fn shadowing_resolves_to_innermost() {
    let mut analyser = SemanticAnalyser::new();
    // Global x
    analyser
        .declare_in_scope(0, "x", DeclKind::Let, 1, 1)
        .expect("global x");
    let s1 = analyser.scope_tree.push_scope(0);
    // Shadow with var
    analyser
        .declare_in_scope(s1, "x", DeclKind::Var, 2, 1)
        .expect("s1 x");
    let s2 = analyser.scope_tree.push_scope(s1);
    // Shadow again with param
    analyser
        .declare_in_scope(s2, "x", DeclKind::Param, 3, 1)
        .expect("s2 x");

    // Resolve from s2 should get Param
    let resolved = analyser.resolve_name(s2, "x", 4, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Param);
    assert_eq!(resolved.line, 3);

    // Resolve from s1 should get Var
    let resolved = analyser.resolve_name(s1, "x", 4, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Var);
    assert_eq!(resolved.line, 2);

    // Resolve from 0 should get Let
    let resolved = analyser.resolve_name(0, "x", 4, 1).expect("resolve");
    assert_eq!(resolved.kind, DeclKind::Let);
    assert_eq!(resolved.line, 1);
}

#[test]
fn declaration_tracks_location() {
    let mut analyser = SemanticAnalyser::new();
    analyser
        .declare_in_scope(0, "myvar", DeclKind::Let, 42, 15)
        .expect("declare");
    let resolved = analyser.resolve_name(0, "myvar", 50, 1).expect("resolve");
    assert_eq!(resolved.line, 42);
    assert_eq!(resolved.col, 15);
}

#[test]
fn many_declarations_in_scope() {
    let mut analyser = SemanticAnalyser::new();
    // Declare many variables
    for i in 0..100 {
        let name = format!("var{}", i);
        analyser
            .declare_in_scope(0, &name, DeclKind::Let, i as u32 + 1, 1)
            .expect(&format!("declare {}", name));
    }
    // Verify they're all resolvable
    for i in 0..100 {
        let name = format!("var{}", i);
        let resolved = analyser
            .resolve_name(0, &name, 200, 1)
            .expect(&format!("resolve {}", name));
        assert_eq!(resolved.line, i as u32 + 1);
    }
    assert!(analyser.is_ok());
}

#[test]
fn let_vs_var_distinction() {
    let mut analyser = SemanticAnalyser::new();
    analyser
        .declare_in_scope(0, "let_var", DeclKind::Let, 1, 1)
        .expect("let");
    analyser
        .declare_in_scope(0, "mut_var", DeclKind::Var, 2, 1)
        .expect("var");

    let let_resolved = analyser.resolve_name(0, "let_var", 3, 1).expect("resolve let");
    let var_resolved = analyser.resolve_name(0, "mut_var", 3, 1).expect("resolve var");

    assert_eq!(let_resolved.kind, DeclKind::Let);
    assert_eq!(var_resolved.kind, DeclKind::Var);
}

#[test]
fn error_line_col_preserved() {
    let mut analyser = SemanticAnalyser::new();
    analyser.resolve_name(0, "missing", 999, 777);
    assert_eq!(analyser.errors.len(), 1);
    // Check line and col are preserved in error
    match &analyser.errors[0] {
        SemanticError::UndefinedVariable { line, col, .. } => {
            assert_eq!(*line, 999);
            assert_eq!(*col, 777);
        }
        _ => panic!("wrong error type"),
    }
}

#[test]
fn clear_errors_with_take() {
    let mut analyser = SemanticAnalyser::new();
    analyser.resolve_name(0, "err1", 1, 1);
    analyser.resolve_name(0, "err2", 2, 1);
    assert_eq!(analyser.errors.len(), 2);
    let errors = analyser.take_errors();
    assert_eq!(errors.len(), 2);
    assert!(analyser.errors.is_empty());
    assert!(analyser.is_ok());
}

#[test]
fn unique_scope_indices() {
    let mut analyser = SemanticAnalyser::new();
    let s1 = analyser.scope_tree.push_scope(0);
    let s2 = analyser.scope_tree.push_scope(0);
    let s3 = analyser.scope_tree.push_scope(s1);
    // All should be different
    assert_eq!(s1, 1);
    assert_eq!(s2, 2);
    assert_eq!(s3, 3);
}

#[test]
fn scope_tree_len_increases() {
    let mut analyser = SemanticAnalyser::new();
    assert_eq!(analyser.scope_tree.len(), 1);
    analyser.scope_tree.push_scope(0);
    assert_eq!(analyser.scope_tree.len(), 2);
    analyser.scope_tree.push_scope(0);
    assert_eq!(analyser.scope_tree.len(), 3);
}
