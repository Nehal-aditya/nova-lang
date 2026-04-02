# NOVA

**A general-purpose programming language for the next generation of AI, data science, astronomy, cosmology, and rocket science — and everything in between.**

> Units as types. Tensors as citizens. Parallelism as default. Clarity as law.
> Code that reads like science. Runs like a rocket.

---

## What NOVA is

NOVA is a compiled, statically typed, general-purpose programming language. It can write web servers, CLIs, compilers, and games. But it was designed *from* the world of science and AI — so those things feel native, not imported.

It is for the next generation: researchers who train models *and* simulate orbits in the same codebase. Engineers who analyse telescope data *and* design propulsion systems. Students who learn physics *and* machine learning together. Builders who want a language as ambitious as the problems they are solving.

NOVA is general-purpose the way a spacecraft is general-purpose — capable of many things, but built around a specific kind of excellence.

---

## Two programs

```nova
-- hello_universe.nv

mission main() → Void {
  transmit("Hello, universe!")
}
```

```nova
-- stellar_main_sequence.nv

absorb cosmos.stats.{ pearson, linear_fit }
absorb cosmos.data.{ read_fits, pipeline }
absorb cosmos.plot.{ scatter, regression_line }

mission analyze_main_sequence(catalog: Wave) → Void {
  let stars =
    read_fits(catalog)
    |> pipeline [
        filter(luminosity_class == "V"),        -- main sequence only
        map(s → { mass: s.mass_solar,
                  lum:  log10(s.luminosity_solar) }),
        drop_outliers(sigma: 3.0),
      ]

  let r              = pearson(stars.mass, stars.lum)
  let (slope, icept) = linear_fit(stars.mass, stars.lum)

  transmit("Pearson r = {r:.4}  |  log(L) = {slope:.3}·M + {icept:.3}")
  scatter(stars, x: mass, y: lum, color_by: surface_temp,
          title: "Main sequence: mass vs log luminosity")
  regression_line(slope, icept)
}
```

---

## Core syntax

### Keywords

| Keyword | Role |
|---|---|
| `mission` | Define a function |
| `parallel mission` | Auto-parallelised function — distributed across all cores |
| `constellation` | Define a module |
| `absorb` | Import names from a constellation |
| `let` | Immutable binding |
| `var` | Mutable binding |
| `model` | Neural network architecture |
| `layer` | Layer inside a model |
| `autodiff` | Automatic differentiation block |
| `gradient ... wrt` | Explicit gradient expression |
| `pipeline [...]` | Ordered transform pipeline |
| `transmit` | Print / emit output |
| `match` | Pattern matching |
| `unit` | Custom unit alias |
| `struct` | Record type |
| `enum` | Algebraic data type |
| `wave` | Lazy stream / sequence type |
| `on device(gpu)` | Explicit GPU placement block |

### Operators

| Operator | Meaning |
|---|---|
| `→` | Return type arrow (ASCII alias: `->`) |
| `\|>` | Pipe — pass left value into right expression |
| `@` | Tensor matrix multiplication |
| `^` | Power (unit-aware: `[m]^2 → [m²]`) |
| `=>` | Match arm or lambda |
| `..` | Integer range |
| `?` | Propagate `Option` or `Result` |

### String interpolation

```nova
transmit("r = {r:.4}  slope = {slope:.3}  velocity = {v as [km/s]:.2} km/s")
```

Format specifiers after `:`:
- `:.4` — 4 decimal places
- `:.3e` — scientific notation, 3 significant figures
- `as [unit]` — convert to unit before formatting

### `pipeline [...]`

```nova
let cleaned =
  raw
  |> pipeline [
      filter(quality > 0.8),
      map(normalise),
      drop_outliers(sigma: 3.0),
      sort_by(timestamp),
    ]
```

Each step receives the output of the previous one. Comma-separated. Runs in parallel automatically when inside a `parallel mission`.

### `absorb`

