#include "leviathan/systems/commands.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>

#include "leviathan/core/entities.hpp"
#include "leviathan/systems/event_effects.hpp"
#include "leviathan/systems/event_engine.hpp"
#include "leviathan/systems/event_evaluator.hpp"
#include "leviathan/systems/order_execution.hpp"
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

// ---- M2.20 internal per-command dispatch ---------------------------------
//
// `dispatch_one` is the shared per-command implementation used by
// both `apply_pending` (legacy M2.3 / M2.18 / M2.19 surface — rejection
// surfaces as `Result::failure`) and `try_apply_pending` (M2.20 surface
// — rejection surfaces as `Result::success` with a `RejectionRecord`).
// The function returns:
//   * `Result::failure` for genuine errors (unknown policy id_code,
//     unknown budget category, non-finite delta, policy-effect
//     resolution failure). Callers propagate the failure unchanged
//     so non-execution validation never gets silently swallowed.
//   * `Result::success` with `applied = true` and `rejection ==
//     std::nullopt` once the per-kind validation has run cleanly and
//     state mutation has happened. The caller's loop pops the queue
//     head and appends the applied_commands log entry.
//   * `Result::success` with `applied = false` and `rejection`
//     populated when the M2.18 / M2.19 order-execution gate
//     rejects. State is untouched in this branch.
struct DispatchOutcome {
    bool applied = false;
    std::optional<RejectionRecord> rejection;
};

