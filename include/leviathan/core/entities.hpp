// Entity placeholders + M1 baseline state.
//
// M0 introduced these as ID-only stubs; M1.1 fleshes out CountryState
// with the runtime numeric fields that M1's economy / stability /
// budget systems will read and (eventually) write. FactionState,
// ProvinceState, PolicyData, and EventDefinition remain ID-only stubs
// in M1.1 and will grow in subsequent sub-milestones (M1.2, M1.4, ...).
//
// Naming convention for CountryState numeric fields:
//   * gdp / tax_revenue / budget_balance      - absolute amounts
//   * legal_tax_burden, fiscal_capacity, ...  - 0-to-1 ratios
//   * stability, legitimacy                   - 0-to-1 ratios
//   * military_power, threat_perception       - 0-to-1 ratios
//
// The DataLoader (M0.7) maps JSON "initial_gdp" / "initial_stability"
// onto the runtime `gdp` / `stability` fields; the other fields share
// names with their JSON counterparts. This keeps the on-disk config
// readable ("initial_..." reads as a baseline) while keeping the
// runtime struct compact.

#ifndef LEVIATHAN_CORE_ENTITIES_HPP
#define LEVIATHAN_CORE_ENTITIES_HPP

#include <string>

#include "leviathan/core/ids.hpp"

namespace leviathan::core {

struct CountryState {
    // Identity
    CountryId   id;
    std::string id_code;
    std::string name;
    std::string display_name;

    // Absolute economic state
    double gdp            = 0.0;
    double tax_revenue    = 0.0;   // runtime-only, derived; not in JSON config
    double budget_balance = 0.0;   // runtime-only, can be negative

    // Fiscal / administrative ratios (0..1)
    double legal_tax_burden          = 0.0;
    double fiscal_capacity           = 0.0;
    double administrative_efficiency = 0.0;
    double central_control           = 0.0;
    double corruption                = 0.0;

    // Political ratios (0..1)
    double stability  = 0.0;
    double legitimacy = 0.0;

    // Strategic ratios (0..1)
    double military_power     = 0.0;
    double threat_perception  = 0.0;
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
