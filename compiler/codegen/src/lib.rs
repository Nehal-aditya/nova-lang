//! NOVA CodeGen Phase: LLVM IR Emission
//!
//! Phase 5 of the NOVA compiler pipeline.
//!
//! ## What this crate does
//!
//! Converts typed, validated NOVA AST to LLVM IR using inkwell:
//!   - Function and mission code generation
//!   - Type mapping to LLVM types
//!   - Memory management and allocation
//!   - Parallel execution code generation
//!   - Optimization passes
//!
//! ## Pipeline Integration
//!
//! The codegen phase receives typed AST from earlier phases:
//!   Parser (C) → Unit Resolver → Type Checker → Semantic Analysis → [CodeGen]
//!
//! The `ast_bridge` module provides FFI bindings to consume the C parser's AST
//! and convert it into Rust structures suitable for code generation.

pub mod ir_emitter;
pub mod ffi;
pub mod parallel_scheduler;
pub mod ast_bridge;

pub use ir_emitter::{
    IREmitter, CodegenError, ModuleBuilder, FunctionBuilder,
};
pub use ffi::FFICodegen;
pub use parallel_scheduler::ParallelScheduler;
pub use ast_bridge::{AstConsumer, ParsedMission, ParsedStatement, ParsedExpression};

use thiserror::Error;

/// Comprehensive error type for code generation.
#[derive(Debug, Clone, Error)]
pub enum LoweringError {
    #[error("codegen error: {0}")]
    CodegenError(String),

    #[error("llvm error: {0}")]
    LLVMError(String),

    #[error("function not found: {0}")]
    FunctionNotFound(String),

    #[error("type mismatch: {0}")]
    TypeMismatch(String),

    #[error("unsupported operation: {0}")]
    UnsupportedOperation(String),
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn placeholder() {
        // Tests in submodules
    }
}
