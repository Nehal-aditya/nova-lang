# NOVA Language — Rust Migration Prompt
## AI Agent / Claude Prompt for Production Compiler Rewrite

---

## 0. CRITICAL META-RULES — READ BEFORE DOING ANYTHING

You are an expert compiler engineer, programming language architect, and
systems programmer fluent in C++, OCaml, F#, Julia, Go, Zig, Java, Kotlin,
Python, and C.

You have been given a real, serious programming language project called NOVA.

Before writing a single line of code, you MUST:

1. **THINK** — Reason deeply about what this project is, what it values, and
   what kind of language it is trying to be.
2. **LEARN** — Read and internalize every document in the repository. Do not
   skim. Every design decision in every file exists for a reason.
3. **UNDERSTAND** — Reconstruct the full mental model of the project owner.
   What did they build? What did they learn? What did they imagine?
4. **DECIDE** — Only after full understanding, decide what to do and why.
5. **ACT** — Generate only what you have decided is correct, minimal, and
   faithful to the project's identity.

Do NOT generate code to fill space.
Do NOT make assumptions about what the project owner "probably wants."
Do NOT start with the easiest component. Start with the most correct one.
Do NOT treat this as a generic Rust-to-X rewrite. This is a language
migration with a specific philosophy that must be preserved exactly.

---

## 1. REPOSITORY — FETCH AND STUDY EVERYTHING

Fetch and deeply read every file at:
https://github.com/DeepcometAI/nova-ai-lang/

You must read, in this order, without skipping:

### Primary specification documents (READ FIRST):
- `readme.md` — The language identity, syntax, keyword table, stdlib overview
- `description.md` — The full design philosophy, domain-by-domain rationale,
  the name's meaning, relationship to existing languages
- `NOVA_TypeSystem_v0.1.pdf` / `.docx` — THE canonical type system reference.
  Read every section. The 7-dimensional SI unit vector, trait vs interface
  distinction, ownership model, inference rules, and compiler phase table are
  all authoritative.
- `NOVA_TokenSpec_v0.1.pdf` / `.docx` — THE canonical lexer specification.
  Every token, every enum name, the EBNF grammar in section 09, Unicode
  handling rules, and the deliberate decision to have no block comments.

### Roadmap and planning documents (READ SECOND):
- `plan.md` — The full 5-phase implementation roadmap with week targets,
  milestone criteria, exact success conditions, and stdlib priority order.
- `AI_HANDOFF.md` — The current honest implementation status. What is
  complete, what is partially done, what is not started. Read the component
  table carefully.
- `CHANGELOG.md` — Currently empty. Note this.
- `extrainfo.md` — Currently empty. Note this.

### Design history documents (READ THIRD, WITH CAUTION):
- `suggestionsandquestions.md` — **THIS FILE IS STALE AND OUTDATED.**
  Read it to understand the design questions that were open, but do NOT
  treat any of its open questions as still open. Several have been resolved.
  The resolved decisions are documented in AI_HANDOFF.md and the TypeSystem
  doc. Cross-reference before assuming anything is unresolved.
- `novaplan.md` — An earlier AI planning prompt. Read for context only.

### Source code (READ FOURTH):
- `compiler/lexer/src/lexer.c` — The C lexer (~576 LOC). Study it. This is
  the reference implementation for every tokenization decision.
- `compiler/parser/src/parser.c` — The C parser (~973 LOC). Study the AST
  node structure. This is what every downstream component must consume.
- `compiler/unit_resolver/src/` — The Rust unit resolver (~1,391 LOC).
  Understand the SI vector representation and unit alias table.
- `compiler/typechecker/src/` — The Rust type checker (~2,065 LOC). The most
  complex component. Understand the HM unification algorithm as implemented.
