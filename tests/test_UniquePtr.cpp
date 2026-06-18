#include <catch2/catch_test_macros.hpp>
#include "UniquePtr.hpp"
#include <utility>

struct LifetimeTracker {
    static int active_instances;
    int value;

    LifetimeTracker(int val) : value(val) { active_instances++; }
    ~LifetimeTracker() { active_instances--; }
};

int LifetimeTracker::active_instances = 0;

TEST_CASE("UniquePtr - Core RAII Creation and Destruction", "[UniquePtr]") {
    // Reset tracker count
    LifetimeTracker::active_instances = 0;

    {
        UniquePtr<LifetimeTracker> ptr(new LifetimeTracker(100));
        REQUIRE(LifetimeTracker::active_instances == 1);
        REQUIRE((*ptr).value == 100);
        REQUIRE(ptr->value == 100);
        REQUIRE(ptr.get() != nullptr);
    } // <-- Scope ends here. Destructor must fire.

    REQUIRE(LifetimeTracker::active_instances == 0);
}

TEST_CASE("UniquePtr - Empty and Null Behavior", "[UniquePtr]") {
    UniquePtr<int> empty_ptr;
    REQUIRE(empty_ptr.get() == nullptr);
}

TEST_CASE("UniquePtr - Move Semantics (Ownership Transfer)", "[UniquePtr]") {
    LifetimeTracker::active_instances = 0;

    {
        UniquePtr<LifetimeTracker> source(new LifetimeTracker(42));
        REQUIRE(LifetimeTracker::active_instances == 1);

        // Explicitly trigger the move constructor
        UniquePtr<LifetimeTracker> destination(std::move(source));

        // Verify ownership was stolen
        REQUIRE(destination.get() != nullptr);
        REQUIRE(destination->value == 42);

        // Verify the source object was safely disarmed/zeroed out
        REQUIRE(source.get() == nullptr);
        REQUIRE(LifetimeTracker::active_instances == 1);
    }

    // Ensure cleanup happens properly from the destination holder
    REQUIRE(LifetimeTracker::active_instances == 0);
}

TEST_CASE("UniquePtr - Move Assignment Operator", "[UniquePtr]") {
    LifetimeTracker::active_instances = 0;

    {
        UniquePtr<LifetimeTracker> ptr1(new LifetimeTracker(10));
        UniquePtr<LifetimeTracker> ptr2(new LifetimeTracker(20));
        REQUIRE(LifetimeTracker::active_instances == 2);

        // Move assign ptr2 into ptr1
        ptr1 = std::move(ptr2);

        // The object tracking '10' should be instantly deleted during assignment
        REQUIRE(LifetimeTracker::active_instances == 1);
        REQUIRE(ptr1->value == 20);
        REQUIRE(ptr2.get() == nullptr);
    }

    REQUIRE(LifetimeTracker::active_instances == 0);
}

TEST_CASE("UniquePtr - Manual Release Hook", "[UniquePtr]") {
    LifetimeTracker::active_instances = 0;
    LifetimeTracker::active_instances = 0;
    LifetimeTracker* raw_captured = nullptr;

    {
        UniquePtr<LifetimeTracker> ptr(new LifetimeTracker(88));
        REQUIRE(LifetimeTracker::active_instances == 1);

        // Relinquish ownership control
        raw_captured = ptr.release();

        REQUIRE(ptr.get() == nullptr);
        REQUIRE(LifetimeTracker::active_instances == 1); // Still alive!
    }

    // Manual deletion is mandatory now because the smart pointer let go
    REQUIRE(LifetimeTracker::active_instances == 1);
    delete raw_captured;
    REQUIRE(LifetimeTracker::active_instances == 0);
}

// 1. A Stateless Custom Deleter for testing
struct CustomMockDeleter {
    static int delete_count;
    void operator()(LifetimeTracker* ptr) const noexcept {
        delete_count++;
        delete ptr;
    }
};
int CustomMockDeleter::delete_count = 0;

// 2. A Stateful, Move-Only Deleter for testing resource tracking transfers
struct StatefulMoveOnlyDeleter {
    int context_id;
    int* trace_counter;

    StatefulMoveOnlyDeleter(int id_val, int* counter) : context_id(id_val), trace_counter(counter) {}

    // Enforce move-only constraints
    StatefulMoveOnlyDeleter(const StatefulMoveOnlyDeleter&) = delete;
    StatefulMoveOnlyDeleter& operator=(const StatefulMoveOnlyDeleter&) = delete;

    StatefulMoveOnlyDeleter(StatefulMoveOnlyDeleter&& other) noexcept 
        : context_id(other.context_id), trace_counter(other.trace_counter) {
        other.context_id = -1;
        other.trace_counter = nullptr;
    }

    StatefulMoveOnlyDeleter& operator=(StatefulMoveOnlyDeleter&& other) noexcept {
        if (this != &other) {
            context_id = other.context_id;
            trace_counter = other.trace_counter;
            other.context_id = -1;
            other.trace_counter = nullptr;
        }
        return *this;
    }

