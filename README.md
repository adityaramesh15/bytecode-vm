# Bytecode-VM

A high-performance, register-based Virtual Machine, isolated runtime environment, and compiler toolchain built from first principles using modern **C++20/C++23**.

> **Project Status: Concluded (Weeks 1–3 Complete & Verified)**  
> This project was developed as an intensive systems-programming exploration following a 5-week compiler and runtime track. Development has concluded after achieving a fully functional, end-to-end working system covering Weeks 1 through 3: from zero-copy assembly parsing and custom arena memory management to an emulated 2-level page table MMU, binary bytecode assembler, and a register-based virtual CPU. Advanced AOT optimization passes (SSA/CFG from Weeks 4–5) remain intentionally unbuilt.

---

## Highlights & Verified Capabilities

- **End-to-End Execution Pipeline:** Assembly source &rarr; zero-copy tokenization &rarr; monadic AST parsing &rarr; 2-pass binary bytecode assembly &rarr; `.bcmv` file serialization &rarr; guest MMU virtual memory paging &rarr; register VM execution.
- **Zero Raw Pointers & Zero Heap Fragmenting:** Custom move-only RAII smart pointers, 64 MB contiguous `LinearArena`, and a standard-compliant `ArenaAllocator` eliminate standard heap allocations in hot runtime paths.
- **Hardware-Conscious Architecture:** 64-byte cache-line aligned register files (`alignas(64)`), contiguous memory layouts, and static structure layout padding optimization.
- **Simulated Operating System Virtual Memory:** Software 2-level Page Table (PDI/PTI) with 4 KB page frames, a 16-entry software Translation Lookaside Buffer (TLB), page permission enforcement (Read/Write/Execute), and frame recycling.
- **Comprehensive Test Suite:** 14 test suites, 80 test cases, and 480 assertions passing under AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan).

---

## Architectural Blueprint

```
 ┌─────────────────────────────────────────────────────────────────────────┐
 │                            ASSEMBLY SOURCE                              │
 └────────────────────────────────────┬────────────────────────────────────┘
                                      │
                                      ▼
                   ┌──────────────────────────────────────┐
                   │       Zero-Copy Lexer & AST          │
                   │    (std::string_view, Concepts,      │
                   │      std::variant, std::expected)     │
                   └──────────────────┬───────────────────┘
                                      │
                                      ▼
                   ┌──────────────────────────────────────┐
                   │        Two-Pass Bytecode Emitter     │
                   │   (32-bit packed binary instructions,│
                   │    label resolution, .bcmv container)│
                   └──────────────────┬───────────────────┘
                                      │
                                      ▼
                   ┌──────────────────────────────────────┐
                   │    Virtual Memory Subsystem (MMU)    │
                   │   (2-Level Page Table, 16-slot TLB,  │
                   │    4KB Frames, RWX Page Protections) │
                   └──────────────────┬───────────────────┘
                                      │
                                      ▼
                   ┌──────────────────────────────────────┐
                   │         Virtual Machine (CPU)        │
                   │  (16 Contiguous GPRs, Call Stack,    │
                   │   Branch Validator, Switch Dispatch) │
                   └──────────────────────────────────────┘
```

The system is partitioned into four core subsystems:

### 1. Compiler Frontend & Parser
- **Zero-Copy Lexical Analyzer (`Lexer`):** Scans assembly text without heap allocation using lightweight `std::string_view` slices into the source buffer.
- **Type-Constrained Pipeline (`CompilerConcepts.hpp`):** Enforces C++20 concepts (`IsRegister`, `IsInstruction`, `IsOperand`, `IsArenaAllocator`) at compile time.
- **Monadic Error Handling (`Parser`):** Employs C++23 monadic `std::expected` and `std::variant` instruction nodes, providing clear line/column error diagnostics without C++ exceptions.
- **Move-Only Smart Pointers (`UniquePtr.hpp`):** Handcrafted move-only pointer abstraction supporting custom deleters and verified leak-free ownership transfers.

### 2. Custom Memory & Cache Subsystem
- **Monotonic Memory Arena (`LinearArena`):** Pre-allocates a 64 MB contiguous memory region via `std::byte`. Provides $O(1)$ bump-pointer allocation with hardware alignment enforcement (`alignas`), memory pinning, and bulk reset.
- **Standard-Compliant Allocator Adapter (`ArenaAllocator<T>`):** Wraps `LinearArena` with `std::allocator_traits` compatibility, allowing standard containers (`std::vector<Token, ArenaAllocator<Token>>`) to allocate directly inside the arena with zero `malloc`/`new` calls.
- **Static Struct Optimizer (`StructOptimizer.hpp`):** Static analyzer tools to measure struct padding, evaluate alignment boundaries, and optimize field order for cache efficiency.

