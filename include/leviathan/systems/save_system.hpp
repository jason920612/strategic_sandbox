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
inline constexpr std::uint32_t kSaveFormatVersion   = 6;
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
