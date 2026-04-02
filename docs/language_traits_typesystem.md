# NOVA Language Traits & Type System

## Language Heritage

NOVA is a multi-paradigm language that unifies the best features from several programming languages:

### Python Influence
- **Readable syntax**: Clean, indentation-aware code structure
- **Dynamic feel**: Expressive constructs like `pipeline [...]` and string interpolation
- **Scientific ecosystem**: `cosmos.*` namespace mirrors NumPy, SciPy, scikit-learn
- **Scripting capability**: Quick prototyping with REPL support

**Example:**
```nova
let stars =
  read_fits(catalog)
  |> pipeline [
      filter(luminosity_class == "V"),
      map(s -> { mass: s.mass_solar, lum: log10(s.luminosity_solar) }),
      drop_outliers(sigma: 3.0),
    ]
```

### Rust Influence
- **Ownership/borrowing**: Memory safety without garbage collection
- **Traits**: Ad-hoc polymorphism and type classes
- **Zero-cost abstractions**: High-level syntax compiles to efficient code
- **Type safety**: Compiler prevents memory errors and data races

**Compiler Implementation:** The type checker, semantic analyzer, and codegen are written in Rust to leverage its ownership model for compiler correctness.

### C Influence
- **Performance**: Lexer and parser in C for maximum speed
- **Manual control**: Direct memory access when needed via FFI
- **Systems programming**: Low-level capabilities for runtime system
- **FFI**: Easy integration with existing C libraries (BLAS, LAPACK, etc.)

**Compiler Implementation:** Lexer (576 LOC) and parser (973 LOC) in C for zero-overhead tokenization.

### Java Influence
- **Interfaces**: Mission signatures as contracts
- **OOP patterns**: Struct/constellation validation
- **Enterprise robustness**: Comprehensive error checking
- **Class hierarchies**: Type relationships and inheritance patterns

**Compiler Implementation:** Interface validator (1,124 LOC) in Java for OOP contract validation.

### Haskell/ML Influence
- **Hindley-Milner type inference**: Automatic type deduction
- **Pattern matching**: `match` expressions for algebraic data types
- **Algebraic data types**: `enum` for sum types
- **Unit types**: Compile-time dimensional analysis

**Example:**
```nova
match result {
  Ok(value) => transmit("Success: {value}"),
  Err(e)    => panic("Failed: {e}"),
}
```

### Julia Influence
- **Multiple dispatch**: Functions specialized on argument types
- **Scientific computing**: First-class support for units and dimensions
- **JIT performance**: Optimized numerical kernels
- **Unit-aware arithmetic**: `1.0[m] + 500.0[cm]` auto-converts to `1.5[m]`

### Fortran Influence
- **Array operations**: Native tensor syntax with `@` for matmul
- **Numerical stability**: Careful handling of floating-point operations
- **HPC heritage**: Parallel-by-default missions
- **Column-major optional**: Support for scientific data layouts

---

## Type System

### Core Principles

1. **Static typing with inference**: All types known at compile time, but rarely need explicit annotations
2. **Units as types**: Physical dimensions are part of the type system
3. **Tensor shapes as types**: Array dimensions checked at compile time when possible
4. **Zero-cost units**: Unit annotations erased after type checking - no runtime overhead

### Type Categories

#### Primitive Types
- `Int` - 64-bit signed integer
- `Float` - 64-bit floating point (with optional unit annotation)
- `Bool` - Boolean (`true` or `false`)
- `String` - UTF-8 text
- `Char` - Unicode scalar value

#### Unit-Annotated Types
```nova
let distance = 1.0[m]           -- Float[m]
let time     = 3600.0[s]        -- Float[s]
let velocity = distance / time  -- Float[m/s] inferred
```

Units are represented as 7-dimensional SI vectors:
- `[m]` -> `(L=1, M=0, T=0, I=0, Theta=0, N=0, J=0)`
- `[kg*m/s^2]` -> `(L=1, M=1, T=-2, ...)` -> recognized as `N`

#### Tensor Types
```nova
let matrix : Tensor[Float, 3x4] = ...
let vector : Tensor[Float, 4]   = ...
let result : Tensor[Float, 3]   = matrix @ vector  -- matmul
```

Shape checking:
- `@` (matmul): `(A,B) @ (B,C) -> (A,C)` - inner dimensions must match
- `+`, `-`: Shapes must be identical or broadcastable
- `reshape`: Output shape must have same element count

#### Composite Types
- `Array[T]` - Dynamic array of type `T`
- `Wave[T]` - Lazy stream/sequence
- `Option[T]` - Optional value (`Some(T)` or `None`)
- `Result[T, E]` - Success or error
- `Dataset[X, Y]` - Training data with inputs and labels

#### User-Defined Types
```nova
struct Star {
  name: String,
  mass_solar: Float,
  luminosity_solar: Float,
  surface_temp: Float[K],
}

enum SpectralType {
  O, B, A, F, G, K, M
}

model Transformer {
  layer embedding(vocab=32000, dim=1024)
  layer repeat(24) {
    layer attention(heads=16, dim=1024)
    layer feedforward(dim=4096)
  }
}
```

### Type Inference

NOVA uses Hindley-Milner Algorithm W, extended with:
- Unit dimension tracking
- Tensor shape inference
- Row polymorphism for struct field access

**Example:**
```nova
let x = 5.0[m/s]              -- Float[m/s] inferred
let y = 10.0[s]               -- Float[s] inferred
let z = x * y                 -- Float[m] inferred automatically
```

### Unit Resolution

The unit resolver runs between parsing and type checking:

