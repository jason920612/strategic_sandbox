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
};

// Execute one simulation run.
//
// Steps:
//   1. Load config from opts.config_path via DataLoader.
//   2. If opts.seed_override is set, replace config.seed.
//   3. Build GameState via make_game_state.
//   4. Log start (info).
//   5. Tick opts.days days. Log month and year boundaries (info)
//      via the TickResult flags; per-day logging is suppressed to
//      keep long-run logs manageable.
//   6. Log end (info).
//   7. Create output_dir (and any parents) if missing.
//   8. Write save.json (M0.8) and events.jsonl (M0.6) to the
//      resolved paths.
//   9. Return RunOutcome summary.
//
// Returns failure if config load fails, if opts.days < 0, or if any
// file write fails. opts.days == 0 is permitted (the simulation
// simply does not advance) but `--days` is REQUIRED at the CLI;
// parse_args() rejects the unset case.
core::Result<RunOutcome> run(const RunnerOptions& opts);

}  // namespace leviathan::systems::runner

#endif  // LEVIATHAN_SYSTEMS_RUNNER_HPP
