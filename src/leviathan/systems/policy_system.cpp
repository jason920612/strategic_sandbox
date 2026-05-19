#include "leviathan/systems/policy_system.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace leviathan::systems::policy {

namespace {

// One resolved target. The two pointer alternatives are exclusive.
// `country_field` points into a CountryState; `faction_fields` is
// non-empty when the target was a faction:* broadcast.
struct ResolvedTarget {
    // For country.* and country.budget.* targets.
    double* country_field = nullptr;
    bool    country_is_ratio = false;

    // For faction:<type>.<field> targets.
    std::vector<double*> faction_fields;
    // Parallel to faction_fields — captures each matched faction's
    // id_code so per-faction error messages can name the entity that
    // produced the overshoot candidate.
    std::vector<std::string> faction_id_codes;
    bool                     faction_is_ratio = false;
};

// One concrete write that pre-flight has cleared. Built during the
// candidate phase; consumed by the commit phase. Per the post-M6.7
// hardening sweep, the candidate value is computed AND validated
// before any mutation happens, so the commit phase can never fail.
struct PreparedWrite {
    double* field_ptr = nullptr;
    double  candidate = 0.0;
};

// Returns a pointer into the given CountryState for the named field,
// or nullptr if the field is unknown. The bool out-param is set true
// when the field is a [0, 1] ratio (so the caller can range-check the
// candidate).
double* country_field_ptr(core::CountryState& c,
                          std::string_view field,
                          bool& is_ratio) {
    is_ratio = true;

    if (field == "legal_tax_burden")          return &c.legal_tax_burden;
    if (field == "fiscal_capacity")           return &c.fiscal_capacity;
    if (field == "administrative_efficiency") return &c.administrative_efficiency;
    if (field == "central_control")           return &c.central_control;
    if (field == "corruption")                return &c.corruption;
    if (field == "stability")                 return &c.stability;
    if (field == "legitimacy")                return &c.legitimacy;
    if (field == "military_power")            return &c.military_power;
    if (field == "threat_perception")         return &c.threat_perception;

    is_ratio = false;
    if (field == "gdp")            return &c.gdp;
    if (field == "tax_revenue")    return &c.tax_revenue;
    if (field == "budget_balance") return &c.budget_balance;

    return nullptr;
}

double* country_budget_field_ptr(core::CountryState& c,
                                 std::string_view category) {
    if (category == "administration")  return &c.budget.administration;
    if (category == "military")        return &c.budget.military;
    if (category == "education")       return &c.budget.education;
    if (category == "welfare")         return &c.budget.welfare;
    if (category == "intelligence")    return &c.budget.intelligence;
    if (category == "infrastructure")  return &c.budget.infrastructure;
    if (category == "industry")        return &c.budget.industry;
    return nullptr;
}

double* faction_field_ptr(core::FactionState& f,
                          std::string_view field,
                          bool& is_ratio) {
    is_ratio = true;
    if (field == "support")    return &f.support;
    if (field == "influence")  return &f.influence;
    if (field == "radicalism") return &f.radicalism;
    if (field == "loyalty")    return &f.loyalty;

    is_ratio = false;
    if (field == "resources")  return &f.resources;

    return nullptr;
}

bool split_faction_target(std::string_view target,
                          std::string_view& out_type,
                          std::string_view& out_field) {
    constexpr std::string_view kPrefix = "faction:";
    if (target.substr(0, kPrefix.size()) != kPrefix) return false;
    const auto rest = target.substr(kPrefix.size());
    const auto dot = rest.find('.');
    if (dot == std::string_view::npos) return false;
    out_type  = rest.substr(0, dot);
    out_field = rest.substr(dot + 1);
    if (out_type.empty() || out_field.empty()) return false;
    return true;
}

// Resolve a single effect target. The function mutates only the
// `out` argument; state itself is not touched.
core::Result<bool> resolve_target(core::GameState& state,
                                  core::CountryId actor,
                                  std::string_view target,
                                  ResolvedTarget& out,
                                  std::size_t effect_index) {
    const std::string ctx = "effects[" + std::to_string(effect_index) + "]";

    auto& country = state.countries[static_cast<std::size_t>(actor.value())];

    // country.budget.<cat>
    constexpr std::string_view kCountryBudget = "country.budget.";
    if (target.substr(0, kCountryBudget.size()) == kCountryBudget) {
        const auto cat = target.substr(kCountryBudget.size());
        double* p = country_budget_field_ptr(country, cat);
        if (p == nullptr) {
            return core::Result<bool>::failure(
                ctx + ": unknown budget category '" + std::string(cat) +
                "' in target '" + std::string(target) + "'");
        }
        out.country_field    = p;
        out.country_is_ratio = true;
        return core::Result<bool>::success(true);
    }

    // country.<field>
    constexpr std::string_view kCountry = "country.";
    if (target.substr(0, kCountry.size()) == kCountry) {
        const auto field = target.substr(kCountry.size());
        bool is_ratio = false;
        double* p = country_field_ptr(country, field, is_ratio);
        if (p == nullptr) {
            return core::Result<bool>::failure(
                ctx + ": unknown country field '" + std::string(field) +
                "' in target '" + std::string(target) + "'");
        }
        out.country_field    = p;
        out.country_is_ratio = is_ratio;
        return core::Result<bool>::success(true);
    }

    // faction:<type>.<field>
    std::string_view type;
    std::string_view field;
    if (split_faction_target(target, type, field)) {
        bool field_known = false;
        bool is_ratio    = false;
        for (auto& f : state.factions) {
            if (f.country != actor) continue;
            if (f.type != type)     continue;
            double* p = faction_field_ptr(f, field, is_ratio);
            if (p == nullptr) {
                return core::Result<bool>::failure(
                    ctx + ": unknown faction field '" + std::string(field) +
                    "' in target '" + std::string(target) + "'");
            }
            out.faction_fields.push_back(p);
            out.faction_id_codes.push_back(f.id_code);
            field_known = true;
        }
        if (!field_known) {
            core::FactionState probe;
            if (faction_field_ptr(probe, field, is_ratio) == nullptr) {
                return core::Result<bool>::failure(
                    ctx + ": unknown faction field '" + std::string(field) +
                    "' in target '" + std::string(target) + "'");
            }
            out.faction_is_ratio = is_ratio;
            return core::Result<bool>::success(true);
        }
        out.faction_is_ratio = is_ratio;
        return core::Result<bool>::success(true);
    }

    return core::Result<bool>::failure(
        ctx + ": unrecognised target syntax '" + std::string(target) +
        "' (expected 'country.<field>', 'country.budget.<category>', "
        "or 'faction:<type>.<field>')");
}

bool op_recognised(std::string_view op) {
    return op == "add" || op == "set";
}

// Compute the candidate value the op would produce. `e.value` is
// already validated finite by the caller. `is_ratio` selects the
// formula:
//
//   - `set`: candidate = delta (direct write; strict-validates).
//
//   - `add` on a NON-RATIO field: candidate = old + delta
//     (linear; gdp / budget_balance / resources allow large signed
//     swings).
//
//   - `add` on a RATIO field: ASYMPTOTIC formula
//
//       positive delta: new = old + delta * (1 - old)
//       negative delta: new = old + delta * old
//
//     Logistic-style update that converges toward the range
//     bounds rather than crossing them. Research grounding:
//     Polity / V-Dem institutional indicators are bounded scales
//     where successive shocks produce diminishing returns near
//     the bounds (Marshall & Jaggers 2002 Polity IV; Coppedge et
//     al. 2011 V-Dem). Besley & Persson 2009 state-capacity model
//     treats capacity as a slow-changing stock with proportionally
//     smaller responses near the asymptote. Replacing the previous
//     linear `old + delta` with this form makes the strict
//     `feedback_no_silent_degradation` validator pass by
//     construction on ratio fields — long-horizon AI policy
//     application can't saturate past 0 or 1.
//
//     Effect-shape consequence for authors: `+0.05` on a 0.50
//     ratio lands at 0.525 (vs the pre-PR linear 0.55); the same
//     `+0.05` on a 0.10 ratio lands at 0.145 (close to linear far
//     from the bound). The asymptotic shape is most visible near
//     the bounds.
double compute_candidate(double old_value, std::string_view op,
                         double delta, bool is_ratio) {
    if (op == "set") return delta;
    if (!is_ratio) return old_value + delta;
    if (delta >= 0.0) return old_value + delta * (1.0 - old_value);
    return old_value + delta * old_value;
}

// Validate a candidate against its target's range. Returns nullopt
// on success; a populated failure string on violation. Caller wraps
// in their own Result<T>.
std::optional<std::string> validate_candidate(
        std::size_t effect_index,
        std::string_view target,
        const std::string& entity_label,  // "" for country targets;
                                          // "faction '<id_code>'" otherwise
        bool is_ratio,
        double candidate) {
    const std::string ctx =
        "effects[" + std::to_string(effect_index) + "] (target '" +
        std::string(target) + "'" +
        (entity_label.empty() ? "" : " on " + entity_label) + ")";

    if (!std::isfinite(candidate)) {
        return ctx + ": candidate value " + std::to_string(candidate) +
            " is not finite";
    }
    if (is_ratio && (candidate < 0.0 || candidate > 1.0)) {
        return ctx + ": candidate value " + std::to_string(candidate) +
            " escapes ratio range [0, 1]";
    }
    return std::nullopt;
}

}  // namespace

