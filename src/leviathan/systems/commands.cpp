#include "leviathan/systems/commands.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

#include "leviathan/core/entities.hpp"
#include "leviathan/systems/policy_system.hpp"

namespace leviathan::systems::commands {

namespace {

// Locate a policy by its on-disk identifier. Returns nullptr if no
// loaded policy matches. id_code is stable across loads; numeric
// PolicyId is not (assigned by scenario-loader index), so we look up
// by string here.
const core::PolicyData* find_policy_by_id_code(
        const core::GameState& state, const std::string& id_code) {
    for (const auto& p : state.policies) {
        if (p.id_code == id_code) return &p;
    }
    return nullptr;
}

// M2.5: resolve a budget-category string to the matching BudgetState
// double field on `c`. Returns nullptr for unknown categories. Kept
// here (not reused from policy_system.cpp's internal helper) so the
// command path doesn't depend on policy::detail.
double* budget_field_ptr(core::CountryState& c, const std::string& cat) {
    if (cat == "administration")  return &c.budget.administration;
    if (cat == "military")        return &c.budget.military;
    if (cat == "education")       return &c.budget.education;
    if (cat == "welfare")         return &c.budget.welfare;
    if (cat == "intelligence")    return &c.budget.intelligence;
    if (cat == "infrastructure")  return &c.budget.infrastructure;
    if (cat == "industry")        return &c.budget.industry;
    return nullptr;
}

}  // namespace

core::Result<ApplyOutcome> apply_pending(core::GameState& state,
                                         CommandQueue& q) {
    namespace pol = leviathan::systems::policy;

    // ---- Precondition: a valid player_country is mandatory. -----
    if (!state.player_country.valid() ||
        state.player_country.value() < 0 ||
        static_cast<std::size_t>(state.player_country.value()) >=
            state.countries.size()) {
        return core::Result<ApplyOutcome>::failure(
            "commands::apply_pending: state.player_country is not a"
            " valid index into state.countries (call run() with"
            " --player COUNTRY_IDCODE before submitting commands)");
    }

    ApplyOutcome outcome;

    // ---- Drain the queue. Stop at first failure. -----------------
    while (!q.pending.empty()) {
        const core::PlayerCommand cmd = q.pending.front();
        const std::string ctx =
            "commands::apply_pending[" +
            std::to_string(outcome.commands_applied) + "]";

        switch (cmd.kind) {
            case core::PlayerCommandKind::EnactPolicy: {
                const core::PolicyData* p =
                    find_policy_by_id_code(state, cmd.policy_id_code);
                if (p == nullptr) {
                    return core::Result<ApplyOutcome>::failure(
                        ctx + ": EnactPolicy unknown policy id_code '" +
                        cmd.policy_id_code + "'");
                }
                auto r = pol::apply_policy_effects(state,
                                                   state.player_country,
                                                   *p);
                if (!r) {
                    return core::Result<ApplyOutcome>::failure(
                        ctx + ": EnactPolicy '" + cmd.policy_id_code +
                        "': " + std::move(r.error()));
                }
                break;
            }
            case core::PlayerCommandKind::AdjustBudget: {
                // Pre-flight: finite delta. The DataLoader doesn't go
                // through this path, so a hand-rolled NaN/Inf would
                // otherwise corrupt the budget silently.
                if (!std::isfinite(cmd.budget_delta)) {
                    return core::Result<ApplyOutcome>::failure(
                        ctx + ": AdjustBudget budget_delta is not finite");
                }
                auto& country =
                    state.countries[static_cast<std::size_t>(
                        state.player_country.value())];
                double* field =
                    budget_field_ptr(country, cmd.budget_category);
                if (field == nullptr) {
                    return core::Result<ApplyOutcome>::failure(
                        ctx + ": AdjustBudget unknown budget_category '" +
                        cmd.budget_category +
                        "' (expected administration|military|education|"
                        "welfare|intelligence|infrastructure|industry)");
                }
                // Apply with the same [0, 1] clamp policy M1.5 uses for
                // ratio fields.
                *field = std::clamp(*field + cmd.budget_delta, 0.0, 1.0);
                break;
            }
        }

        q.pending.erase(q.pending.begin());
        ++outcome.commands_applied;

        // M2.4: log the successful enactment for future replay.
        // Only reached when the per-command dispatch above returned
        // ok(), so the log entry is consistent with the state mutation.
        // A failed command produces no log entry — per-command
        // atomicity covers the log too.
        state.applied_commands.push_back(
            core::AppliedPlayerCommand{state.current_date, cmd});
    }

    return core::Result<ApplyOutcome>::success(std::move(outcome));
}

}  // namespace leviathan::systems::commands
