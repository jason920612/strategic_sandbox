// Central simulation state container.
//
// GameState is intentionally a plain data struct with NO methods. All
// behaviour - advancing time, applying policies, computing budgets,
// evaluating events - lives in system free functions that receive a
// GameState& and mutate it (see RFC-060 §2: "資料集中、系統分離").
//
// The only construction helper is the free function make_game_state(),
// which seeds a fresh GameState from a SimulationConfig. We
// deliberately do not provide a constructor that takes a config: a
// free function keeps the "container is dumb" rule visible, and lets
// later subsystems compose initialisation pipelines without inheriting
// from or extending this struct.

#ifndef LEVIATHAN_CORE_GAME_STATE_HPP
#define LEVIATHAN_CORE_GAME_STATE_HPP

#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/core/player_commands.hpp"
#include "leviathan/core/random_state.hpp"
#include "leviathan/core/simulation_config.hpp"

namespace leviathan::core {

struct GameState {
    GameDate    current_date{1930, 1, 1};
    RandomState rng{};

    // M2.1: which country the player has selected, or
    // CountryId::invalid() when running headless / unattended.
    // Resolved from `--player COUNTRY_IDCODE` by the runner AFTER
    // scenario load. A valid value MUST index into `countries`; the
    // save loader enforces this. The runtime systems never read this
    // field yet (the M1 systems do not branch on the player); future
    // M2 sub-milestones (player command queue, pause / step) will.
    CountryId   player_country = CountryId::invalid();

    std::vector<CountryState>    countries;
    // M4.1: SVG-map node data layer. Each entry is a typed
    // `ProvinceNode` (id_code + name + owner + normalised x/y).
    // Empty unless a scenario manifest's optional `provinces`
    // block was loaded. No system reads the field yet — M4.1 is
    // data only; the future SVG exporter / UI consumes it.
    std::vector<ProvinceNode>    provinces;
    std::vector<FactionState>    factions;
    std::vector<PolicyData>      policies;
    std::vector<EventDefinition> events;
    std::vector<LogEntry>        logs;

    // M2.4: persistent record of commands that `commands::apply_pending`
    // has successfully applied. Append-only at the apply site; never
    // mutated by simulation systems. Foundation for future
    // deterministic replay (RFC-050 §8).
    std::vector<AppliedPlayerCommand> applied_commands;

    // M3.1: interest-group / political-actor data layer. Root-level
    // so future cross-country interactions (foreign-funded media,
    // diaspora pressure, transnational religious networks) compose
    // naturally; each entry's `country` field points back to
    // `countries`. M3.1 is data-only — no M1 or M2 system reads or
    // mutates the list. Loader treats the block as optional in
    // scenario JSON; SaveSystem requires it at the save layer (save
    // format v11).
    std::vector<InterestGroupState> interest_groups;

    // Append-only history of fired events. Each entry records the
    // event that fired, the date it fired, and the per-trigger
    // actor binding at fire time. Round-trips through the save
    // layer (current save format v18). Loader does not populate
    // this from the scenario manifest; history is a runtime
    // accumulation, not a scenario input. Scenario_loader rejects
    // a pre-populated event_history at load_into_state entry
    // (9-container "scenario-load-clean GameState" contract per
    // issue #112 §7).
    //
    // Producers (issue #112 wired flow):
    //   - event_firer::record_match — every parent event drawn
    //     by event_engine::tick_events appends here.
    //   - event_firer::record_followup — every followup picked
    //     by event_engine::recurse_followups_from_event (the
    //     conditional recursive chain inside tick_events AND
    //     the post-ChooseEventOption recursion inside
    //     commands::dispatch_one) appends here, with the
    //     immediate predecessor written into the `followup_of`
    //     log metadata.
    //
    // Consumers:
    //   - state.pending_player_events[i].event_history_index
    //     references a parent EventInstance whose effects were
    //     deferred for the player country.
    //   - save_system serialises every entry into save.json.
    //   - events.jsonl is the per-fire LogEntry trail emitted
    //     alongside each record_match / record_followup.
    std::vector<EventInstance> event_history;

    // RCR-1: RFC-090 §3.6 / §3.7 — pairwise inter-country
    // relationship + threat record. Root-level so a future
    // cross-border system can iterate without indexing through
    // every country. Empty by default; scenario manifest may
    // author entries via the optional `relationships` block
    // (vector of `{from, to, relationship, threat}`). Save
    // format v17 makes the block required at the save layer.
    // Issue #112 wires the AI scorer to READ these values so
    // they finally feed simulation behaviour. The diplomacy-AI
    // *driver* of these values stays M8 / RFC-040 scope.
    std::vector<CountryRelation> relationships;

    // Issue #112: pending player-country event option choices.
    // Populated by `event_engine::tick_events` when a player-country
    // event with `options` non-empty fires. Effects are NOT applied
    // for these events until the player issues a
    // `PlayerCommandKind::ChooseEventOption` command. Save v18
    // persists this vector; scenario_loader rejects a pre-populated
    // entry on entry (M7.1 extended preflight to 10 containers).
    // Order is the natural append-order from tick_events; the
    // command handler resolves by `event_history_index` so order
    // is informational only.
    std::vector<PendingPlayerEvent> pending_player_events;

    // M7.1 (RFC-090 §7.1, RFC-020 §7) — faction demands. Producers:
    // `faction_demands::tick_generate` appends new Pending demands
    // for any faction whose `type` matches an RFC-020 §7 demand
    // kind and whose `radicalism` exceeds the threshold (and which
    // does not already hold a Pending demand of that kind).
    // `faction_demands::tick_expire_and_apply` flips Pending →
    // Expired when `state.current_date >= expires_on` and applies
    // asymptotic radicalism / loyalty drift on the matching
    // faction. Expired entries stay in this vector as an audit
    // trail; the generator allows a fresh Pending demand for the
    // same `(faction_id_code, kind)` to be created later, with a
    // different `created_on`. Save v19 persists this vector;
    // scenario_loader rejects a pre-populated `faction_demands` on
    // entry (10-container preflight). Order is the natural
    // append-order from `tick_generate`.
    std::vector<FactionDemand> faction_demands;
};

// Builds a fresh GameState from `config`:
//   - current_date = config.start_date
//   - rng.seed     = config.seed
//   - rng.counter  = 0
//   - all entity containers start empty
//
// Precondition: config.start_date.is_valid(). The factory does not
// load data, run systems, or emit logs - those concerns belong to
// later milestones (M0.7 DataLoader, M0.4 TimeSystem, M0.6
// LoggingSystem).
GameState make_game_state(const SimulationConfig& config);

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_GAME_STATE_HPP