core::Result<ApplyOutcome> apply_effects_to_actor(
        core::GameState&                       state,
        core::CountryId                        actor,
        const std::vector<core::PolicyEffect>& effects) {
    // Validate the actor first - this is a precondition that doesn't
    // depend on any effect content.
    if (!actor.valid() ||
        actor.value() < 0 ||
        static_cast<std::size_t>(actor.value()) >= state.countries.size()) {
        return core::Result<ApplyOutcome>::failure(
            "apply_effects_to_actor: actor CountryId " +
            std::to_string(actor.value()) +
            " is not a valid index into state.countries");
    }

    // ---- Pre-flight phase 1: resolve every effect's target / op ----
    std::vector<ResolvedTarget> resolved;
    resolved.reserve(effects.size());

    for (std::size_t i = 0; i < effects.size(); ++i) {
        const auto& e = effects[i];

        // PR #16 review: reject NaN / Inf at pre-flight so a manually
        // constructed PolicyData / synthesised effect list can't slip
        // a non-finite value past the DataLoader (which already
        // rejects them) and corrupt state at apply time.
        if (!std::isfinite(e.value)) {
            return core::Result<ApplyOutcome>::failure(
                "effects[" + std::to_string(i) +
                "]: value is not finite");
        }

        if (!op_recognised(e.op)) {
            return core::Result<ApplyOutcome>::failure(
                "effects[" + std::to_string(i) + "]: unrecognised op '" +
                e.op + "' (expected 'add' or 'set')");
        }

        ResolvedTarget rt;
        auto r = resolve_target(state, actor, e.target, rt, i);
        if (!r) {
            return core::Result<ApplyOutcome>::failure(std::move(r.error()));
        }
        resolved.push_back(std::move(rt));
    }

    // ---- Pre-flight phase 2: compute candidates and validate -------
    // Post-M6.7 hardening (`feedback_no_silent_degradation`): the
    // previous `std::clamp(*dst, 0.0, 1.0)` post-op safety net is
    // gone. Each candidate is computed against the CURRENT value of
    // the target and validated BEFORE any state mutation. Any
    // out-of-range or non-finite candidate fails the whole call.
    std::vector<PreparedWrite> writes;
    // Reserve a conservative upper bound (country writes contribute
    // one entry; faction broadcasts contribute up to N).
    writes.reserve(effects.size());

    for (std::size_t i = 0; i < effects.size(); ++i) {
        const auto& e  = effects[i];
        const auto& rt = resolved[i];

        if (rt.country_field != nullptr) {
            const double candidate =
                compute_candidate(*rt.country_field, e.op, e.value,
                                  rt.country_is_ratio);
            if (auto err = validate_candidate(
                    i, e.target, /*entity_label=*/"",
                    rt.country_is_ratio, candidate)) {
                return core::Result<ApplyOutcome>::failure(std::move(*err));
            }
            writes.push_back({rt.country_field, candidate});
        } else {
            for (std::size_t k = 0; k < rt.faction_fields.size(); ++k) {
                double* p = rt.faction_fields[k];
                const std::string& id_code = rt.faction_id_codes[k];
                const double candidate =
                    compute_candidate(*p, e.op, e.value,
                                      rt.faction_is_ratio);
                if (auto err = validate_candidate(
                        i, e.target,
                        /*entity_label=*/"faction '" + id_code + "'",
                        rt.faction_is_ratio, candidate)) {
                    return core::Result<ApplyOutcome>::failure(std::move(*err));
                }
                writes.push_back({p, candidate});
            }
        }
    }

    // ---- Commit: every candidate has been validated ----------------
    ApplyOutcome outcome;
    // Walk effects again to bookkeep the per-effect outcome counters;
    // walk writes in parallel using a manual cursor since faction
    // broadcasts contribute multiple writes per effect.
    std::size_t write_cursor = 0;
    for (std::size_t i = 0; i < effects.size(); ++i) {
        const auto& rt = resolved[i];

        if (rt.country_field != nullptr) {
            *writes[write_cursor].field_ptr = writes[write_cursor].candidate;
            ++write_cursor;
        } else {
            for (std::size_t k = 0; k < rt.faction_fields.size(); ++k) {
                *writes[write_cursor].field_ptr =
                    writes[write_cursor].candidate;
                ++write_cursor;
            }
            outcome.faction_targets_updated +=
                static_cast<int>(rt.faction_fields.size());
        }
        ++outcome.effects_applied;
    }

    return core::Result<ApplyOutcome>::success(std::move(outcome));
}

