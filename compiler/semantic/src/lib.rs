//! NOVA Semantic Analyser
//!
//! Phase 3 of the NOVA compiler pipeline.
//!
//! ## What this crate does
//!
//! After parsing and type checking, the semantic analyser performs:
//!   1. **Name resolution**: links every identifier to its declaration
//!   2. **Scope building**: constructs a scope tree from the AST
//!   3. **Borrow checking**: validates mutability and detects data races in parallel
//!   4. **Custom unit registration**: processes `unit` declarations
//!
//! ## Pipeline position
//!
//!   Parser (C) → Unit Resolver (Rust) → Type Checker (Rust) → [Semantic (Rust)] → Autodiff
//!
//! ## Error handling
//!
//! Errors are collected during analysis (not bailed out immediately) to allow
//! reporting multiple issues in one pass. This improves the user experience.

pub mod scope;
pub mod lifetime;

pub use scope::{Scope, ScopeTree, Declaration, DeclKind, ScopeError};
pub use lifetime::{BorrowChecker, BorrowError, BorrowSet, BorrowEvent, Lifetime};

use nova_unit_resolver::custom_units::CustomUnitRegistry;
use nova_typechecker::{TypeChecker, TypeEnv};
use thiserror::Error;

/// Comprehensive error type for semantic analysis.
#[derive(Debug, Clone, Error)]
pub enum SemanticError {
    #[error("scope error: {0}")]
    ScopeError(String),

    #[error("borrow error: {0}")]
    BorrowError(String),

    #[error("{line}:{col}: undefined variable '{name}'")]
    UndefinedVariable {
        name: String,
        line: u32,
        col: u32,
    },

    #[error("{line}:{col}: undefined type '{name}'")]
    UndefinedType {
        name: String,
        line: u32,
        col: u32,
    },

    #[error("{first_line}:{first_col}: duplicate declaration of '{name}'")]
    DuplicateDeclaration {
        name: String,
        first_line: u32,
        first_col: u32,
        second_line: u32,
        second_col: u32,
    },

    #[error("{line}:{col}: {message}")]
    Other {
        message: String,
        line: u32,
        col: u32,
    },
}

impl SemanticError {
    pub fn line(&self) -> u32 {
        match self {
            SemanticError::ScopeError(_) => 0,
            SemanticError::BorrowError(_) => 0,
            SemanticError::UndefinedVariable { line, .. } => *line,
            SemanticError::UndefinedType { line, .. } => *line,
            SemanticError::DuplicateDeclaration { first_line, .. } => *first_line,
            SemanticError::Other { line, .. } => *line,
        }
    }
}

/// The main semantic analyser.
///
/// This struct holds all state needed for semantic analysis:
///   - The scope tree
///   - The type checker (for context)
///   - The custom unit registry
///   - Error accumulator
pub struct SemanticAnalyser {
    pub scope_tree: ScopeTree,
    pub type_checker: TypeChecker,
    pub type_env: TypeEnv,
    pub unit_registry: CustomUnitRegistry,
    pub borrow_checker: BorrowChecker,
    pub errors: Vec<SemanticError>,
    /// Map AST node ID → scope index (for tracking which scope a node belongs to)
    pub node_to_scope: std::collections::HashMap<u64, usize>,
}

impl std::fmt::Debug for SemanticAnalyser {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("SemanticAnalyser")
            .field("scope_tree", &self.scope_tree)
            .field("type_env", &self.type_env)
            .field("unit_registry", &self.unit_registry)
            .field("borrow_checker", &self.borrow_checker)
            .field("errors", &self.errors)
            .field("node_to_scope", &self.node_to_scope)
            .finish()
    }
}

impl SemanticAnalyser {
    pub fn new() -> Self {
        let scope_tree = ScopeTree::new();
        let type_checker = TypeChecker::new();
        let type_env = TypeEnv::new();
        let unit_registry = CustomUnitRegistry::new();
        let borrow_checker = BorrowChecker::new();

        SemanticAnalyser {
            scope_tree,
            type_checker,
            type_env,
            unit_registry,
            borrow_checker,
            errors: Vec::new(),
            node_to_scope: std::collections::HashMap::new(),
        }
    }

    /// Add an error to the accumulator.
    pub fn record_error(&mut self, error: SemanticError) {
        self.errors.push(error);
    }

    /// Register a declaration in the current scope.
    /// Returns the index of the scope where the declaration was placed.
    pub fn declare_in_scope(
        &mut self,
        scope_idx: usize,
        name: impl Into<String>,
        kind: DeclKind,
        line: u32,
        col: u32,
    ) -> Result<usize, SemanticError> {
        let name_str = name.into();
        if let Some(scope) = self.scope_tree.scope_mut(scope_idx) {
            match scope.declare(&name_str, kind, line, col) {
                Ok(_) => Ok(scope_idx),
                Err(ScopeError::DuplicateDeclaration {
                    name,
                    first_line,
                    first_col,
                    second_line,
                    second_col,
                }) => Err(SemanticError::DuplicateDeclaration {
                    name,
                    first_line,
                    first_col,
                    second_line,
                    second_col,
                }),
                Err(ScopeError::UndefinedName { .. }) => {
                    // Should not happen in declare path
                    Err(SemanticError::Other {
                        message: "internal: unexpected undefined name error".to_string(),
                        line,
                        col,
                    })
                }
            }
        } else {
            Err(SemanticError::Other {
                message: format!("internal: scope {} not found", scope_idx),
                line,
                col,
            })
        }
    }

