# Bytecode-VM (WIP)

A high-performance, register-based Virtual Machine, isolated runtime environment, and optimizing compiler built entirely from first principles using modern C++20/23.

This project is engineered to eliminate standard runtime layout inefficiencies by enforcing strict cache locality, eliminating standard OS heap fragmentation, and implementing custom low-level software-defined hardware abstractions. Using this as a learning tool to understand compiler construction and modern C++. 

---

## Architectural Blueprint

The system is partitioned into four decoupled core subsystems:

* **The Frontend & Compiler:** A zero-copy lexical analyzer and AST parser that ingests a custom assembly language, lowers it into an Intermediate Representation (IR), applies optimization passes, and serializes a packed binary instruction stream.
* **The Custom Memory Subsystem:** A 64 MB hardware-aligned monotonic `LinearArena` and compliant C++ `ArenaAllocator` stack that bypasses `malloc`/`new` execution paths to completely eradicate external heap fragmentation and prioritize L1/L2 cache line retention
* **The Core Execution Engine:** An emulated, register-based virtual CPU processing explicit 32-bit bitmasked instruction operations across a localized contiguous register file inside an optimized fetch-decode-execute interpreter loop.
* **The Runtime Parallel Subsystem:** A lock-free telemetry logging stream and multi-threaded work-stealing engine designed to emulate managed concurrent execution runtimes.

---

## Technical Guardrails Enforced

* **Zero Raw Pointer Ownership:** Every dynamic allocation lifecycle is explicitly bound to `UniquePtr` RAII handles or directly handled through custom monotonic arena allocations.
* **Zero-Copy String Pipelines:** No underlying heap copying or string allocations occur inside token parsing tracks; substrings are sliced entirely as lightweight `std::string_view` abstractions
* **Compile-Time Interface Safety:** All internal type pipelines are validated at compile-time using explicit C++20 constraints and type traits via custom concepts.
* **Strict Diagnostics Validation:** Built under severe `-Wall -Wextra -Werror` constraints and actively audited via ASan and UBSan.

---

## Development Progress Track

### ~~Week 1: Modern C++ Foundations & Language Frontend~~

* [x] ~~**Day 1-2:** Implement custom move-only `UniquePtr` variations and custom `StringView` primitives.~~
* [x] ~~**Day 3:** Build a zero-copy lexical analyzer mapping source code assembly slices.~~
* [x] ~~**Day 4:** Construct C++20 pipeline validation concepts (`IsRegister`, `IsInstruction`).~~
* [x] ~~**Day 5:** Architect the AST Parser using monadic `std::expected` and `std::variant` type sets.~~
* [x] ~~**Day 6-7:** Establish strict CMake sanitizer pipelines and execute comprehensive malformed parsing tests.~~

### ~~Week 2: Memory & Cache Hierarchy Architecture~~

* [x] ~~**Day 1:** Write static structure layout optimization analyzers to eliminate layout padding bytes.~~
* [x] ~~**Day 2:** Code benchmarks profiling cache-miss variances between row-major and column-major lookups.~~
* [x] ~~**Day 3:** Benchmark concurrent false sharing degradation and enforce alignment mitigations via `alignas` constructs.~~
* [x] ~~**Day 4-5:** Author a monotonic `LinearArena` and write a fully custom standard-compliant `ArenaAllocator` adapter.~~
* [x] ~~**Day 6-7:** Refactor AST node trees into flattened index-based arena layouts and profile performance throughput gains.~~

### Week 3: Operating Systems & Runtime Fundamentals

* [x] ~~**Day 1:** Emulate a 2-Level Page Table mapping module coupled with an internal Software TLB cache.~~
* [x] ~~**Day 2:** Implement low-level OS page wrappers (`mmap`/`VirtualAlloc`) managing explicit Read/Write/Execute (`RWX`) segment boundaries.~~
* [x] ~~**Day 3:** Build out the structural switch-dispatch loop covering fundamental instruction sets (`ADD`, `SUB`, `MOV`, `JMP`).~~
* [x] ~~**Day 4:** Structure a contiguous 16-register layout optimized entirely for internal L1 data cache capacity bounds.~~
* [x] ~~**Day 5:** Model physical activation hardware stack routines (`PUSH`, `POP`, `CALL`, `RET`).~~
* [x] ~~**Day 6-7:** Code the binary assembler bit-packer to emit serialized bytecode files directly into execution hooks.~~

### Week 4: Control Flow Graphs, SSA, and Data-Flow Analysis

* [ ] **Day 1-2:** Refactor the flat linear IR into a structural Control Flow Graph (CFG) composed of Basic Blocks and explicit edge transitions.
* [ ] **Day 3:** Implement an iterative Dominator Tree algorithm to calculate dominance frontiers across the CFG.
* [ ] **Day 4:** Construct a Static Single Assignment (SSA) transformation pass, placing mathematical (phi) nodes at dominance frontiers.
* [ ] **Day 5:** Code a global Sparse Conditional Constant Propagation (SCCP) analysis pass utilizing the SSA representation.
* [ ] **Day 6-7:** Implement a backward Data-Flow Analysis framework to perform Global Liveness Analysis and compute active variable lifetimes.

### Week 5: AOT Code Generation, Register Allocation & Vectorization

* [ ] **Day 1-2:** Construct a target instruction selector converting abstract IR operations into mock machine assembly instructions using a Maximal Munch or tree-matching technique.
* [ ] **Day 3-4:** Build a global Register Allocator using a Linear Scan or Graph Coloring algorithm to map infinite SSA variables to a fixed set of physical target registers.
* [ ] **Day 5:** Author an auto-vectorization pass that aggregates contiguous scalar operations into wide SIMD array vectors (modeling SLP concepts).
* [ ] **Day 6-7:** Profile compilation throughput alongside codegen execution traces using Windows Performance Recorder (WPR); optimize hot backend passes for cache-line locality.

---

## Build and Testing Environment

### Prerequisites

* Clang 16+ or GCC 13+ (Fully supporting C++23 features)
* CMake 3.24+
* Ninja Build System (Recommended)

### Compilation Sequence

```bash
# Configure and build with the same preset used by CI (ASan, UBSan, clang-tidy)
cmake --preset ci
cmake --build --preset ci

# Run the test evaluation matrix
./build/vm_tests
```

CI and Clang builds use libc++ for C++23 support (`std::expected`, etc.). On Linux, if ASan reports `alloc-dealloc-mismatch` during exception tests, set `ASAN_OPTIONS=alloc_dealloc_mismatch=0` (CI does this automatically). For exact Ubuntu reproduction on macOS, use an Ubuntu 24.04 container.