core::Result<ApplyOutcome> apply_policy_effects(
        core::GameState& state,
        core::CountryId  actor,
        const core::PolicyData& policy) {
    // M1.15: bound-check duration_days BEFORE any pre-flight or apply.
    // GameDate::advance_days is a per-day loop, so an unbounded
    // duration would stall the call. DataLoader admits anything up to
    // INT_MAX (it can't depend on this module without inverting the
    // layering), so PolicySystem is the last line of defense.
    if (policy.duration_days < 0) {
        return core::Result<ApplyOutcome>::failure(
            "apply_policy_effects: policy.duration_days must be >= 0 (got " +
            std::to_string(policy.duration_days) + ")");
    }
    if (policy.duration_days > kMaxTrackedPolicyDurationDays) {
        return core::Result<ApplyOutcome>::failure(
            "apply_policy_effects: policy.duration_days " +
            std::to_string(policy.duration_days) +
            " exceeds kMaxTrackedPolicyDurationDays (" +
            std::to_string(kMaxTrackedPolicyDurationDays) + ")");
    }

    // ---- Delegate effect-application to the M5.6-extracted helper.
    auto apply_r = apply_effects_to_actor(state, actor, policy.effects);
    if (!apply_r) {
        return core::Result<ApplyOutcome>::failure(std::move(apply_r.error()));
    }

    // ---- M1.15: record this enactment as an active policy --------
    // Reached only after pre-flight passed AND every effect applied.
    // expires_on = state.current_date + policy.duration_days.
    core::GameDate expires_on = state.current_date;
    expires_on.advance_days(policy.duration_days);
    auto& country_ref =
        state.countries[static_cast<std::size_t>(actor.value())];
    country_ref.active_policies.push_back(
        core::ActivePolicy{policy.id_code, expires_on});

    return core::Result<ApplyOutcome>::success(std::move(apply_r).value());
}

}  // namespace leviathan::systems::policy