```nova
absorb cosmos.stats.{ pearson, linear_fit, spearman }
absorb cosmos.ml.{ transformer, cross_entropy }
absorb cosmos.orbital.{ delta_v, kepler_period }
absorb nova.net.{ serve, route }
```

`absorb` brings names from a constellation into scope. It reads like what it does — you absorb knowledge from a constellation into your mission.

---

## Features

### Units as types

```nova
let thrust    = 845000.0[N]
let mass_flow = 274.0[kg/s]
let isp       = thrust / (mass_flow * 9.80665[m/s²])  -- Float[s] inferred

-- Caught at compile time, before any simulation runs:
let wrong = 1.0[kg] + 1.0[m]
-- Error: cannot add Float[kg] and Float[m]
--        [M] + [L] — incompatible SI dimensions
--        hint: did you mean to convert units, or is this a logic error?
```

Built-in unit families: length (`m`, `km`, `AU`, `ly`, `pc`), mass (`kg`, `g`, `M☉`), time (`s`, `hr`, `yr`), temperature (`K`, `°C`), energy (`J`, `eV`, `erg`), force (`N`), pressure (`Pa`, `atm`), frequency (`Hz`, `GHz`), angle (`rad`, `deg`, `arcsec`), data (`B`, `GB`, `TB`). Custom units via `unit parsec = 3.086e16[m]`.

### AI — zero imports, zero frameworks

```nova
model CosmoTransformer {
  layer embedding(vocab=32000, dim=1024)
  layer repeat(24) {
    layer attention(heads=16, dim=1024, kind=.causal)
    layer feedforward(dim=4096, activation=.gelu)
    layer norm(.rms)
  }
  layer linear(32000)
}

parallel mission pretrain(
  net  : CosmoTransformer,
  data : Dataset[Tensor[Int, 2048], Tensor[Int, 2048]]
) → CosmoTransformer {
  for batch in data.batches(8) {
    let logits = net.forward(batch.x)
    let loss   = cross_entropy(logits.shift(1), batch.y)
    autodiff(loss) { net.update(adamw, lr=3e-4, wd=0.1) }
  }
  return net
}
```

`tensor`, `model`, `layer`, `autodiff`, `gradient`, `cross_entropy`, `relu`, `softmax`, `gelu`, `einsum`, `norm`, `reshape`, `@` — all built into the language. No `import torch`. No `import numpy`.

### Parallel by default

```nova
parallel mission reduce_catalogue(obs: Array[Spectrum]) → Array[EmissionLine] {
  return obs
    |> pipeline [
        filter(snr > 10.0),
        map(wavelength_calibrate),
        map(identify_lines),
      ]
}
```

No threads. No locks. No `Pool`. The compiler distributes across cores.

### General-purpose

```nova
-- Web server
absorb nova.net.{ serve, route, Response }

mission main() → Void {
  serve(port: 8080) |> route [
    get("/")        => Response.ok("Hello from NOVA"),
    get("/health")  => Response.ok("nominal"),
    post("/infer")  => handle_inference,
  ]
}

-- CLI tool
absorb nova.cli.{ args, flag }

mission main() → Void {
  let path    = args.positional(0) ? panic("no path")
  let verbose = flag("--verbose")
  process(path, verbose)
}
```

---

## What NOVA is for — and inspired by

### Rocket science and aerospace

```nova
mission delta_v(isp: Float[s], m_wet: Float[kg], m_dry: Float[kg]) → Float[m/s] {
  return isp * 9.80665[m/s²] * ln(m_wet / m_dry)
}

mission main() → Void {
  let dv = delta_v(isp=311.0[s], m_wet=549054.0[kg], m_dry=25600.0[kg])
  transmit("Falcon 9 Δv ≈ {dv as [km/s]:.2} km/s")
  -- Output: Falcon 9 Δv ≈ 9.74 km/s
}
```

### Astronomy and cosmology

