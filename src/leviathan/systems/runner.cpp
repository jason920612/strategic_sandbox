#include "leviathan/systems/runner.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/core/simulation_config.hpp"
#include "leviathan/systems/commands.hpp"
#include "leviathan/systems/data_loader.hpp"
#include "leviathan/systems/diagnostics.hpp"
#include "leviathan/systems/logging_system.hpp"
#include "leviathan/systems/monthly_pipeline.hpp"
#include "leviathan/systems/save_system.hpp"
#include "leviathan/systems/scenario_loader.hpp"
#include "leviathan/systems/svg_export.hpp"
#include "leviathan/systems/time_system.hpp"

namespace leviathan::systems::runner {

namespace {

// ---- arg parsing helpers --------------------------------------------------

core::Result<std::uint64_t> parse_uint64(std::string_view text,
                                         std::string_view flag) {
    if (text.empty()) {
        return core::Result<std::uint64_t>::failure(
            std::string(flag) + ": value is empty");
    }
    std::uint64_t out = 0;
    for (char c : text) {
        if (c < '0' || c > '9') {
            return core::Result<std::uint64_t>::failure(
                std::string(flag) + ": '" + std::string(text) +
                "' is not a non-negative integer");
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
        // Detect overflow before multiplying.
        constexpr std::uint64_t kMax = static_cast<std::uint64_t>(-1);
        if (out > (kMax - digit) / 10) {
            return core::Result<std::uint64_t>::failure(
                std::string(flag) + ": '" + std::string(text) +
                "' overflows uint64");
        }
        out = out * 10 + digit;
    }
    return core::Result<std::uint64_t>::success(out);
}

core::Result<int> parse_positive_int(std::string_view text,
                                     std::string_view flag) {
    auto u = parse_uint64(text, flag);
    if (!u) return core::Result<int>::failure(std::move(u.error()));
    constexpr std::uint64_t kIntMax =
        static_cast<std::uint64_t>(2147483647);  // INT_MAX
    if (u.value() > kIntMax) {
        return core::Result<int>::failure(
            std::string(flag) + ": '" + std::string(text) +
            "' exceeds INT_MAX");
    }
    return core::Result<int>::success(static_cast<int>(u.value()));
}

// M2.13: parse a non-negative finite double from `text`. Exception-
// free; uses std::strtod with full-consumption + finiteness checks.
core::Result<double> parse_nonneg_double(std::string_view text,
                                         std::string_view flag) {
    if (text.empty()) {
        return core::Result<double>::failure(
            std::string(flag) + ": value is empty");
    }
    // strtod wants a null-terminated string; string_view may not be.
    const std::string buf(text);
    char* end = nullptr;
    const double v = std::strtod(buf.c_str(), &end);
    // Must consume the entire input (no trailing garbage like "1.5x").
    if (end == buf.c_str() || end == nullptr || *end != '\0') {
        return core::Result<double>::failure(
            std::string(flag) + ": '" + std::string(text) +
            "' is not a valid floating-point number");
    }
    if (!std::isfinite(v)) {
        return core::Result<double>::failure(
            std::string(flag) + ": '" + std::string(text) +
            "' is not finite (NaN or Inf not allowed)");
    }
    if (v < 0.0) {
        return core::Result<double>::failure(
            std::string(flag) + ": '" + std::string(text) +
            "' must be >= 0");
    }
    return core::Result<double>::success(v);
}

// ---- file write helper ----------------------------------------------------

core::Result<bool> write_string_to_file(const std::filesystem::path& path,
                                        const std::string& content) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return core::Result<bool>::failure(
                path.string() + ": cannot create parent directory: " + ec.message());
        }
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        return core::Result<bool>::failure(
            path.string() + ": cannot open file for writing");
    }
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!f.good()) {
        return core::Result<bool>::failure(path.string() + ": write failed");
    }
    return core::Result<bool>::success(true);
}

}  // namespace