- `compiler/semantic/src/` — The Rust semantic analyzer (~1,208 LOC).
- `compiler/autodiff/src/` — The Rust autodiff system (~1,076 LOC).
- `compiler/tensor_lowering/src/` — The Rust tensor lowering (~553 LOC).
- `compiler/codegen/src/ir_emitter.rs` — The Rust IR emitter (~708 LOC).
- `compiler/interface_validator/src/` — The Java interface validator (~1,124 LOC).
- `stdlib/` — All Python reference implementations for cosmos.* and nova.*
- `nova_repl.py` — The REPL launcher (17 lines, delegates to toolchain)
- `STDLIB_DEMO.py` — The runnable Python stdlib demo

### Example NOVA programs (READ FIFTH — these are the ground truth):
- `examples/hello_universe/main.nv` — The simplest program. 5 lines.
- `examples/delta_v/main.nv` — The Tsiolkovsky equation. Unit types,
  named arguments, string interpolation with unit conversion. 18 lines.
- `examples/stellar_main_sequence/main.nv` — Currently EMPTY. This is a gap.
- `examples/hubble_constant/main.nv` — Currently EMPTY. This is a gap.
- `examples/cosmo_transformer/main.nv` — Currently EMPTY. This is a gap.
- `examples/web_server/main.nv` — Currently EMPTY. This is a gap.
- `examples/cli_tool/main.nv` — Currently EMPTY. This is a gap.

---

## 2. WHAT YOU MUST DEEPLY UNDERSTAND BEFORE PROCEEDING

After reading everything, you must be able to answer all of the following
from memory — without referring back to the docs. If you cannot, re-read.

### About the language identity:
- What does NOVA mean by "Units as types"? How is this different from a
  runtime unit library?
- What is the 7-dimensional SI unit vector? What are the 7 dimensions?
  Give the vector for Float[N], Float[J], Float[Pa].
- What is the difference between a `trait` and an `interface` in NOVA?
  Which is type-level? Which is module-level?
- Why is `mission` called `mission` and not `function`? What does this
  choice signal about who NOVA is for?
- Why are there no block comments? What was the reasoning?
- What does `absorb` do and why is it called `absorb` not `import`?
- What is `transmit` and why is it not called `print`?
- What is a `constellation`? What is `nebula`?
- What is a `Wave` type? How does it differ from an `Array`?
- Why is `parallel mission` a keyword modifier rather than a library call?
- What does `on device(gpu) { ... }` do?
- What is the `pipeline [...]` construct? How does it interact with
  `parallel mission`?
- What does `autodiff(loss) { net.update(...) }` mean exactly?
  How is weight mutation handled in the type system?
- Why does `Void` start with a capital V? Why does `Never`?

### About the compiler architecture:
- Why was C chosen for the lexer and parser?
- What is the AST interchange format between C and the downstream components?
- Why was Rust chosen for the type system originally? What made it 42% of
  the project — and why is that a problem?
- What exactly is the C→Rust FFI bridge problem that motivated this migration?
- What components are genuinely complete vs skeleton?
- Why is the CHANGELOG empty? What does this tell you?
- What are the three design questions that have been resolved since
  suggestionsandquestions.md was written?

### About the migration decision:
- The project owner built the Rust implementation as a **learning and
  experimentation phase**, not as the final production architecture.
  This is critical. You are not fixing broken code. You are replacing a
  deliberate prototype with a production system that the project owner
  always intended to build.
- The Rust code is the **specification** for what the new components must do.
  It tells you what to build. It does not tell you how.
- No single language should dominate the compiler. Rust being 42% was a
  red flag — not because Rust is bad, but because it violated the
  project's own philosophy of using each language where it genuinely excels.

---

## 3. THE MIGRATION DECISION — WHAT AND WHY

### What is being replaced:
All Rust components (42% of the project) are being replaced. The C lexer
and parser are NOT being replaced — they are correct, complete, and fast.
The Java interface validator is NOT being replaced. The Python stdlib
references are NOT being replaced (they will eventually be superseded by
native implementations, but that is Phase 4 work).

