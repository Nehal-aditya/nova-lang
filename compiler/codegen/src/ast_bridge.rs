//! AST Bridge: Connects C Parser AST to Rust Codegen
//!
//! This module provides FFI bindings to consume the C parser's AST
//! and convert it into a form suitable for the Rust typechecker and codegen.

use std::ffi::{CStr, c_char, c_void};
use std::os::raw::c_int;
use crate::ir_emitter::{NovaType, FunctionBuilder, ModuleBuilder, CompiledModule, CodegenError};

// ── FFI Type Definitions (matching C AST) ─────────────────────────────────────

#[repr(C)]
pub struct NovString {
    pub ptr: *const c_char,
    pub len: usize,
}

impl NovString {
    pub fn to_rust_string(&self) -> String {
        if self.ptr.is_null() || self.len == 0 {
            return String::new();
        }
        unsafe {
            let slice = std::slice::from_raw_parts(self.ptr as *const u8, self.len);
            String::from_utf8_lossy(slice).to_string()
        }
    }
}

#[repr(C)]
pub enum NovAstNodeType {
    AST_PROGRAM = 0,
    AST_MISSION_DECL,
    AST_PARAM,
    AST_CONSTELLATION_DECL,
    AST_ABSORB,
    AST_LET_DECL,
    AST_VAR_DECL,
    AST_STRUCT_DECL,
    AST_FIELD,
    AST_ENUM_DECL,
    AST_ENUM_VARIANT,
    AST_TRAIT_DECL,
    AST_INTERFACE_DECL,
    AST_UNIT_DECL,
    AST_MODEL_DECL,
    AST_LAYER_DECL,
    AST_BLOCK,
    AST_RETURN_STMT,
    AST_IF_STMT,
    AST_FOR_STMT,
    AST_WHILE_STMT,
    AST_LOOP_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_EXPR_STMT,
    AST_AUTODIFF_STMT,
    AST_ON_DEVICE_STMT,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_CALL_EXPR,
    AST_NAMED_ARG,
    AST_INDEX_EXPR,
    AST_FIELD_EXPR,
    AST_PIPELINE_EXPR,
    AST_PIPE_EXPR,
    AST_MATCH_EXPR,
    AST_MATCH_ARM,
    AST_LAMBDA_EXPR,
    AST_GRADIENT_EXPR,
    AST_AS_EXPR,
    AST_QUESTION_EXPR,
    AST_RANGE_EXPR,
    AST_STRUCT_LITERAL,
    AST_ARRAY_LITERAL,
    AST_TRANSMIT_EXPR,
    AST_PANIC_EXPR,
    AST_INT_LIT,
    AST_FLOAT_LIT,
    AST_UNIT_LIT,
    AST_STRING_LIT,
    AST_BOOL_LIT,
    AST_NAMED_TYPE,
    AST_UNIT_TYPE,
    AST_TENSOR_TYPE,
    AST_GENERIC_TYPE,
    AST_VOID_TYPE,
    AST_NEVER_TYPE,
    AST_IDENT,
    AST_PATH,
    AST_NODE_COUNT,
}

#[repr(C)]
pub struct NovAstNode {
    pub node_type: NovAstNodeType,
    pub line: u32,
    pub col: u32,
    pub data: *mut c_void, // Opaque pointer to union data
}

// ── Parsed Mission Representation ────────────────────────────────────────────

/// Intermediate representation of a mission after parsing.
#[derive(Debug, Clone)]
pub struct ParsedMission {
    pub name: String,
    pub params: Vec<(String, NovaType)>,
    pub return_type: NovaType,
    pub is_parallel: bool,
    pub body: Vec<ParsedStatement>,
    pub line: u32,
    pub col: u32,
}

/// Intermediate representation of a statement.
#[derive(Debug, Clone)]
pub enum ParsedStatement {
    LetBinding {
        name: String,
        init_expr: ParsedExpression,
    },
    Return {
        value: Option<ParsedExpression>,
    },
    Expression {
        expr: ParsedExpression,
    },
    If {
        condition: ParsedExpression,
        then_block: Vec<ParsedStatement>,
        else_block: Option<Vec<ParsedStatement>>,
    },
    // More statement types as needed...
}

/// Intermediate representation of an expression.
#[derive(Debug, Clone)]
pub enum ParsedExpression {
    IntLiteral(i64),
    FloatLiteral(f64),
    UnitLiteral {
        value: f64,
        unit_str: String,
    },
    BoolLiteral(bool),
    StringLiteral(String),
    Identifier(String),
    BinaryOp {
        op: BinaryOperator,
        left: Box<ParsedExpression>,
        right: Box<ParsedExpression>,
    },
    UnaryOp {
        op: UnaryOperator,
        operand: Box<ParsedExpression>,
    },
    Call {
        callee: String,
        args: Vec<ParsedExpression>,
    },
    // More expression types as needed...
}