std::string usage_text() {
    return
        "Usage: leviathan [options]\n"
        "\n"
        "Options:\n"
        "  --config PATH       Simulation config JSON to load.\n"
        "                      Defaults to data/config/simulation.json.\n"
        "  --days N            How many days to advance. REQUIRED. N >= 0.\n"
        "  --seed N            Override the seed from the config. uint64.\n"
        "  --output DIR        Directory for output artefacts. Created if\n"
        "                      missing. Defaults to out/.\n"
        "  --save PATH         Save file path. Defaults to <output>/save.json.\n"
        "  --log PATH          JSONL log file path. Defaults to\n"
        "                      <output>/events.jsonl.\n"
        "  --summary-csv PATH  Write a per-snapshot CSV summary to PATH.\n"
        "                      Snapshots are taken at start, each month\n"
        "                      boundary, and after sanity checks at the end.\n"
        "                      If omitted, no CSV is written.\n"
        "  --scenario PATH     Scenario manifest JSON to load into GameState\n"
        "                      before ticking (M1.11). Without this flag\n"
        "                      the runner ticks an empty world.\n"
        "  --countries-csv PATH  Write a per-country CSV summary to PATH\n"
        "                      (M1.14). One row per country per snapshot\n"
        "                      point (same cadence as --summary-csv).\n"
        "                      Surfaces gdp / stability / last_gdp_growth_rate\n"
        "                      etc. so multi-month runs are inspectable\n"
        "                      without round-tripping the save.\n"
        "  --factions-csv PATH   Write a per-faction CSV summary to PATH\n"
        "                      (M1.16). One row per faction per snapshot\n"
        "                      point (same cadence as --summary-csv).\n"
        "                      Surfaces support / loyalty / radicalism /\n"
        "                      influence / resources per faction.\n"
        "  --player ID_CODE    Mark COUNTRY_IDCODE as the player country\n"
        "                      (M2.1). Resolved against the loaded\n"
        "                      scenario after --scenario completes; fails\n"
        "                      loudly if the id_code does not match any\n"
        "                      loaded country. Without this flag the run\n"
        "                      is headless (player_country = invalid).\n"
        "  --replay PATH       Replay the command log of the save at PATH\n"
        "                      onto a fresh scenario (M2.8). Requires\n"
        "                      --scenario. If --player is not also set,\n"
        "                      inherits player_country from the loaded\n"
        "                      save. The runner advances day-by-day so\n"
        "                      the M1.10 monthly pipeline fires naturally\n"
        "                      between commands; the resulting save is\n"
        "                      written to the normal output paths.\n"
        "  --verify            After --replay, run diagnostics::compare_states\n"
        "                      against the loaded source and populate\n"
        "                      RunOutcome.verify_mismatches (M2.11).\n"
        "                      Requires --replay. Informational: any\n"
        "                      mismatches are reported but do not fail\n"
        "                      the run (exit code stays 0).\n"
        "  --verify-strict     Make the process exit with EXIT_FAILURE\n"
        "                      when --verify detects any mismatches\n"
        "                      (M2.12). Requires --verify. run() still\n"
        "                      succeeds at the library level — the\n"
        "                      exit-code policy is owned by main(); the\n"
        "                      full mismatch list is still printed to\n"
        "                      stdout before the non-zero exit so CI\n"
        "                      logs capture everything.\n"
        "  --verify-tolerance N\n"
        "                      Override the floating-point tolerance\n"
        "                      passed to diagnostics::compare_states\n"
        "                      (M2.13). Default is 1e-9; useful when a\n"
        "                      long-running simulation has legitimate\n"
        "                      cumulative drift the tight default would\n"
        "                      false-positive on. N must be a finite\n"
        "                      non-negative double. Requires --verify.\n"
        "  --target-date YYYY-MM-DD\n"
        "                      Stop the replay at this date (M2.14).\n"
        "                      Log entries with applied_on > target are\n"
        "                      skipped; after replay, the time system\n"
        "                      is advanced day-by-day until\n"
        "                      current_date == target so the resulting\n"
        "                      save reflects exactly that calendar day.\n"
        "                      Must be a valid Gregorian date and must\n"
        "                      not be before the scenario start date.\n"
        "                      Requires --replay.\n"
        "  --help              Show this help and exit.\n";
}

// ===========================================================================
// parse_args
// ===========================================================================

