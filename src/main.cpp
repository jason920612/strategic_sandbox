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
              << "Milestone 0.9 - headless runner.\n";

    auto run_r = runner::run(opts);
    if (!run_r) {
        std::cerr << "Error: " << run_r.error() << "\n";
        return EXIT_FAILURE;
    }
    const auto& outcome = run_r.value();

    std::cout << "\n--- Run summary ---\n"
              << "Start date    : " << outcome.start_date.to_string() << "\n"
              << "End date      : " << outcome.end_date.to_string()   << "\n"
              << "Days advanced : " << outcome.days_advanced          << "\n"
              << "Log entries   : " << outcome.log_count              << "\n"
              << "Save file     : " << outcome.save_path.string()     << "\n"
              << "Log file      : " << outcome.log_path.string()      << "\n";

    return EXIT_SUCCESS;
}
