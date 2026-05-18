// SaveSystem - serialise / deserialise a GameState as JSON.
//
// Design rules:
//   * Free functions. SaveSystem never owns or mutates a GameState
//     beyond what its arguments demand.
//   * Returns Result<T>. No exceptions leak across the boundary.
//   * Public header has no JSON dependency; nlohmann/json is an
//     implementation detail of save_system.cpp.
//   * The on-disk format carries an explicit `save_version` and
//     `rng_algorithm_version`. Unrecognised values are rejected at
//     load time with a clear message rather than silently producing
//     a half-loaded state.
//
// Replay note: M0.8 stores enough state for SESSION resume (round-
// trip a paused game) but does not yet store the per-tick decision
// log needed for full deterministic REPLAY. Replay support is
// deferred to a later milestone (see RFC-090 and
// docs/m0-8-save-load.md).

#ifndef LEVIATHAN_SYSTEMS_SAVE_SYSTEM_HPP
#define LEVIATHAN_SYSTEMS_SAVE_SYSTEM_HPP

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::save_system {

// Format / algorithm versions written into every save. Loading rejects
// values it does not recognise. Bump these when the schema or the RNG
// algorithm changes incompatibly; old saves then fail loudly rather
// than silently produce a different draw sequence.
//
// Version history:
//   v1 (M0.8)  - initial schema. CountryState had only id/id_code/name/
//                display_name/initial_gdp/initial_stability. factions /
//                provinces / policies / events were reserved as empty
//                arrays.
//   v2 (M1.1)  - CountryState gained 11 runtime numeric fields and
//                renamed initial_gdp/initial_stability to gdp/stability.
//   v3 (M1.2)  - FactionState gained 7+ runtime fields and is now
//                populated in saves. Loading a v2 save with an M1.2
//                binary would have lost factions silently, so we bump
//                rather than rely on the reserved-empty-array
//                "forward-compat" assumption from the M0.8 design note.
//   v4 (M1.3)  - CountryState gained a nested BudgetState (7-category
//                budget allocation). v3 saves miss the budget block.
//   v5 (M1.4)  - PolicyData gained id_code, name, category,
//                duration_days, admin_cost, and an effects vector
//                ({target, op, value}). The policies array is now
//                populated in saves.
//   v6 (M1.12) - CountryState gained `last_gdp_growth_rate` (the
//                most recent gdp_growth_rate written by economy::tick;
//                stability::tick consumes it as the RFC-080 §5
//                EconomicGrowth term). A v5 save would re-load with
//                an UNINITIALISED runtime growth rate; we reject it
//                so a v5 save can't silently change post-load
//                behaviour at the next monthly pipeline call.
//   v7 (M1.15) - CountryState gained `active_policies` (a vector of
//                `{policy_id_code, expires_on}` records, appended by
//                every successful policy::apply_policy_effects).
//                A v6 save lacks the field entirely, so loading it
//                would silently drop already-enacted day-0 policies
//                from any scenario save. We bump rather than tolerate
//                the missing array.
//   v8 (M2.1)  - GameState gained `player_country` at the root level
//                (CountryId; default invalid()/-1 for headless runs).
//                A v7 save has no field for it, and silently defaulting
//                to invalid() on reload would drop a player's country
//                selection. The loader gates strictly and additionally
//                validates that any non-invalid value indexes into
//                `state.countries`.
//   v9 (M2.4)  - GameState gained `applied_commands` (a vector of
//                `{applied_on, command}` records the player command
//                queue appends on every successful enactment). A v8
//                save would silently drop the replay log on reload,
//                so we bump rather than tolerate the missing array.
//   v10 (M2.16) - CountryState gained a nested
//                `government_authority` block (M2.16
//                GovernmentAuthorityState — bureaucratic_compliance,
//                military_loyalty, intelligence_capability,
//                media_control; all [0, 1] doubles). A v9 save lacks
//                the block; silently defaulting on reload would make
//                a v9 save reload as "every country has neutral 0.5
//                authority" regardless of what the user originally
//                authored. We bump strictly. At the save-file level
//                the block is REQUIRED with all four sub-keys present
//                and finite in [0, 1]; DataLoader still treats the
//                block as optional in raw country JSON.
//   v11 (M3.1) - GameState gained `interest_groups` at the root
//                level (M3.1 InterestGroupState — id_code, name,
//                kind, country, influence, loyalty, radicalism). A
//                v10 save lacks the array entirely; silently
//                defaulting to an empty list on reload would drop
//                whatever interest-group set the user originally
//                authored. We bump strictly. At the save-file level
//                the block is REQUIRED (empty array allowed) with
//                every entry validated: non-empty id_code + name +
//                known kind string + country id_code resolving into
//                `state.countries` + three ratio fields in [0, 1].
//                scenario_loader still treats the block as optional
//                in raw scenario JSON.
//   v12 (M4.1) - `GameState::provinces` flipped from a reserved-
//                empty stub (M0 `ProvinceState{id, owner}`) to a
//                first-class typed `ProvinceNode` array (id_code,
//                name, owner, x, y). A v11 save's `provinces`
//                entries (if any) lacked id_code / name / x / y;
//                silently defaulting on reload would either drop
//                the map nodes entirely or fabricate blank
//                identifiers and (0, 0) coordinates. We bump
//                strictly. At the save-file level the `provinces`
//                array is REQUIRED (empty array allowed); every
//                entry is validated: non-empty id_code + name,
//                owner that resolves to a valid index into
//                `state.countries`, and finite x / y in [0, 1].
//                Duplicate id_code rejected. scenario_loader still
//                treats the manifest's `provinces` block as
//                optional in raw scenario JSON.
//   v13 (M5.1) - `GameState::events` flipped from the M0 stub
//                (`EventDefinition{EventId id; std::string name;}`,
//                emitted as an empty array regardless of state)
//                to the M5.1 schema-foundation shape
//                (`EventDefinition{id_code, name, description,
//                triggers, effects}` mirroring `PolicyData`).
//                A v12 save's `events` array was always empty;
//                silently treating it as "no event definitions"
//                on reload would drop whatever event-definition
//                set the user authored under M5.1. We bump
//                strictly. At the save-file level the `events`
//                array is REQUIRED (empty array allowed); every
//                entry is validated: non-empty id_code + name,
//                string description (may be empty), non-empty
//                triggers array (each with allowlisted target,
//                allowlisted op, finite value), effects array
//                (may be empty; each entry has required target,
//                op, finite value — matching `PolicyEffect`
//                load-time rules, no target/op allowlist at load
//                because the future effects applicator will gate
//                that the same way `policy_system` does). Cross-
//                save duplicate event id_code rejected.
//                scenario_loader treats the manifest's `events`
//                block as optional in raw scenario JSON; M5.1
//                does not yet evaluate triggers / fire events /
//                apply effects / emit any new artefact.
//   v14 (M5.4) - new root-level `event_history` array carrying
//                fired-event records (`EventInstance`). A v13
//                save had no concept of fired events (M5.1/M5.2/
//                M5.3 were all schema + evaluator with zero
//                firing); silently defaulting to "no history" on
//                reload would drop any hand-authored M5.4 fixture
//                or future M5.x firer output. We bump strictly.
//                At the save-file level the `event_history`
//                array is REQUIRED (empty array allowed); every
//                entry is validated: non-empty `event_id_code`,
//                valid `fired_on` date string (YYYY-MM-DD shape +
//                `is_valid()`), `actors` array (may be empty)
//                with every actor entry carrying a kind from the
//                allowlist {"country", "interest_group"},
//                non-empty `id_code`, non-empty
//                `country_id_code`, and a non-negative `index`.
//                M5.4 does NOT cross-check that the referenced
//                event still exists in `state.events` — that
//                would prevent the legitimate "load a save into
//                a different scenario manifest" case. M5.4 ships
//                the data layer + save round-trip ONLY: no
//                system creates EventInstance records yet (no
//                auto-fire, no effects application, no runner
//                or monthly integration, no events.jsonl change,
//                no new artefact — still 10).
inline constexpr std::uint32_t kSaveFormatVersion   = 14;
inline constexpr std::uint32_t kRngAlgorithmVersion = 1;

// Serialise a GameState to a pretty-printed JSON string. Always
// succeeds for any valid GameState - serialisation never fails.
std::string serialize(const core::GameState& state);

// Write `state` to `path`. Creates parent directories if needed.
// Returns failure on filesystem error (with the path in the message).
//
// On success the returned Result wraps `true`; the `bool` is purely
// a placeholder because our Result type does not support a `void`
// success payload.
core::Result<bool> save(const core::GameState& state,
                        const std::filesystem::path& path);

// Parse a save document from raw JSON text. `source_label` is used
// only to format error messages (file path for load(), or any
// caller-supplied tag for inline tests).
core::Result<core::GameState> deserialize(std::string_view json_text,
                                          std::string_view source_label = "<inline>");

// Read `path` and deserialize its contents. On file-not-found or
// I/O error returns a failure Result naming the path.
core::Result<core::GameState> load(const std::filesystem::path& path);

}  // namespace leviathan::systems::save_system

#endif  // LEVIATHAN_SYSTEMS_SAVE_SYSTEM_HPP
