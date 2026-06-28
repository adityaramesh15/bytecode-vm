#include <catch2/catch_test_macros.hpp>
#include "VirtualMemoryBuffer.hpp"
#include "LinearArena.hpp"
#include <unistd.h>
#include <sys/mman.h>

using namespace MemoryEngine;

TEST_CASE("Virtual Memory Allocation - Hardware Page Rounding Granularity", "[VirtualMemory]") {
    const auto system_page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    
    // Assert page size is precisely 16KB on Apple Silicon ARM64 platforms
#if defined(__APPLE__) && defined(__arm64__)
    REQUIRE(system_page_size == 16384UZ);
#endif

    SECTION("Sub-page requests are rounded up to exactly one page") {
        VirtualMemoryBuffer buffer(1UZ);
        REQUIRE(buffer.is_allocated());
        REQUIRE(buffer.data() != nullptr);
        REQUIRE(buffer.size() == system_page_size);
    }

    SECTION("Large unaligned requests snap up to the next consecutive page boundary") {
        size_t exact_two_pages = system_page_size * 2UZ;
        VirtualMemoryBuffer buffer(exact_two_pages - 1UZ);
        REQUIRE(buffer.size() == exact_two_pages);
    }
}

TEST_CASE("Virtual Memory Allocation - Strict Move Semantics & RAII Transfer", "[VirtualMemory]") {
    const auto system_page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    
    SECTION("Move constructor shifts ownership without kernel reallocation leaks") {
        VirtualMemoryBuffer source(system_page_size);
        void* original_address = source.data();
        size_t original_size = source.size();

        VirtualMemoryBuffer destination(std::move(source));

        // Destination acquires all raw address parameters
        REQUIRE(destination.is_allocated());
        REQUIRE(destination.data() == original_address);
        REQUIRE(destination.size() == original_size);

        // Source is completely disarmed to safe defaults
        REQUIRE_FALSE(source.is_allocated());
        REQUIRE(source.data() == nullptr);
        REQUIRE(source.size() == 0UZ);
    }

    SECTION("Move assignment releases existing resources cleanly before taking ownership") {
        VirtualMemoryBuffer buffer_a(system_page_size);
        VirtualMemoryBuffer buffer_b(system_page_size * 2UZ);
        void* address_b = buffer_b.data();

        buffer_a = std::move(buffer_b);

        REQUIRE(buffer_a.size() == system_page_size * 2UZ);
        REQUIRE(buffer_a.data() == address_b);
        REQUIRE_FALSE(buffer_b.is_allocated());
    }
}

TEST_CASE("Virtual Memory Allocation - Hardware Permission Boundary Transitions", "[VirtualMemory]") {
    const auto system_page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    VirtualMemoryBuffer buffer(system_page_size, MemoryPermission::ReadWrite);
    
    REQUIRE(buffer.is_allocated());

    SECTION("Transition to Read-Only access map completes successfully") {
        bool success = buffer.protect(MemoryPermission::Read);
        REQUIRE(success);
    }

    SECTION("Transition to executable segment space completes successfully") {
        bool success = buffer.protect(MemoryPermission::ReadExec);
        REQUIRE(success);
    }

    SECTION("Invalid operations on unallocated default buffers are rejected safely") {
        VirtualMemoryBuffer empty_buffer;
        bool success = empty_buffer.protect(MemoryPermission::ReadWrite);
        REQUIRE_FALSE(success);
    }
}

TEST_CASE("LinearArena Integration - Direct Page-Backed Processing Matrix", "[Memory]") {
    const auto system_page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    
    // Request an allocation smaller than one page
    LinearArena arena(1024UZ);

    // The arena capacity must adapt cleanly to match the OS allocation size
    REQUIRE(arena.capacity() == system_page_size);
    REQUIRE(arena.bytes_used() == 0UZ);

    SECTION("Contiguous sequential raw allocations operate normally across direct page views") {
        auto* chunk1 = static_cast<std::byte*>(arena.allocate_raw<uint64_t>(2UZ)); // 16 bytes
        REQUIRE(chunk1 != nullptr);
        REQUIRE(arena.bytes_used() == 16UZ);

        arena.reset();
        REQUIRE(arena.bytes_used() == 0UZ);
    }
}