//! Scope tree and name resolution for NOVA semantic analysis.
//!
//! ## Design
//!
//! A scope is a mapping from identifier names to their declarations.
//! The scope tree is a hierarchical structure where each scope can have
//! a parent scope (for nested blocks, function bodies, etc.).
//!
//! When resolving a name:
//!   1. Check the current (innermost) scope
//!   2. Walk up the parent chain until found
//!   3. If not found anywhere, it's an undefined reference (error)
//!
//! ## Declarations
//!
//! Each declaration has:
//!   - name: the identifier string
//!   - kind: what it declares (Let, Var, Param, Mission, Struct, Enum, etc.)
//!   - location: source line/col for error messages
//!
//! Declarations are immutable once registered — no shadowing within the same scope.

use std::collections::HashMap;

/// What kind of entity a name refers to.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeclKind {
    /// let binding
    Let,
    /// var binding
    Var,
    /// function parameter
    Param,
    /// mission (function) declaration
    Mission,
    /// struct declaration
    Struct,
    /// enum declaration
    Enum,
    /// trait declaration
    Trait,
    /// interface declaration
    Interface,
    /// unit declaration
    Unit,
    /// model declaration (neural network)
    Model,
    /// imported name from absorb clause
    Import,
}

impl std::fmt::Display for DeclKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            DeclKind::Let => write!(f, "let binding"),
            DeclKind::Var => write!(f, "var binding"),
            DeclKind::Param => write!(f, "parameter"),
            DeclKind::Mission => write!(f, "mission"),
            DeclKind::Struct => write!(f, "struct"),
            DeclKind::Enum => write!(f, "enum"),
            DeclKind::Trait => write!(f, "trait"),
            DeclKind::Interface => write!(f, "interface"),
            DeclKind::Unit => write!(f, "unit"),
            DeclKind::Model => write!(f, "model"),
            DeclKind::Import => write!(f, "import"),
        }
    }
}

/// A single declaration in a scope.
#[derive(Debug, Clone)]
pub struct Declaration {
    pub name: String,
    pub kind: DeclKind,
    pub line: u32,
    pub col: u32,
}

impl Declaration {
    pub fn new(name: impl Into<String>, kind: DeclKind, line: u32, col: u32) -> Self {
        Declaration {
            name: name.into(),
            kind,
            line,
            col,
        }
    }
}

/// A single scope: a mapping from names to declarations.
#[derive(Debug, Clone)]
pub struct Scope {
    /// Declarations in this scope, indexed by name.
    decls: HashMap<String, Declaration>,
    /// Index of parent scope (if any) in the scope tree arena. None = module scope.
    parent: Option<usize>,
}

impl Scope {
    pub fn new(parent: Option<usize>) -> Self {
        Scope {
            decls: HashMap::new(),
            parent,
        }
    }

    /// Register a declaration in this scope.
    /// Returns an error if the name is already declared in this scope.
    pub fn declare(
        &mut self,
        name: impl Into<String>,
        kind: DeclKind,
        line: u32,
        col: u32,
    ) -> Result<Declaration, ScopeError> {
        let name = name.into();
        if let Some(existing) = self.decls.get(&name) {
            return Err(ScopeError::DuplicateDeclaration {
                name: name.clone(),
                first_line: existing.line,
                first_col: existing.col,
                second_line: line,
                second_col: col,
            });
        }
        let decl = Declaration::new(&name, kind, line, col);
        self.decls.insert(name, decl.clone());
        Ok(decl)
    }

    /// Look up a name in this scope only (no parent chain).
    pub fn lookup_local(&self, name: &str) -> Option<&Declaration> {
        self.decls.get(name)
    }

    /// Get the parent scope index.
    pub fn parent(&self) -> Option<usize> {
        self.parent
    }

    /// List all declarations in this scope.
    pub fn declarations(&self) -> Vec<&Declaration> {
        self.decls.values().collect()
    }
}

