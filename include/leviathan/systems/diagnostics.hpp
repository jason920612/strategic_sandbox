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
// Per-faction snapshot (M1.16).
//
// Mirrors the M1.14 per-country shape. Same observation-only rule,
// same `std::scientific` + 17-digit double formatting, same snapshot
// cadence when wired through the runner (`--factions-csv PATH`).
// Faction-level CSV is a SEPARATE output stream; it does not change
// the summary CSV (M0.10) or the per-country CSV (M1.14), both of
// which keep their byte-identical determinism contracts.
// ---------------------------------------------------------------------------

// One row of the per-faction CSV. Mirrors `FactionState` runtime
// fields. `country_id_code` is included so external tooling can group
// rows by country without re-joining against another file.
struct FactionSummaryRow {
    core::GameDate date;
    std::string id_code;          // e.g. "GER_military"
    std::string country_id_code;  // e.g. "GER"
    std::string type;             // e.g. "military"
    double      support    = 0.0;
    double      influence  = 0.0;
    double      radicalism = 0.0;
    double      loyalty    = 0.0;
    double      resources  = 0.0;
};

// Build a FactionSummaryRow from `state` for `faction`. Pure
// observation; never mutates state.
//
// Returns failure if `faction` is not a valid index into
// `state.factions`. As with `country_snapshot`, the runner only calls
// this with indexes pulled from `state.factions.size()`, so failure is
// reserved for misuse / programmer errors.
core::Result<FactionSummaryRow> faction_snapshot(const core::GameState& state,
                                                 core::FactionId faction);

// Write the canonical per-faction CSV header. Always:
//   "date,id_code,country_id_code,type,support,influence,"
//   "radicalism,loyalty,resources\n"
// Pinned by tests; bumping a column here is breaking.
void write_faction_csv_header(std::ostream& out);

// Write one FactionSummaryRow as a CSV line. Same doubles policy as
// `write_country_csv_row`.
void write_faction_csv_row(std::ostream& out, const FactionSummaryRow& row);

// ---------------------------------------------------------------------------
// M2.10: programmatic state-comparison API.
//
// `compare_states` walks two GameStates field-by-field and returns a
// list of mismatches. Empty list == states match (modulo the
// deliberately-skipped fields below). Floating-point fields are
// compared with a caller-supplied tolerance; everything else uses
// strict equality.
//
// Used by replay verification (compare a fresh-loaded source save to
// the runner's --replay output), save round-trip tests, and any
// future automation that wants a "did this run produce the same
// state?" answer without diffing JSON blobs.
//
// SCOPE — fields compared (gameplay-relevant):
//   * current_date
//   * player_country
//   * countries[*]: identity strings (id_code, name, display_name),
//     every numeric field (gdp / tax_revenue / budget_balance /
//     legal_tax_burden / fiscal_capacity / administrative_efficiency /
//     central_control / corruption / stability / legitimacy /
//     military_power / threat_perception / last_gdp_growth_rate),
//     the budget block (7 categories), the government_authority
//     block (M2.16: bureaucratic_compliance, military_loyalty,
//     intelligence_capability, media_control), and active_policies
//     entries (policy_id_code + expires_on).
//   * factions[*]: identity strings (id_code, country_id_code, type),
//     every numeric field (support / influence / radicalism /
//     loyalty / resources), and preferred_policies element count.
//   * applied_commands[*]: applied_on date, command kind, and the
//     payload field that matches the kind (policy_id_code for
//     EnactPolicy, budget_category + budget_delta for AdjustBudget).
//   * interest_groups[*] (M3.1): identity strings (id_code, name),
//     kind, country (numeric handle), and the three behavioural
//     ratios (influence, loyalty, radicalism).
//
// SCOPE — fields DELIBERATELY skipped:
//   * rng: not part of gameplay comparison; M2 replay does not yet
//     reach into RNG, and a future divergent-RNG-path comparison
//     would need its own helper.
//   * logs: begin_tick / end_tick / month-rolled-over entries produce
//     boilerplate noise that almost always differs between a
//     pristine simulation and a replay. Replay equivalence cares
//     about end-state, not the log breadcrumbs.
//   * policies: immutable templates loaded from disk. If they differ
//     between two states, the scenario manifests or fixtures differ
//     — that's a config bug, not a divergence.
//   * provinces, events: still reserved-empty in M2; nothing to
//     compare.
//   * simulation_config (not in GameState; lives separately).
// ---------------------------------------------------------------------------

// One difference reported by `compare_states`.
struct StateMismatch {
    // Dotted/bracketed path to the differing field, e.g.
    //   "current_date"
    //   "countries[0].budget.military"
    //   "applied_commands[1].command.budget_delta"
    // The path mirrors how the M2.4 / M0.8 save JSON addresses the
    // field, so the same string is usable from CLI output.
    std::string field_path;

    // Human-readable description of the difference, including both
    // values and (for doubles) the tolerance used. Always non-empty
    // when this struct is emitted.
    std::string detail;
};

struct CompareOptions {
    // Floating-point fields compare equal iff
    // |a - b| <= double_tolerance. Default `1e-9` matches the
    // M0.8 save round-trip precision (doubles serialised with
    // setprecision(17) round-trip to <= 1 ULP, which is well below
    // 1e-9 for the value ranges this simulation uses).
    double double_tolerance = 1e-9;
};

// Walk `a` and `b` field-by-field; push one StateMismatch per
// difference into the returned vector, in canonical order (the order
// the documented scope above lists). Returns an empty vector when
// the two states match (modulo the deliberately-skipped fields).
//
// Pure observation: neither argument is mutated.
std::vector<StateMismatch> compare_states(const core::GameState& a,
                                          const core::GameState& b,
                                          const CompareOptions& opts = {});

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
