// Entity placeholders + M1 baseline state.
//
// M0 introduced these as ID-only stubs; M1.1 fleshed out CountryState;
// M1.2 fleshes out FactionState. ProvinceState, PolicyData, and
// EventDefinition remain ID-only stubs and will grow in subsequent
// sub-milestones (M1.4, ...).
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
#include <vector>

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
    // Identity
    FactionId   id;                  // numeric handle (caller-assigned)
    CountryId   country;             // numeric link to CountryState (caller-resolved)
    std::string id_code;             // on-disk identifier, e.g. "GER_military"
    std::string country_id_code;     // links to CountryState::id_code, e.g. "GER"
    std::string name;                // "German Military"
    std::string type;                // RFC-010 §2.5: military, bureaucracy,
                                     // workers, local_elites, media,
                                     // intelligence, students, technical_elites

    // Behavioural ratios (0..1)
    double support    = 0.0;   // popular backing for the faction
    double influence  = 0.0;   // political clout / share of decision power
    double radicalism = 0.0;   // willingness to escalate
    double loyalty    = 0.0;   // loyalty to the current regime

    // Absolute resources (>= 0). Units are abstract for now; M1.3
    // (economy / budget) decides what they map to.
    double resources  = 0.0;

    // Policy id_codes this faction is inclined to favour. M1.4 will
    // tighten the semantics once PolicyData has shape; for M1.2 it is
    // a free-form list of strings that survives the JSON round trip.
    std::vector<std::string> preferred_policies;
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
