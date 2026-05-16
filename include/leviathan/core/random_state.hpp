// RNG state placeholder.
//
// M0.3 only owns the *state*; M0.5 introduces the RandomService that
// consumes this state to produce deterministic draws. Keeping the
// state and the service separate lets us serialise the RNG state in
// save files (M0.8) without dragging in the service implementation.
//
// Determinism guarantee:
//   Replays are deterministic *within the same draw algorithm*. If
//   we ever change the algorithm, identical (seed, counter) values
//   may yield different draws. Cross-version replay is therefore an
//   open question; M0.5 / M0.8 will revisit this with either an
//   explicit `algorithm_version` field in the save format or a
//   versioned RandomService.

#ifndef LEVIATHAN_CORE_RANDOM_STATE_HPP
#define LEVIATHAN_CORE_RANDOM_STATE_HPP

#include <cstdint>

namespace leviathan::core {

struct RandomState {
    std::uint64_t seed    = 0;
    std::uint64_t counter = 0;
};

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_RANDOM_STATE_HPP
