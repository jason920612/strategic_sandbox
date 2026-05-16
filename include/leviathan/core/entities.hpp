// Entity placeholders.
//
// M0.3 only needs each entity to be addressable by its strong ID so
// GameState's containers have something to hold. Real fields (GDP,
// stability, faction relations, policy effects, etc.) land in
// Milestone 1 as the per-entity systems come online.
//
// The minimal field set was chosen so that:
//   - every entity has its own ID (you can write `country.id` rather
//     than tracking ID via container index);
//   - human-readable names (where natural) survive a round-trip
//     through save / load tests in M0.8.

#ifndef LEVIATHAN_CORE_ENTITIES_HPP
#define LEVIATHAN_CORE_ENTITIES_HPP

#include <string>

#include "leviathan/core/ids.hpp"

namespace leviathan::core {

struct CountryState {
    CountryId   id;
    std::string name;
};

struct ProvinceState {
    ProvinceId id;
    CountryId  owner;
};

struct FactionState {
    FactionId id;
    CountryId country;
};

struct PolicyData {
    PolicyId    id;
    std::string name;
};

struct EventDefinition {
    EventId     id;
    std::string name;
};

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_ENTITIES_HPP
