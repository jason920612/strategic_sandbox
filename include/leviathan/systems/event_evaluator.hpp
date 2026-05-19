// EventEvaluator - deterministic trigger evaluation for M5.1 events.
//
// M5.2 introduced the first event-engine surface beyond M5.1's
// schema foundation: a small free-function API that reads
// `state.events` and reports which event definitions' triggers
// all match the current GameState. M5.3 extends the return shape
// to record **which entity** (country or interest group)
// satisfied each trigger ("actor binding"), prepping for a
// future M5.x sub-milestone that fires events / applies effects
// (both of which need a specific actor to direct the effect at).
//
// M5.3 still does NOT fire events, NOT append history, NOT apply
// effects, NOT integrate with the runner or monthly pipeline,
// and NOT emit anything to `events.jsonl`.
//
// Semantics (unchanged from M5.2):
//
//   * Per-trigger evaluation is a small predicate (target + op +
//     value) against a state slice. M5.2/M5.3 support exactly
//     the M5.1 allowlist:
//
//       country.stability                                          double
//       country.legitimacy                                         double
//       country.government_authority.bureaucratic_compliance       double
//       interest_group.radicalism                                  double
//       interest_group.loyalty                                     double
//
//     Operators: lt / lte / gt / gte. NaN / +-Inf state values are
//     treated as non-matching.
//
//   * Aggregation across entities is "ANY satisfies": a
//     country.* trigger matches when at least one country in
//     state.countries satisfies it; an interest_group.* trigger
//     matches when at least one interest group in
//     state.interest_groups satisfies it. Empty country/IG list
//     therefore evaluates the trigger as FALSE.
//
//   * Combination within one event is "ALL must match" (AND
//     semantics across `def.triggers`). An empty triggers vector
//     is vacuously TRUE.
//
//   * Unknown trigger target / op evaluates to FALSE. The M5.1
//     loader is the gate.
//
//   * State is never mutated.
//
// M5.3 actor-binding semantics:
//
//   * For each satisfied trigger, the evaluator records the
//     **first** entity (in vector order) that satisfied it. For
//     a country-scoped trigger this is the first country in
//     `state.countries` whose field satisfies the comparison;
//     for an interest-group-scoped trigger it is the first IG
//     in `state.interest_groups`. "First" is deterministic
//     because both vectors are canonically ordered (insertion
//     order from scenario load).
//
//   * Collecting EVERY satisfying entity per trigger (instead of
//     just the first) is deferred to a future M5.x. M5.3 records
//     one actor per trigger; the future firer can re-query if it
//     needs the full list. This keeps the M5.3 return shape
//     small and the test surface tractable.
//
// M5.3 deliberate non-goals (carried over from M5.2):
//
//   no firing (no event log entry, no state.applied_commands-
//     style append, no events.jsonl emission)
//   no effects application (M1.5 policy::apply_policy_effects is
//     untouched)
//   no runner / monthly integration (no auto-evaluation; no new
//     RunnerOptions field; no new CLI flag)
//   no historical-once gating, no cooldown, no weight, no
//     exclusivity, no chained / parent / child event id
//   no broader trigger ops (eq / ne / between / in)
//   no broader trigger targets (factions, budget categories,
//     active_policies, current_date, RNG state)
//   no trigger logical operators (and / or / not per event —
//     AND across def.triggers is hard-coded)
//   no save schema change (state.events shape is M5.1's v13;
//     evaluator is purely a read)
//
// M5.2 -> M5.3 API migration:
//
//   The old M5.2 `TriggerMatch { event_index, event_id_code }`
//   is renamed to `EventMatch` and gains a `triggers` vector of
//   `TriggerEvaluation` (one entry per matched trigger). The
//   `event_index` + `event_id_code` field names are preserved
//   so M5.2-style read sites continue to compile.

