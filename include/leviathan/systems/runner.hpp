// Headless simulation runner.
//
// Wires together the systems built in M0.1 - M0.8 behind a tiny CLI.
// No new simulation abstractions are introduced here; the runner is
// orchestration only:
//   parse_args -> RunnerOptions
//   load_simulation_config (M0.7)
//   make_game_state        (M0.3)
//   advance_one_day        (M0.4) + selective logging (M0.6)
//   save                   (M0.8)
//   export_jsonl           (M0.6)
//
// Deterministic by construction: given the same RunnerOptions and the
// same config file, two runs produce byte-identical save.json and
// events.jsonl. This is the first end-to-end realisation of the
// determinism promise from RFC-000 §5 rule 10.

#ifndef LEVIATHAN_SYSTEMS_RUNNER_HPP
#define LEVIATHAN_SYSTEMS_RUNNER_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"
#include "leviathan/systems/diagnostics.hpp"

namespace leviathan::systems::runner {

// Parsed command-line options. parse_args() is responsible for
// populating this; run() consumes it.
//
// Required: `days`.
// All other fields have sensible defaults so a user can typically
// invoke the binary with only `--days N`.
struct RunnerOptions {
    std::filesystem::path                config_path = "data/config/simulation.json";
    int                                  days        = 0;     // 0 = unset -> error
    std::optional<std::uint64_t>         seed_override;       // overrides config.seed
    std::filesystem::path                output_dir  = "out";
    std::optional<std::filesystem::path> save_path;           // defaults to output_dir/save.json
    std::optional<std::filesystem::path> log_path;            // defaults to output_dir/events.jsonl
    std::optional<std::filesystem::path> summary_csv_path;    // unset = no CSV written
    std::optional<std::filesystem::path> scenario_path;       // unset = empty-world run (M1.11)
    std::optional<std::filesystem::path> countries_csv_path;  // unset = no per-country CSV (M1.14)
    std::optional<std::filesystem::path> factions_csv_path;   // unset = no per-faction CSV (M1.16)
    std::optional<std::string>           player_id_code;      // M2.1: --player COUNTRY_IDCODE; unset = headless run
    std::optional<std::filesystem::path> replay_path;         // M2.8: --replay PATH; load this save's command log and replay onto a fresh scenario
    bool                                 verify      = false; // M2.11: --verify; requires --replay; compare replayed state to source after end_tick
    bool                                 show_help   = false;
};

// Returns the help / usage text printed by --help and prepended to
// argument-error messages.
std::string usage_text();

// Parse a fully populated argv (including argv[0]). Returns
// RunnerOptions on success, or a failure Result with a human-readable
// message that names the offending flag.
//
// Special case: if --help is present, the returned options has
// show_help = true and other fields may be empty. Callers should
// check show_help first and print usage_text() rather than dispatch
// run().
core::Result<RunnerOptions> parse_args(int argc, const char* const* argv);

// Summary returned from run(). main() formats this for stdout.
struct RunOutcome {
    core::GameDate         start_date;
    core::GameDate         end_date;
    int                    days_advanced;
    std::size_t            log_count;
    std::filesystem::path  save_path;
    std::filesystem::path  log_path;
    std::optional<std::filesystem::path> summary_csv_path;
    std::size_t            summary_rows         = 0;
    std::size_t            sanity_issues_logged = 0;
    // M1.10: how many month-boundary monthly-pipeline ticks ran. With
    // an empty `state.countries`, `monthly::tick_all_countries` still
    // executes but processes zero countries; the counter increments
    // once per crossed month boundary regardless.
    int                    monthly_ticks        = 0;
    // M1.14: per-country diagnostic CSV output. Mirrors
    // summary_csv_path: passthrough from RunnerOptions so main() can
    // print it. `countries_csv_rows` counts the number of data rows
    // (NOT including the header) written across all snapshot points.
    std::optional<std::filesystem::path> countries_csv_path;
    std::size_t            countries_csv_rows   = 0;
    // M1.16: per-faction diagnostic CSV output. Same shape as the
    // M1.14 country variant; mirrors the same snapshot cadence.
    std::optional<std::filesystem::path> factions_csv_path;
    std::size_t            factions_csv_rows    = 0;
    // M2.8: count of commands replayed from the loaded save when
    // `--replay PATH` is set. Zero when --replay was not used. The
    // outcome's `days_advanced` / `monthly_ticks` fields are
    // populated by the underlying TickController as usual, so a
    // replay run reports the same shape as a normal run plus this
    // extra counter.
    int                    replay_commands_replayed = 0;
    // M2.11: mismatches reported by `diagnostics::compare_states`
    // when `--verify` is set alongside `--replay`. Empty when
    // --verify is not requested OR when the replayed state matches
    // the source. The runner does NOT fail the run on mismatch
    // — verification is informational; the caller (typically
    // main.cpp / a CI harness) decides what to do with the list.
    std::vector<diagnostics::StateMismatch> verify_mismatches;
};

// Execute one simulation run.
//
// Steps:
//   1. Load config from opts.config_path via DataLoader.
//   2. If opts.seed_override is set, replace config.seed.
//   3. Build GameState via make_game_state.
//   4. Delegate to run_state(state, opts) for the tick loop +
//      diagnostics + file writes.
//
// Returns failure if config load fails, if opts.days < 0, or if any
// file write fails. opts.days == 0 is permitted (the simulation
// simply does not advance) but `--days` is REQUIRED at the CLI;
// parse_args() rejects the unset case.
core::Result<RunOutcome> run(const RunnerOptions& opts);

// Same as run() but operates on a pre-built GameState. Used by tests
// that need to inject countries / factions / policies BEFORE the
// tick loop (the canonical M0.9 runner intentionally does not load
// country files - that's a future scenario-loader sub-milestone).
//
// On each day:
//   - advance_one_day (M0.4); month / year boundary log lines as M0.9.
//   - When TickResult.month_changed is true, call
//     monthly::tick_all_countries(state) (M1.9). Any sub-system
//     failure aborts the run with a failure Result; partial state
//     is left as-is (the M1.9 pipeline is documented as non-atomic).
//   - Snapshots for --summary-csv are taken at month boundaries.
//
// The monthly pipeline is invoked AFTER the month-changed log line
// so the log retains its canonical M0.9 ordering. RunOutcome.monthly_ticks
// counts month boundaries crossed (one per pipeline call). With an
// empty state.countries the pipeline still runs but processes 0.
//
// As of M2.2 this is a thin composition over `begin_tick` /
// `step_one_day` / `end_tick`. External drivers (interactive REPL,
// pause / step UI, future M2.3 command queue) can invoke those
// primitives directly to interleave their own work between days.
// run_state's behaviour is unchanged.
core::Result<RunOutcome> run_state(core::GameState& state,
                                   const RunnerOptions& opts);

// ============================================================================
// M2.2 - step-at-a-time primitives.
//
// Lets an outer driver pause / step / resume the simulation without losing
// the in-flight orchestration buffers (summary / country / faction CSV rows,
// monthly_ticks counter, captured start_date). The controller lives OUTSIDE
// the persistent GameState — it is never saved or loaded. A future M2.3+
// command queue will sit between begin_tick and step_one_day, mutating
// state with player commands before the next day advances.
// ============================================================================

// Runtime / orchestration state held between tick steps. Default-
// constructed before the first `begin_tick`. Fields are exposed for
// inspection by tests and future drivers; mutating them after
// `begin_tick` has run is unsupported and will desync the writes
// `end_tick` performs.
struct TickController {
    // Captured at begin_tick: state.current_date BEFORE any tick.
    core::GameDate start_date{};