### The new compiler language assignment:

| Component | Old (Rust) | New | Reasoning |
|---|---|---|---|
| Unit Resolver | Rust | **Zig** | Zero C FFI overhead, shares C structs directly, explicit allocators, simple data transformation — no complex abstractions needed |
| Type Checker | Rust | **OCaml or F#** | Hindley-Milner was invented in ML. HM is a functional algorithm. Pattern matching on AST nodes, algebraic types for type expressions, recursive unification — OCaml/F# make this natural and shorter. F# has units of measure natively, making it self-referentially honest for NOVA's type checker. |
| Semantic Analyzer | Rust | **Zig** | Scope management, symbol tables, binding resolution, parallel safety checking — structural AST traversal. Systems-level work. No functional abstractions needed. Zig is correct here. |
| Autodiff System | Rust | **Julia** | Zygote.jl, Enzyme.jl, Diffractor.jl — the state of the art in autodiff is Julia. The autodiff system and cosmos.ml naturally share the same language layer. Julia's multiple dispatch maps onto how autodiff rules work. |
| Tensor Lowering | Rust | **Julia** | Same layer as autodiff, deeply intertwined. Tensor shape inference and matmul lowering are numerical work. Julia's array model is designed for exactly this. |
| IR Emitter | Rust | **C++** | LLVM is written in C++. The LLVM C++ API is the primary, best-documented, most stable interface. inkwell (Rust LLVM bindings) is a wrapper that lags LLVM releases. Using C++ eliminates the binding problem entirely. |

### The AST bridge solution:
The C parser serializes the AST to **JSON** using a minimal C JSON emitter
(cJSON or a custom 200-line implementation). Downstream components (Zig, OCaml/F#,
Julia, C++) all read the AST from JSON. This eliminates all FFI struct layout
problems. The overhead is negligible — JSON parsing of a compiler AST takes
single-digit milliseconds even on large files.

### The stdlib language assignment (for future Phase 4):

| Layer | Language | Reasoning |
|---|---|---|
| cosmos.* numerical core | Julia | Scientific computing language of the field |
| cosmos.* wrappers | Python (transitional) | Bridge to irreplaceable existing tools |
| nova.* general purpose | Go | Speed, simplicity, excellent stdlib |
| nova.* performance paths | C++ | Hot paths that Julia/Go call into |
| Package manager (nebula) | Go | Single binary, fast, excellent for CLIs |
| Formatter (nova-fmt) | Go | Same reasoning as gofmt |
| Language server (nova-ls) | C++ | LSP needs maximum responsiveness |
| REPL | Python → Julia | Bootstrap then mature |

---

## 4. YOUR TASK — ORDERED AND PRECISE

You will execute this migration in strict order. Do not skip phases.
Do not begin a phase until the previous one is complete and verified.

---

### PHASE M0 — Write the missing example programs (DO THIS FIRST)

Before touching any compiler code, write the 5 missing `.nv` example files.
These are the ground truth for what the compiler must handle.

Writing them will surface any syntax ambiguities before the compiler is built.
Every design decision becomes real when you write actual NOVA programs.

Write complete, correct, idiomatic NOVA for each:

**`examples/stellar_main_sequence/main.nv`**
Use: `absorb`, `pipeline [...]`, `filter`, `map`, `drop_outliers`,
`pearson`, `linear_fit`, `scatter`, `regression_line`, `Wave` type,
`read_fits`, `log10`, string interpolation with format specifiers,
`parallel mission`. This is the flagship scientific demo.

**`examples/hubble_constant/main.nv`**
Use: `absorb cosmos.astro`, `absorb cosmos.stats`, `read_fits`,
`pipeline [...]`, `filter`, `map`, `linear_fit`, unit type `Float[km/s/Mpc]`
as the return type guaranteed by the type system. Show the Hubble constant
derivation from Type Ia supernova catalogue data.