core::Result<DispatchOutcome> dispatch_one(core::GameState& state,
                                           const core::PlayerCommand& cmd,
                                           const std::string& ctx) {
    namespace pol = leviathan::systems::policy;
    namespace oe  = leviathan::systems::order_execution;

    switch (cmd.kind) {
        case core::PlayerCommandKind::EnactPolicy: {
            auto eval_r = oe::evaluate(state, cmd);
            if (!eval_r) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": EnactPolicy '" + cmd.policy_id_code +
                    "': order_execution::evaluate failed: " +
                    std::move(eval_r.error()));
            }
            if (eval_r.value().status == oe::ExecutionStatus::Rejected) {
                RejectionRecord rj;
                rj.kind            = cmd.kind;
                rj.policy_id_code  = cmd.policy_id_code;
                rj.compliance      = eval_r.value().inputs.bureaucratic_compliance;
                rj.threshold       = oe::kEnactPolicyComplianceThreshold;
                rj.resistance      = eval_r.value().resistance;
                DispatchOutcome out;
                out.applied   = false;
                out.rejection = std::move(rj);
                return core::Result<DispatchOutcome>::success(std::move(out));
            }

            const core::PolicyData* p =
                find_policy_by_id_code(state, cmd.policy_id_code);
            if (p == nullptr) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": EnactPolicy unknown policy id_code '" +
                    cmd.policy_id_code + "'");
            }
            auto r = pol::apply_policy_effects(state,
                                               state.player_country,
                                               *p);
            if (!r) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": EnactPolicy '" + cmd.policy_id_code +
                    "': " + std::move(r.error()));
            }
            DispatchOutcome out;
            out.applied = true;
            return core::Result<DispatchOutcome>::success(std::move(out));
        }
        case core::PlayerCommandKind::AdjustBudget: {
            auto eval_r = oe::evaluate(state, cmd);
            if (!eval_r) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": AdjustBudget '" + cmd.budget_category +
                    "': order_execution::evaluate failed: " +
                    std::move(eval_r.error()));
            }
            if (eval_r.value().status == oe::ExecutionStatus::Rejected) {
                const double selected =
                    (cmd.budget_category == "military")
                        ? eval_r.value().inputs.military_loyalty
                        : eval_r.value().inputs.bureaucratic_compliance;
                RejectionRecord rj;
                rj.kind            = cmd.kind;
                rj.budget_category = cmd.budget_category;
                rj.compliance      = selected;
                rj.threshold       = oe::kAdjustBudgetComplianceThreshold;
                rj.resistance      = eval_r.value().resistance;
                DispatchOutcome out;
                out.applied   = false;
                out.rejection = std::move(rj);
                return core::Result<DispatchOutcome>::success(std::move(out));
            }

            if (!std::isfinite(cmd.budget_delta)) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": AdjustBudget budget_delta is not finite");
            }
            auto& country =
                state.countries[static_cast<std::size_t>(
                    state.player_country.value())];
            double* field =
                budget_field_ptr(country, cmd.budget_category);
            if (field == nullptr) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": AdjustBudget unknown budget_category '" +
                    cmd.budget_category +
                    "' (expected administration|military|education|"
                    "welfare|intelligence|infrastructure|industry)");
            }
            // Post-M6.7 hardening + research-grounded asymptotic
            // ratio update (Polity / V-Dem / Besley & Persson —
            // bounded institutional indicators with diminishing
            // returns near the bounds). Validate the current
            // field as a finite ratio, then apply the asymptotic
            // add: positive delta scales by `1 - old`, negative
            // scales by `old`. The candidate is in `[0, 1]` by
            // construction; the explicit check is a single-ULP
            // floating-point safety net + formula-rebalance
            // regression catch.
            if (!std::isfinite(*field) || *field < 0.0 || *field > 1.0) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": AdjustBudget current country.budget." +
                    cmd.budget_category + " = " +
                    std::to_string(*field) +
                    " is not a finite ratio in [0, 1]");
            }
            const double delta = cmd.budget_delta;
            const double candidate = (delta >= 0.0)
                ? *field + delta * (1.0 - *field)
                : *field + delta * (*field);
            if (!std::isfinite(candidate) ||
                candidate < 0.0 || candidate > 1.0) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": AdjustBudget country.budget." +
                    cmd.budget_category + " asymptotic-add candidate " +
                    std::to_string(candidate) + " (from " +
                    std::to_string(*field) + " with delta " +
                    std::to_string(delta) +
                    ") escapes ratio range [0, 1]");
            }
            *field = candidate;
            DispatchOutcome out;
            out.applied = true;
            return core::Result<DispatchOutcome>::success(std::move(out));
        }
        case core::PlayerCommandKind::ChooseEventOption: {
            // Issue #112: resolve a deferred player-country event option.
            // Locate the pending entry by event_history_index. Reject
            // with structured error string when not found, when the
            // pending entry is not the player's country, or when
            // option_id_code doesn't match any of the definition's
            // options. On success: apply effects per the event's
            // option_effect_mode, remove the pending entry, then trigger
            // conditional followup recursion starting from the chosen
            // parent event.
            const std::size_t hist_idx = cmd.event_history_index;

            // Locate the pending entry. Iterate by hand because order
            // matters for the "first match wins" semantics if two
            // player events were somehow pending for the same history
            // index (loader-prevented but defensive).
            auto pending_it = state.pending_player_events.begin();
            for (; pending_it != state.pending_player_events.end();
                 ++pending_it) {
                if (pending_it->event_history_index == hist_idx) {
                    break;
                }
            }
            if (pending_it == state.pending_player_events.end()) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": ChooseEventOption: no pending entry "
                    "for event_history_index=" +
                    std::to_string(hist_idx));
            }

            // Validate player country still matches.
            if (!state.player_country.valid() ||
                static_cast<std::size_t>(state.player_country.value())
                    >= state.countries.size()) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": ChooseEventOption: state.player_country "
                    "is invalid");
            }
            const auto& player_country =
                state.countries[static_cast<std::size_t>(
                    state.player_country.value())];
            if (pending_it->country_id_code != player_country.id_code) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": ChooseEventOption: pending entry's "
                    "country '" + pending_it->country_id_code +
                    "' does not match current player_country '" +
                    player_country.id_code + "'");
            }

            // Locate the event definition by id_code.
            std::size_t def_idx = state.events.size();
            for (std::size_t i = 0; i < state.events.size(); ++i) {
                if (state.events[i].id_code == pending_it->event_id_code) {
                    def_idx = i;
                    break;
                }
            }
            if (def_idx >= state.events.size()) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": ChooseEventOption: pending event '" +
                    pending_it->event_id_code +
                    "' no longer present in state.events");
            }
            const auto& def = state.events[def_idx];

            // Locate the option by id_code.
            const core::EventOption* chosen_opt = nullptr;
            for (const auto& opt : def.options) {
                if (opt.id_code == cmd.option_id_code) {
                    chosen_opt = &opt;
                    break;
                }
            }
            if (chosen_opt == nullptr) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": ChooseEventOption: option '" +
                    cmd.option_id_code + "' is not one of event '" +
                    def.id_code + "' options");
            }

            // Apply effects per the author-controlled mode.
            const auto& parent_instance =
                state.event_history[hist_idx];
            auto apply_r = event_effects::apply_option_effects_with_mode(
                state, parent_instance, def, *chosen_opt,
                def.option_effect_mode);
            if (!apply_r) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": ChooseEventOption: " +
                    std::move(apply_r.error()));
            }

            // Remove the pending entry BEFORE followup recursion so
            // any cycle that re-fires this event id sees a clean
            // pending vector.
            state.pending_player_events.erase(pending_it);

            // Conditional followup recursion (depth-N, cycle-guarded).
            // Re-fetch country since apply may have shifted state.
            const auto& refreshed_country =
                state.countries[static_cast<std::size_t>(
                    state.player_country.value())];
            const std::string tag =
                "commands.ChooseEventOption." + def.id_code;
            auto follow_r =
                event_engine::recurse_followups_from_event(
                    state, hist_idx, def_idx,
                    refreshed_country, tag);
            if (!follow_r) {
                return core::Result<DispatchOutcome>::failure(
                    ctx + ": ChooseEventOption '" + def.id_code +
                    "' followup: " + std::move(follow_r.error()));
            }

            DispatchOutcome out;
            out.applied = true;
            return core::Result<DispatchOutcome>::success(std::move(out));
        }
    }
    // Unreachable — every `PlayerCommandKind` variant returns above.
    return core::Result<DispatchOutcome>::failure(
        ctx + ": dispatch_one reached unreachable code (unknown kind)");
}

