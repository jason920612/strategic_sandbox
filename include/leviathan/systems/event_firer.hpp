// EventFirer - convert M5.3 EventMatch / RCR-1 followup definitions
// into M5.4 EventInstance records and append them to
// GameState::event_history, AND emit per-fire LogEntries into
// state.logs so events.jsonl surfaces event firings (RFC-090 §5.9).
//
// M5.5 originally shipped the firer as history-only (no log
// emission, no apply, no monthly integration). RCR-1 (the one-time
// corrective PR that closes the RFC-090 §M5 / §5.9 / §5.12 gap from
// issue #105; see docs/rfc-090-010-compliance-audit.md) extends the
// firer's contract to satisfy RFC-090 §5.9 (per-fire event log) and
// RFC-090 §5.12 (followup-event chain firing primitive). The
// behaviour the firer ACTUALLY has after RCR-1 is documented below;
// the original M5.5 "history-only" contract is preserved verbatim
// in M5.5's design note in docs/m5-5-event-firer-skeleton.md as
// archaeology.
//
// What the firer does (RCR-1 contract):
//
//   record_match(state, match, fired_on):
//     * Reads the caller-supplied `EventMatch::triggers` actor
//       binding. For each `TriggerEvaluation::actor`:
//         - Country kind: writes an EventInstanceActor with
//           `kind="country"`, copies `id_code`, sets
//           `country_id_code = id_code` (a country IS its own
//           owning country), preserves `index`.
//         - InterestGroup kind: writes an EventInstanceActor with
//           `kind="interest_group"`, copies the IG's `id_code`,
//           resolves the owning country's id_code via a linear
//           scan of `state.countries` by `CountryId`, preserves
//           `index`. If the lookup fails (state is internally
//           inconsistent), `country_id_code` stays empty and the
//           save-layer validation rejects on the next round-trip.
//     * Sets `EventInstance::event_id_code = match.event_id_code`,
//       `fired_on = fired_on` (caller-supplied), and appends to
//       `state.event_history`.
//     * Appends ONE `core::LogEntry`:
//         category = "event_fired"
//         source   = "event_firer"
//         date     = fired_on
//         message  = "event <id> fired"
//         metadata = {event_id_code, actor_kind, actor_id_code,
//                     country_id_code}  (first actor; "<none>" /
//                     empty for vacuous-actor cases)
//       This log entry flows through M0.6 LoggingSystem into
//       `events.jsonl` on `end_tick`. RFC-090 §5.9 acceptance.
//
//   record_matches(state, matches, fired_on):
//     * Batch form of record_match. Walks `matches` in input order;
//       FireOutcome.recorded counts appended entries. Empty input
//       is a no-op.
//
//   record_followup(state, parent_instance, followup_def, fired_on):
//     * RCR-1 (RFC-090 §5.12) deterministic chain-firing primitive.
//     * Appends one `EventInstance` to `state.event_history` whose
//       `event_id_code = followup_def.id_code`, `fired_on =
//       fired_on`, and `actors = parent_instance.actors` (inherited
//       — followups are not triggered by their own EventTrigger
//       match, they are triggered by the parent's fire).
//     * Appends ONE `core::LogEntry` mirroring record_match's
//       shape, with two additional metadata entries:
//         followup_of = parent_instance.event_id_code
//         message     = "event <id> fired (followup of <parent>)"
//
// What the firer does NOT do:
//
//   no effects application (M1.5 policy::apply_policy_effects /
//     M5.6 event_effects::apply_event_effects own that path)
//   no option selection (M5.4 / RFC-090 §5.4 / §5.8 — see
//     event_effects::select_default_option and
//     apply_default_option_effects)
//   no automatic recursive followup cascade (record_followup is
//     the deterministic chain-firing primitive; a runner-policy
//     consumer would have to loop record_followup over a sequence
//     of resolved followups itself — wiring that loop into the
//     runner would change M5.7 snapshot-evaluation semantics and
//     is intentionally out of scope for the RCR-1 corrective batch)
//   no applied_commands append (events are not player commands)
//   no consumption of state.rng (preserves the M5 RNG-free
//     guarantee for the event engine)
//   no cooldown / weight / exclusivity / historical-once gating
//     (caller policy; M5.5 / RCR-1 just records what the caller
//     asks for)
//   no dedup (calling twice with the same input appends twice)
//   no scenario_loader / new RunnerOptions / new CLI flag /
//     new PlayerCommandKind change initiated by this module
//   no UI surface
//
// Save / artefact context (RCR-1 era):
//
//   save format = v17 (post-RCR-1; v16 -> v17 batched migration
//     covers EventDefinition.weight_modifiers / options /
//     followup_event_ids plus several non-event surfaces — the
//     firer itself adds no new persistent field).
//   artefact contract = 11 unconditional artefacts (post-RCR-1;
//     RCR-1 added annual_world_stats.csv as the 11th — also
//     unrelated to the firer, listed here only for context).
//   The canonical 1930_minimal scenario stays no-fire under the
//     M5 invariant (its events' triggers don't match the
//     canonical authored values), so canonical events.jsonl
//     bytes stay byte-identical with the M5 close-out even
//     though record_match now emits.
//
// Composition note:
//
//   The firer is the brick that runner / event_engine call as:
//
//       const auto matches = event_evaluator::match_events(state);
//       event_firer::record_matches(state, matches, state.current_date);
//
//   The M5.7 `event_engine::tick_events` composition runs as
//   step 7 of `monthly::tick_all_countries` (M5.8 wiring).

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