**`examples/cosmo_transformer/main.nv`**
Use: `model`, `layer`, `repeat`, `attention`, `feedforward`, `norm`,
`embedding`, `linear`, `parallel mission`, `autodiff`, `cross_entropy`,
`adamw`, `Dataset`, `batches`, `net.forward`, `net.update`. This is the
flagship AI demo.

**`examples/web_server/main.nv`**
Use: `absorb nova.net`, `serve`, `route`, `get`, `post`, `Response`,
`mission main() → Void`. Show that NOVA is genuinely general-purpose.

**`examples/cli_tool/main.nv`**
Use: `absorb nova.cli`, `args`, `flag`, `transmit`. A real command-line
tool that processes an input file with a `--verbose` flag.

Rules for writing these:
- Use `--` for all comments, never `//` or `#`
- Use `→` for return type arrows (not `->`)
- Use `transmit` not `print`
- Use `mission` not `fn` or `def` or `function`
- Use `absorb` not `import` or `use`
- Use `constellation` not `module` or `namespace`
- Use `let` for immutable, `var` for mutable
- All numeric literals with physical meaning carry units: `311.0[s]` not `311.0`
- Named arguments at call sites: `delta_v(isp=311.0[s], ...)` not positional
- `pipeline [...]` steps are comma-separated, each on its own line, indented
- String interpolation: `"{value as [unit]:.decimals}"` syntax
- Do NOT invent syntax that isn't in the TokenSpec or TypeSystem doc

---

### PHASE M1 — JSON AST Bridge in the C Parser

Add JSON serialization to `compiler/parser/src/parser.c`.

Requirements:
- Every AST node type defined in `plan.md` section 0.4 must serialize
- Include source spans (file, line, column) on every node — required for
  error messages
- Use cJSON (include as a single header) or write a minimal custom serializer
- The JSON schema must be documented in `docs/ast_schema.md`
- Output is written to stdout or a temp file — the orchestration layer decides
- Add a `--emit-ast-json` flag to the parser binary
- Write a test: parse `hello_universe.nv` and `delta_v.nv`, verify the JSON
  output matches the expected AST structure

Do NOT change the C parser's internal AST representation.
Do NOT change the lexer.
Do NOT add any Rust code.

---

### PHASE M2 — Unit Resolver in Zig

Rewrite `compiler/unit_resolver/` in Zig.

The Rust unit resolver is your specification. Read it completely.
Reproduce its behavior exactly. Do not add features. Do not remove features.

Requirements:
- Reads the JSON AST from Phase M1
- Implements the full 7-dimensional SI unit vector: [L, M, T, I, Θ, N, J]
- Implements all unit inference rules from the TypeSystem doc section 4.2:
  - multiplication: vector addition of exponents
  - division: vector subtraction
  - addition/subtraction: only if vectors identical, else error
  - power: scalar multiplication of exponents
- Implements the full unit alias table from plan.md section 1.2:
  N, J, Pa, W, AU, ly, pc, M☉, L☉, eV, atm — and all others listed
- Handles user-defined `unit` declarations (adds to alias table)
- Auto-converts compatible units on addition: `1.0[kg] + 500.0[g] → 1.5[kg]`
- Rejects incompatible dimensions with domain-aware error messages that name
  the SI dimensions by name, not by vector index
- Recognizes derived units: reports `kg·m/s²` as `N` in error messages
- Outputs annotated AST JSON with unit types resolved on every literal node
- Compiles to a single binary: `nova-unit-resolve`

Zig-specific requirements:
- Use Zig's build system (build.zig)
- No external dependencies beyond Zig's standard library
- All allocations go through an explicit allocator passed at startup
- Tests in `compiler/unit_resolver/tests/` covering:
  - All SI dimension arithmetic rules
  - All unit aliases
  - All error cases (incompatible dimensions, unknown units)
  - The `delta_v.nv` example: verify `isp[s] * g0[m/s²] * ln(Float[1])` → `Float[m/s]`

