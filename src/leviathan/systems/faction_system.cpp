#include "leviathan/systems/faction_system.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "internal/numeric_guards.hpp"

namespace leviathan::systems::faction {

namespace ng = leviathan::systems::detail;

namespace {

// One faction's prepared drift candidates. Built during pre-flight
// (no state mutation); consumed during commit (no further validation
// needed).
struct PreparedDrift {
    core::FactionState* faction = nullptr;
    double              new_loyalty = 0.0;
    double              new_support = 0.0;
};

}  // namespace

core::Result<ReactionOutcome> react(core::GameState& state,
                                    core::CountryId  country) {
    // Precondition: country resolves to a valid index. We refuse to
    // mutate any faction if the actor is bogus.
    if (!country.valid() ||
        country.value() < 0 ||
        static_cast<std::size_t>(country.value()) >= state.countries.size()) {
        return core::Result<ReactionOutcome>::failure(
            "faction::react: country CountryId " +
            std::to_string(country.value()) +
            " is not a valid index into state.countries");
    }

    const auto& c = state.countries[static_cast<std::size_t>(country.value())];

    // Validate country-side inputs once; both feed every faction.
    if (auto err = ng::require_unit_ratio<ReactionOutcome>(
            "faction::react", "country", c.id_code,
            "country.stability", c.stability)) {
        return *err;
    }
    if (auto err = ng::require_unit_ratio<ReactionOutcome>(
            "faction::react", "country", c.id_code,
            "country.legitimacy", c.legitimacy)) {
        return *err;
    }

    // ---- Pre-flight: compute every drift candidate and validate ----
    std::vector<PreparedDrift> prepared;
    for (auto& f : state.factions) {
        if (f.country != country) continue;

        // Validate per-faction inputs (drift formula reads loyalty +
        // support directly; either being non-finite would poison the
        // candidate even without overshoot).
        if (auto err = ng::require_unit_ratio<ReactionOutcome>(
                "faction::react", "faction", f.id_code,
                "faction.loyalty", f.loyalty)) {
            return *err;
        }
        if (auto err = ng::require_unit_ratio<ReactionOutcome>(
                "faction::react", "faction", f.id_code,
                "faction.support", f.support)) {
            return *err;
        }

        // Drift formulas (M1.6, unchanged):
        //   loyalty += (country.stability  - loyalty) * 0.10
        //   support += (country.legitimacy - support) * 0.05
        const double new_loyalty =
            f.loyalty + (c.stability - f.loyalty) * kLoyaltyDriftRate;
        const double new_support =
            f.support + (c.legitimacy - f.support) * kSupportDriftRate;

        // Candidate-validate-commit (post-M6.7 hardening): the previous
        // post-drift `std::clamp` is gone. With every input already
        // validated in `[0, 1]` and drift rates in `(0, 1)`, the math
        // is provably in `[0, 1]` by construction — but the explicit
        // check guards against future drift-rate changes and surfaces
        // any upstream bug that managed to slip past the input
        // validation above.
        if (auto err = ng::require_unit_ratio<ReactionOutcome>(
                "faction::react", "faction", f.id_code,
                "faction.loyalty (post-drift candidate)", new_loyalty)) {
            return *err;
        }
        if (auto err = ng::require_unit_ratio<ReactionOutcome>(
                "faction::react", "faction", f.id_code,
                "faction.support (post-drift candidate)", new_support)) {
            return *err;
        }

        prepared.push_back({&f, new_loyalty, new_support});
    }

    // ---- Commit ----------------------------------------------------
    ReactionOutcome outcome;
    for (auto& p : prepared) {
        p.faction->loyalty = p.new_loyalty;
        p.faction->support = p.new_support;
        ++outcome.factions_updated;
    }

    return core::Result<ReactionOutcome>::success(outcome);
}

}  // namespace leviathan::systems::faction