core::Result<RunnerOptions> parse_args(int argc, const char* const* argv) {
    RunnerOptions opts;
    bool days_seen = false;

    auto need_value = [&](int i, std::string_view flag) -> core::Result<std::string_view> {
        if (i + 1 >= argc) {
            return core::Result<std::string_view>::failure(
                std::string(flag) + " requires a value");
        }
        return core::Result<std::string_view>::success(std::string_view(argv[i + 1]));
    };

    for (int i = 1; i < argc; ++i) {
        const std::string_view a(argv[i]);

        if (a == "--help" || a == "-h") {
            opts.show_help = true;
            return core::Result<RunnerOptions>::success(std::move(opts));
        }

        if (a == "--config") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            opts.config_path = std::filesystem::path(std::string(v.value()));
            ++i;
        } else if (a == "--days") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            auto n = parse_positive_int(v.value(), a);
            if (!n) return core::Result<RunnerOptions>::failure(std::move(n.error()));
            opts.days  = n.value();
            days_seen  = true;
            ++i;
        } else if (a == "--seed") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            auto n = parse_uint64(v.value(), a);
            if (!n) return core::Result<RunnerOptions>::failure(std::move(n.error()));
            opts.seed_override = n.value();
            ++i;
        } else if (a == "--output") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            opts.output_dir = std::filesystem::path(std::string(v.value()));
            ++i;
        } else if (a == "--save") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            opts.save_path = std::filesystem::path(std::string(v.value()));
            ++i;
        } else if (a == "--log") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            opts.log_path = std::filesystem::path(std::string(v.value()));
            ++i;
        } else if (a == "--summary-csv") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            opts.summary_csv_path = std::filesystem::path(std::string(v.value()));
            ++i;
        } else if (a == "--scenario") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            opts.scenario_path = std::filesystem::path(std::string(v.value()));
            ++i;
        } else if (a == "--countries-csv") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            opts.countries_csv_path = std::filesystem::path(std::string(v.value()));
            ++i;
        } else if (a == "--factions-csv") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            opts.factions_csv_path = std::filesystem::path(std::string(v.value()));
            ++i;
        } else if (a == "--player") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            opts.player_id_code = std::string(v.value());
            ++i;
        } else if (a == "--replay") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            opts.replay_path = std::filesystem::path(std::string(v.value()));
            ++i;
        } else if (a == "--verify") {
            opts.verify = true;
        } else if (a == "--verify-strict") {
            opts.verify_strict = true;
        } else if (a == "--verify-tolerance") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            auto t = parse_nonneg_double(v.value(), a);
            if (!t) return core::Result<RunnerOptions>::failure(std::move(t.error()));
            opts.verify_tolerance = t.value();
            ++i;
        } else if (a == "--target-date") {
            auto v = need_value(i, a);
            if (!v) return core::Result<RunnerOptions>::failure(std::move(v.error()));
            auto d = core::GameDate::parse(v.value());
            if (!d) {
                return core::Result<RunnerOptions>::failure(
                    std::string(a) + ": '" + std::string(v.value()) +
                    "' is not a valid Gregorian date (" + std::move(d.error()) + ")");
            }
            opts.target_date = d.value();
            ++i;
        } else {
            return core::Result<RunnerOptions>::failure(
                "unknown flag: " + std::string(a));
        }
    }

    if (!days_seen) {
        return core::Result<RunnerOptions>::failure(
            "--days is required (run with --help for usage)");
    }
    // M2.11: --verify is only meaningful inside --replay.
    if (opts.verify && !opts.replay_path.has_value()) {
        return core::Result<RunnerOptions>::failure(
            "--verify requires --replay (--verify is a no-op without"
            " a source save to compare the replayed state against)");
    }
    // M2.12: --verify-strict only makes sense alongside --verify.
    if (opts.verify_strict && !opts.verify) {
        return core::Result<RunnerOptions>::failure(
            "--verify-strict requires --verify (strict mode is a"
            " policy on top of the verify step; without --verify"
            " there are no mismatches to gate on)");
    }
    // M2.13: --verify-tolerance only meaningful inside --verify.
    if (opts.verify_tolerance.has_value() && !opts.verify) {
        return core::Result<RunnerOptions>::failure(
            "--verify-tolerance requires --verify (the tolerance"
            " is consumed by compare_states inside the verify step;"
            " without --verify there is nothing to tune)");
    }
    // M2.14: --target-date only meaningful inside --replay.
    if (opts.target_date.has_value() && !opts.replay_path.has_value()) {
        return core::Result<RunnerOptions>::failure(
            "--target-date requires --replay (the target date"
            " scopes the replay window; without --replay there is"
            " no log to truncate and no replay loop to extend)");
    }
    return core::Result<RunnerOptions>::success(std::move(opts));
}

// ===========================================================================
// run
// ===========================================================================