// Format the M2.18 / M2.19 rejection error message from a
// RejectionRecord. Kept in one place so `apply_pending` and any
// future caller that wants the legacy string surface produce
// byte-identical messages.
std::string format_rejection_message(const std::string& ctx,
                                     const RejectionRecord& rj) {
    std::ostringstream os;
    if (rj.kind == core::PlayerCommandKind::EnactPolicy) {
        os << ctx << ": EnactPolicy '" << rj.policy_id_code
           << "' rejected by order_execution gate"
           << " (bureaucratic_compliance=" << rj.compliance
           << " < threshold=" << rj.threshold << ")";
    } else if (rj.kind == core::PlayerCommandKind::ChooseEventOption) {
        // ChooseEventOption is NOT order-execution-gated (player
        // choices on offered options don't go through the
        // compliance gate; rejections come from
        // pending-entry / option-id-code validation in
        // dispatch_one, which returns Result::failure directly
        // and never lands here).
        os << ctx << ": ChooseEventOption "
           << "rejected by order_execution gate (unexpected)";
    } else {
        os << ctx << ": AdjustBudget category '" << rj.budget_category
           << "' rejected by order_execution gate"
           << " (compliance=" << rj.compliance
           << " < threshold=" << rj.threshold << ")";
    }
    return os.str();
}

}  // namespace

