// FactionDemands implementation — M7.1 (RFC-090 §7.1 +
// RFC-020 §7).
//
// See include/leviathan/systems/faction_demands.hpp for the
// public contract (lifecycle, faction-type allowlist, game-
// model coefficient grounding, deterministic non-RNG path).

#include "leviathan/systems/faction_demands.hpp"

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>

namespace leviathan::systems::faction_demands {

namespace {

// Closed enum-string allowlist. Adding a future kind here is an
// additive save bump; renaming an existing variant is a save
// schema break.
constexpr std::string_view kIncreaseMilitaryBudget          = "increase_military_budget";
constexpr std::string_view kExpandWelfare                   = "expand_welfare";
constexpr std::string_view kReligiousEducationAuthority     = "religious_education_authority";
constexpr std::string_view kLocalAutonomy                   = "local_autonomy";
constexpr std::string_view kTechnocratResearchFunding       = "technocrat_research_funding";
constexpr std::string_view kIntelligenceSurveillanceAuthority = "intelligence_surveillance_authority";

constexpr std::string_view kStatusPending = "pending";
constexpr std::string_view kStatusExpired = "expired";

// RFC-020 §7 faction-type allowlist. Strings match the existing
// `FactionState::type` convention (snake_case english labels
// already used by canonical faction JSONs).
constexpr std::string_view kFactionTypeMilitary       = "military";
constexpr std::string_view kFactionTypeWorkers        = "workers";
constexpr std::string_view kFactionTypeReligious      = "religious";
constexpr std::string_view kFactionTypeLocalElites    = "local_elites";
constexpr std::string_view kFactionTypeTechnicalElites = "technical_elites";
constexpr std::string_view kFactionTypeIntelligence   = "intelligence";

bool is_unit_ratio(double v) noexcept {
    return std::isfinite(v) && v >= 0.0 && v <= 1.0;
}

// Compute `created_on + kFactionDemandLifetimeDays` deterministically.
// GameDate::advance_days handles month / year rollover including
// leap years (see M0.2). Pure on a local copy; the caller's
// `created_on` is not mutated.
core::GameDate compute_expires_on(const core::GameDate& created_on) {
    core::GameDate d = created_on;
    d.advance_days(kFactionDemandLifetimeDays);
    return d;
}

// Build the deterministic id_code:
//   "{faction_id_code}_demand_{kind_snake}_{YYYY-MM-DD}"
std::string compose_demand_id_code(
        const std::string&             faction_id_code,
        core::FactionDemandKind        kind,
        const core::GameDate&          created_on) {
    return faction_id_code + "_demand_" +
           kind_to_string(kind) + "_" +
           created_on.to_string();
}

}  // namespace

std::string kind_to_string(core::FactionDemandKind kind) {
    switch (kind) {
        case core::FactionDemandKind::IncreaseMilitaryBudget:
            return std::string(kIncreaseMilitaryBudget);
        case core::FactionDemandKind::ExpandWelfare:
            return std::string(kExpandWelfare);
        case core::FactionDemandKind::ReligiousEducationAuthority:
            return std::string(kReligiousEducationAuthority);
        case core::FactionDemandKind::LocalAutonomy:
            return std::string(kLocalAutonomy);
        case core::FactionDemandKind::TechnocratResearchFunding:
            return std::string(kTechnocratResearchFunding);
        case core::FactionDemandKind::IntelligenceSurveillanceAuthority:
            return std::string(kIntelligenceSurveillanceAuthority);
    }
    // C++ switch exhaustiveness: unreachable for a well-formed
    // enum value. Emit an explicit sentinel to surface a future
    // mismatch loudly (e.g. a new variant added without updating
    // this switch).
    return "<unknown_faction_demand_kind>";
}

core::Result<core::FactionDemandKind> kind_from_string(std::string_view s) {
    if (s == kIncreaseMilitaryBudget)          return core::Result<core::FactionDemandKind>::success(core::FactionDemandKind::IncreaseMilitaryBudget);
    if (s == kExpandWelfare)                   return core::Result<core::FactionDemandKind>::success(core::FactionDemandKind::ExpandWelfare);
    if (s == kReligiousEducationAuthority)     return core::Result<core::FactionDemandKind>::success(core::FactionDemandKind::ReligiousEducationAuthority);
    if (s == kLocalAutonomy)                   return core::Result<core::FactionDemandKind>::success(core::FactionDemandKind::LocalAutonomy);
    if (s == kTechnocratResearchFunding)       return core::Result<core::FactionDemandKind>::success(core::FactionDemandKind::TechnocratResearchFunding);
    if (s == kIntelligenceSurveillanceAuthority) return core::Result<core::FactionDemandKind>::success(core::FactionDemandKind::IntelligenceSurveillanceAuthority);
    return core::Result<core::FactionDemandKind>::failure(
        std::string("faction_demands::kind_from_string: unknown kind '") +
        std::string(s) + "'");
}

std::string status_to_string(core::FactionDemandStatus status) {
    switch (status) {
        case core::FactionDemandStatus::Pending: return std::string(kStatusPending);
        case core::FactionDemandStatus::Expired: return std::string(kStatusExpired);
    }
    return "<unknown_faction_demand_status>";
}

core::Result<core::FactionDemandStatus> status_from_string(std::string_view s) {
    if (s == kStatusPending) return core::Result<core::FactionDemandStatus>::success(core::FactionDemandStatus::Pending);
    if (s == kStatusExpired) return core::Result<core::FactionDemandStatus>::success(core::FactionDemandStatus::Expired);
    return core::Result<core::FactionDemandStatus>::failure(
        std::string("faction_demands::status_from_string: unknown status '") +
        std::string(s) + "'");
}

core::Result<FactionTypeMappingResult> map_faction_type_to_demand_kind(
        std::string_view faction_type) {
    FactionTypeMappingResult out;
    if (faction_type == kFactionTypeMilitary) {
        out.has_kind = true;
        out.kind     = core::FactionDemandKind::IncreaseMilitaryBudget;
    } else if (faction_type == kFactionTypeWorkers) {
        out.has_kind = true;
        out.kind     = core::FactionDemandKind::ExpandWelfare;
    } else if (faction_type == kFactionTypeReligious) {
        out.has_kind = true;
        out.kind     = core::FactionDemandKind::ReligiousEducationAuthority;
    } else if (faction_type == kFactionTypeLocalElites) {
        out.has_kind = true;
        out.kind     = core::FactionDemandKind::LocalAutonomy;
    } else if (faction_type == kFactionTypeTechnicalElites) {
        out.has_kind = true;
        out.kind     = core::FactionDemandKind::TechnocratResearchFunding;
    } else if (faction_type == kFactionTypeIntelligence) {
        out.has_kind = true;
        out.kind     = core::FactionDemandKind::IntelligenceSurveillanceAuthority;
    }
    // Out-of-list faction types (bureaucracy / media / students /
    // farmers / nationalists / aristocracy / etc.) succeed with
    // has_kind = false. RFC-020 §7 does not enumerate them, so
    // M7.1 deliberately produces no demand kind. A future
    // sub-milestone that needs additional kinds widens both the
    // FactionDemandKind enum and this switch in lock-step.
    return core::Result<FactionTypeMappingResult>::success(out);
}

core::Result<GenerateOutcome> tick_generate(
        core::GameState&        state,
        const core::GameDate&   current_date) {
    GenerateOutcome out;

    if (!current_date.is_valid()) {
        return core::Result<GenerateOutcome>::failure(
            std::string("faction_demands::tick_generate: "
                        "current_date '") +
            current_date.to_string() +
            "' is not a valid calendar date");
    }

    out.factions_considered = static_cast<int>(state.factions.size());

    // ---- Pass 1: collect (faction_id_code, kind) pairs that
    //              already have a Pending demand. -------------------
    std::unordered_set<std::string> pending_pairs;
    for (const auto& d : state.faction_demands) {
        if (d.status == core::FactionDemandStatus::Pending) {
            pending_pairs.insert(
                d.faction_id_code + "|" + kind_to_string(d.kind));
        }
    }

    // ---- Pass 2: walk factions; for each candidate
    //              (type in RFC-020 §7 allowlist AND
    //               radicalism > threshold AND
    //               no matching Pending demand already), validate
    //              the faction's invariants BEFORE appending.
    //
    // Pre-flight only validates factions that WOULD generate a
    // demand: per `feedback_no_silent_degradation`, a malformed
    // candidate must fail loudly, but factions outside the
    // RFC-020 §7 allowlist (e.g. type "bureaucracy" / "media" /
    // "students" / "farmers") are not §7 actors and their
    // pre-existing field state is none of M7.1's concern. The
    // M1.6 `faction::react` contract owns the per-tick
    // numerical validation of every faction's `support`,
    // `loyalty`, `radicalism`, and `influence`; M7.1 layers an
    // ADDITIONAL semantic check (id_code / country_id_code /
    // country-resolution) only on the candidates it would
    // actually mutate.
    //
    // Insertion order = state.factions order (deterministic).
    for (const auto& f : state.factions) {
        auto map_r = map_faction_type_to_demand_kind(f.type);
        if (!map_r) {
            return core::Result<GenerateOutcome>::failure(
                std::move(map_r.error()));
        }
        if (!map_r.value().has_kind) {
            continue;  // type outside RFC-020 §7 allowlist
        }
        // Candidate: the type matches §7. We MUST validate
        // radicalism is a finite ratio before comparing it to
        // the threshold (a NaN would silently pass the `>` test
        // depending on the implementation; reject loudly).
        if (!is_unit_ratio(f.radicalism)) {
            return core::Result<GenerateOutcome>::failure(
                "faction_demands::tick_generate: faction '" +
                f.id_code + "' (type '" + f.type +
                "') radicalism = " +
                std::to_string(f.radicalism) +
                " is not a finite ratio in [0, 1]");
        }
        if (f.radicalism <= kFactionDemandGenerateRadicalismThreshold) {
            continue;
        }
        // Below this point we will append a demand for this
        // faction. The remaining invariants must hold.
        if (f.id_code.empty()) {
            return core::Result<GenerateOutcome>::failure(
                "faction_demands::tick_generate: a faction with "
                "type '" + f.type +
                "' qualifies for a demand but has empty id_code");
        }
        if (f.country_id_code.empty()) {
            return core::Result<GenerateOutcome>::failure(
                "faction_demands::tick_generate: faction '" +
                f.id_code + "' has empty country_id_code");
        }
        bool found_country = false;
        for (const auto& c : state.countries) {
            if (c.id_code == f.country_id_code) {
                found_country = true;
                break;
            }
        }
        if (!found_country) {
            return core::Result<GenerateOutcome>::failure(
                "faction_demands::tick_generate: faction '" +
                f.id_code + "' country_id_code '" +
                f.country_id_code +
                "' does not match any entry in state.countries");
        }
        if (!is_unit_ratio(f.loyalty)) {
            return core::Result<GenerateOutcome>::failure(
                "faction_demands::tick_generate: faction '" +
                f.id_code + "' loyalty = " +
                std::to_string(f.loyalty) +
                " is not a finite ratio in [0, 1]");
        }

        const core::FactionDemandKind kind = map_r.value().kind;
        const std::string pair_key =
            f.id_code + "|" + kind_to_string(kind);
        if (pending_pairs.find(pair_key) != pending_pairs.end()) {
            continue;
        }

        core::FactionDemand demand;
        demand.faction_id_code = f.id_code;
        demand.country_id_code = f.country_id_code;
        demand.kind            = kind;
        demand.created_on      = current_date;
        demand.expires_on      = compute_expires_on(current_date);
        demand.status          = core::FactionDemandStatus::Pending;
        demand.id_code         = compose_demand_id_code(
            f.id_code, kind, current_date);

        state.faction_demands.push_back(std::move(demand));
        pending_pairs.insert(pair_key);
        ++out.demands_generated;
    }

    return core::Result<GenerateOutcome>::success(out);
}

core::Result<ExpireOutcome> tick_expire_and_apply(
        core::GameState&        state,
        const core::GameDate&   current_date) {
    ExpireOutcome out;

    if (!current_date.is_valid()) {
        return core::Result<ExpireOutcome>::failure(
            std::string("faction_demands::tick_expire_and_apply: "
                        "current_date '") +
            current_date.to_string() +
            "' is not a valid calendar date");
    }

    // ---- Pass 1: identify expiring demands + validate ---------------
    struct Pending {
        std::size_t demand_index;
        std::size_t faction_index;
    };
    std::vector<Pending> expiring;
    for (std::size_t i = 0; i < state.faction_demands.size(); ++i) {
        const auto& d = state.faction_demands[i];
        if (d.status != core::FactionDemandStatus::Pending) {
            continue;
        }
        if (!(current_date >= d.expires_on)) {
            continue;
        }
        // Resolve faction.
        std::size_t faction_index = state.factions.size();
        for (std::size_t fi = 0; fi < state.factions.size(); ++fi) {
            if (state.factions[fi].id_code == d.faction_id_code) {
                faction_index = fi;
                break;
            }
        }
        if (faction_index >= state.factions.size()) {
            return core::Result<ExpireOutcome>::failure(
                "faction_demands::tick_expire_and_apply: demand '" +
                d.id_code + "' faction_id_code '" +
                d.faction_id_code +
                "' does not match any entry in state.factions");
        }
        const auto& f = state.factions[faction_index];
        if (!is_unit_ratio(f.radicalism)) {
            return core::Result<ExpireOutcome>::failure(
                "faction_demands::tick_expire_and_apply: faction '" +
                f.id_code + "' radicalism = " +
                std::to_string(f.radicalism) +
                " is not a finite ratio in [0, 1]");
        }
        if (!is_unit_ratio(f.loyalty)) {
            return core::Result<ExpireOutcome>::failure(
                "faction_demands::tick_expire_and_apply: faction '" +
                f.id_code + "' loyalty = " +
                std::to_string(f.loyalty) +
                " is not a finite ratio in [0, 1]");
        }
        // Pre-compute the drift candidates (asymptotic form) and
        // verify they land in [0, 1]. Per the post-PR #115
        // hardening rule, the asymptotic form is the standard
        // for ratio fields:
        //   add(value, delta)      = value + delta × (1 - value)
        //   subtract(value, delta) = value - delta × value
        // For delta ∈ [0, 1] and value ∈ [0, 1], both candidates
        // land in [0, 1] by algebra. The check below is the
        // belt-and-braces guard.
        const double rad_candidate =
            f.radicalism +
            kFactionDemandExpireRadicalismAsymptoticDelta *
                (1.0 - f.radicalism);
        const double loy_candidate =
            f.loyalty -
            kFactionDemandExpireLoyaltyAsymptoticDelta *
                f.loyalty;
        if (!is_unit_ratio(rad_candidate)) {
            return core::Result<ExpireOutcome>::failure(
                "faction_demands::tick_expire_and_apply: faction '" +
                f.id_code + "' post-expire radicalism candidate = " +
                std::to_string(rad_candidate) +
                " escaped [0, 1]");
        }
        if (!is_unit_ratio(loy_candidate)) {
            return core::Result<ExpireOutcome>::failure(
                "faction_demands::tick_expire_and_apply: faction '" +
                f.id_code + "' post-expire loyalty candidate = " +
                std::to_string(loy_candidate) +
                " escaped [0, 1]");
        }
        expiring.push_back(Pending{i, faction_index});
    }

    // ---- Pass 2: commit. Status flip + drift in one pass. ----------
    std::unordered_set<std::size_t> affected_factions;
    for (const auto& p : expiring) {
        auto& d = state.faction_demands[p.demand_index];
        d.status = core::FactionDemandStatus::Expired;

        auto& f = state.factions[p.faction_index];
        f.radicalism = f.radicalism +
                       kFactionDemandExpireRadicalismAsymptoticDelta *
                           (1.0 - f.radicalism);
        f.loyalty    = f.loyalty -
                       kFactionDemandExpireLoyaltyAsymptoticDelta *
                           f.loyalty;
        affected_factions.insert(p.faction_index);
    }

    out.demands_expired   = static_cast<int>(expiring.size());
    out.factions_affected = static_cast<int>(affected_factions.size());
    return core::Result<ExpireOutcome>::success(out);
}

}  // namespace leviathan::systems::faction_demands
