// Smoke test for M0.1.
//
// Verifies only that the build pipeline can produce a runnable test
// binary and that CTest can discover and run it. There are no real
// production types to exercise yet; that arrives in M0.2.

#include <cassert>
#include <cstdlib>
#include <iostream>

namespace {

int run_arithmetic_smoke() {
    constexpr int answer = 1 + 1;
    assert(answer == 2 && "basic arithmetic broke - toolchain is wrong");
    return answer == 2 ? 0 : 1;
}

}  // namespace

int main() {
    if (run_arithmetic_smoke() != 0) {
        std::cerr << "smoke_test: arithmetic check failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "smoke_test: ok\n";
    return EXIT_SUCCESS;
}
