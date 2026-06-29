#include <catch2/catch_test_macros.hpp>
#include "LinearArena.hpp"
#include "VirtualMemory.hpp"

using namespace MemoryEngine;

TEST_CASE("Virtual Memory - Core 2-Level Translation Pass", "[VirtualMemory]") {
    // Allocation: Provide a 1MB local arena mock system stick
    LinearArena arena(1024UZ * 1024UZ);
    MemoryManagementUnit mmu(arena);

    // Map a page at a random sparse virtual location: 0x00401000
    uint32_t sample_vaddr = 0x00401000U;
    auto map_res = mmu.map_page(sample_vaddr, true, true);
    REQUIRE(map_res.has_value());

    // First translation must result in a TLB Miss but valid physical address conversion
    auto trans_res1 = mmu.translate(sample_vaddr, false);
    REQUIRE(trans_res1.has_value());
    REQUIRE(mmu.tlb_misses() == 1);
    REQUIRE(mmu.tlb_hits() == 0);

    // Subsequent access to the same page must trigger a clean cache hit
    auto trans_res2 = mmu.translate(sample_vaddr + 0x10U, false); // different offset, same page
    REQUIRE(trans_res2.has_value());
    REQUIRE(mmu.tlb_misses() == 1);
    REQUIRE(mmu.tlb_hits() == 1);
    
    // Check byte alignment offsets map cleanly
    REQUIRE((*trans_res2 - *trans_res1) == 0x10UZ);
}

TEST_CASE("Virtual Memory - Translated address does not alias page table storage", "[VirtualMemory]") {
    LinearArena arena(1024UZ * 1024UZ);
    MemoryManagementUnit mmu(arena);

    uint32_t vaddr = 0x00402000U;
    REQUIRE(mmu.map_page(vaddr, true, true).has_value());

    auto phys = mmu.translate(vaddr, true);
    REQUIRE(phys.has_value());

    // Page-table metadata lives at the start of the arena; user data must not overlap it.
    REQUIRE(*phys >= sizeof(PageTable));

    auto* arena_base = static_cast<std::byte*>(arena.current_allocations().data());
    constexpr std::byte marker{0xAB};
    arena_base[*phys] = marker;

    REQUIRE(arena_base[0] != marker);
    REQUIRE(arena_base[*phys] == marker);
}

TEST_CASE("Virtual Memory - Sparse Space and Permission Safeguards", "[VirtualMemory]") {
    LinearArena arena(1024UZ * 512UZ);
    MemoryManagementUnit mmu(arena);

    // 1. Accessing unmapped addresses must cause an unadulterated PageFault mapping error
    auto unmapped_lookup = mmu.translate(0x00800000U, false);
    REQUIRE_FALSE(unmapped_lookup.has_value());
    REQUIRE(unmapped_lookup.error() == VirtualMemoryError::PageFault);

    // 2. Map a strict Read-Only page
    uint32_t ro_vaddr = 0x10000000U;
    REQUIRE(mmu.map_page(ro_vaddr, true, false).has_value());

    // Reading should succeed cleanly
    REQUIRE(mmu.translate(ro_vaddr, false).has_value());

    // Writing to a Read-Only frame mapping must instantly fire an AccessViolation constraint error
    auto illegal_write = mmu.translate(ro_vaddr, true);
    REQUIRE_FALSE(illegal_write.has_value());
    REQUIRE(illegal_write.error() == VirtualMemoryError::AccessViolation);
}

TEST_CASE("Virtual Memory - Frame Recycler Cleanup Pipeline", "[VirtualMemory]") {
    LinearArena arena(1024UZ * 64UZ); // Small size limiting page capacity bounds
    MemoryManagementUnit mmu(arena);

    uint32_t target_addr = 0x30000000U;
    
    // Map, record arena footprint, then free the range layout
    REQUIRE(mmu.map_page(target_addr, true, true).has_value());
    const size_t footprint_after_first_map = arena.bytes_used();

    REQUIRE(mmu.unmap_page(target_addr).has_value());

    // Re-mapping should pull the frame straight out of the internal Recycler Free List
    REQUIRE(mmu.map_page(0x40000000U, true, true).has_value());
    REQUIRE(arena.bytes_used() == footprint_after_first_map);
}