---

### PHASE M3 — Type Checker in OCaml or F#

Rewrite `compiler/typechecker/` in OCaml (preferred) or F#.

The Rust type checker is your specification. Read it completely.
It implements Hindley-Milner with unit dimension vectors and tensor shapes.
Your implementation must be functionally identical.

**Language choice decision:**
- Choose OCaml if you want the most direct mapping to HM literature and
  maximum pattern matching expressiveness
- Choose F# if you want the .NET ecosystem, slightly gentler syntax, and
  the self-referential honesty of using a language with built-in units of
  measure to implement NOVA's unit type checker
- Document your choice and reasoning in `compiler/typechecker/README.md`
- Do NOT choose a different language without explaining why in detail

Requirements:
- Reads the annotated AST JSON from Phase M2
- Implements Hindley-Milner Algorithm W, extended with:
  - Unit dimension vectors (7D SI basis) — treat as type parameters
  - Tensor shape types — rank enforced, matmul shape checked
  - Row polymorphism for struct field access
  - Trait bounds on mission type parameters
- Every type error message must be domain-aware (see plan.md section 1.6):
  - Unit mismatch: names the physical dimensions involved, not the vectors
  - Shape mismatch: prints both tensor shapes and the failing operation
  - Missing field: includes edit-distance suggestion
  - Parallel safety: names the mutated input and explains why it's illegal
- Outputs fully type-annotated AST JSON
- Compiles to a single binary: `nova-typecheck`
- Must correctly type-check all 7 example programs (including the 5 you
  wrote in Phase M0)
- Must correctly reject `1.0[kg] + 1.0[m]` with a clear error naming
  [M] (mass) and [L] (length) as the incompatible dimensions

Performance requirement from plan.md:
`hello_universe.nv` must complete type checking in under 50ms.

---

### PHASE M4 — Semantic Analyzer in Zig

Rewrite `compiler/semantic/` in Zig.

The Rust semantic analyzer is your specification. Read it completely.

Requirements:
- Reads the type-annotated AST JSON from Phase M3
- Scope management: resolves every identifier to its binding
- Lifetime tracking: ensures no use-after-move (simplified, not full Rust borrow checking)
- Parallel safety: verifies that `parallel mission` inputs are not mutated
  (immutable inputs model — the decision from AI_HANDOFF.md)
- Constellation export checking: verifies that `absorb` names are actually
  exported by the referenced constellation
- Mission call validation: argument count, named argument matching, type compatibility
- Enum exhaustiveness: all `match` arms present (compile error if not)
- Outputs semantically validated AST JSON
- Compiles to a single binary: `nova-semantic`

---

### PHASE M5 — Autodiff and Tensor Lowering in Julia

Rewrite `compiler/autodiff/` and `compiler/tensor_lowering/` in Julia.

The Rust implementations are your specification. Read both completely.

Requirements:
- Reads the semantically validated AST JSON from Phase M4
- **Autodiff system:**
  - Implements reverse-mode automatic differentiation (backprop) for scalar losses
  - Implements forward-mode for vector-to-scalar functions (Jacobian-vector products)
  - `autodiff(loss) { ... }` → append gradient computation after forward pass
  - `gradient(expr, wrt vars)` → forward-mode trace
  - Dynamic tape (runtime) for v1 — confirmed decision from AI_HANDOFF.md
  - Computational graph stored in IR metadata — no runtime graph object
- **Tensor lowering:**
  - Shape checking: all tensor shapes resolved or marked for runtime check
  - Matmul `@`: `(A,B) @ (B,C) → (A,C)` — inner dimensions checked at compile time
  - reshape, transpose, concat, split — output shapes statically known where possible
  - Layer chain shape inference inside `model` blocks
  - Dispatch matmul to cblas_dgemm on CPU
