// Strong, tag-templated identifier types for cross-cutting domain entities.
//
// Each alias below is a *distinct* type. CountryId and ProvinceId both wrap
// an int, but they cannot be compared, assigned, or implicitly converted
// across the boundary. This is intentional: deterministic simulation code
// will pass thousands of integers around and we want the compiler to catch
// "I meant a country, not a province" mistakes.
//
// Default-constructed StrongId is the invalid sentinel (value == -1). Use
// `StrongId::invalid()` for explicit intent.

#ifndef LEVIATHAN_CORE_IDS_HPP
#define LEVIATHAN_CORE_IDS_HPP

#include <cstddef>
#include <functional>

namespace leviathan::core {

template <typename Tag>
class StrongId {
public:
    using underlying_type = int;

    constexpr StrongId() noexcept = default;
    constexpr explicit StrongId(underlying_type value) noexcept : value_(value) {}

    constexpr underlying_type value() const noexcept { return value_; }
    constexpr bool valid() const noexcept { return value_ >= 0; }

    static constexpr StrongId invalid() noexcept { return StrongId{}; }

    friend constexpr bool operator==(StrongId a, StrongId b) noexcept {
        return a.value_ == b.value_;
    }
    friend constexpr bool operator!=(StrongId a, StrongId b) noexcept {
        return a.value_ != b.value_;
    }
    friend constexpr bool operator<(StrongId a, StrongId b) noexcept {
        return a.value_ < b.value_;
    }
    friend constexpr bool operator<=(StrongId a, StrongId b) noexcept {
        return a.value_ <= b.value_;
    }
    friend constexpr bool operator>(StrongId a, StrongId b) noexcept {
        return a.value_ > b.value_;
    }
    friend constexpr bool operator>=(StrongId a, StrongId b) noexcept {
        return a.value_ >= b.value_;
    }

private:
    underlying_type value_ = -1;  // -1 reserved for "invalid"
};

namespace id_tags {
struct Country;
struct Province;
struct Faction;
struct Policy;
struct Event;
struct Character;
}  // namespace id_tags

using CountryId   = StrongId<id_tags::Country>;
using ProvinceId  = StrongId<id_tags::Province>;
using FactionId   = StrongId<id_tags::Faction>;
using PolicyId    = StrongId<id_tags::Policy>;
using EventId     = StrongId<id_tags::Event>;
using CharacterId = StrongId<id_tags::Character>;

}  // namespace leviathan::core

namespace std {

template <typename Tag>
struct hash<leviathan::core::StrongId<Tag>> {
    std::size_t operator()(leviathan::core::StrongId<Tag> id) const noexcept {
        return std::hash<int>{}(id.value());
    }
};

}  // namespace std

#endif  // LEVIATHAN_CORE_IDS_HPP
