//! Simplified borrow checker and lifetime inference.
//!
//! ## Design
//!
//! NOVA's borrow checking is simplified compared to Rust:
//!   1. We track mutability (let vs var) at the declaration site.
//!   2. We detect when a `var` (mutable binding) is borrowed in a `parallel mission`.
//!   3. Data race detection: if multiple parallel branches borrow the same `var`,
//!      that's an error (simplified region model).
//!
//! ## Lifetimes
//!
//! Lifetimes are not explicitly annotated by users. Instead:
//!   - Every reference is inferred to have a lifetime scoped to a block.
//!   - Cross-function references require return values (no implicit escaping).
//!   - For parallel missions, we check that borrowed vars don't escape into concurrent branches.

use crate::scope::{Declaration, DeclKind};
use std::collections::{HashMap, HashSet};

/// A reference to a variable (with line/col for error reporting).
#[derive(Debug, Clone)]
pub struct BorrowEvent {
    pub var_name: String,
    pub is_mutable: bool,
    pub line: u32,
    pub col: u32,
}

impl BorrowEvent {
    pub fn new(var_name: impl Into<String>, is_mutable: bool, line: u32, col: u32) -> Self {
        BorrowEvent {
            var_name: var_name.into(),
            is_mutable,
            line,
            col,
        }
    }
}

/// Tracks which variables are borrowed (read or written) in a block of code.
#[derive(Debug, Clone, Default)]
pub struct BorrowSet {
    /// Variables borrowed immutably.
    pub immutable_borrows: HashSet<String>,
    /// Variables borrowed mutably.
    pub mutable_borrows: HashSet<String>,
}

impl BorrowSet {
    pub fn new() -> Self {
        BorrowSet {
            immutable_borrows: HashSet::new(),
            mutable_borrows: HashSet::new(),
        }
    }

    /// Record an immutable borrow.
    pub fn borrow_immutable(&mut self, var: impl Into<String>) {
        self.immutable_borrows.insert(var.into());
    }

    /// Record a mutable borrow.
    pub fn borrow_mutable(&mut self, var: impl Into<String>) {
        self.mutable_borrows.insert(var.into());
    }

    /// Check if a variable is borrowed at all.
    pub fn is_borrowed(&self, var: &str) -> bool {
        self.immutable_borrows.contains(var) || self.mutable_borrows.contains(var)
    }

    /// Check if a variable is borrowed mutably.
    pub fn is_mutably_borrowed(&self, var: &str) -> bool {
        self.mutable_borrows.contains(var)
    }

    /// Merge two borrow sets (for combining branches).
    pub fn merge(&mut self, other: &BorrowSet) {
        self.immutable_borrows
            .extend(other.immutable_borrows.iter().cloned());
        self.mutable_borrows
            .extend(other.mutable_borrows.iter().cloned());
    }

    /// Find conflicts: variables that are borrowed mutably in both sets.
    pub fn conflict_with(&self, other: &BorrowSet) -> Vec<String> {
        let mut conflicts = Vec::new();

        // Mutable-mutable conflict
        for var in self.mutable_borrows.iter() {
            if other.mutable_borrows.contains(var) {
                conflicts.push(var.clone());
            }
        }

        // Mutable-immutable conflict (one mutable, other immutable on same var)
        for var in self.mutable_borrows.iter() {
            if other.immutable_borrows.contains(var) {
                conflicts.push(var.clone());
            }
        }
        for var in self.immutable_borrows.iter() {
            if other.mutable_borrows.contains(var) {
                conflicts.push(var.clone());
            }
        }

        conflicts.sort();
        conflicts.dedup();
        conflicts
    }
}

/// A lifetime: the scope where a reference is valid.
/// Simplified: lifetimes are tied to blocks/scopes, not explicitly annotated.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Lifetime {
    /// The scope index where this lifetime is bound.
    pub scope_id: usize,
}

impl Lifetime {
    pub fn new(scope_id: usize) -> Self {
        Lifetime { scope_id }
    }
}

