#include <catch2/catch_test_macros.hpp>
#include "StructOptimizer.hpp"

using namespace MemoryEngine;

TEST_CASE("Memory Engine - Automated Layout Optimization Pass", "[Memory]") {
    
    // SCENARIO 1: Define a highly unoptimized, varying architectural telemetry layout
    std::vector<StructMember> chaotic_struct = {
        {"flag_a",       1, 1}, // char/bool
        {"payload_64",   8, 8}, // int64_t (Triggers massive padding gap)
        {"short_b",      2, 2}, // int16_t
        {"matrix_ptr",   8, 8}, // double/pointer (Triggers another padding gap)
        {"flag_c",       1, 1}  // uint8_t
    };

    // 1. Run evaluation pass on the unoptimized input variant
    LayoutReport unoptimized_report = StructOptimizer::evaluate_layout(chaotic_struct);
    
    // Verify our simulation accurately maps compiler behavior (1+7 padding + 8 + 2+6 padding + 8 + 1 + 7 tail padding = 40)
    REQUIRE(unoptimized_report.total_size == 40);
    REQUIRE(unoptimized_report.total_padding == 20); // 20 bytes wasted!

    // 2. Execute the Optimization Script / Automation Algorithm Pass
    std::vector<StructMember> optimized_fields = StructOptimizer::optimize_layout(chaotic_struct);

    // 3. Re-evaluate the programmatically reordered fields
    LayoutReport optimized_report = StructOptimizer::evaluate_layout(optimized_fields);

    // Verify the engine automatically squashed the layout down to its mathematical limit
    REQUIRE(optimized_report.total_size == 24);
    REQUIRE(optimized_report.total_padding == 4); // Saved 16 bytes of slack space automatically!
    
    // Assert the script correctly reordered fields by descending alignment priority
    REQUIRE(optimized_fields[0].name == "payload_64");
    REQUIRE(optimized_fields[1].name == "matrix_ptr");
    REQUIRE(optimized_fields[2].name == "short_b");
}

TEST_CASE("Memory Engine - Varying Structural Layout Evaluation Boundary Constraints", "[Memory]") {
    // SCENARIO 2: Test an entirely different structure configuration to prove generalized flexibility
    std::vector<StructMember> nested_subsystem_layout = {
        {"opcode_byte",   1, 1},
        {"reg_index",     1, 1},
        {"immediate_val", 8, 8}
    };

    LayoutReport raw_report = StructOptimizer::evaluate_layout(nested_subsystem_layout);
    REQUIRE(raw_report.total_size == 16); // 1 + 1 + (6 bytes padding) + 8 = 16 bytes total
    REQUIRE(raw_report.total_padding == 6);

    // Run automation pipeline
    auto optimized_subsystem = StructOptimizer::optimize_layout(nested_subsystem_layout);
    LayoutReport optimized_report = StructOptimizer::evaluate_layout(optimized_subsystem);

    // After sorting: 8 + 1 + 1 = 10 -> rounded to multiple of 8 = 16 bytes total size.
    // Notice: For this distinct shape, total size remains 16, but internal structure changes.
    REQUIRE(optimized_report.total_size == 16);
    REQUIRE(optimized_report.total_padding == 6); // Wasted space is unavoidable due to trailing rounding boundaries
    REQUIRE(optimized_subsystem[0].name == "immediate_val");
}

TEST_CASE("Memory Engine - Rejects zero alignment members", "[Memory]") {
    std::vector<StructMember> invalid_layout = {
        {"bad_field", 4, 0}
    };
    REQUIRE_THROWS_AS(StructOptimizer::evaluate_layout(invalid_layout), std::invalid_argument);
}