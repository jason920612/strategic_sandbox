#include "leviathan/systems/policy_system.hpp"

#include <algorithm>
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
    bool                 faction_is_ratio = false;
};

// Returns a pointer into the given CountryState for the named field,
// or nullptr if the field is unknown. The bool out-param is set true
// when the field is a [0, 1] ratio (so the caller can clamp).
double* country_field_ptr(core::CountryState& c,
                          std::string_view field,
                          bool& is_ratio) {
    is_ratio = true;

    // Top-level CountryState numeric fields.
    if (field == "legal_tax_burden")          return &c.legal_tax_burden;
    if (field == "fiscal_capacity")           return &c.fiscal_capacity;
    if (field == "administrative_efficiency") return &c.administrative_efficiency;
    if (field == "central_control")           return &c.central_control;
    if (field == "corruption")                return &c.corruption;
    if (field == "stability")                 return &c.stability;
    if (field == "legitimacy")                return &c.legitimacy;
    if (field == "military_power")            return &c.military_power;
    if (field == "threat_perception")         return &c.threat_perception;

    // Absolute (non-ratio) fields.
    is_ratio = false;
    if (field == "gdp")            return &c.gdp;
    if (field == "tax_revenue")    return &c.tax_revenue;
    if (field == "budget_balance") return &c.budget_balance;

    return nullptr;
}

double* country_budget_field_ptr(core::CountryState& c,
                                 std::string_view category) {
    // All budget categories are [0, 1] ratios.
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

// Splits "faction:military.support" into ("military", "support").
// Returns false if the target doesn't have the expected shape.
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
                // First matching faction confirms the field is unknown
                // for THIS faction shape; fail fast.
                return core::Result<bool>::failure(
                    ctx + ": unknown faction field '" + std::string(field) +
                    "' in target '" + std::string(target) + "'");
            }
            out.faction_fields.push_back(p);
            field_known = true;
        }
        if (!field_known) {
            // No factions of this type belong to the actor. We don't
            // know yet whether `field` is valid - validate against a
            // dummy FactionState so a typo is still caught at
            // pre-flight time.
            core::FactionState probe;
            if (faction_field_ptr(probe, field, is_ratio) == nullptr) {
                return core::Result<bool>::failure(
                    ctx + ": unknown faction field '" + std::string(field) +
                    "' in target '" + std::string(target) + "'");
            }
            // Valid field, just zero matching factions. The effect
            // will be a silent no-op at apply time.
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

void apply_op(double* dst, std::string_view op, double value, bool is_ratio) {
    if (op == "add") {
        *dst += value;
    } else {  // "set"
        *dst = value;
    }
    if (is_ratio) {
        *dst = std::clamp(*dst, 0.0, 1.0);
    }
}

}  // namespace

core::Result<ApplyOutcome> apply_policy_effects(
        core::GameState& state,
        core::CountryId  actor,
        const core::PolicyData& policy) {
    // Validate the actor first - this is a precondition that doesn't
    // depend on any effect content.
    if (!actor.valid() ||
        actor.value() < 0 ||
        static_cast<std::size_t>(actor.value()) >= state.countries.size()) {
        return core::Result<ApplyOutcome>::failure(
            "apply_policy_effects: actor CountryId " +
            std::to_string(actor.value()) +
            " is not a valid index into state.countries");
    }

    // ---- Pre-flight: resolve every effect's target / op ----------
    std::vector<ResolvedTarget> resolved;
    resolved.reserve(policy.effects.size());

    for (std::size_t i = 0; i < policy.effects.size(); ++i) {
        const auto& e = policy.effects[i];

        // PR #16 review: reject NaN / Inf at pre-flight so a manually
        // constructed PolicyData can't slip a non-finite value past
        // the DataLoader (which already rejects them) and corrupt
        // state at apply time.
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

    // ---- Apply ---------------------------------------------------
    ApplyOutcome outcome;
    for (std::size_t i = 0; i < policy.effects.size(); ++i) {
        const auto& e  = policy.effects[i];
        auto& rt       = resolved[i];

        if (rt.country_field != nullptr) {
            apply_op(rt.country_field, e.op, e.value, rt.country_is_ratio);
        } else {
            // faction broadcast - may be empty (no-op)
            for (double* p : rt.faction_fields) {
                apply_op(p, e.op, e.value, rt.faction_is_ratio);
            }
            outcome.faction_targets_updated +=
                static_cast<int>(rt.faction_fields.size());
        }
        ++outcome.effects_applied;
    }

    return core::Result<ApplyOutcome>::success(std::move(outcome));
}

}  // namespace leviathan::systems::policy
