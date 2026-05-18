// ScenarioLoader - builds a non-empty GameState from a manifest file.
//
// M1.11 separates the "what's in the world" concern from the
// "tick the world" concern. The DataLoader (M0.7 / M1.1 / M1.2 / M1.4)
// already knows how to parse a single country / faction / policy
// JSON document; the ScenarioLoader composes those parsers into a
// loader that fills `state.countries`, `state.factions`, and
// `state.policies` from a manifest.
//
// Manifest schema:
//
//   {
//     "scenario": {
//       "countries": [ "countries/germany.json", ... ],
//       "factions":  [ "factions/ger_military.json", ... ],
//       "policies":  [ "policies/raise_taxes.json", ... ],
//       "starting_policies": [                                  // optional (M1.13)
//         { "policy": "raise_taxes", "actor": "GER" },
//         ...
//       ]
//     }
//   }
//
// `countries`, `factions`, `policies` are required (they may be empty
// arrays). `starting_policies` is optional; absent means "no day-0
// enactment". Each path entry is resolved against
//   manifest_path.parent_path().parent_path()
// so a manifest at `data/scenarios/1930_minimal.json` resolves
// `"countries/germany.json"` as `data/countries/germany.json`.
//
// ID assignment rules:
//   * CountryId{i} where `i` is the country's vector index.
//   * FactionId{i} where `i` is the faction's vector index.
//   * PolicyId{i}  where `i` is the policy's  vector index.
// The loader assigns these AFTER the DataLoader parses each entity;
// the parsed entities leave their numeric ids at the invalid default,
// which the loader overwrites by vector index.
//
// Validation rules (each rejects with a Result::failure):
//   * Each entity's `id_code` must be unique within its kind.
//     Duplicates fail with a message naming both occurrences.
//   * Every loaded faction's `country_id_code` must match a loaded
//     country's `id_code`; FactionState::country is set to the
//     numeric CountryId of that match.
//   * `state.countries` / `state.factions` / `state.policies` must
//     all be empty on entry to load_into_state. The M1.11 loader
//     does not append; callers that want incremental loading must
//     build their own pipeline.
//
// What this loader does NOT do:
//   * Schedule policies over time. Entries in `starting_policies`
//     are applied exactly once at load time via M1.5's
//     `apply_policy_effects`. There is NO duration queue, NO
//     monthly scheduler, NO AI / event-triggered enactment. Policies
//     not named in `starting_policies` remain inert templates in
//     `state.policies` until a future system enacts them.
//   * Touch RNG, logs, or the date. The save-format version is
//     owned by SaveSystem (`kSaveFormatVersion`) and is not
//     changed by scenario loading; M1.13 introduces no new
//     persistent state, so the bump cadence stays where SaveSystem
//     defines it.
//   * Validate cross-references beyond faction->country and the
//     M1.13 starting_policies resolution (policy id_code -> loaded
//     PolicyData, actor id_code -> loaded CountryId). A policy's
//     effect `target = "faction:bureaucracy.support"` is not
//     resolved here - that's M1.5's job.
//   * Discover scenarios on disk. The caller (typically the runner)
//     hands in a manifest path explicitly.

#ifndef LEVIATHAN_SYSTEMS_SCENARIO_LOADER_HPP
#define LEVIATHAN_SYSTEMS_SCENARIO_LOADER_HPP

#include <filesystem>
#include <string_view>
#include <vector>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::scenario_loader {

// A scenario can request day-0 policy enactment via the optional
// `starting_policies` array (M1.13). Each entry names a policy by
// `id_code` and the country that enacts it. After the manifest's
// countries / factions / policies have been loaded into GameState,
// the loader resolves each entry and calls
// `policy::apply_policy_effects(state, actor, policy)` exactly once.
//
// This is NOT a duration queue, NOT a scheduler, NOT AI. It only
// runs at load time. Subsequent monthly pipeline calls do not look
// at this list.
struct StartingPolicy {
    std::string policy_id_code;  // matches a loaded PolicyData::id_code
    std::string actor_id_code;   // matches a loaded CountryState::id_code
};

