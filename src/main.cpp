// Headless simulation entry point.
//
// As of M0.9 main() is a thin shell over leviathan::systems::runner.
// Parse argv, dispatch run(), and print a short summary on success.
// Any failure prints an error to stderr and exits with code 1.

#include <cstdlib>
#include <iostream>
#include <string>

#include "leviathan/systems/runner.hpp"

namespace {

constexpr const char* kProjectName    = "Project Leviathan";
constexpr const char* kProjectVersion = "0.1.0";

}  // namespace

int main(int argc, char** argv) {
    namespace runner = leviathan::systems::runner;

    auto parsed = runner::parse_args(argc, argv);
    if (!parsed) {
        std::cerr << kProjectName << " " << kProjectVersion << "\n"
                  << "Error: " << parsed.error() << "\n\n"
                  << runner::usage_text();
        return EXIT_FAILURE;
    }
    const runner::RunnerOptions opts = std::move(parsed).value();

    if (opts.show_help) {
        std::cout << kProjectName << " " << kProjectVersion << "\n\n"
                  << runner::usage_text();
        return EXIT_SUCCESS;
    }

    std::cout << kProjectName << " " << kProjectVersion << "\n"
              << "Headless strategic sandbox prototype.\n";

    auto run_r = runner::run(opts);
    if (!run_r) {
        std::cerr << "Error: " << run_r.error() << "\n";
        return EXIT_FAILURE;
    }
    const auto& outcome = run_r.value();

    std::cout << "\n--- Run summary ---\n"
              << "Start date          : " << outcome.start_date.to_string() << "\n"
              << "End date            : " << outcome.end_date.to_string()   << "\n"
              << "Days advanced       : " << outcome.days_advanced          << "\n"
              << "Log entries         : " << outcome.log_count              << "\n"
              << "Sanity issues       : " << outcome.sanity_issues_logged   << "\n"
              << "Save file           : " << outcome.save_path.string()     << "\n"
              << "Log file            : " << outcome.log_path.string()      << "\n";
    if (outcome.summary_csv_path.has_value()) {
        std::cout
              << "Summary CSV file    : " << outcome.summary_csv_path.value().string() << "\n"
              << "Summary CSV rows    : " << outcome.summary_rows                       << "\n";
    }
    if (outcome.countries_csv_path.has_value()) {
        std::cout
              << "Countries CSV file  : " << outcome.countries_csv_path.value().string() << "\n"
              << "Countries CSV rows  : " << outcome.countries_csv_rows                  << "\n";
    }
    if (outcome.factions_csv_path.has_value()) {
        std::cout
              << "Factions CSV file   : " << outcome.factions_csv_path.value().string() << "\n"
              << "Factions CSV rows   : " << outcome.factions_csv_rows                  << "\n";
    }
    // M3.5: interest_groups.csv is unconditional, so always print.
    std::cout
              << "Interest groups CSV : " << outcome.interest_groups_csv_path.string()  << "\n"
              << "Interest groups rows: " << outcome.interest_groups_csv_rows           << "\n";
    // M3.6: two formula-trace CSVs, also unconditional.
    std::cout
              << "Country feedback CSV: " << outcome.interest_group_country_feedback_csv_path.string()     << "\n"
              << "Country feedback rows: " << outcome.interest_group_country_feedback_csv_rows             << "\n"
              << "Authority press CSV : " << outcome.interest_group_authority_pressure_csv_path.string()   << "\n"
              << "Authority press rows: " << outcome.interest_group_authority_pressure_csv_rows            << "\n";
    // M3.10: third formula-trace CSV (military_pressure), also unconditional.
    std::cout
              << "Military press CSV  : " << outcome.interest_group_military_pressure_csv_path.string()    << "\n"
              << "Military press rows : " << outcome.interest_group_military_pressure_csv_rows             << "\n";
    if (opts.replay_path.has_value()) {
        std::cout
              << "Replay source       : " << opts.replay_path.value().string() << "\n"
              << "Commands replayed   : " << outcome.replay_commands_replayed << "\n";
        if (opts.target_date.has_value()) {
            std::cout
              << "Target date         : " << opts.target_date.value().to_string() << "\n";
        }
    }
    if (opts.verify) {
        std::cout
              << "Verify mismatches   : " << outcome.verify_mismatches.size() << "\n";
        for (const auto& m : outcome.verify_mismatches) {
            std::cout << "  - " << m.field_path << " : " << m.detail << "\n";
        }
        if (opts.verify_tolerance.has_value()) {
            std::cout
              << "Verify tolerance    : " << opts.verify_tolerance.value() << "\n";
        }
    }

    // M2.12: --verify-strict makes main exit non-zero when --verify
    // detects mismatches. We print the full mismatch list first so
    // CI logs capture every divergence.
    if (opts.verify_strict && !outcome.verify_mismatches.empty()) {
        std::cout << "Strict mode: failing run on "
                  << outcome.verify_mismatches.size()
                  << " mismatch(es).\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