    void operator()(LifetimeTracker* ptr) const noexcept {
        if (trace_counter != nullptr) {
            (*trace_counter)++;
        }
        delete ptr;
    }
};

TEST_CASE("UniquePtr - Zero-Overhead Memory Footprint Optimizations", "[UniquePtr]") {
    // DevDiv Guardrail: If a deleter is stateless, the smart pointer MUST occupy 
    // the exact same space as a standard raw pointer (8 bytes on 64-bit platforms).
    // This proves that your C++20 [[no_unique_address]] attribute is operating correctly.
    
    static_assert(sizeof(UniquePtr<int>) == sizeof(int*), 
                  "Optimization Violation: Default-constructed UniquePtr size overhead detected!");
                  
    static_assert(sizeof(UniquePtr<LifetimeTracker, CustomMockDeleter>) == sizeof(LifetimeTracker*),
                  "Optimization Violation: Stateless custom deleter padding detected!");
                  
    REQUIRE(sizeof(UniquePtr<LifetimeTracker, CustomMockDeleter>) == sizeof(LifetimeTracker*));
}

TEST_CASE("UniquePtr - Stateless Custom Deleter Invocation", "[UniquePtr]") {
    CustomMockDeleter::delete_count = 0;
    LifetimeTracker::active_instances = 0;

    {
        UniquePtr<LifetimeTracker, CustomMockDeleter> ptr(new LifetimeTracker(500));
        REQUIRE(LifetimeTracker::active_instances == 1);
        REQUIRE(CustomMockDeleter::delete_count == 0);
    } // Out of scope

    // Verify the custom deleter route was selected instead of global delete
    REQUIRE(LifetimeTracker::active_instances == 0);
    REQUIRE(CustomMockDeleter::delete_count == 1);
}

TEST_CASE("UniquePtr - Stateful Move-Only Deleter Propagation", "[UniquePtr]") {
    LifetimeTracker::active_instances = 0;
    int deletion_trace_runs = 0;

    {
        // Construct a stateful deleter tied to our local tracking variable
        StatefulMoveOnlyDeleter tracking_deleter(42, &deletion_trace_runs);

        UniquePtr<LifetimeTracker, StatefulMoveOnlyDeleter> source_ptr(
            new LifetimeTracker(777), 
            std::move(tracking_deleter)
        );

        REQUIRE(LifetimeTracker::active_instances == 1);
        REQUIRE(source_ptr.get_deleter().context_id == 42);

        // Ownership transfer: moves both internal raw pointer and stateful deleter properties
        UniquePtr<LifetimeTracker, StatefulMoveOnlyDeleter> dest_ptr(std::move(source_ptr));

        REQUIRE(source_ptr.get() == nullptr);
        REQUIRE(source_ptr.get_deleter().context_id == -1); // Cleared out via move
        
        REQUIRE(dest_ptr.get() != nullptr);
        REQUIRE(dest_ptr.get_deleter().context_id == 42);   // Safely transferred
        REQUIRE(deletion_trace_runs == 0);                  // Destructor hasn't fired yet
    } 

    // Scope exit: Verify destructor successfully evaluated via moved stateful deleter
    REQUIRE(LifetimeTracker::active_instances == 0);
    REQUIRE(deletion_trace_runs == 1);
}

TEST_CASE("UniquePtr - Move Assignment Edge Cases & Null-Safety Guardrails", "[UniquePtr]") {
    LifetimeTracker::active_instances = 0;
    int deletion_trace_runs = 0;

    // SCENARIO: Move-assigning an unpopulated (empty) pointer into another empty pointer.
    // This validates that your null safety routing doesn't trip or crash on empty states.
    {
        UniquePtr<LifetimeTracker, StatefulMoveOnlyDeleter> empty_source(nullptr, StatefulMoveOnlyDeleter(101, &deletion_trace_runs));
        UniquePtr<LifetimeTracker, StatefulMoveOnlyDeleter> empty_dest(nullptr, StatefulMoveOnlyDeleter(102, &deletion_trace_runs));

        empty_dest = std::move(empty_source);
        
        REQUIRE(empty_dest.get() == nullptr);
        REQUIRE(empty_source.get() == nullptr);
        REQUIRE(deletion_trace_runs == 0);
    }

    // SCENARIO: Move-assigning an unpopulated pointer into an active pointer holding an object.
    // The active target object must clean up immediately, and the pointer must become empty.
    {
        UniquePtr<LifetimeTracker, StatefulMoveOnlyDeleter> active_target(new LifetimeTracker(9), StatefulMoveOnlyDeleter(201, &deletion_trace_runs));
        UniquePtr<LifetimeTracker, StatefulMoveOnlyDeleter> empty_source(nullptr, StatefulMoveOnlyDeleter(202, &deletion_trace_runs));

        REQUIRE(LifetimeTracker::active_instances == 1);
        
        active_target = std::move(empty_source); // Should trigger destruction of '9'

        REQUIRE(LifetimeTracker::active_instances == 0);
        REQUIRE(deletion_trace_runs == 1); // Triggered by active_target's original deleter context 201
        REQUIRE(active_target.get() == nullptr);
    }
}