/// The complete scope tree for a module/compilation unit.
/// Uses an arena to store scopes, avoiding pointer issues.
#[derive(Debug, Clone)]
pub struct ScopeTree {
    /// Arena of scopes. scopes[0] is always the module-level scope.
    scopes: Vec<Scope>,
}

impl ScopeTree {
    pub fn new() -> Self {
        // Module-level scope (no parent)
        let module_scope = Scope::new(None);
        ScopeTree {
            scopes: vec![module_scope],
        }
    }

    /// Get the module-level scope (index 0).
    pub fn module_scope(&self) -> &Scope {
        &self.scopes[0]
    }

    /// Get the module-level scope mutably.
    pub fn module_scope_mut(&mut self) -> &mut Scope {
        &mut self.scopes[0]
    }

    /// Create a new child scope with the given parent.
    /// Returns the index of the new scope.
    pub fn push_scope(&mut self, parent: usize) -> usize {
        let new_scope = Scope::new(Some(parent));
        self.scopes.push(new_scope);
        self.scopes.len() - 1
    }

    /// Get a scope by index.
    pub fn scope(&self, idx: usize) -> Option<&Scope> {
        self.scopes.get(idx)
    }

    /// Get a scope mutably by index.
    pub fn scope_mut(&mut self, idx: usize) -> Option<&mut Scope> {
        self.scopes.get_mut(idx)
    }

    /// Resolve a name starting from the given scope and walking up the parent chain.
    /// Returns the declaration and the scope index where it was found, or None if not found.
    pub fn resolve(&self, scope_idx: usize, name: &str) -> Option<(Declaration, usize)> {
        let mut current = scope_idx;
        loop {
            if let Some(scope) = self.scope(current) {
                if let Some(decl) = scope.lookup_local(name) {
                    return Some((decl.clone(), current));
                }
                if let Some(parent) = scope.parent() {
                    current = parent;
                } else {
                    break;
                }
            } else {
                break;
            }
        }
        None
    }

    /// Total number of scopes in the tree.
    pub fn len(&self) -> usize {
        self.scopes.len()
    }

    pub fn is_empty(&self) -> bool {
        self.scopes.is_empty()
    }
}

impl Default for ScopeTree {
    fn default() -> Self {
        Self::new()
    }
}

/// Errors that can occur during scope management.
#[derive(Debug, Clone)]
pub enum ScopeError {
    DuplicateDeclaration {
        name: String,
        first_line: u32,
        first_col: u32,
        second_line: u32,
        second_col: u32,
    },
    UndefinedName {
        name: String,
        line: u32,
        col: u32,
    },
}

impl std::fmt::Display for ScopeError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ScopeError::DuplicateDeclaration {
                name,
                first_line,
                first_col,
                second_line,
                second_col,
            } => write!(
                f,
                "duplicate declaration of '{}': first at {}:{}, then at {}:{}",
                name, first_line, first_col, second_line, second_col
            ),
            ScopeError::UndefinedName {
                name,
                line,
                col,
            } => write!(f, "{}:{}: undefined name '{}'", line, col, name),
        }
    }
}

impl std::error::Error for ScopeError {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn declare_in_scope() {
        let mut scope = Scope::new(None);
        let decl = scope
            .declare("x", DeclKind::Let, 1, 5)
            .expect("should declare");
        assert_eq!(decl.name, "x");
        assert_eq!(decl.kind, DeclKind::Let);
    }

    #[test]
    fn duplicate_declaration_in_scope() {
        let mut scope = Scope::new(None);
        scope
            .declare("x", DeclKind::Let, 1, 5)
            .expect("first declaration");
        let result = scope.declare("x", DeclKind::Var, 2, 10);
        assert!(result.is_err());
        if let Err(ScopeError::DuplicateDeclaration {
            name,
            first_line,
            second_line,
            ..
        }) = result
        {
            assert_eq!(name, "x");
            assert_eq!(first_line, 1);
            assert_eq!(second_line, 2);
        }
    }

    #[test]
    fn lookup_local() {
        let mut scope = Scope::new(None);
        scope
            .declare("x", DeclKind::Let, 1, 5)
            .expect("should declare");
        let found = scope.lookup_local("x");
        assert!(found.is_some());
        assert_eq!(found.unwrap().kind, DeclKind::Let);
    }

