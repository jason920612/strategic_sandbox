// FactionConflict implementation — M7.4 (RFC-090 §7.4 +
// RFC-020 §8).
//
// See include/leviathan/systems/faction_conflict.hpp for the
// public contract (rivalry pair allowlist, coefficient
// grounding, deterministic non-RNG path).

#include "leviathan/systems/faction_conflict.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace leviathan::systems::faction_conflict {

namespace {

bool is_unit_ratio(double v) noexcept {
    return std::isfinite(v) && v >= 0.0 && v <= 1.0;
}

// RFC-020 §8 rivalry pairs (symmetric — the order within a
// pair is arbitrary, the algorithm scans both sides). Five
// pairs, intelligence appears in two of them (against
// military, against media). Faction type strings match the
// existing FactionState::type convention (snake_case english
// labels already used by canonical faction JSONs).
struct RivalryPair {
    std::string_view a;
    std::string_view b;
};

constexpr std::array<RivalryPair, 5> kRivalryPairs = {{
    {"military",   "intelligence"},
    {"workers",    "technical_elites"},
    {"bureaucracy", "local_elites"},
    {"students",   "religious"},
    {"media",      "intelligence"},
}};

}  // namespace

core::Result<PressureOutcome>
tick_apply_pressure(core::GameState& state) {
    using R = core::Result<PressureOutcome>;
    PressureOutcome out;

    // ---- Pass 1: per-country, per-pair, collect candidates --------
    // For each country and each rivalry pair, gather the
    // indices of factions on each side whose influence
    // strictly exceeds the threshold. Validate each
    // candidate's radicalism / loyalty + the asymptotic drift
    // candidate BEFORE any mutation.
    struct Drift {
        std::size_t faction_index;
        double      new_radicalism;
    };
    std::vector<Drift> drifts;
    std::unordered_set<std::size_t> drifted_indices_global;

    for (std::size_t ci = 0; ci < state.countries.size(); ++ci) {
        const auto& c = state.countries[ci];
        for (const auto& pair : kRivalryPairs) {
            // Side A factions in this country with influence > threshold.
            std::vector<std::size_t> side_a;
            std::vector<std::size_t> side_b;
            for (std::size_t fi = 0; fi < state.factions.size(); ++fi) {
                const auto& f = state.factions[fi];
                if (f.country_id_code != c.id_code) {
                    continue;
                }
                if (!is_unit_ratio(f.influence)) {
                    // Even before checking type — an out-of-range
                    // influence on any candidate-eligible faction
                    // (i.e. one in this country) is a runtime
                    // invariant violation that M1.6 / save layer
                    // should have caught. Reject loudly.
                    return R::failure(
                        "faction_conflict::tick_apply_pressure: "
                        "faction '" + f.id_code + "' influence = " +
                        std::to_string(f.influence) +
                        " is not a finite ratio in [0, 1]");
                }
                if (f.influence <= kFactionConflictInfluenceThreshold) {
                    continue;
                }
                if (f.type == pair.a) {
                    side_a.push_back(fi);
                } else if (f.type == pair.b) {
                    side_b.push_back(fi);
                }
            }
            if (side_a.empty() || side_b.empty()) {
                continue;  // dormant rivalry on this (country, pair)
            }
            ++out.pairs_active;

            // Active pair: validate + plan drift for every
            // participating faction.
            auto plan_drift = [&](std::size_t fi) -> core::Result<bool> {
                const auto& f = state.factions[fi];
                if (!is_unit_ratio(f.radicalism)) {
                    return core::Result<bool>::failure(
                        "faction_conflict::tick_apply_pressure: "
                        "faction '" + f.id_code + "' radicalism = " +
                        std::to_string(f.radicalism) +
                        " is not a finite ratio in [0, 1]");
                }
                if (!is_unit_ratio(f.loyalty)) {
                    return core::Result<bool>::failure(
                        "faction_conflict::tick_apply_pressure: "
                        "faction '" + f.id_code + "' loyalty = " +
                        std::to_string(f.loyalty) +
                        " is not a finite ratio in [0, 1]");
                }
                const double candidate =
                    f.radicalism +
                    kFactionConflictAsymptoticRadicalismDelta *
                        (1.0 - f.radicalism);
                if (!is_unit_ratio(candidate)) {
                    return core::Result<bool>::failure(
                        "faction_conflict::tick_apply_pressure: "
                        "faction '" + f.id_code +
                        "' post-conflict radicalism candidate = " +
                        std::to_string(candidate) +
                        " escaped [0, 1]");
                }
                drifts.push_back(Drift{fi, candidate});
                drifted_indices_global.insert(fi);
                return core::Result<bool>::success(true);
            };
            for (std::size_t fi : side_a) {
                auto r = plan_drift(fi);
                if (!r) {
                    return R::failure(std::move(r.error()));
                }
            }
            for (std::size_t fi : side_b) {
                auto r = plan_drift(fi);
                if (!r) {
                    return R::failure(std::move(r.error()));
                }
            }
        }
    }

    // ---- Pass 2: commit. Apply every planned drift. --------------
    // Multiple drifts may target the same faction in this
    // tick (intelligence is in two §8 pairs — both can fire on
    // a single faction). Drifts compound asymptotically; the
    // order is insertion order, which is deterministic from
    // the pass-1 walk.
    for (const auto& d : drifts) {
        auto& f = state.factions[d.faction_index];
        // The planned new value was validated against the
        // CURRENT radicalism at the time of planning. After
        // an earlier drift mutated this faction's radicalism,
        // the second-and-later drifts are computed from the
        // updated value to keep the math monotonic-and-
        // asymptotic.
        const double current = f.radicalism;
        const double candidate =
            current +
            kFactionConflictAsymptoticRadicalismDelta *
                (1.0 - current);
        if (!is_unit_ratio(candidate)) {
            return R::failure(
                "faction_conflict::tick_apply_pressure: "
                "faction '" + f.id_code +
                "' compounded drift escaped [0, 1] "
                "(current=" + std::to_string(current) +
                " candidate=" + std::to_string(candidate) + ")");
        }
        f.radicalism = candidate;
    }

    out.factions_drifted = static_cast<int>(drifted_indices_global.size());
    return R::success(out);
}

}  // namespace leviathan::systems::faction_conflict
