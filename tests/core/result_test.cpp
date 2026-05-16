#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "leviathan/core/result.hpp"

using leviathan::core::Result;

TEST_CASE("Result::success holds the value") {
    auto r = Result<int>::success(42);
    CHECK(r.ok());
    CHECK_FALSE(r.failed());
    CHECK(static_cast<bool>(r));
    CHECK(r.value() == 42);
}

TEST_CASE("Result::failure holds the error message") {
    auto r = Result<int>::failure("boom");
    CHECK_FALSE(r.ok());
    CHECK(r.failed());
    CHECK_FALSE(static_cast<bool>(r));
    CHECK(r.error() == "boom");
}

TEST_CASE("Result::value_or returns fallback on failure") {
    auto good = Result<int>::success(5);
    auto bad  = Result<int>::failure("nope");

    CHECK(good.value_or(99) == 5);
    CHECK(bad.value_or(99)  == 99);
}

TEST_CASE("Result can carry a non-copyable value") {
    auto r = Result<std::unique_ptr<int>>::success(std::make_unique<int>(7));
    REQUIRE(r.ok());
    CHECK(*r.value() == 7);
}

TEST_CASE("Result with custom error type") {
    enum class ErrCode { kOutOfRange, kMalformed };

    auto r = Result<int, ErrCode>::failure(ErrCode::kMalformed);
    REQUIRE(r.failed());
    CHECK(r.error() == ErrCode::kMalformed);
}
