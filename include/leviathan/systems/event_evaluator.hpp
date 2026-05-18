// EventEvaluator - deterministic trigger evaluation for M5.1 events.
//
// M5.2 introduces the FIRST event-engine surface beyond M5.1's
// schema foundation. Scope is deliberately tiny: a free-function
// API that reads `state.events` (M5.1 typed EventDefinition) and
// reports which event definitions' triggers all match the current
// GameState. It does NOT fire events, NOT append history, NOT
// apply effects, NOT integrate with the runner or monthly
// pipeline, and NOT emit anything to `events.jsonl`.
//
// Semantics:
//
//   * Per-trigger evaluation is a small predicate (target + op +
//     value) against a state slice. M5.2 supports exactly the
//     M5.1 allowlist:
//
//       country.stability                                          double
//       country.legitimacy                                         double
//       country.government_authority.bureaucratic_compliance       double
//       interest_group.radicalism                                  double
//       interest_group.loyalty                                     double
//
//     Operators: lt / lte / gt / gte. NaN / +-Inf state values are
//     treated as non-matching (no IEEE-754 surprises in the gate).
//
//   * Aggregation across entities is "ANY satisfies": a
//     country.* trigger matches when at least one country in
//     state.countries satisfies it; an interest_group.* trigger
//     matches when at least one interest group in
//     state.interest_groups satisfies it. Empty country/IG list
//     therefore evaluates the trigger as FALSE (existential
//     quantifier over an empty set).
//
//   * Combination within one event is "ALL must match" (AND
//     semantics across `def.triggers`). An empty triggers vector
//     is vacuously TRUE (mathematical convention; the M5.1 loader
//     rejects empty triggers, so this case is unreachable via
//     canonical load paths but the API still pins the semantics).
//
//   * Unknown trigger target / op (e.g. a hand-built def that
//     bypasses the loader allowlist) evaluates to FALSE. The
//     loader is the gate; the evaluator does not duplicate
//     allowlist messaging. No exception, no Result type, no log.
//
//   * State is never mutated. Calling `match_events` repeatedly
//     on the same GameState returns the same vector.
//
// M5.2 deliberate non-goals:
//
//   no firing (no event log entry, no `state.applied_commands`-
//     style append, no events.jsonl emission)
//   no effects application (M1.5 policy::apply_policy_effects is
//     untouched; M5.x will compose evaluator + applicator later)
//   no per-actor selection (a country.* match doesn't yet record
//     which country satisfied; effects-application will need that
//     and add it in its own sub-milestone)
//   no runner / monthly integration (no auto-evaluation on month
//     boundaries; no new RunnerOptions field; no new CLI flag)
//   no historical-once gating, no cooldown, no weight, no
//     exclusivity, no chained / parent / child event id
//   no broader trigger ops (`eq` / `ne` / `between` / `in`)
//   no broader trigger targets (factions, budget categories,
//     active_policies, current_date, RNG state)
//   no trigger logical operators (`and` / `or` / `not` per
//     event — AND across `def.triggers` is hard-coded)
//   no save schema change (state.events shape is M5.1's v13;
//     evaluator is purely a read)

#ifndef LEVIATHAN_SYSTEMS_EVENT_EVALUATOR_HPP
#define LEVIATHAN_SYSTEMS_EVENT_EVALUATOR_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"

namespace leviathan::systems::event_evaluator {

// A single successful event match. `event_index` is the position
// in `state.events`; `event_id_code` mirrors `state.events[i].id_code`
// so callers don't need to deref the events vector themselves.
struct TriggerMatch {
    std::size_t event_index = 0;
    std::string event_id_code;
};

// Evaluate a single EventTrigger against the current state.
// Returns true iff the trigger's target / op / value combination
// is satisfied by at least one entity of the appropriate kind.
// Returns false when:
//   - target is not in the M5.1 allowlist
//   - op is not in the M5.1 allowlist
//   - value is non-finite
//   - the appropriate entity list is empty
//   - no entity in the appropriate list satisfies the predicate
// Reads state; never mutates it.
bool trigger_matches(const core::GameState&    state,
                     const core::EventTrigger& trig);

// Evaluate a single EventDefinition.
// Returns true iff ALL of def.triggers individually match the
// current state (AND semantics). Empty triggers vector is
// vacuously true (the M5.1 loader rejects empty triggers, so this
// path is unreachable through canonical loads). Reads state;
// never mutates it.
bool evaluate(const core::GameState&         state,
              const core::EventDefinition&   def);

// Evaluate every event definition in state.events and return one
// TriggerMatch per matching event, in canonical order (vector
// order of state.events). Empty result vector if nothing matches
// or state.events is empty. Reads state; never mutates it.
std::vector<TriggerMatch> match_events(const core::GameState& state);

}  // namespace leviathan::systems::event_evaluator

#endif  // LEVIATHAN_SYSTEMS_EVENT_EVALUATOR_HPP
