// DataLoader - parse simulation configuration and entity JSON.
//
// Design rules (M0.7 reviewer checklist):
//   * Free functions. No methods on GameState; the loader never
//     mutates a GameState directly. It returns parsed values via
//     Result<T>; the caller composes them with make_game_state() or
//     pushes them into state.countries on its own line.
//   * No simulation side effects. The loader never advances time,
//     never draws from the RNG, never logs.
//   * No hard coupling to LoggingSystem. Read errors come back as
//     Result::failure(message). Callers may choose to forward those
//     messages to log_error(), or print them, or surface them in a
//     UI - that decision lives outside this header.
//   * No exceptions across the boundary. The underlying JSON library
//     uses exceptions internally; we catch / use exception-free APIs
//     so the public surface is Result-only.

#ifndef LEVIATHAN_SYSTEMS_DATA_LOADER_HPP
#define LEVIATHAN_SYSTEMS_DATA_LOADER_HPP

#include <filesystem>
#include <string_view>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/result.hpp"
#include "leviathan/core/simulation_config.hpp"

namespace leviathan::systems::data_loader {

// Parse a simulation-config JSON document from raw text. `source_label`
// is used only to format error messages (e.g. "<inline>", a file path,
// or any other caller-supplied tag).
//
// Expected shape (see docs/m0-7-data-loader.md for the full schema):
//   {
//     "simulation": {
//       "start_date":  "YYYY-MM-DD",   // required
//       "end_date":    "YYYY-MM-DD",   // optional, defaults to 2000-12-31
//       "seed":        <unsigned int>, // optional, defaults to 0
//       "daily_tick":  <bool>          // optional, defaults to true
//     }
//   }
//
// Error categories:
//   - "<source>: JSON parse error: ..."
//   - "<source>: missing required field '<path>'"
//   - "<source>: '<path>' has wrong type (expected <X>)"
//   - "<source>: '<path>' = \"...\" is not a real Gregorian date"
core::Result<core::SimulationConfig> parse_simulation_config(
    std::string_view json_text,
    std::string_view source_label = "<inline>");

// Reads `path` and parses its contents via parse_simulation_config.
// On read failure (missing / unreadable file), returns a failure
// Result with a message that names the path.
core::Result<core::SimulationConfig> load_simulation_config(
    const std::filesystem::path& path);

// Parse a single-country JSON document.
//
// Expected shape (M1.1):
//   {
//     "id":                        "GER",      // required string code
//     "name":                      "Germany",  // required
//     "display_name":              "Germany",  // optional; defaults to name
//
//     "initial_gdp":               100.0,      // required, finite, >= 0
//                                              // -> CountryState::gdp
//     "initial_stability":         0.55,       // required, in [0, 1]
//                                              // -> CountryState::stability
//
//     "legal_tax_burden":          0.20,       // required, in [0, 1]
//     "fiscal_capacity":           0.50,       // required, in [0, 1]
//     "administrative_efficiency": 0.55,       // required, in [0, 1]
//     "central_control":           0.60,       // required, in [0, 1]
//     "corruption":                0.25,       // required, in [0, 1]
//     "legitimacy":                0.55,       // required, in [0, 1]
//     "military_power":            0.50,       // required, in [0, 1]
//     "threat_perception":         0.30,       // required, in [0, 1]
//
//     "budget": {                              // required JSON object (M1.3)
//       "administration":  0.25,               // every field required, [0, 1]
//       "military":        0.35,
//       "education":       0.10,
//       "welfare":         0.10,
//       "intelligence":    0.05,
//       "infrastructure":  0.10,
//       "industry":        0.05
//     }
//   }
//
// Any required field that is missing, has the wrong type, is
// non-finite, or violates its declared range produces a failure
// Result naming the field. Runtime-only fields (tax_revenue,
// budget_balance) are not read from JSON and start at 0.
//
// The numeric `CountryState::id` is left at its invalid default; the
// caller is responsible for assigning numeric IDs (typically by
// insertion order into state.countries).
core::Result<core::CountryState> parse_country(
    std::string_view json_text,
    std::string_view source_label = "<inline>");

core::Result<core::CountryState> load_country(
    const std::filesystem::path& path);

// Parse a single-faction JSON document.
//
// Expected shape (M1.2):
//   {
//     "id":                "GER_military",  // required string code
//     "country":           "GER",           // required; CountryState id_code link
//     "type":              "military",      // required (free-form string)
//     "name":              "Reichswehr",    // required
//
//     "support":           0.45,  // required, in [0, 1]
//     "influence":         0.70,  // required, in [0, 1]
//     "radicalism":        0.30,  // required, in [0, 1]
//     "loyalty":           0.55,  // required, in [0, 1]
//     "resources":         1.20,  // required, >= 0
//
//     "preferred_policies": [     // required (may be []); strings only
//       "increase_military_budget"
//     ]
//   }
//
// Numeric handles (FactionState::id, FactionState::country) stay at
// their invalid defaults; the caller assigns numeric IDs after
// loading. FactionState::country_id_code carries the on-disk link
// to a CountryState ("GER") for the caller to resolve.
core::Result<core::FactionState> parse_faction(
    std::string_view json_text,
    std::string_view source_label = "<inline>");

core::Result<core::FactionState> load_faction(
    const std::filesystem::path& path);

}  // namespace leviathan::systems::data_loader

#endif  // LEVIATHAN_SYSTEMS_DATA_LOADER_HPP