#ifndef LEVIATHAN_SYSTEMS_EVENT_EVALUATOR_HPP
#define LEVIATHAN_SYSTEMS_EVENT_EVALUATOR_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::event_evaluator {

// M5.3: classify a trigger actor by which state vector it lives
// in. country.* triggers bind to `state.countries`;
// interest_group.* triggers bind to `state.interest_groups`.
enum class TriggerActorKind {
    Country,
    InterestGroup,
};

// M5.3: the entity that satisfied a trigger. For a country-scoped
// trigger this is a country; for an interest-group-scoped trigger
// this is an interest group. `id_code` mirrors the entity's
// stable string identity; `country` is set for both kinds (an
// interest group knows its owning country); `index` is the
// position in the appropriate state vector (so the future firer
// can deref without an id_code lookup).
struct TriggerActor {
    TriggerActorKind kind    = TriggerActorKind::Country;
    std::string      id_code;
    core::CountryId  country = core::CountryId::invalid();
    std::size_t      index   = 0;
};

// M5.3: one entry per def.triggers, recording the trigger
// position and the first entity that satisfied it.
struct TriggerEvaluation {
    std::size_t  trigger_index = 0;     // position in def.triggers
    TriggerActor actor;
};

// M5.3: replaces the M5.2 TriggerMatch.
// `event_index` and `event_id_code` are preserved verbatim from
// M5.2 so M5.2-shaped read sites keep working. `triggers` is the
// new M5.3 field: one TriggerEvaluation per def.triggers, in def
// order. Only populated when ALL triggers matched (i.e. only
// present in the result of evaluate_match / match_events).
struct EventMatch {
    std::size_t                    event_index = 0;
    std::string                    event_id_code;
    std::vector<TriggerEvaluation> triggers;
};

// Evaluate a single EventTrigger against the current state.
// Returns true iff at least one entity satisfies the trigger.
// Cheap predicate form retained from M5.2.
bool trigger_matches(const core::GameState&    state,
                     const core::EventTrigger& trig);

// M5.3: like trigger_matches but returns the **first** satisfying
// entity (in vector order). nullopt iff no entity satisfies
// (which is also when trigger_matches would return false).
std::optional<TriggerActor>
trigger_actor(const core::GameState&    state,
              const core::EventTrigger& trig);

// Evaluate a single EventDefinition.
// Returns true iff ALL of def.triggers individually match the
// current state. Empty triggers vector is vacuously true.
// Cheap predicate form retained from M5.2.
bool evaluate(const core::GameState&       state,
              const core::EventDefinition& def);

// M5.3: like evaluate but returns the full per-trigger actor
// binding when matched. nullopt iff any trigger fails to match
// (which is also when evaluate would return false). For the
// vacuous-true empty-triggers case, returns an EventMatch with
// an empty `triggers` vector.
std::optional<EventMatch>
evaluate_match(const core::GameState&       state,
               const core::EventDefinition& def);

// Evaluate every event definition in state.events and return one
// EventMatch per matching event, in canonical order (vector
// order of state.events). Each EventMatch carries the full
// per-trigger actor binding. Empty result vector if nothing
// matches or state.events is empty. Reads state; never mutates
// it.
std::vector<EventMatch> match_events(const core::GameState& state);

// Issue #112: per-country scoped match. Returns at most one
// EventMatch per event in state.events; each match guarantees ALL
// triggers bound INSIDE the named country:
//   - `country.*` triggers must hold for state.countries[country_id]
//     specifically (no leakage to other countries).
//   - `interest_group.*` triggers must hold for some IG whose
//     owning `country == country_id` (no leakage from IGs in
//     other countries).
//   - Mixed-trigger events require both sides to bind within
//     this country.
// The same event template may match independently for multiple
// countries — one country's match does NOT consume or suppress
// another's. Used by `event_engine::tick_events` to drive the
// per-country, per-category weighted draw.
std::vector<EventMatch>
match_events_for_country(const core::GameState& state,
                         core::CountryId        country_id);