- Outputs lowered IR JSON ready for the C++ IR emitter
- Run as: `julia compiler/autodiff_tensor/main.jl <input.json> <output.json>`

Julia-specific requirements:
- Use Julia's built-in JSON3.jl for JSON I/O
- Structure as a proper Julia package with Project.toml
- Tests cover all autodiff rules and tensor shape inference cases

---

### PHASE M6 — IR Emitter in C++ with Native LLVM API

Rewrite `compiler/codegen/` in C++ using the LLVM C++ API directly.

The Rust IR emitter is your specification. Read it completely.
But the implementation uses the native LLVM C++ API — not inkwell, not strings.

Requirements:
- Reads the lowered IR JSON from Phase M5
- Emits real LLVM IR using `llvm::IRBuilder<>`, `llvm::Module`, `llvm::Function`
- Lowers missions to LLVM functions
- Lowers `Float[unit]` to LLVM `double` (units erased — zero runtime cost)
- Lowers `Tensor` to heap-allocated strided arrays (row-major)
- Dispatches tensor matmul `@` to `cblas_dgemm`
- Implements the minimal runtime:
  - `transmit(str)` → `printf` wrapper
  - `panic(msg)` → `fprintf(stderr, ...)` + `exit(1)`
  - Tensor memory allocator: `malloc`/`free` for v1
- Optimization pipeline: use LLVM's standard pass manager
  (inlining, loop vectorization, dead code elimination)
- Writes the compiled binary to disk
- Build with CMake + find_package(LLVM)
- Target: LLVM 17 or later

End-to-end test: `hello_universe.nv` must compile to a working binary
that prints "Hello, universe!" when executed.

---

### PHASE M7 — Compiler Orchestration Binary

Create the main compiler binary that chains all phases.

This is the missing piece that makes NOVA real.

Requirements:
- Written in C++ (same layer as the IR emitter)
- Command: `nova build <file.nv>` and `nova run <file.nv>`
- Chains: C lexer → C parser (JSON AST) → Zig unit resolver → OCaml/F#
  type checker → Zig semantic analyzer → Julia autodiff+tensor → C++ IR emitter
- Each phase runs as a subprocess OR is linked as a library (your choice,
  document the tradeoff)
- Source location tracking: every error message includes file, line, column
- Error reporting: collect errors from all phases, report together
- `--emit-tokens`: stop after lexing, print token stream
- `--emit-ast`: stop after parsing, print AST JSON
- `--emit-typed-ast`: stop after type checking, print type-annotated AST
- `--emit-ir`: stop after IR emission, print LLVM IR text
- `--opt`: enable optimization pipeline
- Milestone: `nova run examples/hello_universe/main.nv` prints "Hello, universe!"
- Milestone: `nova run examples/delta_v/main.nv` prints "Falcon 9 Δv ≈ 9.74 km/s"

---

### PHASE M8 — Update All Documentation

After all phases are complete:

1. **`CHANGELOG.md`** — Write a complete changelog from v0.1 (Rust prototype)
   to v0.2 (production multi-language architecture). Document every decision.

2. **`AI_HANDOFF.md`** — Update the implementation status table. Mark Rust
   components as replaced. Document the new language assignments with reasoning.

3. **`docs/architecture.md`** — Write a new architecture document with the
   updated compiler pipeline diagram, language assignments, and the JSON AST
   interchange format.

4. **`docs/build.md`** — Write complete build instructions for the new
   multi-language compiler. What tools are needed (clang++, zig, ocaml/dotnet,
   julia, go), how to build each component, how to build the orchestration binary.

5. **`suggestionsandquestions.md`** — Mark all resolved questions as resolved
   with the actual decisions. Remove stale content. Add any new open questions
   that arose during the migration.

6. **`extrainfo.md`** — Fill in with any additional context about the migration
   that future contributors need to know.

---

## 5. RULES YOU MUST FOLLOW THROUGHOUT