    // Count of `month_changed` boundaries observed so far (one per
    // monthly_pipeline call). Mirrors `RunOutcome::monthly_ticks`.
    int monthly_ticks = 0;

    // Number of times `step_one_day` has been called since
    // `begin_tick`. Equals `RunOutcome::days_advanced` after
    // `end_tick`.
    int days_stepped = 0;

    // M0.10 / M1.14 / M1.16 snapshot row buffers. Filled at the
    // start, on each month boundary, and at end_tick. Empty when the
    // corresponding --*-csv flag is unset.
    std::vector<diagnostics::SummaryRow>        summary_rows;
    std::vector<diagnostics::CountrySummaryRow> country_rows;
    std::vector<diagnostics::FactionSummaryRow> faction_rows;

    // Lifecycle flags. begin_tick sets started=true; end_tick sets
    // ended=true. step_one_day / end_tick refuse to run if started is
    // false, and step_one_day / begin_tick refuse to run if ended is
    // true.
    bool started = false;
    bool ended   = false;
};

// Resolve --player (if set), capture state.current_date as
// `ctrl.start_date`, emit the "simulation start" log entry, and write
// the initial start-of-run snapshot row to each populated CSV buffer.
//
// Failure cases:
//   - ctrl.started is already true (double-begin)
//   - ctrl.ended is true (resurrecting a finished controller)
//   - opts.player_id_code is set but state.countries is empty
//   - opts.player_id_code is set but no country with that id_code
//     was loaded
core::Result<bool> begin_tick(core::GameState& state,
                              const RunnerOptions& opts,
                              TickController& ctrl);

// Advance the simulation by exactly one day. Emits month_changed /
// year_changed lifecycle logs as appropriate, runs
// `monthly::tick_all_countries` on month boundaries, increments
// `ctrl.monthly_ticks`, and appends snapshot rows on month boundaries
// when the matching CSV flag is set.
//
// Failure cases:
//   - ctrl.started is false (must call begin_tick first)
//   - ctrl.ended is true (controller already finalised)
//   - the M1.10 monthly pipeline fails
//
// On failure, partial day state is left in place (advance_one_day +
// boundary logs may have already happened). The function does not
// roll back; the M1.9 pipeline was already documented as non-atomic.
core::Result<bool> step_one_day(core::GameState& state,
                                const RunnerOptions& opts,
                                TickController& ctrl);

// Emit "simulation end" log, run sanity_check, log every issue,
// append the final post-sanity snapshot row to each populated CSV
// buffer, resolve output paths, write save.json / events.jsonl /
// summary.csv / countries.csv / factions.csv as appropriate, and
// return the populated RunOutcome.
//
// Sets `ctrl.ended = true` on success. Failure cases:
//   - ctrl.started is false (cannot end an unstarted controller)
//   - ctrl.ended is true (double-end)
//   - any file-write step fails
core::Result<RunOutcome> end_tick(core::GameState& state,
                                  const RunnerOptions& opts,
                                  TickController& ctrl);

}  // namespace leviathan::systems::runner

#endif  // LEVIATHAN_SYSTEMS_RUNNER_HPP