core::Result<RunOutcome> run(const RunnerOptions& opts) {
    namespace dl = leviathan::systems::data_loader;

    if (opts.days < 0) {
        return core::Result<RunOutcome>::failure("--days must be >= 0");
    }

    // ---- Load config ------------------------------------------------------
    auto cfg_r = dl::load_simulation_config(opts.config_path);
    if (!cfg_r) {
        return core::Result<RunOutcome>::failure(std::move(cfg_r.error()));
    }
    auto cfg = std::move(cfg_r).value();
    if (opts.seed_override.has_value()) {
        cfg.seed = opts.seed_override.value();
    }

    // ---- Build state ------------------------------------------------------
    auto state = core::make_game_state(cfg);

    // ---- Optional scenario load (M1.11) -----------------------------------
    // When --scenario PATH is set, populate state.countries / factions /
    // policies BEFORE the tick loop runs so the monthly pipeline actually
    // has something to mutate. Without it the runner ticks an empty
    // world (M0.9 behaviour) and the M1.10 pipeline processes 0 countries
    // per call.
    if (opts.scenario_path.has_value()) {
        namespace sl = leviathan::systems::scenario_loader;
        auto load_r = sl::load_into_state(state, opts.scenario_path.value());
        if (!load_r) {
            return core::Result<RunOutcome>::failure(std::move(load_r.error()));
        }
    }

    // ---- Optional replay mode (M2.8) --------------------------------------
    // `--replay PATH` loads a save, then replays its applied_commands log
    // onto the fresh scenario state via M2.7 replay_with_time. The user
    // diffs the resulting save against the source themselves (the CLI
    // does NOT auto-compare).
    if (opts.replay_path.has_value()) {
        namespace ss  = leviathan::systems::save_system;
        namespace cmd = leviathan::systems::commands;

        // Replay requires a scenario-loaded baseline state. Reject
        // loudly rather than silently replay onto an empty world.
        if (state.countries.empty()) {
            return core::Result<RunOutcome>::failure(
                "--replay " + opts.replay_path.value().string() +
                ": --scenario is required to provide the fresh state"
                " baseline for replay (the replayed save's countries"
                " are NOT reused)");
        }

        // M2.14: validate --target-date against the scenario start
        // BEFORE any artefact-producing work. `state.current_date`
        // at this point is the scenario start date because no tick
        // has run yet.
        if (opts.target_date.has_value() &&
            opts.target_date.value() < state.current_date) {
            return core::Result<RunOutcome>::failure(
                "--target-date " + opts.target_date.value().to_string() +
                " is before the scenario start date " +
                state.current_date.to_string() +
                " (target must be on or after the scenario start)");
        }

        // Load the save being replayed.
        auto loaded_r = ss::load(opts.replay_path.value());
        if (!loaded_r) {
            return core::Result<RunOutcome>::failure(
                "--replay: " + std::move(loaded_r.error()));
        }
        const auto loaded = std::move(loaded_r).value();

        // Auto-inherit player_country from the loaded save when the
        // user didn't pass --player. Matches the most common replay
        // usage (replay a save without re-specifying the player).
        // begin_tick leaves state.player_country alone when
        // opts.player_id_code is unset, so this assignment survives.
        if (!opts.player_id_code.has_value()) {
            state.player_country = loaded.player_country;
        }

        TickController ctrl;
        auto begin_r = begin_tick(state, opts, ctrl);
        if (!begin_r) {
            return core::Result<RunOutcome>::failure(std::move(begin_r.error()));
        }

        // M2.14: when --target-date is set, replay only entries with
        // applied_on <= target_date. The log is monotonic non-
        // decreasing (M2.7 contract) so a single forward pass is
        // sufficient — once an entry is past the target, every
        // subsequent entry is too.
        const auto* log_ptr = &loaded.applied_commands;
        std::vector<core::AppliedPlayerCommand> truncated;
        if (opts.target_date.has_value()) {
            const auto td = opts.target_date.value();
            for (const auto& e : loaded.applied_commands) {
                if (e.applied_on > td) break;
                truncated.push_back(e);
            }
            log_ptr = &truncated;
        }

        auto replay_r = cmd::replay_with_time(state, opts, ctrl, *log_ptr);
        if (!replay_r) {
            return core::Result<RunOutcome>::failure(std::move(replay_r.error()));
        }

        // M2.14: after replay, advance the time system day-by-day
        // until current_date == target_date. The natural M1.10 monthly
        // pipeline boundary handling falls out of `step_one_day`.
        // This loop is a no-op when target_date <= the last replayed
        // entry's applied_on (or when --target-date isn't set).
        if (opts.target_date.has_value()) {
            const auto td = opts.target_date.value();
            while (state.current_date < td) {
                auto step_r = step_one_day(state, opts, ctrl);
                if (!step_r) {
                    return core::Result<RunOutcome>::failure(
                        "--target-date " + td.to_string() +
                        ": step_one_day failed advancing toward target: " +
                        std::move(step_r.error()));
                }
            }
        }

        auto end_r = end_tick(state, opts, ctrl);
        if (!end_r) {
            return core::Result<RunOutcome>::failure(std::move(end_r.error()));
        }
        auto outcome = std::move(end_r).value();
        outcome.replay_commands_replayed = replay_r.value().commands_replayed;

        // M2.11: optional verify — compare the replayed state to the
        // loaded source via diagnostics::compare_states. Informational
        // only; the run still succeeds regardless of mismatch count.
        // M2.13: when --verify-tolerance is set, override the default
        // double_tolerance on the CompareOptions struct.
        if (opts.verify) {
            namespace dg = leviathan::systems::diagnostics;
            dg::CompareOptions cmp_opts;
            if (opts.verify_tolerance.has_value()) {
                cmp_opts.double_tolerance = opts.verify_tolerance.value();
            }
            outcome.verify_mismatches =
                dg::compare_states(state, loaded, cmp_opts);
        }

        return core::Result<RunOutcome>::success(std::move(outcome));
    }

    return run_state(state, opts);
}