// One interest group / political actor entry inline in the
// scenario manifest (M3.1). Field names match the on-disk JSON;
// resolution against loaded countries + the
// `InterestGroupKind` enum happens inside `load_into_state`.
struct ManifestInterestGroup {
    std::string id_code;
    std::string name;
    std::string kind;             // matches an `InterestGroupKind` spelling
    std::string country_id_code;  // matches a loaded CountryState::id_code
    double influence  = 0.5;
    double loyalty    = 0.5;
    double radicalism = 0.0;
};

// One map-node entry parsed from a province file (M4.1). Each
// province file at `<base>/provinces/<name>.json` carries a
// `provinces` array of objects with this shape. The
// `owner_id_code` is resolved against loaded countries inside
// `load_into_state` and the result populates
// `core::ProvinceNode::owner`.
struct ManifestProvince {
    std::string id_code;
    std::string name;
    std::string owner_id_code;  // matches a loaded CountryState::id_code
    double x = 0.0;             // normalised map x in [0, 1]
    double y = 0.0;             // normalised map y in [0, 1]
};

// One event-definition entry parsed from an event file (M5.1
// shape; M6.1 adds `true_cause`). Each event file at
// `<base>/events/<name>.json` carries an `events` array of
// objects shaped like:
//   {
//     "id":          "low_stability_unrest",
//     "name":        "Low Stability Unrest",
//     "description": "...",
//     "true_cause":  "...",                     // M6.1 (RFC-090 §6.1)
//     "triggers": [
//       { "target": "country.stability", "op": "lt", "value": 0.30 }
//     ],
//     "effects": [
//       { "target": "country.stability", "op": "add", "value": -0.02 }
//     ]
//   }
// `triggers` is required, non-empty; `effects` is required, may
// be empty (warning-only events). Trigger ops are allowlisted
// at load time {lt, lte, gt, gte}; trigger targets are allowlisted
// against a small set tied to existing M1–M3 state. Effects
// reuse `core::PolicyEffect` shape — the load-time validation
// mirrors `data_loader::parse_policy` (required string `target`,
// string `op`, finite `value`; no target/op allowlist at load).
// Cross-file uniqueness of event `id_code` is enforced inside
// `load_into_state`.
//
// M6.1 adds `true_cause` (RFC-090 §6.1) as a required non-empty
// string. It is the author-written truth narrative; M6.1 stores
// and round-trips it but no system consumes it yet (later M6
// sub-milestones 6.2 visible_report, 6.3 information_accuracy
// will read it).
struct ManifestEvent {
    std::string                      id_code;
    std::string                      name;
    std::string                      description;
    std::string                      true_cause;   // M6.1
    std::vector<core::EventTrigger>  triggers;
    std::vector<core::PolicyEffect>  effects;
};

// Parsed manifest. Paths are stored verbatim from JSON; resolving
// them against a base directory is the caller's job (or use
// `load_into_state`, which does it).
struct ScenarioManifest {
    std::vector<std::filesystem::path> countries;
    std::vector<std::filesystem::path> factions;
    std::vector<std::filesystem::path> policies;
    // M1.13: optional. Missing key in JSON => empty vector. Manifests
    // authored before M1.13 (no `starting_policies` field) are
    // accepted unchanged.
    std::vector<StartingPolicy> starting_policies;
    // M3.1: optional. Missing key in JSON => empty vector. Manifests
    // authored before M3.1 (no `interest_groups` field) are accepted
    // unchanged. Present-but-malformed blocks are rejected.
    std::vector<ManifestInterestGroup> interest_groups;
    // M4.1: optional. Array of relative file paths, each pointing
    // to a province file with the shape
    // `{ "provinces": [ {id, name, owner, x, y}, ... ] }`. Missing
    // key in JSON => empty vector (older manifests stay valid).
    // Present-but-malformed at any layer is rejected loudly.
    std::vector<std::filesystem::path> provinces;
    // M5.1: optional. Array of relative file paths, each pointing
    // to an event file with the shape
    // `{ "events": [ {id, name, description, triggers, effects}, ... ] }`.
    // Missing key in JSON => empty vector (older manifests stay
    // valid). Present-but-malformed at any layer is rejected
    // loudly. Cross-file uniqueness of event id_code is enforced
    // inside `load_into_state`.
    std::vector<std::filesystem::path> events;
};

