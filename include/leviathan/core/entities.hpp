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
    // Numeric handle. Default-constructed = invalid. Assigning real
    // numeric IDs (e.g. by insertion order in state.countries) is the
    // caller's job; the JSON loader leaves this as invalid because
    // the on-disk identifier is the string `id_code`.
    CountryId   id;

    // On-disk identifier from JSON, e.g. "GER". Stable across saves
    // and human-authored data, unlike the numeric `id`.
    std::string id_code;

    std::string name;
    std::string display_name;

    // Economic / political baselines, loaded from JSON. M0.7 stores
    // them; the simulation systems that consume them arrive in
    // Milestone 1 (M1.1 onwards).
    double initial_gdp       = 0.0;
    double initial_stability = 0.0;
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
