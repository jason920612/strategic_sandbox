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
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

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
// Per-country snapshot (M1.14).
//
// The summary CSV intentionally collapses every country to a count.
// M1.14 adds a SEPARATE per-country snapshot type and a SEPARATE CSV
// format so callers (the runner via --countries-csv) can inspect
// `last_gdp_growth_rate` and the other per-country runtime numerics
// without round-tripping a save file. The existing SummaryRow + 4-
// column summary CSV are unchanged so M0.10's byte-identical
// determinism contract still holds.
// ---------------------------------------------------------------------------

// One row of the per-country CSV.
struct CountrySummaryRow {
    core::GameDate date;
    std::string id_code;              // e.g. "GER"
    double       gdp                  = 0.0;
    double       tax_revenue          = 0.0;
    double       budget_balance       = 0.0;
    double       stability            = 0.0;
    double       legitimacy           = 0.0;
    double       last_gdp_growth_rate = 0.0;   // the M1.14 motivator
};

// Build a CountrySummaryRow from `state` for `country`. Pure
// observation; never mutates state.
//
// Returns failure if `country` is not a valid index into
// `state.countries`. The runner only calls this for indexes it
// iterated from state.countries.size(), so failure is reserved
// for misuse / programmer errors.
core::Result<CountrySummaryRow> country_snapshot(const core::GameState& state,
                                                 core::CountryId country);

// Write the canonical per-country CSV header. Always:
//   "date,id_code,gdp,tax_revenue,budget_balance,"
//   "stability,legitimacy,last_gdp_growth_rate\n"
// Like the summary header, this is pinned by tests; bumping a column
// here is breaking.
void write_country_csv_header(std::ostream& out);

// Write one CountrySummaryRow as a CSV line.
//
// Doubles are formatted with std::setprecision(17) so the round-trip
// from double -> text -> double is exact and same-process / same-
// state always produces byte-identical text. Trailing zeros are
// kept by `std::scientific`; not pretty for human reading, but the
// determinism contract is more important than aesthetics here.
void write_country_csv_row(std::ostream& out, const CountrySummaryRow& row);

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
