#include <catch2/catch_test_macros.hpp>
#include "StringView.hpp"


TEST_CASE("StringView basic properties", "[StringView]") {
    StringView empty;
    REQUIRE(empty.empty());
    REQUIRE(empty.size() == 0);
    REQUIRE(empty.data() == nullptr);

    StringView literal("MOV R1, 42");
    REQUIRE_FALSE(literal.empty());
    REQUIRE(literal.size() == 10);
    REQUIRE(literal[0] == 'M');

    StringView substr("MOV R1, 42", 3);
    REQUIRE_FALSE(substr.empty());
    REQUIRE(substr.size() == 3);
    REQUIRE(substr[substr.size() - 1] == 'V');
}