1. **Parse unit expressions** into 7D SI vectors
2. **Build alias table** from built-in and user-defined units
3. **Auto-convert** compatible units on addition/subtraction
4. **Reject incompatible** dimensions with clear error messages
5. **Recognize derived units** (e.g., `kg*m/s^2` displayed as `N`)

**Built-in unit families:**
- Length: `m`, `km`, `AU`, `ly`, `pc`
- Mass: `kg`, `g`, `M_sun`
- Time: `s`, `hr`, `yr`
- Temperature: `K`, `C`
- Energy: `J`, `eV`, `erg`
- Force: `N`, `lbf`
- Pressure: `Pa`, `atm`, `bar`
- Frequency: `Hz`, `GHz`
- Angle: `rad`, `deg`, `arcsec`
- Data: `B`, `GB`, `TB`

**Custom units:**
```nova
unit parsec = 3.086e16[m]
unit light_year = 9.461e15[m]
unit solar_mass = 1.989e30[kg]
```

### Error Messages

Domain-aware, not generic:

**Unit mismatch:**
```
Error: cannot add Float[kg] and Float[m]
       dimension [M] vs [L] - incompatible SI dimensions
       hint: did you mean to convert units, or is this a logic error?
```

**Shape mismatch:**
```
Error: matmul (3,4) @ (5,6) - inner dimensions 4 and 5 do not match
       expected: (A,B) @ (B,C) -> (A,C)
       hint: transpose the first matrix? try: left.transpose() @ right
```

**Missing field:**
```
Error: type `Star` has no field `luminosity_solar`
       available fields: name, mass_solar, luminosity_solar, surface_temp
       hint: did you mean `luminosity_solar`?
```

---

## Compiler Architecture

### Multi-Language Pipeline

```
+-------------------------------------------------------------+
|                    NOVA Compiler Pipeline                    |
+-------------------------------------------------------------+
|                                                              |
|  NOVA Source (.nv)                                           |
|         |                                                    |
|  +--------------+                                            |
|  |   Lexer      | C (576 LOC)                                |
|  |              | Zero-copy tokenization, UTF-8              |
|  +--------------+                                            |
|         |                                                    |
|  +--------------+                                            |
|  |   Parser     | C (973 LOC)                                |
|  |              | Recursive descent, source spans            |
|  +--------------+                                            |
|         |                                                    |
|  AST (C structs / JSON interchange)                          |
|         |                                                    |
|  +--------------+  +--------------+  +------------------+    |
|  |Unit Resolver |->| Type Checker |->|Semantic Analyzer |    |
|  |   (Rust)     |  |   (Rust)     |  |    (Rust)        |    |
|  |  1,391 LOC   |  |  2,065 LOC   |  |    1,208 LOC     |    |
|  +--------------+  +--------------+  +------------------+    |
|                                              |               |
|  +--------------+  +--------------+  +------------------+    |
|  |  Autodiff    |<- |   Tensor     |<-|   IR Emitter     |    |
|  |  (Rust)      |  |  Lowering    |  |    (Rust)        |    |
|  |  1,076 LOC   |  |  (Rust)      |  |    708 LOC       |    |
|  +--------------+  +--------------+  +------------------+    |
|                                              |               |
|  +--------------+  +--------------+                          |
|  |  Interface   |<-|   LLVM IR    |                          |
|  |  Validator   |  |   (strings)  |                          |
|  |  (Java)      |  |              |                          |
|  |  1,124 LOC   |  |              |                          |
|  +--------------+  +--------------+                          |
|                                              |               |
|  LLVM IR -> opt -> llc -> clang -> Native Binary             |
|                                                              |
+-------------------------------------------------------------+

Total: ~9,500+ lines across C, Rust, Java
```

### Why Multi-Language?

| Language | Role | Rationale |
|----------|------|-----------|
| **C** | Lexer/Parser | Maximum performance, mature ecosystem, excellent FFI, zero-overhead |
| **Rust** | Type System -> Codegen | Ownership prevents compiler bugs, strong type inference libs, memory-safe |
| **Java** | Interface Validation | Enterprise-grade OOP patterns, robust contract validation |
| **Python** | Stdlib References | Rapid prototyping, mirrors scientific Python ecosystem |

### FFI Boundaries

```
NOVA Source 
   |
[C Lexer/Parser] -> outputs AST as C structs or JSON
   |
[Rust Type Checker] -> reads AST via FFI, produces typed IR
   |
[Rust Codegen] -> emits LLVM IR strings
   |
[LLVM Tools] -> compiles to native binary
```

---

## Implementation Status

### Complete (~9,500 LOC)

| Component | Language | LOC | File Path |
|-----------|----------|-----|-----------|
| Lexer | C | 576 | `compiler/lexer/src/lexer.c` |
| Parser | C | 973 | `compiler/parser/src/parser.c` |
| Unit Resolver | Rust | 1,391 | `compiler/unit_resolver/src/` |
| Type Checker | Rust | 2,065 | `compiler/typechecker/src/` |
| Semantic Analyzer | Rust | 1,208 | `compiler/semantic/src/` |
| Autodiff System | Rust | 1,076 | `compiler/autodiff/src/` |
| Tensor Lowering | Rust | 553 | `compiler/tensor_lowering/src/` |
| IR Emitter | Rust | 708 | `compiler/codegen/src/ir_emitter.rs` |
| Interface Validator | Java | 1,124 | `compiler/interface_validator/src/` |
| Stdlib (refs) | Python | ~2,000+ | `stdlib/` |

### Integration Needed

- LLVM backend integration (inkwell or external toolchain)
- Compiler main binary (orchestration)
- Runtime system (`transmit`, memory allocators, tensor runtime)

### Not Started

- GPU backend (CUDA/Metal)
- Distributed missions
- Jupyter kernel
- Full standard library implementations
