//! LLVM IR Emitter
//!
//! Converts NOVA AST to LLVM IR using inkwell bindings.

use std::collections::HashMap;
use thiserror::Error;

/// Errors that can occur during IR emission.
#[derive(Debug, Clone, Error)]
pub enum CodegenError {
    #[error("type error: {0}")]
    TypeError(String),

    #[error("unknown function: {0}")]
    UnknownFunction(String),

    #[error("invalid operand: {0}")]
    InvalidOperand(String),

    #[error("unsupported operation: {0}")]
    UnsupportedOperation(String),

    #[error("ir generation failed: {0}")]
    IRGenerationFailed(String),
}

/// NOVA type representation for codegen.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum NovaType {
    Float,
    Int,
    Bool,
    Void,
}

impl NovaType {
    pub fn to_string(&self) -> String {
        match self {
            NovaType::Float => "f64".to_string(),
            NovaType::Int => "i64".to_string(),
            NovaType::Bool => "i1".to_string(),
            NovaType::Void => "void".to_string(),
        }
    }
}

/// Represents an LLVM value in computation.
#[derive(Debug, Clone)]
pub struct Value {
    pub name: String,
    pub ty: NovaType,
}

impl Value {
    pub fn new(name: String, ty: NovaType) -> Self {
        Value { name, ty }
    }
}

/// Builder for LLVM functions.
pub struct FunctionBuilder {
    name: String,
    return_type: NovaType,
    parameters: Vec<(String, NovaType)>,
    body: Vec<String>, // IR instructions as strings
}

impl FunctionBuilder {
    pub fn new(name: String, return_type: NovaType) -> Self {
        FunctionBuilder {
            name,
            return_type,
            parameters: Vec::new(),
            body: Vec::new(),
        }
    }

    pub fn add_parameter(&mut self, name: String, ty: NovaType) -> Result<(), CodegenError> {
        self.parameters.push((name, ty));
        Ok(())
    }

    pub fn emit_instruction(&mut self, instr: String) {
        self.body.push(instr);
    }

    pub fn emit_return(&mut self, value: Option<&Value>) -> Result<(), CodegenError> {
        match value {
            Some(v) => self.emit_instruction(format!("ret {} %{}", v.ty.to_string(), v.name)),
            None => self.emit_instruction("ret void".to_string()),
        }
        Ok(())
    }

    pub fn emit_add(&mut self, result: String, left: &Value, right: &Value) -> Result<Value, CodegenError> {
        if left.ty != right.ty {
            return Err(CodegenError::TypeError(format!(
                "type mismatch in add: {} vs {}",
                left.ty.to_string(),
                right.ty.to_string()
            )));
        }
        let op = match left.ty {
            NovaType::Float => "fadd",
            NovaType::Int => "add",
            NovaType::Bool | NovaType::Void => {
                return Err(CodegenError::UnsupportedOperation(format!(
                    "unsupported add for type {}",
                    left.ty.to_string()
                )))
            }
        };
        self.emit_instruction(format!(
            "%{} = {} {} %{}, %{}",
            result,
            op,
            left.ty.to_string(),
            left.name,
            right.name
        ));
        Ok(Value::new(result, left.ty))
    }

    pub fn emit_mul(&mut self, result: String, left: &Value, right: &Value) -> Result<Value, CodegenError> {
        if left.ty != right.ty {
            return Err(CodegenError::TypeError(format!(
                "type mismatch in mul: {} vs {}",
                left.ty.to_string(),
                right.ty.to_string()
            )));
        }
        let op = match left.ty {
            NovaType::Float => "fmul",
            NovaType::Int => "mul",
            NovaType::Bool | NovaType::Void => {
                return Err(CodegenError::UnsupportedOperation(format!(
                    "unsupported mul for type {}",
                    left.ty.to_string()
                )))
            }
        };
        self.emit_instruction(format!(
            "%{} = {} {} %{}, %{}",
            result,
            op,
            left.ty.to_string(),
            left.name,
            right.name
        ));
        Ok(Value::new(result, left.ty))
    }

    pub fn build(self) -> FunctionDefinition {
        FunctionDefinition {
            name: self.name,
            return_type: self.return_type,
            parameters: self.parameters,
            body: self.body,
        }
    }
}

/// Compiled function definition.
#[derive(Debug, Clone)]
pub struct FunctionDefinition {
    pub name: String,
    pub return_type: NovaType,
    pub parameters: Vec<(String, NovaType)>,
    pub body: Vec<String>,
}

