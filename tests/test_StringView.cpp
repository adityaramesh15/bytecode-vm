#include <catch2/catch_test_macros.hpp>
#include "StringView.hpp"

TEST_CASE("StringView - Default and Empty Construction", "[StringView]") {
    StringView view;
    REQUIRE(view.size() == 0);
    REQUIRE(view.empty());
    REQUIRE(view.data() == nullptr);
}

TEST_CASE("StringView - Raw String Literal Construction", "[StringView]") {
    StringView view("MOV");
    REQUIRE(view.size() == 3);
    REQUIRE_FALSE(view.empty());
    REQUIRE(view.data() != nullptr);
    REQUIRE(view[0] == 'M');
    REQUIRE(view[1] == 'O');
    REQUIRE(view[2] == 'V');
}

TEST_CASE("StringView - Explicit Length Sub-slice Construction", "[StringView]") {
    const char* buffer = "MOV R1, 42";
    // Create a view that isolates just "R1" out of the middle of the string
    StringView view(buffer + 4, 2); 
    
    REQUIRE(view.size() == 2);
    REQUIRE(view[0] == 'R');
    REQUIRE(view[1] == '1');
}

TEST_CASE("StringView - Null Pointer Safety", "[StringView]") {
    StringView view(nullptr);
    REQUIRE(view.empty());
    REQUIRE(view.size() == 0);
    REQUIRE(view.data() == nullptr);
}