### On NOVA's design philosophy:
- Never violate the language identity. NOVA is for scientists, engineers,
  astronomers, and AI researchers. Every decision must serve them.
- Units are not optional annotations. They are types. The type checker must
  treat `Float[m/s]` and `Float[m/s²]` as completely different types.
- `mission`, `transmit`, `absorb`, `constellation` — never change this
  vocabulary. It is locked at v0.2 and is part of the language's identity.
- The compiler should embody NOVA's philosophy: each component in the language
  genuinely best suited to that phase's work.

### On code quality:
- Every component must have tests before being considered complete.
- Error messages are part of the language. They must be domain-aware,
  physical, and written for scientists — not compiler engineers.
- Each component must compile in a single command documented in its README.
- No component may have an undocumented dependency.

### On the migration specifically:
- The Rust code is the specification, not the template. Read it to understand
  what to build. Write the new code from scratch in the new language.
- Do not transliterate Rust into the new language. Write idiomatic code in
  the target language that achieves the same behavior.
- If you discover the Rust implementation has a bug or an incomplete feature,
  note it in a `MIGRATION_NOTES.md` file and implement the correct behavior
  in the new language.
- Do not add features that are not in the current specification. NOVA's
  feature set is defined by the TokenSpec, TypeSystem doc, and plan.md.

### On the language choices:
- Zig for unit resolver and semantic analyzer: use explicit allocators,
  no hidden allocations, direct C struct compatibility where needed.
- OCaml/F# for type checker: write functional, pattern-matching idiomatic
  code. The type checker should read like a textbook on type theory.
- Julia for autodiff and tensor lowering: use Julia's multiple dispatch
  naturally. Autodiff rules should dispatch on operation type.
- C++ for IR emitter and orchestration: use RAII, avoid raw pointers,
  use LLVM's C++ API as designed.
- Do NOT use Rust anywhere in the production compiler.

---

## 6. WHAT SUCCESS LOOKS LIKE

You are done when all of the following are true:

**Compilation milestone:**
```
nova run examples/hello_universe/main.nv
→ Hello, universe!

nova run examples/delta_v/main.nv
→ Falcon 9 Δv ≈ 9.74 km/s

nova build examples/delta_v/main.nv
→ Compiles in under 100ms on a modern laptop
```

**Type safety milestone:**
```
-- test_unit_error.nv
let wrong = 1.0[kg] + 1.0[m]

nova build test_unit_error.nv
→ error: cannot add Float[kg] and Float[m]
         dimension [M] (mass) + [L] (length) — incompatible
         hint: did you mean to scale one of these, or is this a logic error?
         --> test_unit_error.nv:2:16
```

**Architecture milestone:**
- No single language exceeds 30% of total compiler LOC
- Rust is at 0%
- Every compiler phase has a standalone binary that can be run independently
- Every component has a README with build instructions and test instructions

**Documentation milestone:**
- CHANGELOG.md is populated
- AI_HANDOFF.md reflects current accurate status
- All 7 example `.nv` files are complete and correct
- `docs/build.md` allows a new contributor to build the compiler from scratch

---

## 7. FINAL REMINDER

NOVA is not a toy project. It is a serious, long-term programming language
designed to be the first language where the entire scientific world — machine
learning, astronomy, orbital mechanics, quantum mechanics, spectroscopy,
thermodynamics — ships in the standard library, unit-typed, parallel-by-default,
and compiled to native binaries.

The project owner built the Rust prototype to learn. They learned. Now they
are building the real thing. Your job is to help them build it right — faithful
to their vision, grounded in their specification, and honest about what each
language is genuinely best for.

Think before you act.
Learn before you decide.
Build what the project deserves.

---

*This prompt was written with full knowledge of the NOVA repository as of
April 2026, including the TokenSpec v0.1, TypeSystem v0.1, plan.md,
AI_HANDOFF.md, description.md, readme.md, all example .nv files, and the
full stdlib and compiler source structure.*