impl FunctionDefinition {
    pub fn emit_signature(&self) -> String {
        let param_str = self
            .parameters
            .iter()
            .map(|(name, ty)| format!("{} %{}", ty.to_string(), name))
            .collect::<Vec<_>>()
            .join(", ");
        format!(
            "define {} @{}({})",
            self.return_type.to_string(),
            self.name,
            param_str
        )
    }

    pub fn emit_full(&self) -> String {
        let mut ir = self.emit_signature();
        ir.push_str(" {\n");
        for instr in &self.body {
            ir.push_str("  ");
            ir.push_str(instr);
            ir.push('\n');
        }
        ir.push('}');
        ir
    }
}

/// Builder for LLVM modules.
pub struct ModuleBuilder {
    name: String,
    functions: Vec<FunctionDefinition>,
    globals: HashMap<String, (NovaType, String)>,
}

impl ModuleBuilder {
    pub fn new(name: String) -> Self {
        ModuleBuilder {
            name,
            functions: Vec::new(),
            globals: HashMap::new(),
        }
    }

    pub fn add_function(&mut self, func: FunctionDefinition) {
        self.functions.push(func);
    }

    pub fn add_global(&mut self, name: String, ty: NovaType, init: String) {
        self.globals.insert(name, (ty, init));
    }

    pub fn build(self) -> CompiledModule {
        CompiledModule {
            name: self.name,
            functions: self.functions,
            globals: self.globals,
        }
    }
}

/// Compiled LLVM module.
#[derive(Debug, Clone)]
pub struct CompiledModule {
    pub name: String,
    pub functions: Vec<FunctionDefinition>,
    pub globals: HashMap<String, (NovaType, String)>,
}

impl CompiledModule {
    pub fn emit_ir(&self) -> String {
        // LLVM IR modules do not have an outer `module {}` wrapper.
        // This emits a textual LLVM IR module that can be fed to `llvm-as` / `opt`.
        let mut ir = format!("; ModuleID = '{}'\n", self.name);
        ir.push_str(&format!("source_filename = \"{}\"\n\n", self.name));
        ir.push_str("; Global definitions\n");

        for (name, (ty, init)) in &self.globals {
            ir.push_str(&format!("@{} = global {} {}\n", name, ty.to_string(), init));
        }

        ir.push('\n');
        ir.push_str("; Function definitions\n");
        for func in &self.functions {
            ir.push_str(&func.emit_full());
            ir.push_str("\n\n");
        }
        ir
    }
}

/// Main IR emitter orchestrator.
pub struct IREmitter {
    module_builder: ModuleBuilder,
}

impl IREmitter {
    pub fn new(module_name: String) -> Self {
        IREmitter {
            module_builder: ModuleBuilder::new(module_name),
        }
    }

    pub fn create_function(&self, name: String, return_type: NovaType) -> FunctionBuilder {
        FunctionBuilder::new(name, return_type)
    }

    pub fn add_function(&mut self, func: FunctionDefinition) -> Result<(), CodegenError> {
        self.module_builder.add_function(func);
        Ok(())
    }

    pub fn add_global(
        &mut self,
        name: String,
        ty: NovaType,
        init: String,
    ) -> Result<(), CodegenError> {
        self.module_builder.add_global(name, ty, init);
        Ok(())
    }