    #[test]
    fn lookup_nonexistent() {
        let scope = Scope::new(None);
        assert!(scope.lookup_local("x").is_none());
    }

    #[test]
    fn scope_tree_module_scope() {
        let tree = ScopeTree::new();
        assert_eq!(tree.len(), 1);
        let ms = tree.module_scope();
        assert!(ms.parent().is_none());
    }

    #[test]
    fn scope_tree_push_scope() {
        let mut tree = ScopeTree::new();
        let child = tree.push_scope(0);
        assert_eq!(child, 1);
        assert_eq!(tree.len(), 2);
        let scope = tree.scope(child).expect("scope 1");
        assert_eq!(scope.parent(), Some(0));
    }

    #[test]
    fn scope_tree_resolve_in_same_scope() {
        let mut tree = ScopeTree::new();
        tree.module_scope_mut()
            .declare("x", DeclKind::Let, 1, 5)
            .expect("should declare");
        let found = tree.resolve(0, "x");
        assert!(found.is_some());
        let (decl, scope_idx) = found.unwrap();
        assert_eq!(decl.name, "x");
        assert_eq!(scope_idx, 0);
    }

    #[test]
    fn scope_tree_resolve_up_chain() {
        let mut tree = ScopeTree::new();
        tree.module_scope_mut()
            .declare("x", DeclKind::Let, 1, 5)
            .expect("should declare in module");
        let child = tree.push_scope(0);
        tree.scope_mut(child)
            .unwrap()
            .declare("y", DeclKind::Let, 2, 5)
            .expect("should declare in child");

        // Resolve y in child scope
        let found_y = tree.resolve(child, "y");
        assert!(found_y.is_some());
        let (decl_y, _) = found_y.unwrap();
        assert_eq!(decl_y.name, "y");

        // Resolve x in child scope (should walk up to module)
        let found_x = tree.resolve(child, "x");
        assert!(found_x.is_some());
        let (decl_x, scope_x) = found_x.unwrap();
        assert_eq!(decl_x.name, "x");
        assert_eq!(scope_x, 0); // Found in module scope
    }

    #[test]
    fn scope_tree_shadowing_in_nested_scope() {
        let mut tree = ScopeTree::new();
        tree.module_scope_mut()
            .declare("x", DeclKind::Let, 1, 5)
            .expect("module x");
        let child = tree.push_scope(0);
        tree.scope_mut(child)
            .unwrap()
            .declare("x", DeclKind::Var, 2, 5)
            .expect("child x (shadowing)");

        // Resolving x in child scope should find the inner one
        let (decl, scope_idx) = tree.resolve(child, "x").expect("resolve x");
        assert_eq!(scope_idx, child); // Found in child scope
        assert_eq!(decl.kind, DeclKind::Var); // The shadowed one
    }

    #[test]
    fn scope_tree_undefined_name() {
        let tree = ScopeTree::new();
        let found = tree.resolve(0, "nonexistent");
        assert!(found.is_none());
    }

    #[test]
    fn scope_tree_deeply_nested() {
        let mut tree = ScopeTree::new();
        tree.module_scope_mut()
            .declare("a", DeclKind::Let, 1, 1)
            .expect("a");
        let s1 = tree.push_scope(0);
        tree.scope_mut(s1)
            .unwrap()
            .declare("b", DeclKind::Let, 2, 1)
            .expect("b");
        let s2 = tree.push_scope(s1);
        tree.scope_mut(s2)
            .unwrap()
            .declare("c", DeclKind::Let, 3, 1)
            .expect("c");
        let s3 = tree.push_scope(s2);

        // From s3, we should be able to resolve a, b, c
        assert!(tree.resolve(s3, "a").is_some());
        assert!(tree.resolve(s3, "b").is_some());
        assert!(tree.resolve(s3, "c").is_some());
        assert!(tree.resolve(s3, "d").is_none());
    }
}

