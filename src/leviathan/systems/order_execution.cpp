#include "leviathan/systems/order_execution.hpp"

#include <cstddef>
#include <string>

namespace leviathan::systems::order_execution {

core::Result<OrderExecutionOutcome> evaluate(
        const core::GameState& state,
        const core::PlayerCommand& command) {
    // Preconditions: same shape as commands::apply_pending. The
    // function would have no actor without a valid player country.
    if (!state.player_country.valid() ||
        state.player_country.value() < 0 ||
        static_cast<std::size_t>(state.player_country.value()) >=
            state.countries.size()) {
        return core::Result<OrderExecutionOutcome>::failure(
            "order_execution::evaluate: state.player_country is not"
            " a valid index into state.countries (set"
            " --player COUNTRY_IDCODE or assign player_country"
            " before evaluating an order)");
    }

    const auto& country = state.countries[static_cast<std::size_t>(
        state.player_country.value())];

    OrderExecutionOutcome outcome;
    outcome.inputs.bureaucratic_compliance =
        country.government_authority.bureaucratic_compliance;
    outcome.inputs.military_loyalty =
        country.government_authority.military_loyalty;
    outcome.inputs.intelligence_capability =
        country.government_authority.intelligence_capability;
    outcome.inputs.media_control =
        country.government_authority.media_control;

    // M2.18: per-command-kind gate. Only EnactPolicy is evaluated
    // in this PR; AdjustBudget (and any future kind not listed
    // below) keeps the default Accepted + resistance 0.0.
    switch (command.kind) {
        case core::PlayerCommandKind::EnactPolicy: {
            const double compliance =
                outcome.inputs.bureaucratic_compliance;
            outcome.resistance = 1.0 - compliance;
            outcome.status =
                (compliance >= kEnactPolicyComplianceThreshold)
                    ? ExecutionStatus::Accepted
                    : ExecutionStatus::Rejected;
            break;
        }
        case core::PlayerCommandKind::AdjustBudget: {
            // No gate yet. resistance stays at 0.0 ("no gate
            // evaluated"), status stays at Accepted.
            break;
        }
    }

    return core::Result<OrderExecutionOutcome>::success(std::move(outcome));
}

}  // namespace leviathan::systems::order_execution
