// RCR-1: Annual world statistics (RFC-090 §3.9 / RFC-010 §5).
//
// New `leviathan::systems::annual_stats` module that aggregates
// per-year world statistics into a deterministic byte-stable CSV
// row format. The runner emits one row at every year-boundary
// crossing (and one final row at end_tick if the simulation
// hasn't already crossed a boundary on the last day) into the new
// unconditional artefact `<output_dir>/annual_world_stats.csv`.
//
// Adding `annual_world_stats.csv` bumps the runner's unconditional
// artefact contract from 10 to 11. Integration tests that count
// artefacts (M5.9 integration tests and the RCR-1 compliance
// integration test) are updated to assert 11 in lockstep.
//
// Determinism:
//   - Per-row aggregates are pure functions of `state` at
//     snapshot time.
//   - Double formatting uses `std::scientific` +
//     `setprecision(17)` per the M1.14 / M1.16 byte-stable
//     pattern.
//   - Vector-order iteration over `state.countries` for any
//     per-country fold; no sort.
//
// What the module does NOT do:
//   - No new state field. `total_gdp` etc. are derived at
//     snapshot time.
//   - No RNG.
//   - No log emission.
//   - No mutation of any other system's surface — annual stats
//     is a read-only diagnostic, layered on top of the existing
//     monthly pipeline.

#ifndef LEVIATHAN_SYSTEMS_ANNUAL_STATS_HPP
#define LEVIATHAN_SYSTEMS_ANNUAL_STATS_HPP

#include <cstddef>
#include <ostream>
#include <string>

#include "leviathan/core/game_state.hpp"

namespace leviathan::systems::annual_stats {

// One annual-stats CSV row. Fields are stable for forward-
// compatibility — append-only additions on the right.
struct AnnualRow {
    int         year             = 0;     // simulated calendar year at snapshot
    std::size_t country_count    = 0;     // state.countries.size()
    double      avg_stability    = 0.0;   // mean over countries
    double      avg_legitimacy   = 0.0;   // mean over countries
    double      avg_gdp          = 0.0;   // mean over countries
    double      avg_corruption   = 0.0;   // mean over countries
    double      total_gdp        = 0.0;   // sum over countries
    std::size_t event_history_count = 0;  // state.event_history.size()
};

// Compute a row from a snapshot of `state`. `year` is supplied by
// the caller (the runner passes the year of the just-crossed
// year-boundary). Empty `state.countries` yields zero counts and
// zeroed averages (no division by zero — avg fields stay 0.0).
AnnualRow snapshot(const core::GameState& state, int year);

// Byte-stable CSV header. Trailing '\n'. Mirrors the M1.14 /
// M1.16 CSV conventions.
//
// Columns:
//   date,year,country_count,avg_stability,avg_legitimacy,
//   avg_gdp,avg_corruption,total_gdp,event_history_count
//
// `date` is the snapshot date in ISO-8601 (YYYY-MM-DD) form —
// matches the M0.10 summary.csv leading column convention.
std::string write_csv_header();

// Format `row` as a CSV line with trailing '\n'. `snapshot_date`
// is rendered as the first column.
std::string write_csv_row(const core::GameDate& snapshot_date,
                          const AnnualRow&      row);

}  // namespace leviathan::systems::annual_stats

#endif  // LEVIATHAN_SYSTEMS_ANNUAL_STATS_HPP