### 3. Operating System Virtual Memory Emulation
- **Hardware-Aligned Page Frames:** Physical frames and page tables reside in contiguous memory aligned to 4096 bytes.
- **Two-Level Paging Architecture (`MemoryManagementUnit`):** 32-bit virtual addresses mapped via a 10-bit Page Directory Index (PDI), a 10-bit Page Table Index (PTI), and a 12-bit intra-page offset.
- **Software TLB Cache:** 16-entry fully associative Translation Lookaside Buffer with invalidation on unmap and permission update.
- **Page Table Entries (PTE):** Packed 32-bit entries encoding Physical Frame Number (PFN) in upper 20 bits and Present, Readable, Writable bits in the lower flags.
- **Native OS Page Wrappers (`VirtualMemoryBuffer.hpp`):** RAII abstraction around platform virtual memory APIs (`mmap`/`munmap` with `PROT_READ`, `PROT_WRITE`, `PROT_EXEC` on POSIX; `VirtualAlloc`/`VirtualFree` on Windows).

### 4. Execution Engine & Bytecode Format
- **32-Bit Instruction Bitmasking:** Compact little-endian bytecode layout. Instructions use a 32-bit header containing opcode (4 bits), operand kinds (4 bits each), register indices (4 bits each), and 12 reserved bits, followed by optional 32-bit extension words for sign-extended immediates or absolute branch offsets.
- **Binary Assembler (`BytecodeEmitter`):** Two-pass assembler calculating instruction byte lengths, recording label targets, and resolving jumps/calls.
- **Binary File Container (`BytecodeFile`):** File format validation with magic bytes `0x42434D56` (`"BCMV"`) and file version tracking.
- **Register File:** Contiguous 16 General-Purpose Registers (`R0` through `R15`) with 64-byte hardware cache alignment (`alignas(64)`), plus dedicated instruction pointer (`ip`) and stack pointer (`sp`).
- **Activation Hardware Stack:** Hardware stack routines supporting function call frames via `PUSH`, `POP`, `CALL`, and `RET`.
- **Pre-Execution Target Validation:** Validates ahead-of-time that all branch and call targets land on valid instruction boundaries within the code segment before execution begins.

---

## Instruction Set Architecture (ISA)

The current engine implements 8 fundamental operations:

| Opcode | Mnemonic | Operands | Description |
| :--- | :--- | :--- | :--- |
| `0x0` | `MOV` | `Rd, Rs` or `Rd, Imm` | Copy register value or load immediate into destination register |
| `0x1` | `ADD` | `Rd, Rs` or `Rd, Imm` | Add register or immediate into destination register |
| `0x2` | `SUB` | `Rd, Rs` or `Rd, Imm` | Subtract register or immediate from destination register |
| `0x3` | `JMP` | `<label>` | Unconditional absolute branch to code segment offset |
| `0x4` | `PUSH`| `Rs` or `Imm` | Push register value or immediate onto the activation stack |
| `0x5` | `POP` | `Rd` | Pop value from activation stack into destination register |
| `0x6` | `CALL`| `<label>` | Push return address (`ip + next`) onto stack and jump to label |
| `0x7` | `RET` | *(none)* | Pop return address from stack and restore `ip` |

*(Note: Additional conditional branch opcodes `BEQ`, `BNE`, `BLT` exist in the AST/instruction definitions for future expansion, but backend execution loop support was not wired prior to project conclusion).*

---

## End-to-End Demonstration Program

The following assembly program is built into `src/main.cpp`:

```assembly
MOV R1, 42
ADD R2, R1
PUSH R2
CALL add_one
POP R3
JMP done

add_one:
ADD R2, R1
RET

done:
MOV R0, 0
```

When compiled and run:
1. Emits a 52-byte `.bcmv` binary container file.
2. Loads code into simulated guest virtual memory at base address `0x00400000`.
3. Executes through the virtual register file:
   - `R1 = 42`
   - `R2 = 84` (`42 + 42` across the function call)
   - `R3 = 42` (restored from stack)
   - `Final IP = 52`

---

## Cache & Hardware Benchmarks

The repository includes dedicated hardware profiling benchmarks in `benchmarks/`:

