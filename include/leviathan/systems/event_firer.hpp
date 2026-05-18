// EventFirer - convert M5.3 EventMatch into M5.4 EventInstance and
// append to GameState::event_history.
//
// M5.5 ships the FIRER skeleton: a small free-function bridge that
// takes the M5.3 evaluator's output (`event_evaluator::EventMatch`
// with per-trigger actor binding) plus the caller-supplied
// `fired_on` date and produces M5.4 `EventInstance` records on
// `state.event_history`. The firer does NOT apply effects, NOT
// integrate with the runner or monthly pipeline, NOT emit
// anything to `events.jsonl`, NOT append to `state.logs` /
// `state.applied_commands`, NOT touch the canonical fixtures.
//
// What the firer does:
//
//   * Reads the caller-supplied `EventMatch::triggers` actor
//     binding. For each `TriggerEvaluation::actor`:
//       - Country kind: writes an EventInstanceActor with
//         `kind="country"`, copies `id_code`, sets
//         `country_id_code = id_code` (a country IS its own
//         owning country), preserves `index`.
//       - InterestGroup kind: writes an EventInstanceActor with
//         `kind="interest_group"`, copies the IG's `id_code`,
//         resolves the owning country's id_code via a linear
//         scan of `state.countries` by `CountryId`, preserves
//         `index`. If the lookup fails (state is internally
//         inconsistent — the evaluator shouldn't have produced
//         this actor in the first place), the actor's
//         `country_id_code` stays empty, and the save-layer
//         validation will reject it on the next save round-trip
//         (loud failure, not silent corruption).
//   * Sets `EventInstance::event_id_code = match.event_id_code`.
//   * Sets `EventInstance::fired_on = fired_on` (caller-supplied;
//     the firer does not consult `state.current_date` because
//     that's the runner's policy decision).
//   * Appends one `EventInstance` per match to
//     `state.event_history` in input order.
//
// What the firer does NOT do:
//
//   no effects application (M1.5 policy::apply_policy_effects is
//     untouched; that lands in a separate M5.x sub-milestone)
//   no log entry on fire (no append to state.logs)
//   no events.jsonl change (no LoggingSystem call)
//   no applied_commands append (events are not player commands)
//   no runner / monthly integration (no auto-fire cadence; no
//     new RunnerOptions field; no new CLI flag)
//   no cooldown / weight / exclusivity / historical-once gating
//     (those would consult event_history to DECIDE whether to
//     record; M5.5 just records what the caller asked for)
//   no dedup (calling twice with the same match appends twice)
//   no save format bump (still v14; firer just writes the
//     M5.4 data type)
//   no event_evaluator change
//   no scenario_loader change
//   no UI surface
//   no new artefact (still 10)
//   no new PlayerCommandKind
//
// Composition note:
//
//   M5.5 is the brick a future M5.x runner-integration PR will
//   call as:
//
//       const auto matches = event_evaluator::match_events(state);
//       event_firer::record_matches(state, matches, state.current_date);
//
//   M5.5 does NOT wire this composition itself — that's the
//   runner-integration sub-milestone's job. M5.5 only ships the
//   conversion brick + tests that exercise it stand-alone.

#ifndef LEVIATHAN_SYSTEMS_EVENT_FIRER_HPP
#define LEVIATHAN_SYSTEMS_EVENT_FIRER_HPP

#include <cstddef>
#include <vector>

#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/systems/event_evaluator.hpp"

namespace leviathan::systems::event_firer {

// Summary of one record_matches call. M5.5 keeps this minimal —
// future M5.x can add per-fire status fields (skipped due to
// cooldown / exclusivity / etc.) without changing the existing
// `recorded` count semantic.
struct FireOutcome {
    std::size_t recorded = 0;   // entries appended to event_history
};

// Convert one EventMatch into an EventInstance and append it to
// state.event_history. `fired_on` is the caller-supplied date —
// the firer does not consult state.current_date because firing
// cadence is a runner-policy decision (not a firer-policy one).
//
// Always succeeds: even if an interest_group actor's owning-
// country lookup fails (state internally inconsistent), the
// resulting EventInstanceActor.country_id_code is left empty
// and the save layer's validation will catch it on the next
// round-trip. No exception, no Result type.
void record_match(core::GameState&                                 state,
                  const event_evaluator::EventMatch&               match,
                  const core::GameDate&                            fired_on);

// Batch form of record_match. Walks `matches` in input order;
// each successful record bumps FireOutcome::recorded. Empty
// input is a no-op (returns FireOutcome{0}). Calling twice with
// the same input appends twice — the firer does not dedup.
FireOutcome record_matches(core::GameState&                                  state,
                           const std::vector<event_evaluator::EventMatch>&   matches,
                           const core::GameDate&                             fired_on);

// RCR-1: RFC-090 §5.12 — followup-event-chain firing primitive.
//
// Given a `parent_instance` (already fired and recorded in
// `state.event_history`) and a `followup_definition` resolved
// via `event_effects::resolve_followup_ids`, append a new
// `EventInstance` to `state.event_history` representing the
// followup fire. The followup's `actors` are *inherited* from
// the parent (same first-actor-wins convention used by
// `event_effects::apply_event_effects`), since followups are
// not triggered by their own EventTrigger match — they are
// triggered by the parent's fire.
//
// Side effects mirror `record_match`:
//   - Appends one EventInstance to state.event_history.
//   - Appends one LogEntry{category="event_fired", ...} to
//     state.logs so events.jsonl records the followup fire
//     (with metadata {event_id_code, actor_kind,
//      actor_id_code, country_id_code}, mirroring record_match).
//
// Determinism: identical inputs produce byte-identical
// state mutation. RNG-free.
//
// No automatic cascade is performed: this is the
// deterministic primitive. A runner-policy that wants
// recursive followup firing must call `record_followup`
// itself (or via a loop). RCR-1 ships the primitive; the
// auto-cascade-wiring is intentionally out of scope to
// preserve M5.7 snapshot-evaluation semantics in
// `event_engine::tick_events`.
void record_followup(core::GameState&             state,
                     const core::EventInstance&   parent_instance,
                     const core::EventDefinition& followup_definition,
                     const core::GameDate&        fired_on);

}  // namespace leviathan::systems::event_firer

#endif  // LEVIATHAN_SYSTEMS_EVENT_FIRER_HPP
