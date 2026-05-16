// Stub entry point for the Leviathan simulation core.
//
// M0.1 only proves that the project can configure, build, run, and exit
// cleanly. The headless simulation runner (M0.9) will replace this with
// a real CLI; until then this binary just prints a banner and exits 0.

#include <cstdlib>
#include <iostream>

namespace {

constexpr const char* kProjectName    = "Project Leviathan";
constexpr const char* kProjectVersion = "0.1.0";

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
    std::cout << kProjectName << " " << kProjectVersion << "\n"
              << "Milestone 0.1 - project skeleton only.\n"
              << "No simulation logic is wired up yet.\n";
    return EXIT_SUCCESS;
}
