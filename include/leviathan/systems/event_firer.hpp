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
// What the firer (this module) does NOT do — these concerns
// live elsewhere now that issue #112 has wired the full event
// engine:
//
//   no effects application — `event_effects::apply_event_effects`
//     handles base effects; `event_effects::apply_option_effects_with_mode`
//     handles option-bearing events under the author-controlled
//     `EventOptionEffectMode`; for player-country options the
//     `commands::dispatch_one` ChooseEventOption handler is the
//     apply path (post-deferral).
//   no option SELECTION — `event_effects::select_best_option_for_country`
//     is the state-based AI chooser; the player resolves choices
//     via `PlayerCommandKind::ChooseEventOption`. The legacy
//     `event_effects::select_default_option` is now a tests-only
//     deterministic-first-option primitive, not the engine path.
//   no recursion control — `event_engine::tick_events` and
//     `event_engine::recurse_followups_from_event` own the
//     conditional followup chain (depth-N up to
//     `kMaxFollowupDepth = 5`, cycle-guarded). `record_followup`
//     itself stays depth-agnostic and writes the IMMEDIATE
//     PREDECESSOR into `followup_of` metadata; the caller picks
//     the parent.
//   no applied_commands append (events are not player commands;
//     the ChooseEventOption *command* that resolves a deferred
//     event option is what appends to applied_commands).
//   no consumption of state.rng — the engine's weighted-random
//     draws happen in `event_engine::tick_events` and
//     `recurse_followups_*` (via `random::weighted_choice`).
//     The firer primitive itself records what it's told.
//   no cooldown / exclusivity / historical-once gating (engine
//     policy lives in tick_events).
//   no dedup (calling twice with the same input appends twice;
//     tick_events maintains a per-country tick-scope dedup set).
//   no scenario_loader / new RunnerOptions / new CLI flag change
//     initiated by this module.
//   no UI surface (the player-choice path uses the existing
//     `--commands` script / `apply_pending` channel — not a
//     graphical UI).
//
// Save / artefact context:
//
//   save format is owned by the save layer; the firer itself adds no
//     persistent schema field.
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
//   The engine calls `record_match` after selecting one event from a
//   per-country / per-category weighted draw, and calls
//   `record_followup` for each selected conditional followup. Batch
//   `record_matches` remains a low-level deterministic helper.

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