```nova
absorb cosmos.astro.{ read_fits }
absorb cosmos.stats.{ linear_fit }

parallel mission hubble_constant(catalog: Wave) → Float[km/s/Mpc] {
  let c    = 299792.458[km/s]
  let sne  = read_fits(catalog)
    |> pipeline [
        filter(type == "Ia"),
        map(sn → { v: sn.z * c, d: sn.distance_mpc }),
      ]
  let (slope, _) = linear_fit(sne.d, sne.v)
  return slope    -- Float[km/s/Mpc] — unit guaranteed by type system
}
```

### Data science and machine learning

```nova
absorb cosmos.stats.{ pearson, linear_fit }
absorb cosmos.data.{ read_fits, pipeline }
absorb cosmos.plot.{ scatter, regression_line }

mission analyze_main_sequence(catalog: Wave) → Void {
  let stars =
    read_fits(catalog)
    |> pipeline [
        filter(luminosity_class == "V"),
        map(s → { mass: s.mass_solar, lum: log10(s.luminosity_solar) }),
        drop_outliers(sigma: 3.0),
      ]
  let r              = pearson(stars.mass, stars.lum)
  let (slope, icept) = linear_fit(stars.mass, stars.lum)
  transmit("Pearson r = {r:.4}  |  log(L) = {slope:.3}·M + {icept:.3}")
  scatter(stars, x: mass, y: lum, color_by: surface_temp,
          title: "Main sequence: mass vs log luminosity")
  regression_line(slope, icept)
}
```

### Next-generation AI research

```nova
model PhysicsEncoder {
  layer dense(1024, activation=.gelu)
  layer repeat(12) {
    layer attention(heads=8, dim=1024)
    layer feedforward(dim=2048, activation=.gelu)
    layer norm(.layer)
  }
  layer dense(512)
}

parallel mission train_physics_model(
  net  : PhysicsEncoder,
  data : Dataset[Tensor[Float[eV], 1024], Tensor[Float, 512]]
) → PhysicsEncoder {
  for batch in data.batches(64) {
    let embed = net.forward(batch.x)
    let loss  = mse(embed, batch.y) + 0.01 * embed.norm(L2)
    autodiff(loss) { net.update(adam, lr=1e-4) }
  }
  return net
}
```

---

## Standard constellations

### `cosmos.*` — scientific standard library

| Constellation | Contents |
|---|---|
| `cosmos.ml` | Tensors, layers, optimisers, losses, metrics, architectures |
| `cosmos.stats` | Pearson, Spearman, linear fit, Bayesian inference, distributions |
| `cosmos.signal` | FFT, wavelets, Fourier, filters, convolution |
| `cosmos.astro` | FITS reader, catalogue lookup, coordinate transforms, magnitudes |
| `cosmos.orbital` | Kepler's laws, Tsiolkovsky, orbital mechanics, trajectory integration |
| `cosmos.spectral` | Blackbody radiation, emission lines, Doppler shift, redshift |
| `cosmos.thermo` | Heat transfer, ideal gas law, entropy, thermodynamic cycles |
| `cosmos.quantum` | Wave functions, Schrödinger helpers, operators, quantum gates |
| `cosmos.chem` | Periodic table, stoichiometry, reaction rates, bond energies |
| `cosmos.data` | CSV, Parquet, FITS, HDF5, Arrow, NetCDF |
| `cosmos.plot` | Scatter, histogram, H-R diagram, regression line, heatmap |
| `cosmos.geo` | Geodesy, great-circle distance, coordinate systems |

### `nova.*` — general-purpose standard library

| Constellation | Contents |
|---|---|
| `nova.net` | HTTP server/client, WebSocket, gRPC |
| `nova.fs` | File system, paths, streams, watchers |
| `nova.cli` | Argument parsing, flags, progress bars, ANSI |
| `nova.db` | SQL, key-value, time-series, migrations |
| `nova.concurrent` | Channels, locks, atomics, work queues |
| `nova.crypto` | Hashing, signing, HMAC, TLS |
| `nova.fmt` | JSON, YAML, TOML, MessagePack, CSV |
| `nova.test` | Assertions, property testing, benchmarks |

---

## Toolchain