#[derive(Debug, Clone, Copy)]
pub enum BinaryOperator {
    Add,
    Sub,
    Mul,
    Div,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or,
}

#[derive(Debug, Clone, Copy)]
pub enum UnaryOperator {
    Neg,
    Not,
}

// ── AST Consumer ──────────────────────────────────────────────────────────────

/// Consumes parsed missions and generates LLVM IR.
pub struct AstConsumer {
    module_builder: ModuleBuilder,
}

impl AstConsumer {
    pub fn new(module_name: String) -> Self {
        AstConsumer {
            module_builder: ModuleBuilder::new(module_name),
        }
    }

    /// Consume a parsed mission and emit IR.
    pub fn consume_mission(&mut self, mission: ParsedMission) -> Result<(), CodegenError> {
        let mut func_builder = FunctionBuilder::new(mission.name.clone(), mission.return_type);

        // Add parameters
        for (param_name, param_type) in &mission.params {
            func_builder.add_parameter(param_name.clone(), *param_type)?;
        }

        // Generate body instructions
        for stmt in &mission.body {
            self.emit_statement(&mut func_builder, stmt)?;
        }

        // Add implicit return void if needed
        if mission.return_type == NovaType::Void {
            func_builder.emit_return(None)?;
        }

        let func_def = func_builder.build();
        self.module_builder.add_function(func_def);

        Ok(())
    }

    fn emit_statement(
        &self,
        builder: &mut FunctionBuilder,
        stmt: &ParsedStatement,
    ) -> Result<(), CodegenError> {
        match stmt {
            ParsedStatement::LetBinding { name, init_expr } => {
                let value = self.emit_expression(builder, init_expr)?;
                // In real implementation, we'd store this in a local variable
                // For now, just ensure the expression is evaluated
                let _ = (name, value);
            }
            ParsedStatement::Return { value } => {
                if let Some(expr) = value {
                    let val = self.emit_expression(builder, expr)?;
                    builder.emit_return(Some(&val))?;
                } else {
                    builder.emit_return(None)?;
                }
            }
            ParsedStatement::Expression { expr } => {
                let _ = self.emit_expression(builder, expr)?;
            }
            ParsedStatement::If { condition, then_block, else_block } => {
                self.emit_if(builder, condition, then_block, else_block.as_deref())?;
            }
        }
        Ok(())
    }

    fn emit_if(
        &self,
        builder: &mut FunctionBuilder,
        condition: &ParsedExpression,
        then_block: &[ParsedStatement],
        else_block: Option<&[ParsedStatement]>,
    ) -> Result<(), CodegenError> {
        use std::sync::atomic::{AtomicU32, Ordering};
        static LABEL_COUNTER: AtomicU32 = AtomicU32::new(0);
        
        let id = LABEL_COUNTER.fetch_add(1, Ordering::Relaxed);
        let then_label = format!("then_{}", id);
        let else_label = format!("else_{}", id);
        let merge_label = format!("merge_{}", id);

        // Emit condition
        let cond_val = self.emit_expression(builder, condition)?;

        // Branch based on condition
        builder.emit_cond_br(&cond_val, &then_label, &else_label)?;

        // Then block
        builder.emit_label(&then_label);
        for stmt in then_block {
            self.emit_statement(builder, stmt)?;
        }
        builder.emit_br(&merge_label);

        // Else block
        builder.emit_label(&else_label);
        if let Some(else_stmts) = else_block {
            for stmt in else_stmts {
                self.emit_statement(builder, stmt)?;
            }
        }
        builder.emit_br(&merge_label);

        // Merge point
        builder.emit_label(&merge_label);

        Ok(())
    }

