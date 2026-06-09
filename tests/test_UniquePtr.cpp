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