// RFC-090 §5.3 / §5.6 — deterministic weighted RANKER (the
// underlying weight computation; the §5.7 DRAW that consumes
// these weights is wired in `event_engine::tick_events` —
// see below). For each event in `state.events`, compute a
// numeric `weight` as `kBaseWeight + sum(modifier.weight_delta)`
// over every `WeightModifier` whose `target` / `op` / `value`
// comparison currently holds against any country / interest
// group in `state` (same ANY-entity-satisfies aggregation as
// `trigger_matches`). The result is sorted by descending weight,
// with original event vector order as the deterministic
// tie-break.
//
// Post-M6.7 hardening (`feedback_no_silent_degradation`):
// malformed modifiers no longer silently contribute zero.
// Returns `Result::failure` naming the event id_code,
// modifier index, and offending field for:
//   - non-finite `weight_delta` (NaN / ±Inf),
//   - non-finite modifier `value`,
//   - unrecognised modifier `target` or `op`,
//   - non-finite accumulated weight after applying matches.
// On success the returned vector still contains one entry per
// `state.events` entry.
//
// `kBaseWeight` is `1.0` — author-supplied modifiers raise or
// lower priority from there. Empty `weight_modifiers` keeps
// the event at base weight.
//
// **`rank_weighted_events` itself is RNG-free** — it produces
// the per-event weight vector that the §5.7 weighted-random
// draw consumes. The draw lives one level up in
// `event_engine::tick_events`, which feeds the weights into
// `random::weighted_choice(state.rng, …)` per
// (country, category) bucket. So the canonical M1.17 / M2 /
// M3 / M4 / M5 byte-identical baselines are preserved only
// while the canonical scenario has zero events matching their
// triggers (no matches → no draws → no RNG consumption); any
// scenario that produces matches WILL consume `state.rng`.
//
// Returns one entry per event in `state.events`, even when an
// event's weight is exactly the base weight. Callers that only
// care about events whose triggers currently match should
// compose this with `match_events`.
struct WeightedEventCandidate {
    std::size_t event_index = 0;
    std::string event_id_code;
    double      weight = 0.0;
};

inline constexpr double kBaseWeight = 1.0;

core::Result<std::vector<WeightedEventCandidate>>
rank_weighted_events(const core::GameState& state);

// `select_weighted_event` — DEPRECATED for engine use as of
// issue #112. Retained as a deterministic single-pick helper
// (highest-weight currently-matched event with stable
// tie-break on event vector index, RNG-free) for ad-hoc
// callers that genuinely want top-1 selection without the
// state.rng dependency. NOTE: `event_engine::tick_events` no
// longer calls this — RFC-090 §5.7 "事件抽選" requires a
// weighted RANDOM draw, which the engine does directly via
// `random::weighted_choice` using `state.rng` on each
// (country, category) bucket. Use this helper only when you
// want a deterministic top-pick (e.g. tests / diagnostics);
// do NOT use it as the §5.7 implementation.
//
// Post-M6.7 hardening (`feedback_no_silent_degradation` +
// `feedback_api_signature_expresses_failure`): the previous
// `std::optional<WeightedEventCandidate>` return shape
// silently absorbed a `rank_weighted_events` failure into
// `std::nullopt`, which the strict-validation discipline
// forbids. The signature is now
// `core::Result<std::optional<WeightedEventCandidate>>`:
//   - no matched events                  -> success(nullopt)
//   - rank_weighted_events failure       -> failure(error)
//     (malformed weight modifier, non-finite weight_delta,
//      unrecognised target/op, non-finite accumulated weight)
//   - top matched candidate by descending weight + vector
//     tie-break                           -> success(candidate)
core::Result<std::optional<WeightedEventCandidate>>
select_weighted_event(const core::GameState& state);

}  // namespace leviathan::systems::event_evaluator

#endif  // LEVIATHAN_SYSTEMS_EVENT_EVALUATOR_HPP