// ===========================================================================
// M2.2: tick primitives — begin_tick / step_one_day / end_tick.
// ===========================================================================

namespace {

// Append one CountrySummaryRow per country to the controller's
// country buffer at the current GameState time. Pulled out so every
// snapshot point (start, per-month, final post-sanity) shares one
// call site.
core::Result<bool> snapshot_all_countries(const core::GameState& s,
                                          TickController& ctrl) {
    namespace dg = leviathan::systems::diagnostics;
    for (std::size_t i = 0; i < s.countries.size(); ++i) {
        const core::CountryId id{static_cast<int>(i)};
        auto r = dg::country_snapshot(s, id);
        if (!r) {
            return core::Result<bool>::failure(std::move(r.error()));
        }
        ctrl.country_rows.push_back(std::move(r).value());
    }
    return core::Result<bool>::success(true);
}

// Same shape as above for the per-faction snapshot.
core::Result<bool> snapshot_all_factions(const core::GameState& s,
                                         TickController& ctrl) {
    namespace dg = leviathan::systems::diagnostics;
    for (std::size_t i = 0; i < s.factions.size(); ++i) {
        const core::FactionId id{static_cast<int>(i)};
        auto r = dg::faction_snapshot(s, id);
        if (!r) {
            return core::Result<bool>::failure(std::move(r.error()));
        }
        ctrl.faction_rows.push_back(std::move(r).value());
    }
    return core::Result<bool>::success(true);
}

// M3.5: same shape for interest groups. Unlike the per-country /
// per-faction variants this runs unconditionally on every snapshot
// point — interest_groups.csv is always written, header-only when
// `state.interest_groups` is empty.
core::Result<bool> snapshot_all_interest_groups(const core::GameState& s,
                                                TickController& ctrl) {
    namespace dg = leviathan::systems::diagnostics;
    for (std::size_t i = 0; i < s.interest_groups.size(); ++i) {
        auto r = dg::interest_group_snapshot(s, i);
        if (!r) {
            return core::Result<bool>::failure(std::move(r.error()));
        }
        ctrl.interest_group_rows.push_back(std::move(r).value());
    }
    return core::Result<bool>::success(true);
}

}  // namespace

