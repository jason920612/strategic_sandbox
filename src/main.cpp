// Stub entry point.
//
// M0.2 adds a tiny GameDate demo so the binary visibly exercises the new
// core library. The real CLI (config loading, headless run, save/log
// output) lands in M0.9.

#include <cstdlib>
#include <iostream>

#include "leviathan/core/game_date.hpp"

namespace {

constexpr const char* kProjectName    = "Project Leviathan";
constexpr const char* kProjectVersion = "0.1.0";

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
    std::cout << kProjectName << " " << kProjectVersion << "\n"
              << "Milestone 0.2 - core types only.\n"
              << "No simulation logic is wired up yet.\n";

    leviathan::core::GameDate today{1930, 1, 1};
    std::cout << "Demo start date : " << today.to_string() << "\n";
    today.advance_days(31);
    std::cout << "After 31 days   : " << today.to_string() << "\n";
    today.advance_days(365);
    std::cout << "After 365 days  : " << today.to_string() << "\n";

    return EXIT_SUCCESS;
}