1. **Spatial Locality (`matrix_benchmark.cpp`):** Measures cache-miss variances between row-major (cache line friendly) versus column-major traversals across large 2D matrices.
2. **False Sharing & MESI Invalidation (`false_sharing_benchmark.cpp`):** Demonstrates multi-threaded throughput degradation when adjacent worker threads write to the same 64-byte cache line, and demonstrates mitigation via `alignas(std::hardware_destructive_interference_size)`.
3. **Data-Oriented AST Traversal (`ast_traversal_benchmark.cpp`):** Compares pointer-chasing linked tree traversals against flat, contiguous arena-backed array layouts.

---

## Curriculum Roadmap & Status

| Phase | Milestone | Status | Notes |
| :--- | :--- | :---: | :--- |
| **Week 1** | **Modern C++ Foundations & Language Frontend** | **Completed** | `UniquePtr`, `StringView`, zero-copy `Lexer`, Concepts, monadic `Parser`. |
| **Week 2** | **Memory & Cache Hierarchy Architecture** | **Completed** | `LinearArena`, `ArenaAllocator`, `StructOptimizer`, DoD & false sharing benchmarks. |
| **Week 3** | **Operating Systems & Runtime Fundamentals** | **Completed** | 2-level Page Table MMU, TLB, `VirtualMemoryBuffer`, 16 GPR CPU, `.bcmv` assembler, runtime interpreter. |
| **Week 4** | **Control Flow Graphs, SSA, and Data-Flow Analysis** | *Concluded* | Left unbuilt; basic block partitioning and dominance frontiers not implemented. |
| **Week 5** | **AOT Code Generation, Register Allocation & Vectorization** | *Concluded* | Left unbuilt; graph coloring and SIMD SLP passes not implemented. |

---

## Building and Running

### Prerequisites
- **Compiler:** Clang 16+, GCC 13+, or AppleClang with full C++23 support (`std::expected`, `std::string_view` features)
- **Build System:** CMake 3.24+ and Ninja or Make
- **Libraries:** Catch2 v3 (automatically retrieved via `FetchContent`)

### Compilation

```bash
# Configure the build
cmake -B build -S .

# Compile the test suite and VM executable
cmake --build build

# Run the full test suite (80 test cases, 480 assertions)
./build/vm_tests

# Run the end-to-end VM demonstration
./build/bytecode_vm
```

### Running Benchmarks

```bash
# Build benchmark targets
cmake --build build --target matrix_benchmark false_sharing_benchmark ast_traversal_benchmark

# Execute benchmarks
./build/matrix_benchmark
./build/false_sharing_benchmark
./build/ast_traversal_benchmark
```

---

## Repository Structure

```
├── include/
│   ├── AST.hpp                  # AST instruction and operand structures
│   ├── ArenaAllocator.hpp       # C++ standard-compliant arena allocator adapter
│   ├── BytecodeEmitter.hpp      # Two-pass binary bytecode assembler
│   ├── BytecodeFile.hpp         # .bcmv file container reader and writer
│   ├── BytecodeFormat.hpp       # 32-bit packed instruction format & bitmasks
│   ├── CompilerConcepts.hpp     # C++20 type validation concepts
│   ├── Instruction.hpp          # Instruction opcodes and operand definitions
│   ├── Lexer.hpp                # Zero-copy assembly lexical analyzer
│   ├── LinearArena.hpp          # 64MB monotonic memory arena
│   ├── Parser.hpp               # Monadic recursive-descent parser (std::expected)
│   ├── StringView.hpp           # Non-owning string slice abstraction
│   ├── StructOptimizer.hpp      # Struct alignment and padding analyzer
│   ├── UniquePtr.hpp            # Handcrafted move-only RAII smart pointer
│   ├── VirtualMachine.hpp       # 16-register virtual CPU and execution loop
│   ├── VirtualMemory.hpp        # 2-level Page Table MMU and Software TLB
│   ├── VirtualMemoryBuffer.hpp  # OS virtual page allocation wrapper (mmap / VirtualAlloc)
│   └── VMTypes.hpp              # Common VM error codes and result types
├── src/
│   └── main.cpp                 # End-to-end demonstration program
├── tests/                       # 14 Catch2 test suites covering all subsystems
├── benchmarks/                  # Cache locality, false sharing, and DoD benchmarks
├── CMakeLists.txt               # CMake configuration with ASan/UBSan instrumentation
└── CMakePresets.json            # Presets for CI and local development
```