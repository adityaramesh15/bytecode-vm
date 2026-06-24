#include "../include/UniquePtr.hpp"
#include "../include/LinearArena.hpp"
#include "../include/ArenaAllocator.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>

using namespace MemoryEngine;


struct PtrNode {
    UniquePtr<PtrNode> left;
    UniquePtr<PtrNode> right;
    uint32_t value;
    uint16_t kind;
};

struct FlatNode {
    uint32_t left_idx;
    uint32_t right_idx;
    uint32_t value;
    uint16_t kind;
    
    static constexpr uint32_t NullIndex = 0xFFFFFFFF;
};

void escape_optimization(uint64_t result) {
    [[maybe_unused]] volatile uint64_t dummy = result;
}


// POINTER-CHASING BASELINE MECHANICS (Standard Heap)
UniquePtr<PtrNode> build_pointer_tree(int depth, std::mt19937& rng) {
    if (depth <= 0) return UniquePtr<PtrNode>(nullptr);

    auto node = UniquePtr<PtrNode>(new PtrNode());
    node->value = rng() % 100;
    node->kind = static_cast<uint16_t>(depth % 5);
    
    node->left = build_pointer_tree(depth - 1, rng);
    node->right = build_pointer_tree(depth - 1, rng);
    return node;
}

uint64_t traverse_pointer_tree(const PtrNode* node) {
    if (!node) return 0;
    return node->value + node->kind + 
           traverse_pointer_tree(node->left.get()) + 
           traverse_pointer_tree(node->right.get());
}


// DATA-ORIENTED ARENA MECHANICS (Linear Arena)
template <typename Container>
uint32_t build_flat_tree(int depth, std::mt19937& rng, Container& storage) {
    if (depth <= 0) return FlatNode::NullIndex;

    auto current_idx = static_cast<uint32_t>(storage.size());
    storage.emplace_back(FlatNode{FlatNode::NullIndex, FlatNode::NullIndex, 0, 0});

    storage[current_idx].value = rng() % 100;
    storage[current_idx].kind = static_cast<uint16_t>(depth % 5);

    uint32_t left = build_flat_tree(depth - 1, rng, storage);
    storage[current_idx].left_idx = left;

    uint32_t right = build_flat_tree(depth - 1, rng, storage);
    storage[current_idx].right_idx = right;

    return current_idx;
}

template <typename Container>
uint64_t traverse_flat_tree(const Container& storage, uint32_t idx) {
    if (idx == FlatNode::NullIndex) return 0;
    
    const auto& node = storage[idx];
    return node.value + node.kind + 
           traverse_flat_tree(storage, node.left_idx) + 
           traverse_flat_tree(storage, node.right_idx);
}




int main() {
    constexpr int TREE_DEPTH = 18; 
    constexpr int RUN_ITERATIONS = 20;
    const size_t total_nodes = (1UZ << TREE_DEPTH);
    
    std::cout << "[i] Initializing True Arena-Backed Architectural Benchmark...\n";
    std::cout << "[i] Size of PtrNode:  " << sizeof(PtrNode) << " bytes\n";
    std::cout << "[i] Size of FlatNode: " << sizeof(FlatNode) << " bytes\n\n";

    // --- Benchmark Setup 1: Pointer Tree ---
    std::mt19937 rng1(1337);
    auto ptr_root = build_pointer_tree(TREE_DEPTH, rng1);

    // --- Benchmark Setup 2: Flat Arena-Backed Tree ---
    std::mt19937 rng2(1337);
    




    LinearArena arena(1024UZ * 1024UZ * 16UZ); 
    ArenaAllocator<FlatNode> allocator(arena); 
    std::vector<FlatNode, ArenaAllocator<FlatNode>> flat_storage(allocator);
    
    flat_storage.reserve(total_nodes); 
    
    size_t memory_before_build = arena.bytes_used();
    uint32_t flat_root_idx = build_flat_tree(TREE_DEPTH, rng2, flat_storage);
    size_t memory_after_build = arena.bytes_used();

    std::cout << "[+] Arena Allocation Metrics Captured:\n";
    std::cout << "    -> Backing Block Bytes Used: " << (memory_after_build - memory_before_build) << " bytes\n\n";

    
    
    // --- Execution Pass 1: Pointer Tree Traversals ---
    std::cout << "[*] Commencing Pointer-Chasing Tree Traversals (" << RUN_ITERATIONS << " passes)...\n";
    auto start_ptr = std::chrono::high_resolution_clock::now();

    uint64_t ptr_accumulator = 0;
    for (int i = 0; i < RUN_ITERATIONS; ++i) {
        ptr_accumulator += traverse_pointer_tree(ptr_root.get()); //
    }
    
    auto end_ptr = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ptr_duration = end_ptr - start_ptr;
    escape_optimization(ptr_accumulator);
    std::cout << "    -> Pointer Layout Total Time: " << ptr_duration.count() << " ms\n\n";



    // --- Execution Pass 2: Flat Arena-Backed Tree Traversals ---
    std::cout << "[*] Commencing Contiguous Arena-Backed Tree Traversals (" << RUN_ITERATIONS << " passes)...\n";
    auto start_flat = std::chrono::high_resolution_clock::now();
    
    uint64_t flat_accumulator = 0;
    for (int i = 0; i < RUN_ITERATIONS; ++i) {
        flat_accumulator += traverse_flat_tree(flat_storage, flat_root_idx);
    }
    
    auto end_flat = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> flat_duration = end_flat - start_flat;
    escape_optimization(flat_accumulator);
    std::cout << "    -> Flat Arena Layout Total Time: " << flat_duration.count() << " ms\n\n";

    
    
    // Integrity sanity verification rule
    if (ptr_accumulator != flat_accumulator) {
        std::cerr << "[!] CRITICAL ERROR: Accumulator output verification mismatch!\n";
        return 1;
    }

    double speedup = ptr_duration.count() / flat_duration.count();
    std::cout << "[+] Architecture Verification Pass Complete.\n";
    std::cout << "[+] Contiguous Flat Arena layout achieved a " << speedup << "x speedup factor over standard dynamic pointer chasing.\n";

    return 0;
}