core::Result<bool> begin_tick(core::GameState& state,
                              const RunnerOptions& opts,
                              TickController& ctrl) {
    namespace lg = leviathan::systems::logging;
    namespace dg = leviathan::systems::diagnostics;

    if (ctrl.started) {
        return core::Result<bool>::failure(
            "begin_tick: controller already started (double-begin)");
    }
    if (ctrl.ended) {
        return core::Result<bool>::failure(
            "begin_tick: controller already ended; build a fresh"
            " TickController to start a new run");
    }

    // M2.1: resolve --player COUNTRY_IDCODE against the loaded scenario.
    // Must run BEFORE the start log / first snapshot so a bad id_code
    // aborts cleanly without emitting any artefact.
    if (opts.player_id_code.has_value()) {
        const std::string& want = opts.player_id_code.value();
        if (state.countries.empty()) {
            return core::Result<bool>::failure(
                "--player " + want +
                " requires a non-empty state.countries (typically via"
                " --scenario)");
        }
        bool found = false;
        for (std::size_t i = 0; i < state.countries.size(); ++i) {
            if (state.countries[i].id_code == want) {
                state.player_country =
                    core::CountryId{static_cast<int>(i)};
                found = true;
                break;
            }
        }
        if (!found) {
            return core::Result<bool>::failure(
                "--player " + want +
                ": no country with that id_code in the loaded scenario");
        }
    }

    ctrl.start_date = state.current_date;

    // Initial log. Metadata is intentionally path-independent so two
    // runs with the same options produce byte-identical event logs.
    lg::log_info(state, "lifecycle", "runner", "simulation start",
                 {{"days_requested", std::to_string(opts.days)},
                  {"seed",           std::to_string(state.rng.seed)}});

    if (opts.summary_csv_path.has_value()) {
        ctrl.summary_rows.push_back(dg::snapshot(state));
    }
    if (opts.countries_csv_path.has_value()) {
        auto r = snapshot_all_countries(state, ctrl);
        if (!r) return r;
    }
    if (opts.factions_csv_path.has_value()) {
        auto r = snapshot_all_factions(state, ctrl);
        if (!r) return r;
    }
    // M3.5: interest_groups.csv is always written, so we snapshot at
    // every cadence point regardless of any opt-in flag.
    {
        auto r = snapshot_all_interest_groups(state, ctrl);
        if (!r) return r;
    }

    ctrl.started = true;
    return core::Result<bool>::success(true);
}

core::Result<bool> step_one_day(core::GameState& state,
                                const RunnerOptions& opts,
                                TickController& ctrl) {
    namespace lt = leviathan::systems::time;
    namespace lg = leviathan::systems::logging;
    namespace dg = leviathan::systems::diagnostics;
    namespace mp = leviathan::systems::monthly;

    if (!ctrl.started) {
        return core::Result<bool>::failure(
            "step_one_day: controller has not been started (call"
            " begin_tick first)");
    }
    if (ctrl.ended) {
        return core::Result<bool>::failure(
            "step_one_day: controller already ended");
    }

    const lt::TickResult tr = lt::advance_one_day(state);
    if (tr.month_changed) {
        lg::log_info(state, "time", "runner", "month rolled over");
    }
    if (tr.year_changed) {
        lg::log_info(state, "time", "runner", "year rolled over");
    }
    // M1.10: monthly pipeline runs at each month boundary, AFTER the
    // month-rolled-over log line so the canonical M0.9 log ordering
    // is preserved. The pipeline itself writes no logs.
    //
    // M3.6: drain the monthly outcome's formula-trace vectors into
    // the controller's per-run buffers. Empty when no country was
    // updated by the corresponding system this tick.
    if (tr.month_changed) {
        auto pipe_r = mp::tick_all_countries(state);
        if (!pipe_r) {
            return core::Result<bool>::failure(
                "monthly pipeline failed on " +
                state.current_date.to_string() + ": " +
                std::move(pipe_r.error()));
        }
        auto& outcome = pipe_r.value();
        for (auto& row : outcome.interest_group_country_feedback_trace_rows) {
            ctrl.interest_group_country_feedback_rows.push_back(std::move(row));
        }
        for (auto& row : outcome.interest_group_authority_pressure_trace_rows) {
            ctrl.interest_group_authority_pressure_rows.push_back(std::move(row));
        }
        ++ctrl.monthly_ticks;
    }
    if (opts.summary_csv_path.has_value() && tr.month_changed) {
        ctrl.summary_rows.push_back(dg::snapshot(state));
    }
    if (opts.countries_csv_path.has_value() && tr.month_changed) {
        auto r = snapshot_all_countries(state, ctrl);
        if (!r) return r;
    }
    if (opts.factions_csv_path.has_value() && tr.month_changed) {
        auto r = snapshot_all_factions(state, ctrl);
        if (!r) return r;
    }
    if (tr.month_changed) {
        auto r = snapshot_all_interest_groups(state, ctrl);
        if (!r) return r;
    }

    ++ctrl.days_stepped;
    return core::Result<bool>::success(true);
}

