// InterestGroupKind <-> string round-trip helpers.
//
// Single source of truth for the kind-string mapping. Save (M3.1
// save format v11), scenario loader (M3.1 manifest parser), and
// diagnostics (M3.5 interest-group CSV) all route through these
// two free functions so adding a future `InterestGroupKind` variant
// only requires one edit.
//
// The functions live in `leviathan::core` because:
//   * they describe a property of the enum, not of a system;
//   * placing them next to `InterestGroupState` keeps callers' include
//     surface small;
//   * core is the lowest layer in the dependency graph, so neither
//     save_system nor scenario_loader inverts layering by depending
//     on the other.

#ifndef LEVIATHAN_CORE_INTEREST_GROUP_KIND_HPP
#define LEVIATHAN_CORE_INTEREST_GROUP_KIND_HPP

#include <string>
#include <string_view>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::core {

// Stable canonical spelling. Variant names ("Bureaucracy",
// "Military", ...) must NOT be renamed; on-disk saves and scenario
// JSON depend on the exact strings.
//
// Returns a sentinel `"UnknownInterestGroupKind"` when called with an
// out-of-range enum value (exhaustive-switch hardening, mirrors
// `player_command_kind_to_string`). Callers that need a closed-set
// guarantee should still pattern-match on the enum.
std::string interest_group_kind_to_string(InterestGroupKind kind);

// Parse a kind string. Fails with a message that lists every known
// variant when `s` is unrecognised, so the loader's error path is
// useful without callers having to hand-craft it.
Result<InterestGroupKind> interest_group_kind_from_string(std::string_view s);

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_INTEREST_GROUP_KIND_HPP
