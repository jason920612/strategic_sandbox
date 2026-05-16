#include "leviathan/systems/runner.hpp"

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
#include "leviathan/systems/data_loader.hpp"
#include "leviathan/systems/diagnostics.hpp"
#include "leviathan/systems/logging_system.hpp"
#include "leviathan/systems/monthly_pipeline.hpp"
#include "leviathan/systems/save_system.hpp"
#include "leviathan/systems/scenario_loader.hpp"
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
        } else {
            return core::Result<RunnerOptions>::failure(
                "unknown flag: " + std::string(a));
        }
    }

    if (!days_seen) {
        return core::Result<RunnerOptions>::failure(
            "--days is required (run with --help for usage)");
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

    return run_state(state, opts);
}

core::Result<RunOutcome> run_state(core::GameState& state,
                                   const RunnerOptions& opts) {
    namespace lt = leviathan::systems::time;
    namespace lg = leviathan::systems::logging;
    namespace ss = leviathan::systems::save_system;
    namespace dg = leviathan::systems::diagnostics;
    namespace mp = leviathan::systems::monthly;

    if (opts.days < 0) {
        return core::Result<RunOutcome>::failure("--days must be >= 0");
    }

    const core::GameDate start_date = state.current_date;

    // Snapshot buffer for --summary-csv. We collect rows in-memory and
    // dump them at the end; long enough runs that this matters can
    // stream later. Empty buffer = no CSV requested.
    std::vector<dg::SummaryRow> summary_rows;
    const bool want_csv = opts.summary_csv_path.has_value();

    int monthly_ticks = 0;

    // Initial log. Metadata is intentionally path-independent so two
    // runs with the same options produce byte-identical event logs.
    lg::log_info(state, "lifecycle", "runner", "simulation start",
                 {{"days_requested", std::to_string(opts.days)},
                  {"seed",           std::to_string(state.rng.seed)}});

    if (want_csv) {
        summary_rows.push_back(dg::snapshot(state));
    }

    // ---- Tick -------------------------------------------------------------
    for (int i = 0; i < opts.days; ++i) {
        const lt::TickResult r = lt::advance_one_day(state);
        if (r.month_changed) {
            lg::log_info(state, "time", "runner", "month rolled over");
        }
        if (r.year_changed) {
            lg::log_info(state, "time", "runner", "year rolled over");
        }
        // M1.10: monthly pipeline runs at each month boundary, AFTER
        // the month-rolled-over log line so the canonical M0.9 log
        // ordering is preserved. The pipeline itself writes no logs.
        if (r.month_changed) {
            auto pipe_r = mp::tick_all_countries(state);
            if (!pipe_r) {
                return core::Result<RunOutcome>::failure(
                    "monthly pipeline failed on " +
                    state.current_date.to_string() + ": " +
                    std::move(pipe_r.error()));
            }
            ++monthly_ticks;
        }
        if (want_csv && r.month_changed) {
            summary_rows.push_back(dg::snapshot(state));
        }
    }

    lg::log_info(state, "lifecycle", "runner", "simulation end",
                 {{"days_advanced", std::to_string(opts.days)}});

    // ---- Sanity checks ---------------------------------------------------
    // Run AFTER the end log but BEFORE the final snapshot, so any
    // issues become part of the log stream and are reflected in the
    // final CSV row's log_count.
    const auto issues = dg::sanity_check(state);
    for (const auto& iss : issues) {
        lg::log_error(state, "diagnostics", "runner", iss.message,
                      {{"code", iss.code}});
    }

    if (want_csv) {
        summary_rows.push_back(dg::snapshot(state));
    }

    // ---- Resolve output paths --------------------------------------------
    const auto save_path = opts.save_path.value_or(opts.output_dir / "save.json");
    const auto log_path  = opts.log_path.value_or(opts.output_dir / "events.jsonl");

    // ---- Write save -------------------------------------------------------
    auto save_r = ss::save(state, save_path);
    if (!save_r) {
        return core::Result<RunOutcome>::failure(std::move(save_r.error()));
    }

    // ---- Write JSONL log via the LoggingSystem exporter -------------------
    std::ostringstream ss_log;
    lg::export_jsonl(ss_log, state);
    auto log_w = write_string_to_file(log_path, ss_log.str());
    if (!log_w) {
        return core::Result<RunOutcome>::failure(std::move(log_w.error()));
    }

    // ---- Write summary CSV (if requested) --------------------------------
    if (want_csv) {
        std::ostringstream csv;
        dg::write_csv_header(csv);
        for (const auto& row : summary_rows) {
            dg::write_csv_row(csv, row);
        }
        auto csv_w = write_string_to_file(opts.summary_csv_path.value(), csv.str());
        if (!csv_w) {
            return core::Result<RunOutcome>::failure(std::move(csv_w.error()));
        }
    }

    // ---- Outcome ----------------------------------------------------------
    RunOutcome outcome;
    outcome.start_date           = start_date;
    outcome.end_date             = state.current_date;
    outcome.days_advanced        = opts.days;
    outcome.log_count            = state.logs.size();
    outcome.save_path            = save_path;
    outcome.log_path             = log_path;
    outcome.summary_csv_path     = opts.summary_csv_path;
    outcome.summary_rows         = summary_rows.size();
    outcome.sanity_issues_logged = issues.size();
    outcome.monthly_ticks        = monthly_ticks;
    return core::Result<RunOutcome>::success(std::move(outcome));
}

}  // namespace leviathan::systems::runner