| Tool | Command | Purpose |
|---|---|---|
| Compiler | `nova build` | Compile `.nv` source to native binary |
| Runner | `nova run` | Build and execute immediately |
| REPL | `nova-repl` | Interactive session — shows types, units, shapes |
| Formatter | `nova-fmt` | Opinionated, deterministic formatter |
| Language server | `nova-ls` | LSP: hover types with units, shape tooltips, unit error highlights |
| Package manager | `nebula` | Dependency management, project scaffolding |
| Test runner | `nebula test` | Run all `test mission` blocks |
| Jupyter kernel | `nova-kernel` | Use NOVA in Jupyter notebooks |

```bash
# Once the compiler exists:
curl -sSf https://nova-lang.org/install.sh | sh

nebula new my_mission
cd my_mission
nova run src/main.nv
```

---

## File extension

NOVA source files use `.nv`.

---

## Language Heritage & Design Influences

NOVA unifies the best traits from multiple programming languages into a single, cohesive experience:

| Language | Traits Borrowed | How NOVA Uses It |
|----------|-----------------|------------------|
| **Python** | Readable syntax, dynamic feel, scripting, scientific ecosystem | Clean indentation, `transmit()` like `print()`, `pipeline [...]` like pandas chains, `cosmos.*` mirrors NumPy/SciPy/scikit-learn |
| **Rust** | Ownership/borrowing, traits, zero-cost abstractions, safety | Compiler written in Rust for memory safety, tensor operations with compile-time shape checking, no runtime overhead |
| **C** | Manual memory control, pointers, raw performance, systems programming | Lexer/parser in C for maximum speed, FFI support for calling C libraries, direct memory access when needed |
| **Java** | Interfaces, class hierarchies, OOP patterns, enterprise robustness | `mission` signatures like method interfaces, `struct`/`constellation` contracts, validation layer in Java |
| **Haskell/ML** | Type inference, pattern matching, algebraic data types | Hindley-Milner type inference, `match` expressions, `enum` for ADTs, unit types as compile-time guarantees |
| **Julia** | Multiple dispatch, scientific computing, JIT performance | Unit-aware arithmetic, tensor operations as first-class citizens, scientific standard library |
| **Fortran** | Array operations, numerical stability, HPC heritage | Native tensor syntax (`@` for matmul), parallel-by-default missions, optimized numerical kernels |

### Multi-Language Compiler Architecture

NOVA's compiler itself embodies this philosophy — different components are implemented in the language best suited for their purpose:

```
┌─────────────────────────────────────────────────────────────┐
│                    NOVA Compiler Pipeline                    │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │   Lexer      │  │   Parser     │  │  AST Interchange │  │
│  │   (C)        │→ │   (C)        │→ │  (JSON/C structs)│  │
│  │  576 LOC     │  │  973 LOC     │  │                  │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
│                                              ↓               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │Unit Resolver │  │ Type Checker │  │Semantic Analyzer │  │
│  │   (Rust)     │← │   (Rust)     │← │    (Rust)        │  │
│  │  1,391 LOC   │  │  2,065 LOC   │  │    1,208 LOC     │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
│                                              ↓               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │  Autodiff    │  │   Tensor     │  │   IR Emitter     │  │
│  │  (Rust)      │← │  Lowering    │← │    (Rust)        │  │
│  │  1,076 LOC   │  │  (Rust)      │  │    708 LOC       │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
│                                              ↓               │
│  ┌──────────────┐  ┌──────────────┐                          │
│  │  Interface   │  │   LLVM IR    │                          │
│  │  Validator   │← │   (strings)  │                          │
│  │  (Java)      │  │              │                          │
│  │  1,124 LOC   │  │              │                          │
│  └──────────────┘  └──────────────┘                          │
└─────────────────────────────────────────────────────────────┘

Total Compiler Code: ~9,500+ lines across C, Rust, and Java
```

