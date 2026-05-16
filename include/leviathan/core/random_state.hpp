// RNG state placeholder.
//
// M0.3 only owns the *state*; M0.5 introduces the RandomService that
// consumes this state to produce deterministic draws. Keeping the
// state and the service separate lets us:
//   - serialise the RNG state in save files (M0.8) without dragging in
//     the service implementation, and
//   - keep replays deterministic across versions even if the draw
//     algorithm changes (the counter advances in a known sequence).

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