/// Errors in borrow checking.
#[derive(Debug, Clone)]
pub enum BorrowError {
    /// A mutable variable is borrowed in multiple parallel branches.
    MutableBorrowInParallel {
        var_name: String,
        first_branch_line: u32,
        first_branch_col: u32,
        second_branch_line: u32,
        second_branch_col: u32,
    },
    /// A mutable borrow conflicts with an immutable borrow.
    BorrowConflict {
        var_name: String,
        first_line: u32,
        first_col: u32,
        second_line: u32,
        second_col: u32,
    },
    /// A variable declared as `let` is used mutably.
    MutableUseOfImmutable {
        var_name: String,
        declared_line: u32,
        declared_col: u32,
        use_line: u32,
        use_col: u32,
    },
}

impl std::fmt::Display for BorrowError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            BorrowError::MutableBorrowInParallel {
                var_name,
                first_branch_line,
                first_branch_col,
                second_branch_line,
                second_branch_col,
            } => write!(
                f,
                "mutable variable '{}' borrowed in multiple parallel branches: first at {}:{}, also at {}:{}",
                var_name, first_branch_line, first_branch_col, second_branch_line, second_branch_col
            ),
            BorrowError::BorrowConflict {
                var_name,
                first_line,
                first_col,
                second_line,
                second_col,
            } => write!(
                f,
                "conflicting borrows of '{}': immutable at {}:{}, mutable at {}:{}",
                var_name, first_line, first_col, second_line, second_col
            ),
            BorrowError::MutableUseOfImmutable {
                var_name,
                declared_line,
                declared_col,
                use_line,
                use_col,
            } => write!(
                f,
                "mutable use of immutable variable '{}' (declared at {}:{}) at {}:{}",
                var_name, declared_line, declared_col, use_line, use_col
            ),
        }
    }
}

impl std::error::Error for BorrowError {}

/// Tracks borrowing for a block of code.
#[derive(Debug)]
pub struct BorrowChecker {
    /// Scopes and their borrow information.
    borrow_map: HashMap<usize, BorrowSet>,
}

impl BorrowChecker {
    pub fn new() -> Self {
        BorrowChecker {
            borrow_map: HashMap::new(),
        }
    }

    /// Initialize borrowing for a scope.
    pub fn init_scope(&mut self, scope_id: usize) {
        self.borrow_map.insert(scope_id, BorrowSet::new());
    }

    /// Record a borrow in a scope.
    pub fn record_borrow(&mut self, scope_id: usize, var: impl Into<String>, is_mutable: bool) {
        if let Some(borrows) = self.borrow_map.get_mut(&scope_id) {
            if is_mutable {
                borrows.borrow_mutable(var);
            } else {
                borrows.borrow_immutable(var);
            }
        }
    }

    /// Check mutability constraints on a declaration vs a borrow.
    pub fn check_mutability(
        &self,
        decl: &Declaration,
        is_mutable_use: bool,
        use_line: u32,
        use_col: u32,
    ) -> Result<(), BorrowError> {
        // If declared as `let` (immutable) but used mutably, that's an error.
        if decl.kind == DeclKind::Let && is_mutable_use {
            return Err(BorrowError::MutableUseOfImmutable {
                var_name: decl.name.clone(),
                declared_line: decl.line,
                declared_col: decl.col,
                use_line,
                use_col,
            });
        }
        Ok(())
    }

    /// Validate that mutable variables in a parallel mission don't have data races.
    pub fn check_parallel_borrows(
        &self,
        branch_scopes: Vec<usize>,
        _var_mutability: &HashMap<String, bool>,
    ) -> Result<(), BorrowError> {
        // Collect borrows from each branch
        let branch_borrows: Vec<_> = branch_scopes
            .iter()
            .map(|&scope_id| {
                self.borrow_map
                    .get(&scope_id)
                    .cloned()
                    .unwrap_or_default()
            })
            .collect();

        // Check for conflicting mutable borrows across branches
        for i in 0..branch_borrows.len() {
            for j in (i + 1)..branch_borrows.len() {
                let conflicts = branch_borrows[i].conflict_with(&branch_borrows[j]);
                if !conflicts.is_empty() {
                    // For now, report the first conflict
                    let var_name = conflicts[0].clone();
                    return Err(BorrowError::MutableBorrowInParallel {
                        var_name,
                        first_branch_line: 0,  // Simplified: we don't track exact line yet
                        first_branch_col: 0,
                        second_branch_line: 0,
                        second_branch_col: 0,
                    });
                }
            }
        }

        Ok(())
    }
}