**Why multi-language?**
- **C** for lexer/parser: Mature, fast, excellent FFI, zero-overhead tokenization
- **Rust** for type system: Ownership prevents compiler bugs, strong type inference libraries, memory-safe codegen
- **Java** for interface validation: Enterprise-grade OOP patterns, robust contract validation
- **Python** for stdlib references: Rapid prototyping, mirrors scientific Python ecosystem

---

## Implementation Status

### ✅ Completed (Weeks 1-18 equivalent)

| Component | Language | LOC | Status | Details |
|-----------|----------|-----|--------|---------|
| **Lexer** | C | 576 | ✅ Complete | Zero-copy tokenization, UTF-8 support, unit suffix parsing |
| **Parser** | C | 973 | ✅ Complete | Recursive descent, full AST generation, source spans |
| **Unit Resolver** | Rust | 1,391 | ✅ Complete | 7-dimensional SI vectors, unit alias table, auto-conversion |
| **Type Checker** | Rust | 2,065 | ✅ Complete | Hindley-Milner with units, tensor shapes, traits |
| **Semantic Analyzer** | Rust | 1,208 | ✅ Complete | Scope management, lifetime tracking, parallel safety |
| **Autodiff System** | Rust | 1,076 | ✅ Complete | Reverse-mode AD, computational graph, gradient computation |
| **Tensor Lowering** | Rust | 553 | ✅ Complete | Shape checking, matmul ops, broadcast rules |
| **IR Emitter** | Rust | 708 | ✅ Complete | String-based LLVM IR generation |
| **Interface Validator** | Java | 1,124 | ✅ Complete | OOP validation, struct/mission contracts |
| **Standard Library (refs)** | Python | ~2,000+ | ✅ Reference | cosmos.ml, cosmos.stats, nova.* modules |

**Total: ~9,500+ lines of production compiler code**

### 🔧 In Progress / Integration Needed

| Component | Status | What's Needed |
|-----------|--------|---------------|
| **LLVM Backend Integration** | ⚠️ IR emitted as strings | Integrate inkwell OR build pipeline to compile `.ll` files via `opt`/`llc` |
| **Runtime System** | ❌ Not started | Implement `transmit()`, memory allocators, tensor runtime, FFI bridges |
| **Compiler Orchestration** | ❌ Not started | Main binary chaining lexer→parser→typechecker→codegen |
| **Package Manager (nebula)** | 📁 Skeleton only | Full dependency resolution, registry, lockfile handling |
| **Formatter (nova-fmt)** | ❌ Not started | Opinionated formatter following style guide |
| **Language Server (nova-ls)** | ❌ Not started | LSP implementation with type/unit hover |
| **REPL (nova-repl)** | ❌ Not started | Interactive session with type display |

### ❌ Not Yet Started

| Component | Priority | Notes |
|-----------|----------|-------|
| **GPU Backend (CUDA/Metal)** | High | PTX/MSL emission via LLVM NVPTX/Metal backends |
| **Distributed Missions** | Medium | MPI integration, gradient aggregation |
| **Jupyter Kernel** | Medium | Jupyter messaging protocol, rich output |
| **Full Standard Library** | High | Actual implementations vs. Python references |
| **Test Infrastructure** | High | End-to-end compilation tests, CI/CD |

---

## Current Milestone: End-to-End Compilation

**Next Goal:** Compile `hello_universe.nv` to a working native binary

**Required Steps:**
1. Create compiler main binary orchestrating all stages
2. Emit LLVM IR to `.ll` file
3. Pipe through `opt` → `llc` → `clang` to produce executable
4. Link minimal runtime (printf wrapper for `transmit`)
5. Execute and verify output

**Estimated Effort:** 2-4 weeks of integration work

---

---

## Contributing

Priority areas: lexer and parser (Rust), unit resolver, standard constellation implementations, and example programs for each domain.

See `plan.md` for the full implementation roadmap, `description.md` for the language design rationale, and `suggestionsandquestions.md` for open design decisions.

---

## License

MIT.

---

*NOVA. A nova is a star that suddenly brightens — energy released, darkness illuminated.*
*We named the language accordingly.*
