#include <doctest/doctest.h>

#include "leviathan/core/string_utils.hpp"

using leviathan::core::string_utils::trim;

TEST_CASE("trim leaves clean strings alone") {
    CHECK(trim("abc")        == "abc");
    CHECK(trim("a b c")      == "a b c");
}

TEST_CASE("trim strips leading and trailing whitespace") {
    CHECK(trim("   abc")     == "abc");
    CHECK(trim("abc   ")     == "abc");
    CHECK(trim("   abc   ")  == "abc");
}

TEST_CASE("trim handles mixed whitespace characters") {
    CHECK(trim(" \t\nabc \r\v\f") == "abc");
}

TEST_CASE("trim of all-whitespace returns empty") {
    CHECK(trim("   ").empty());
    CHECK(trim("").empty());
}

TEST_CASE("trim preserves interior whitespace") {
    CHECK(trim("  a  b  ") == "a  b");
}