impl Default for BorrowChecker {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn borrow_set_immutable() {
        let mut bs = BorrowSet::new();
        bs.borrow_immutable("x");
        assert!(bs.is_borrowed("x"));
        assert!(!bs.is_mutably_borrowed("x"));
    }

    #[test]
    fn borrow_set_mutable() {
        let mut bs = BorrowSet::new();
        bs.borrow_mutable("x");
        assert!(bs.is_borrowed("x"));
        assert!(bs.is_mutably_borrowed("x"));
    }

    #[test]
    fn borrow_set_conflict_mutable_mutable() {
        let mut bs1 = BorrowSet::new();
        bs1.borrow_mutable("x");
        let mut bs2 = BorrowSet::new();
        bs2.borrow_mutable("x");
        let conflicts = bs1.conflict_with(&bs2);
        assert!(conflicts.contains(&"x".to_string()));
    }

    #[test]
    fn borrow_set_conflict_mutable_immutable() {
        let mut bs1 = BorrowSet::new();
        bs1.borrow_mutable("x");
        let mut bs2 = BorrowSet::new();
        bs2.borrow_immutable("x");
        let conflicts = bs1.conflict_with(&bs2);
        assert!(conflicts.contains(&"x".to_string()));
    }

    #[test]
    fn borrow_set_no_conflict_different_vars() {
        let mut bs1 = BorrowSet::new();
        bs1.borrow_mutable("x");
        let mut bs2 = BorrowSet::new();
        bs2.borrow_mutable("y");
        let conflicts = bs1.conflict_with(&bs2);
        assert!(conflicts.is_empty());
    }

    #[test]
    fn borrow_set_merge() {
        let mut bs1 = BorrowSet::new();
        bs1.borrow_immutable("x");
        let mut bs2 = BorrowSet::new();
        bs2.borrow_mutable("y");
        bs1.merge(&bs2);
        assert!(bs1.is_borrowed("x"));
        assert!(bs1.is_borrowed("y"));
        assert!(bs1.is_mutably_borrowed("y"));
    }

    #[test]
    fn checker_check_mutability_let_immutable_ok() {
        let checker = BorrowChecker::new();
        let decl = Declaration::new("x", DeclKind::Let, 1, 1);
        let result = checker.check_mutability(&decl, false, 2, 1);
        assert!(result.is_ok());
    }

    #[test]
    fn checker_check_mutability_let_mutable_error() {
        let checker = BorrowChecker::new();
        let decl = Declaration::new("x", DeclKind::Let, 1, 1);
        let result = checker.check_mutability(&decl, true, 2, 1);
        assert!(result.is_err());
        match result.unwrap_err() {
            BorrowError::MutableUseOfImmutable { var_name, .. } => {
                assert_eq!(var_name, "x");
            }
            _ => panic!("unexpected error"),
        }
    }

    #[test]
    fn checker_check_mutability_var_mutable_ok() {
        let checker = BorrowChecker::new();
        let decl = Declaration::new("x", DeclKind::Var, 1, 1);
        let result = checker.check_mutability(&decl, true, 2, 1);
        assert!(result.is_ok());
    }

    #[test]
    fn checker_init_scope() {
        let mut checker = BorrowChecker::new();
        checker.init_scope(0);
        assert!(checker.borrow_map.contains_key(&0));
    }

    #[test]
    fn checker_record_borrow_immutable() {
        let mut checker = BorrowChecker::new();
        checker.init_scope(0);
        checker.record_borrow(0, "x", false);
        let bs = &checker.borrow_map[&0];
        assert!(bs.is_borrowed("x"));
        assert!(!bs.is_mutably_borrowed("x"));
    }

    #[test]
    fn checker_record_borrow_mutable() {
        let mut checker = BorrowChecker::new();
        checker.init_scope(0);
        checker.record_borrow(0, "x", true);
        let bs = &checker.borrow_map[&0];
        assert!(bs.is_borrowed("x"));
        assert!(bs.is_mutably_borrowed("x"));
    }

    #[test]
    fn lifetime_new() {
        let lt = Lifetime::new(5);
        assert_eq!(lt.scope_id, 5);
    }
}