// Summary returned from a successful `load_into_state`.
struct ScenarioLoadOutcome {
    int countries_loaded         = 0;
    int factions_loaded          = 0;
    int policies_loaded          = 0;
    // M1.13: count of `starting_policies` entries that were applied
    // successfully (each entry = one apply_policy_effects call).
    int starting_policies_applied = 0;
    // M4.1: count of `ProvinceNode` entries appended to
    // `state.provinces` across every province file referenced by
    // the manifest. Zero when the manifest's `provinces` block is
    // absent or every referenced file holds an empty array.
    int provinces_loaded         = 0;
    // M5.1: count of `EventDefinition` entries appended to
    // `state.events` across every event file referenced by the
    // manifest. Zero when the manifest's `events` block is absent
    // or every referenced file holds an empty array. M5.1 only
    // loads / validates / stores event definitions; no firing,
    // no evaluator, no effects application yet.
    int events_loaded            = 0;
};

// Parse a scenario manifest from raw JSON text.
//
// `source_label` is used only in error messages (e.g. "<inline>" or
// the originating file path).
//
// Failure cases (each carries the source label and a path hint):
//   * JSON parse error.
//   * Top-level is not an object.
//   * `scenario` missing / not an object.
//   * `scenario.countries`, `scenario.factions`, `scenario.policies`
//     missing or not an array.
//   * Any array element that is not a string.
//   * (M1.13) If `scenario.starting_policies` is present and not an
//     array, OR any entry is not an object with string `policy` and
//     `actor` fields, fail with the offending index.
//   * (M1.13) `scenario.starting_policies` may be absent; this
//     parses as an empty vector (no day-0 enactments).
core::Result<ScenarioManifest> parse_manifest(
    std::string_view json_text,
    std::string_view source_label = "<inline>");

// Read the manifest at `manifest_path`, then load every referenced
// country / faction / policy into `state`. Paths inside the manifest
// are resolved against `manifest_path.parent_path().parent_path()`
// (so a manifest at `data/scenarios/foo.json` treats
// `"countries/germany.json"` as `data/countries/germany.json`).
//
// Failure cases:
//   * Manifest file cannot be opened / parsed.
//   * Any referenced country / faction / policy file cannot be
//     opened, parsed, or fails its DataLoader validation.
//   * `state.countries`, `state.factions`, or `state.policies` is
//     non-empty before the call.
//   * Any of the validation rules in the header comment is violated
//     (duplicate id_code, missing faction.country reference).
//   * (M1.13) Any `starting_policies` entry names a `policy` id_code
//     that doesn't match a loaded policy, or an `actor` id_code that
//     doesn't match a loaded country.
//   * (M1.13) Any `policy::apply_policy_effects` call fails (e.g.,
//     a policy effect targets an unknown country field). The error
//     bubbles up unchanged; M1.5 already guarantees apply is atomic
//     per-call, but day-0 enactments earlier in the list have
//     already mutated state.
//
// On any failure the state's vectors are left in whatever partial
// state the loader produced: M1.11 documents this rather than
// papering over it. Atomic loading is a follow-up sub-milestone.
core::Result<ScenarioLoadOutcome> load_into_state(
    core::GameState& state,
    const std::filesystem::path& manifest_path);

}  // namespace leviathan::systems::scenario_loader

#endif  // LEVIATHAN_SYSTEMS_SCENARIO_LOADER_HPP
