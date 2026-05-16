#include <doctest/doctest.h>

#include <unordered_map>

#include "leviathan/core/ids.hpp"

using leviathan::core::CharacterId;
using leviathan::core::CountryId;
using leviathan::core::EventId;
using leviathan::core::FactionId;
using leviathan::core::PolicyId;
using leviathan::core::ProvinceId;

TEST_CASE("StrongId default-constructs to invalid") {
    CountryId id;
    CHECK_FALSE(id.valid());
    CHECK(id == CountryId::invalid());
    CHECK(id.value() == -1);
}

TEST_CASE("StrongId explicit construction round-trips") {
    CountryId a{7};
    CHECK(a.valid());
    CHECK(a.value() == 7);
}

TEST_CASE("StrongId equality and ordering work within one type") {
    CountryId a{1};
    CountryId b{1};
    CountryId c{2};

    CHECK(a == b);
    CHECK(a != c);
    CHECK(a < c);
    CHECK(c > a);
    CHECK(a <= b);
    CHECK(a >= b);
}

TEST_CASE("Different tag aliases are distinct types (compile-time check)") {
    // The body of this test_case is mostly a compile-time assertion:
    // these template instantiations must not be interchangeable.
    static_assert(!std::is_same_v<CountryId, ProvinceId>);
    static_assert(!std::is_same_v<CountryId, FactionId>);
    static_assert(!std::is_same_v<FactionId, PolicyId>);
    static_assert(!std::is_same_v<PolicyId, EventId>);
    static_assert(!std::is_same_v<EventId, CharacterId>);
    CHECK(true);
}

TEST_CASE("StrongId is usable as an unordered_map key") {
    std::unordered_map<CountryId, int> lookup;
    lookup[CountryId{1}]  = 100;
    lookup[CountryId{42}] = 4200;

    CHECK(lookup.at(CountryId{1}) == 100);
    CHECK(lookup.at(CountryId{42}) == 4200);
    CHECK(lookup.size() == 2);
}

TEST_CASE("invalid() sentinel does not collide with valid IDs") {
    CountryId real{0};  // 0 is a valid value
    CHECK(real != CountryId::invalid());
    CHECK(real.valid());
}
