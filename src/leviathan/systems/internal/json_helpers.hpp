// Shared JSON-validation helpers used by data_loader.cpp and
// save_system.cpp.
//
// This header is PRIVATE to the leviathan_systems library: it lives
// under src/ (not include/), and the library exposes
// CMAKE_CURRENT_SOURCE_DIR as a PRIVATE include directory so
// internal/json_helpers.hpp resolves only inside the library.
// The public surface of the systems library never mentions
// nlohmann/json or these helpers.
//
// Extraction history: M1.4. Before that, near-identical copies lived
// in data_loader.cpp and save_system.cpp (and a third in
// save_system.cpp via the country_from_json + faction_from_json
// load_num lambdas). Centralising them here matches the M1.3
// design-note prediction that M1.4's policy loader would be the
// natural extraction point.

#ifndef LEVIATHAN_SYSTEMS_INTERNAL_JSON_HELPERS_HPP
#define LEVIATHAN_SYSTEMS_INTERNAL_JSON_HELPERS_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "leviathan/core/result.hpp"

namespace leviathan::systems::detail {

// Use ordered_json so callers (save_system.cpp) that need
// deterministic key-order on iteration get it without choosing the
// json type themselves.
using json = nlohmann::ordered_json;

// Builds "<source>: <msg>" with no extra punctuation.
std::string fmt_err(std::string_view source, std::string_view msg);

// Walks a dotted path like "simulation.start_date" through an object.
// Returns nullptr if any segment is missing or any intermediate value
// is not an object.
const json* navigate(const json& root, std::string_view dotted_path);

// Required-field helpers. Each returns a failure Result whose error
// message names the field, types, and (where applicable) the bad
// value. Strict type checks; non-finite numbers are rejected by
// require_number / require_ratio / require_nonneg_number.

core::Result<std::string>   require_string(const json& root,
                                            std::string_view path,
                                            std::string_view source);
core::Result<double>        require_number(const json& root,
                                            std::string_view path,
                                            std::string_view source);
core::Result<double>        require_nonneg_number(const json& root,
                                                   std::string_view path,
                                                   std::string_view source);
core::Result<double>        require_ratio(const json& root,
                                          std::string_view path,
                                          std::string_view source);

// Returns the number iff it parses as a non-negative integer. The
// uint64_t result preserves the full unsigned range (this is how
// simulation.seed handles values past INT64_MAX).
core::Result<std::uint64_t> require_u64(const json& root,
                                         std::string_view path,
                                         std::string_view source);

}  // namespace leviathan::systems::detail

#endif  // LEVIATHAN_SYSTEMS_INTERNAL_JSON_HELPERS_HPP
