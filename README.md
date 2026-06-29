# Bytecode-VM (WIP)

A high-performance, register-based Virtual Machine, isolated runtime environment, and optimizing compiler built entirely from first principles using modern C++20/23.

This project is engineered to eliminate standard runtime layout inefficiencies by enforcing strict cache locality, eliminating standard OS heap fragmentation, and implementing custom low-level software-defined hardware abstractions. Using this as a learning tool to undertsand compiler construction and modern C++. 

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
* [ ] **Day 4:** Structure a contiguous 16-register layout optimized entirely for internal L1 data cache capacity bounds.
* [ ] **Day 5:** Model physical activation hardware stack routines (`PUSH`, `POP`, `CALL`, `RET`).
* [ ] **Day 6-7:** Code the binary assembler bit-packer to emit serialized bytecode files directly into execution hooks.

### Week 4: Multi-Threading, Concurrency & Managed Runtimes

* [ ] **Day 1-2:** Construct a reusable thread pool and concurrent task processing queue utilizing modern signaling patterns.
* [ ] **Day 3-4:** Build a lock-free Single-Producer Single-Consumer (SPSC) ring buffer to pass atomic runtime metrics across pipeline threads.
* [ ] **Day 5-6:** Implement Thread-Local Allocation Buffers (TLABs) inside the arena memory model to fully negate cross-core cache line thrashing.
* [ ] **Day 7:** System consolidation under concurrent race stress testing conditions.

### Week 5: Compiler Optimizations & Tooling Deep Dive

* [ ] **Day 1:** Re-engineer the compilation track to map out a linear Single Static Assignment (SSA) style Intermediate Representation.
* [ ] **Day 2-3:** Author AST optimization passes executing constant-folding and dead-code elimination routines.
* [ ] **Day 4-5:** Profile execution loops using hardware performance counters (`perf`/WPR) and inject compiler intrinsics alongside `[[likely]]`/`[[unlikely]]` optimization attributes.
* [ ] **Day 6-7:** Establish complete end-to-end regression pipelines compiling and executing mathematics suites within the virtual environment.

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

On macOS, Clang uses Apple libc++; on Linux CI, Clang uses libstdc++. Both run the same sanitizers, warnings, and clang-tidy checks. For exact Ubuntu reproduction on macOS, use an Ubuntu 24.04 container.