core::Result<ApplyOutcome> apply_pending(core::GameState& state,
                                         CommandQueue& q) {
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

        auto r = dispatch_one(state, cmd, ctx);
        if (!r) {
            return core::Result<ApplyOutcome>::failure(std::move(r.error()));
        }
        if (r.value().rejection.has_value()) {
            // M2.18 / M2.19 surface preserved byte-identical:
            // rejection is reported as Result::failure with the
            // legacy message format. M2.20 `try_apply_pending`
            // is the alternative if a caller wants the structured
            // record instead.
            return core::Result<ApplyOutcome>::failure(
                format_rejection_message(ctx, r.value().rejection.value()));
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

// ===========================================================================
// M2.20 - try_apply_pending: structured rejection surface
// ===========================================================================

core::Result<ApplyWithReportOutcome> try_apply_pending(
        core::GameState& state, CommandQueue& q) {
    // Same precondition as `apply_pending`. Non-execution errors
    // remain `Result::failure` so callers can keep relying on the
    // failure path to mean "something is genuinely broken, not
    // just politically resisted".
    if (!state.player_country.valid() ||
        state.player_country.value() < 0 ||
        static_cast<std::size_t>(state.player_country.value()) >=
            state.countries.size()) {
        return core::Result<ApplyWithReportOutcome>::failure(
            "commands::try_apply_pending: state.player_country is not"
            " a valid index into state.countries (call run() with"
            " --player COUNTRY_IDCODE before submitting commands)");
    }

    ApplyWithReportOutcome result;

    while (!q.pending.empty()) {
        const core::PlayerCommand cmd = q.pending.front();
        const std::string ctx =
            "commands::try_apply_pending[" +
            std::to_string(result.apply.commands_applied) + "]";

        auto r = dispatch_one(state, cmd, ctx);
        if (!r) {
            // Genuine validation / system error. Propagate
            // unchanged so a typo'd policy id_code or a NaN
            // budget delta still surfaces as a hard failure.
            return core::Result<ApplyWithReportOutcome>::failure(
                std::move(r.error()));
        }
        if (r.value().rejection.has_value()) {
            // Execution gate stopped the drain. Surface the
            // record as a `Result::success` outcome so the
            // caller can inspect it programmatically. The
            // rejected command stays at the head of the queue,
            // exactly like `apply_pending` leaves it.
            result.rejection = std::move(r.value().rejection);
            return core::Result<ApplyWithReportOutcome>::success(
                std::move(result));
        }

        q.pending.erase(q.pending.begin());
        ++result.apply.commands_applied;
        state.applied_commands.push_back(
            core::AppliedPlayerCommand{state.current_date, cmd});
    }

    return core::Result<ApplyWithReportOutcome>::success(std::move(result));
}

// ===========================================================================
// M2.6: replay
// ===========================================================================

core::Result<ReplayOutcome> replay(
        core::GameState& target_state,
        const std::vector<core::AppliedPlayerCommand>& log) {
    // ---- Preconditions ----------------------------------------------------
    if (!target_state.player_country.valid() ||
        target_state.player_country.value() < 0 ||
        static_cast<std::size_t>(target_state.player_country.value()) >=
            target_state.countries.size()) {
        return core::Result<ReplayOutcome>::failure(
            "commands::replay: target_state.player_country is not a"
            " valid index into state.countries (caller must set"
            " player_country before replay)");
    }
    if (!target_state.applied_commands.empty()) {
        return core::Result<ReplayOutcome>::failure(
            "commands::replay: target_state.applied_commands must be"
            " empty on entry (replay would otherwise mix the new"
            " entries with prior ones; build a fresh state from the"
            " scenario loader and call replay on that)");
    }

    // ---- Replay loop ------------------------------------------------------
    ReplayOutcome outcome;
    for (std::size_t i = 0; i < log.size(); ++i) {
        const auto& entry = log[i];

        // Force the date so the new log entry's applied_on matches
        // the source. The simulation systems are NOT ticked during
        // replay (M2.6 prototype limit).
        target_state.current_date = entry.applied_on;

        // Build a 1-element queue and dispatch through the existing
        // apply_pending path. This reuses every M2.3/M2.4/M2.5
        // guarantee: precondition validation, per-command atomicity,
        // log-on-success append, the M1.5/M1.15 effect machinery.
        CommandQueue q;
        q.pending.push_back(entry.command);
        auto r = apply_pending(target_state, q);
        if (!r) {
            return core::Result<ReplayOutcome>::failure(
                "commands::replay[" + std::to_string(i) + "]: " +
                std::move(r.error()));
        }
        ++outcome.commands_replayed;
    }

    return core::Result<ReplayOutcome>::success(std::move(outcome));
}

// ===========================================================================
// M2.7: replay_with_time
// ===========================================================================

core::Result<ReplayOutcome> replay_with_time(
        core::GameState& state,
        const runner::RunnerOptions& opts,
        runner::TickController& ctrl,
        const std::vector<core::AppliedPlayerCommand>& log) {
    // ---- Preconditions ----------------------------------------------------
    if (!state.player_country.valid() ||
        state.player_country.value() < 0 ||
        static_cast<std::size_t>(state.player_country.value()) >=
            state.countries.size()) {
        return core::Result<ReplayOutcome>::failure(
            "commands::replay_with_time: state.player_country is not a"
            " valid index into state.countries (caller must set"
            " player_country before replay)");
    }
    if (!state.applied_commands.empty()) {
        return core::Result<ReplayOutcome>::failure(
            "commands::replay_with_time: state.applied_commands must be"
            " empty on entry (build a fresh state from the scenario"
            " loader, not a reloaded save)");
    }
    if (!ctrl.started) {
        return core::Result<ReplayOutcome>::failure(
            "commands::replay_with_time: TickController has not been"
            " started (caller must call runner::begin_tick first)");
    }
    if (ctrl.ended) {
        return core::Result<ReplayOutcome>::failure(
            "commands::replay_with_time: TickController already ended;"
            " build a fresh controller to start a new replay");
    }

    // ---- Replay loop ------------------------------------------------------
    ReplayOutcome outcome;
    for (std::size_t i = 0; i < log.size(); ++i) {
        const auto& entry = log[i];

        // Monotonicity: rejects out-of-order entries. The check is
        // against current state.current_date, so the first entry that
        // tries to go back in time fails — even if it is itself in
        // order relative to entry i-1 (the state may already be past
        // that date if the caller stepped beforehand).
        if (entry.applied_on < state.current_date) {
            return core::Result<ReplayOutcome>::failure(
                "commands::replay_with_time[" + std::to_string(i) +
                "]: out-of-order log entry (applied_on " +
                entry.applied_on.to_string() +
                " < current_date " + state.current_date.to_string() +
                "); replay requires monotonically non-decreasing dates");
        }

        // Advance day-by-day until we reach the entry's date. Each
        // step_one_day call runs the boundary logs + monthly pipeline
        // when applicable; ctrl.days_stepped / monthly_ticks are
        // updated accordingly.
        while (state.current_date < entry.applied_on) {
            auto step_r = runner::step_one_day(state, opts, ctrl);
            if (!step_r) {
                return core::Result<ReplayOutcome>::failure(
                    "commands::replay_with_time[" + std::to_string(i) +
                    "]: step_one_day failed advancing toward " +
                    entry.applied_on.to_string() + ": " +
                    std::move(step_r.error()));
            }
        }

        // Dispatch through apply_pending so the M2.3 atomicity +
        // M2.4 log-append + M1.5/M1.15 effect machinery all run.
        CommandQueue q;
        q.pending.push_back(entry.command);
        auto apply_r = apply_pending(state, q);
        if (!apply_r) {
            return core::Result<ReplayOutcome>::failure(
                "commands::replay_with_time[" + std::to_string(i) +
                "]: " + std::move(apply_r.error()));
        }
        ++outcome.commands_replayed;
    }

    return core::Result<ReplayOutcome>::success(std::move(outcome));
}

// ===========================================================================
// M2.21 - scripted-driver helper
// ===========================================================================

core::Result<ApplyWithReportOutcome> apply_command_script(
        core::GameState& state,
        const std::vector<core::PlayerCommand>& script) {
    // The script is consumed once; build a local queue so the
    // caller's vector is left untouched. `try_apply_pending`
    // pops from `q.pending` as it drains; success / rejection /
    // failure semantics inherit directly from there.
    CommandQueue q;
    q.pending = script;
    return try_apply_pending(state, q);
}

}  // namespace leviathan::systems::commands
