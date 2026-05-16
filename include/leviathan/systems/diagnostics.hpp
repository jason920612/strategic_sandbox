// DiagnosticsSystem - observation-only summaries and sanity checks.
//
// Design rules:
//   * Every function in this header takes the GameState by const reference.
//     Diagnostics NEVER mutates state. If a check finds something wrong,
//     it RETURNS an Issue; it is the caller's job (typically the runner)
//     to decide what to do with it - log it, abort, etc.
//   * Free functions only. No DiagnosticsSystem class.
//   * No exceptions across the boundary.
//   * CSV output format is documented and stable - it is the second
//     byte-stable artefact the project exports (alongside JSONL logs).

#ifndef LEVIATHAN_SYSTEMS_DIAGNOSTICS_HPP
#define LEVIATHAN_SYSTEMS_DIAGNOSTICS_HPP

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"

namespace leviathan::systems::diagnostics {

// One row of the summary CSV. Fields are the M0.10 spec minimum.
// Later milestones can extend this; readers should tolerate extra
// columns appended on the right.
struct SummaryRow {
    core::GameDate date;
    std::size_t    country_count = 0;
    std::size_t    log_count     = 0;
    std::uint64_t  seed          = 0;
};

// Build a SummaryRow from the current state. Pure observation:
// state is not mutated.
SummaryRow snapshot(const core::GameState& state);

// Write the canonical CSV header. Always:
//   "date,country_count,log_count,seed\n"
void write_csv_header(std::ostream& out);

// Write a single row. Always:
//   "YYYY-MM-DD,<country_count>,<log_count>,<seed>\n"
// No quoting (none of our fields ever contains a comma).
void write_csv_row(std::ostream& out, const SummaryRow& row);

// ---------------------------------------------------------------------------
// Sanity checks.
// ---------------------------------------------------------------------------

enum class Severity {
    Error,    // breaks an invariant; the run cannot be trusted.
};

struct Issue {
    Severity    severity = Severity::Error;
    std::string code;       // stable machine-readable identifier
    std::string message;    // human-readable description
};

// Run all sanity checks. Returns issues in a stable order. An empty
// vector means "no problems found". The function does not mutate
// `state` and does not write to `state.logs` - it only observes.
//
// Current checks:
//   * "invalid_date"          - state.current_date.is_valid() == false
//   * "invalid_country_id"    - some country.id has no valid value
//   * "duplicate_country_id"  - two or more countries share the same id
std::vector<Issue> sanity_check(const core::GameState& state);

}  // namespace leviathan::systems::diagnostics

#endif  // LEVIATHAN_SYSTEMS_DIAGNOSTICS_HPP