    pub fn emit_module(self) -> CompiledModule {
        self.module_builder.build()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn nova_type_to_string() {
        assert_eq!(NovaType::Float.to_string(), "f64");
        assert_eq!(NovaType::Int.to_string(), "i64");
        assert_eq!(NovaType::Bool.to_string(), "i1");
        assert_eq!(NovaType::Void.to_string(), "void");
    }

    #[test]
    fn value_creation() {
        let val = Value::new("x".to_string(), NovaType::Float);
        assert_eq!(val.name, "x");
        assert_eq!(val.ty, NovaType::Float);
    }

    #[test]
    fn function_builder_create() {
        let builder = FunctionBuilder::new("add".to_string(), NovaType::Float);
        assert_eq!(builder.name, "add");
        assert_eq!(builder.return_type, NovaType::Float);
    }

    #[test]
    fn function_builder_parameters() {
        let mut builder = FunctionBuilder::new("add".to_string(), NovaType::Float);
        builder
            .add_parameter("x".to_string(), NovaType::Float)
            .expect("add x");
        builder
            .add_parameter("y".to_string(), NovaType::Float)
            .expect("add y");
        assert_eq!(builder.parameters.len(), 2);
    }

    #[test]
    fn function_signature_generation() {
        let mut builder = FunctionBuilder::new("add".to_string(), NovaType::Float);
        builder
            .add_parameter("x".to_string(), NovaType::Float)
            .expect("add x");
        builder
            .add_parameter("y".to_string(), NovaType::Float)
            .expect("add y");
        let func = builder.build();
        let sig = func.emit_signature();
        assert!(sig.contains("define f64 @add"));
        assert!(sig.contains("%x"));
        assert!(sig.contains("%y"));
    }

    #[test]
    fn emit_add_instruction() {
        let mut builder = FunctionBuilder::new("add".to_string(), NovaType::Float);
        let x = Value::new("x".to_string(), NovaType::Float);
        let y = Value::new("y".to_string(), NovaType::Float);
        let result = builder.emit_add("z".to_string(), &x, &y).expect("emit add");
        assert_eq!(result.name, "z");
        assert_eq!(result.ty, NovaType::Float);
    }

    #[test]
    fn emit_mul_instruction() {
        let mut builder = FunctionBuilder::new("mul".to_string(), NovaType::Float);
        let x = Value::new("x".to_string(), NovaType::Float);
        let y = Value::new("y".to_string(), NovaType::Float);
        let result = builder.emit_mul("z".to_string(), &x, &y).expect("emit mul");
        assert_eq!(result.name, "z");
        assert_eq!(result.ty, NovaType::Float);
    }

    #[test]
    fn type_mismatch_in_add() {
        let mut builder = FunctionBuilder::new("add".to_string(), NovaType::Float);
        let x = Value::new("x".to_string(), NovaType::Float);
        let y = Value::new("y".to_string(), NovaType::Int);
        let result = builder.emit_add("z".to_string(), &x, &y);
        assert!(result.is_err());
    }

    #[test]
    fn module_builder_create() {
        let builder = ModuleBuilder::new("test".to_string());
        assert_eq!(builder.name, "test");
    }

    #[test]
    fn module_builder_add_function() {
        let mut builder = ModuleBuilder::new("test".to_string());
        let func_builder = FunctionBuilder::new("add".to_string(), NovaType::Float);
        let func = func_builder.build();
        builder.add_function(func);
        let module = builder.build();
        assert_eq!(module.functions.len(), 1);
    }

    #[test]
    fn module_builder_add_global() {
        let mut builder = ModuleBuilder::new("test".to_string());
        builder.add_global("pi".to_string(), NovaType::Float, "3.14159".to_string());
        let module = builder.build();
        assert!(module.globals.contains_key("pi"));
    }

    #[test]
    fn ir_emitter_create() {
        let emitter = IREmitter::new("test".to_string());
        let _ = emitter;
    }

    #[test]
    fn ir_emitter_add_function() {
        let mut emitter = IREmitter::new("test".to_string());
        let func_builder = FunctionBuilder::new("add".to_string(), NovaType::Float);
        let func = func_builder.build();
        emitter.add_function(func).expect("add function");
    }

    #[test]
    fn ir_emitter_add_global() {
        let mut emitter = IREmitter::new("test".to_string());
        emitter
            .add_global("pi".to_string(), NovaType::Float, "3.14159".to_string())
            .expect("add global");
    }

    #[test]
    fn full_ir_generation() {
        let mut func_builder = FunctionBuilder::new("add".to_string(), NovaType::Float);
        func_builder
            .add_parameter("x".to_string(), NovaType::Float)
            .expect("add x");
        func_builder
            .add_parameter("y".to_string(), NovaType::Float)
            .expect("add y");

        let x = Value::new("x".to_string(), NovaType::Float);
        let y = Value::new("y".to_string(), NovaType::Float);
        let result = func_builder
            .emit_add("result".to_string(), &x, &y)
            .expect("emit add");
        func_builder
            .emit_return(Some(&result))
            .expect("emit return");

        let func = func_builder.build();
        let ir = func.emit_full();
        assert!(ir.contains("define f64 @add"));
        assert!(ir.contains("fadd"));
        assert!(ir.contains("ret"));
    }

    #[test]
    fn compiled_module_ir_output() {
        let mut emitter = IREmitter::new("nova_program".to_string());
        let func_builder = FunctionBuilder::new("main".to_string(), NovaType::Int);
        let func = func_builder.build();
        emitter.add_function(func).expect("add function");

        let module = emitter.emit_module();
        let ir = module.emit_ir();
        assert!(ir.contains("nova_program"));
        assert!(ir.contains("@main"));
    }
}