    /// Resolve a name from a given scope.
    /// Returns the declaration if found, or records an error if not.
    pub fn resolve_name(
        &mut self,
        scope_idx: usize,
        name: &str,
        line: u32,
        col: u32,
    ) -> Option<Declaration> {
        match self.scope_tree.resolve(scope_idx, name) {
            Some((decl, _)) => Some(decl),
            None => {
                self.record_error(SemanticError::UndefinedVariable {
                    name: name.to_string(),
                    line,
                    col,
                });
                None
            }
        }
    }

    /// Get the current scope (module-level for now; extended as we process nested items).
    pub fn current_scope_mut(&mut self) -> &mut Scope {
        self.scope_tree.module_scope_mut()
    }

    /// Verify we have no accumulated errors.
    pub fn is_ok(&self) -> bool {
        self.errors.is_empty()
    }

    /// Get all accumulated errors.
    pub fn take_errors(&mut self) -> Vec<SemanticError> {
        std::mem::take(&mut self.errors)
    }
}

impl Default for SemanticAnalyser {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn new_analyser() {
        let analyser = SemanticAnalyser::new();
        assert!(analyser.errors.is_empty());
        assert!(analyser.is_ok());
        assert_eq!(analyser.scope_tree.len(), 1);
    }

    #[test]
    fn declare_in_module_scope() {
        let mut analyser = SemanticAnalyser::new();
        let result = analyser.declare_in_scope(0, "foo", DeclKind::Let, 1, 5);
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 0);
    }

    #[test]
    fn declare_duplicate_in_module_scope() {
        let mut analyser = SemanticAnalyser::new();
        analyser
            .declare_in_scope(0, "foo", DeclKind::Let, 1, 5)
            .expect("first declaration");
        let result = analyser.declare_in_scope(0, "foo", DeclKind::Var, 2, 5);
        assert!(result.is_err());
        match result.unwrap_err() {
            SemanticError::DuplicateDeclaration {
                name,
                first_line,
                second_line,
                ..
            } => {
                assert_eq!(name, "foo");
                assert_eq!(first_line, 1);
                assert_eq!(second_line, 2);
            }
            _ => panic!("unexpected error type"),
        }
    }

    #[test]
    fn resolve_name_in_module_scope() {
        let mut analyser = SemanticAnalyser::new();
        analyser
            .declare_in_scope(0, "x", DeclKind::Let, 1, 5)
            .expect("declare x");
        let decl = analyser.resolve_name(0, "x", 2, 5);
        assert!(decl.is_some());
        assert_eq!(decl.unwrap().name, "x");
    }

    #[test]
    fn resolve_undefined_name() {
        let mut analyser = SemanticAnalyser::new();
        let decl = analyser.resolve_name(0, "undefined", 5, 10);
        assert!(decl.is_none());
        assert_eq!(analyser.errors.len(), 1);
        match &analyser.errors[0] {
            SemanticError::UndefinedVariable { name, line, .. } => {
                assert_eq!(name, "undefined");
                assert_eq!(*line, 5);
            }
            _ => panic!("unexpected error"),
        }
    }

    #[test]
    fn push_child_scope() {
        let mut analyser = SemanticAnalyser::new();
        // Declare in module scope
        analyser
            .declare_in_scope(0, "global", DeclKind::Let, 1, 1)
            .expect("declare global");
        // Create a child scope
        let child_idx = analyser.scope_tree.push_scope(0);
        assert_eq!(child_idx, 1);
        // Declare in child
        analyser
            .declare_in_scope(child_idx, "local", DeclKind::Let, 2, 1)
            .expect("declare local");
        // Resolve global from child (walk up)
        let found_global = analyser.resolve_name(child_idx, "global", 3, 1);
        assert!(found_global.is_some());
        assert_eq!(found_global.unwrap().name, "global");
        // Resolve local from child (same scope)
        let found_local = analyser.resolve_name(child_idx, "local", 3, 1);
        assert!(found_local.is_some());
        assert_eq!(found_local.unwrap().name, "local");
    }

    #[test]
    fn shadowing_in_nested_scope() {
        let mut analyser = SemanticAnalyser::new();
        analyser
            .declare_in_scope(0, "x", DeclKind::Let, 1, 1)
            .expect("module x");
        let child_idx = analyser.scope_tree.push_scope(0);
        analyser
            .declare_in_scope(child_idx, "x", DeclKind::Var, 2, 1)
            .expect("child x (shadowing)");
        // Resolve from child should find the shadowed one
        let found = analyser.resolve_name(child_idx, "x", 3, 1).expect("resolve x");
        assert_eq!(found.kind, DeclKind::Var);
    }

    #[test]
    fn error_accumulation() {
        let mut analyser = SemanticAnalyser::new();
        // Generate multiple errors
        analyser.resolve_name(0, "undefined1", 5, 10);
        analyser.resolve_name(0, "undefined2", 10, 5);
        assert_eq!(analyser.errors.len(), 2);
        assert!(!analyser.is_ok());
    }

    #[test]
    fn take_errors() {
        let mut analyser = SemanticAnalyser::new();
        analyser.resolve_name(0, "undefined", 5, 10);
        assert!(!analyser.is_ok());
        let errors = analyser.take_errors();
        assert_eq!(errors.len(), 1);
        assert!(analyser.is_ok()); // Errors cleared
    }
}

