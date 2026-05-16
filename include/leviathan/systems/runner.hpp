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

#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/result.hpp"

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
core::Result<RunOutcome> run_state(core::GameState& state,
                                   const RunnerOptions& opts);

}  // namespace leviathan::systems::runner

#endif  // LEVIATHAN_SYSTEMS_RUNNER_HPP