    fn emit_expression(
        &self,
        builder: &mut FunctionBuilder,
        expr: &ParsedExpression,
    ) -> Result<crate::ir_emitter::Value, CodegenError> {
        use std::sync::atomic::{AtomicU32, Ordering};
        static TEMP_COUNTER: AtomicU32 = AtomicU32::new(0);

        match expr {
            ParsedExpression::IntLiteral(value) => {
                let temp_name = format!("tmp_{}", TEMP_COUNTER.fetch_add(1, Ordering::Relaxed));
                builder.emit_instruction(format!(
                    "%{} = add i64 {}, 0",
                    temp_name, value
                ));
                Ok(crate::ir_emitter::Value::new(temp_name, NovaType::Int))
            }
            ParsedExpression::FloatLiteral(value) => {
                let temp_name = format!("tmp_{}", TEMP_COUNTER.fetch_add(1, Ordering::Relaxed));
                builder.emit_instruction(format!(
                    "%{} = fadd double {}, 0.0",
                    temp_name, value
                ));
                Ok(crate::ir_emitter::Value::new(temp_name, NovaType::Float))
            }
            ParsedExpression::Identifier(name) => {
                // In a real implementation, we'd look up the variable
                // For now, assume it's a parameter or previously defined variable
                Ok(crate::ir_emitter::Value::new(name.clone(), NovaType::Float))
            }
            ParsedExpression::BinaryOp { op, left, right } => {
                let left_val = self.emit_expression(builder, left)?;
                let right_val = self.emit_expression(builder, right)?;
                
                let temp_name = format!("tmp_{}", TEMP_COUNTER.fetch_add(1, Ordering::Relaxed));
                
                match op {
                    BinaryOperator::Add => {
                        builder.emit_add(temp_name.clone(), &left_val, &right_val)
                    }
                    BinaryOperator::Sub => {
                        builder.emit_sub(temp_name.clone(), &left_val, &right_val)
                    }
                    BinaryOperator::Mul => {
                        builder.emit_mul(temp_name.clone(), &left_val, &right_val)
                    }
                    BinaryOperator::Div => {
                        builder.emit_div(temp_name.clone(), &left_val, &right_val)
                    }
                    _ => Err(CodegenError::UnsupportedOperation(
                        format!("binary operator {:?}", op)
                    ))
                }
            }
            ParsedExpression::Call { callee, args } => {
                let arg_values: Vec<_> = args
                    .iter()
                    .map(|arg| self.emit_expression(builder, arg))
                    .collect::<Result<_, _>>()?;
                
                // Assume float return type for now
                let result_name = format!("tmp_{}", TEMP_COUNTER.fetch_add(1, Ordering::Relaxed));
                let result = builder.emit_call(
                    result_name.clone(),
                    callee,
                    NovaType::Float,
                    &arg_values,
                )?;
                
                Ok(result.unwrap_or_else(|| 
                    crate::ir_emitter::Value::new(result_name, NovaType::Void)
                ))
            }
            _ => Err(CodegenError::UnsupportedOperation(
                format!("expression type {:?}", expr)
            )),
        }
    }

    /// Build the final module.
    pub fn build(self) -> CompiledModule {
        self.module_builder.build()
    }
}

// ── Helper Functions ──────────────────────────────────────────────────────────

/// Parse a simple type string into NovaType.
pub fn parse_type_string(type_str: &str) -> Option<NovaType> {
    match type_str {
        "Float" | "f64" | "double" => Some(NovaType::Float),
        "Int" | "i64" => Some(NovaType::Int),
        "Bool" | "i1" => Some(NovaType::Bool),
        "Void" => Some(NovaType::Void),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn novstring_to_rust() {
        let s = b"hello";
        let nov_s = NovString {
            ptr: s.as_ptr() as *const c_char,
            len: s.len(),
        };
        assert_eq!(nov_s.to_rust_string(), "hello");
    }

    #[test]
    fn parse_type_float() {
        assert_eq!(parse_type_string("Float"), Some(NovaType::Float));
    }

    #[test]
    fn parse_type_int() {
        assert_eq!(parse_type_string("Int"), Some(NovaType::Int));
    }

    #[test]
    fn ast_consumer_create() {
        let consumer = AstConsumer::new("test_module");
        assert!(!consumer.module_builder.name.is_empty());
    }

    #[test]
    fn consume_simple_mission() {
        let mut consumer = AstConsumer::new("test".to_string());
        
        let mission = ParsedMission {
            name: "add_numbers".to_string(),
            params: vec![
                ("x".to_string(), NovaType::Float),
                ("y".to_string(), NovaType::Float),
            ],
            return_type: NovaType::Float,
            is_parallel: false,
            body: vec![
                ParsedStatement::Return {
                    value: Some(ParsedExpression::BinaryOp {
                        op: BinaryOperator::Add,
                        left: Box::new(ParsedExpression::Identifier("x".to_string())),
                        right: Box::new(ParsedExpression::Identifier("y".to_string())),
                    }),
                },
            ],
            line: 1,
            col: 1,
        };

        consumer.consume_mission(mission).expect("consume mission");
        let module = consumer.build();
        let ir = module.emit_ir();
        
        assert!(ir.contains("@add_numbers"));
        assert!(ir.contains("fadd"));
    }
}