core::Result<RunOutcome> end_tick(core::GameState& state,
                                  const RunnerOptions& opts,
                                  TickController& ctrl) {
    namespace lg  = leviathan::systems::logging;
    namespace ss  = leviathan::systems::save_system;
    namespace dg  = leviathan::systems::diagnostics;
    namespace svg = leviathan::systems::svg_export;

    if (!ctrl.started) {
        return core::Result<RunOutcome>::failure(
            "end_tick: controller has not been started (call"
            " begin_tick first)");
    }
    if (ctrl.ended) {
        return core::Result<RunOutcome>::failure(
            "end_tick: controller already ended (double-end)");
    }

    lg::log_info(state, "lifecycle", "runner", "simulation end",
                 {{"days_advanced", std::to_string(ctrl.days_stepped)}});

    // ---- Sanity checks ---------------------------------------------------
    // Run AFTER the end log but BEFORE the final snapshot, so any
    // issues become part of the log stream and are reflected in the
    // final CSV row's log_count.
    const auto issues = dg::sanity_check(state);
    for (const auto& iss : issues) {
        lg::log_error(state, "diagnostics", "runner", iss.message,
                      {{"code", iss.code}});
    }

    if (opts.summary_csv_path.has_value()) {
        ctrl.summary_rows.push_back(dg::snapshot(state));
    }
    if (opts.countries_csv_path.has_value()) {
        auto r = snapshot_all_countries(state, ctrl);
        if (!r) {
            return core::Result<RunOutcome>::failure(std::move(r.error()));
        }
    }
    if (opts.factions_csv_path.has_value()) {
        auto r = snapshot_all_factions(state, ctrl);
        if (!r) {
            return core::Result<RunOutcome>::failure(std::move(r.error()));
        }
    }
    {
        auto r = snapshot_all_interest_groups(state, ctrl);
        if (!r) {
            return core::Result<RunOutcome>::failure(std::move(r.error()));
        }
    }

    // ---- Resolve output paths --------------------------------------------
    const auto save_path = opts.save_path.value_or(opts.output_dir / "save.json");
    const auto log_path  = opts.log_path.value_or(opts.output_dir / "events.jsonl");
    // M3.5: interest_groups.csv is always written. The path defaults
    // to <output_dir>/interest_groups.csv, mirroring save / log.
    const auto interest_groups_csv_path =
        opts.interest_groups_csv_path.value_or(
            opts.output_dir / "interest_groups.csv");
    // M3.6: two formula-trace CSVs, also always written. Default
    // paths sit next to interest_groups.csv under output_dir.
    const auto interest_group_country_feedback_csv_path =
        opts.interest_group_country_feedback_csv_path.value_or(
            opts.output_dir / "interest_group_country_feedback.csv");
    const auto interest_group_authority_pressure_csv_path =
        opts.interest_group_authority_pressure_csv_path.value_or(
            opts.output_dir / "interest_group_authority_pressure.csv");
    // M4.2: provinces.svg is always written. The path defaults to
    // <output_dir>/provinces.svg, mirroring the M3.5 / M3.6
    // unconditional pattern.
    const auto provinces_svg_path =
        opts.provinces_svg_path.value_or(
            opts.output_dir / "provinces.svg");

    auto save_r = ss::save(state, save_path);
    if (!save_r) {
        return core::Result<RunOutcome>::failure(std::move(save_r.error()));
    }

    std::ostringstream ss_log;
    lg::export_jsonl(ss_log, state);
    auto log_w = write_string_to_file(log_path, ss_log.str());
    if (!log_w) {
        return core::Result<RunOutcome>::failure(std::move(log_w.error()));
    }

    if (opts.summary_csv_path.has_value()) {
        std::ostringstream csv;
        dg::write_csv_header(csv);
        for (const auto& row : ctrl.summary_rows) {
            dg::write_csv_row(csv, row);
        }
        auto csv_w = write_string_to_file(opts.summary_csv_path.value(), csv.str());
        if (!csv_w) {
            return core::Result<RunOutcome>::failure(std::move(csv_w.error()));
        }
    }
    if (opts.countries_csv_path.has_value()) {
        std::ostringstream csv;
        dg::write_country_csv_header(csv);
        for (const auto& row : ctrl.country_rows) {
            dg::write_country_csv_row(csv, row);
        }
        auto csv_w = write_string_to_file(opts.countries_csv_path.value(), csv.str());
        if (!csv_w) {
            return core::Result<RunOutcome>::failure(std::move(csv_w.error()));
        }
    }
    if (opts.factions_csv_path.has_value()) {
        std::ostringstream csv;
        dg::write_faction_csv_header(csv);
        for (const auto& row : ctrl.faction_rows) {
            dg::write_faction_csv_row(csv, row);
        }
        auto csv_w = write_string_to_file(opts.factions_csv_path.value(), csv.str());
        if (!csv_w) {
            return core::Result<RunOutcome>::failure(std::move(csv_w.error()));
        }
    }
    // M3.5: unconditionally write the interest-groups CSV. The buffer
    // is empty for canonical scenarios with zero interest groups, so
    // the file ends up header-only — but it always exists, keeping
    // the artefact set predictable for downstream tooling.
    {
        std::ostringstream csv;
        dg::write_interest_group_csv_header(csv);
        for (const auto& row : ctrl.interest_group_rows) {
            dg::write_interest_group_csv_row(csv, row);
        }
        auto csv_w = write_string_to_file(interest_groups_csv_path, csv.str());
        if (!csv_w) {
            return core::Result<RunOutcome>::failure(std::move(csv_w.error()));
        }
    }
    // M3.6: unconditionally write the country-feedback trace CSV.
    // Empty buffer → header-only file (canonical scenarios with no
    // interest groups, or runs with `--days` < one month).
    {
        std::ostringstream csv;
        dg::write_country_feedback_csv_header(csv);
        for (const auto& row : ctrl.interest_group_country_feedback_rows) {
            dg::write_country_feedback_csv_row(csv, row);
        }
        auto csv_w = write_string_to_file(
            interest_group_country_feedback_csv_path, csv.str());
        if (!csv_w) {
            return core::Result<RunOutcome>::failure(std::move(csv_w.error()));
        }
    }
    // M3.6: unconditionally write the authority-pressure trace CSV.
    {
        std::ostringstream csv;
        dg::write_authority_pressure_csv_header(csv);
        for (const auto& row : ctrl.interest_group_authority_pressure_rows) {
            dg::write_authority_pressure_csv_row(csv, row);
        }
        auto csv_w = write_string_to_file(
            interest_group_authority_pressure_csv_path, csv.str());
        if (!csv_w) {
            return core::Result<RunOutcome>::failure(std::move(csv_w.error()));
        }
    }
    // M4.2: unconditionally write the SVG render of state.provinces.
    // Empty `state.provinces` produces a header-only `<svg>`; the
    // artefact-on-disk contract is consistent regardless of node
    // count. svg_export::write_provinces handles the directory
    // creation + I/O internally.
    {
        auto svg_w = svg::write_provinces(state, provinces_svg_path);
        if (!svg_w) {
            return core::Result<RunOutcome>::failure(std::move(svg_w.error()));
        }
    }

    RunOutcome outcome;
    outcome.start_date           = ctrl.start_date;
    outcome.end_date             = state.current_date;
    outcome.days_advanced        = ctrl.days_stepped;
    outcome.log_count            = state.logs.size();
    outcome.save_path            = save_path;
    outcome.log_path             = log_path;
    outcome.summary_csv_path     = opts.summary_csv_path;
    outcome.summary_rows         = ctrl.summary_rows.size();
    outcome.sanity_issues_logged = issues.size();
    outcome.monthly_ticks        = ctrl.monthly_ticks;
    outcome.countries_csv_path   = opts.countries_csv_path;
    outcome.countries_csv_rows   = ctrl.country_rows.size();
    outcome.factions_csv_path    = opts.factions_csv_path;
    outcome.factions_csv_rows    = ctrl.faction_rows.size();
    outcome.interest_groups_csv_path = interest_groups_csv_path;
    outcome.interest_groups_csv_rows = ctrl.interest_group_rows.size();
    outcome.interest_group_country_feedback_csv_path =
        interest_group_country_feedback_csv_path;
    outcome.interest_group_country_feedback_csv_rows =
        ctrl.interest_group_country_feedback_rows.size();
    outcome.interest_group_authority_pressure_csv_path =
        interest_group_authority_pressure_csv_path;
    outcome.interest_group_authority_pressure_csv_rows =
        ctrl.interest_group_authority_pressure_rows.size();
    outcome.provinces_svg_path = provinces_svg_path;

    ctrl.ended = true;
    return core::Result<RunOutcome>::success(std::move(outcome));
}

core::Result<RunOutcome> run_state(core::GameState& state,
                                   const RunnerOptions& opts) {
    if (opts.days < 0) {
        return core::Result<RunOutcome>::failure("--days must be >= 0");
    }

    TickController ctrl;
    auto begin_r = begin_tick(state, opts, ctrl);
    if (!begin_r) {
        return core::Result<RunOutcome>::failure(std::move(begin_r.error()));
    }
    for (int i = 0; i < opts.days; ++i) {
        auto step_r = step_one_day(state, opts, ctrl);
        if (!step_r) {
            return core::Result<RunOutcome>::failure(std::move(step_r.error()));
        }
    }
    return end_tick(state, opts, ctrl);
}

}  // namespace leviathan::systems::runner
