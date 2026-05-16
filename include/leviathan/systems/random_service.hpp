// Deterministic random service.
//
// Every random draw in the simulation must go through this header. We
// deliberately do NOT use std::random_device, std::uniform_int_distribution,
// or std::uniform_real_distribution: the former pulls non-deterministic
// entropy, the latter two have implementation-defined output across
// standard library vendors. Cross-platform replays must be bit-identical.
//
// Algorithm: a splitmix64-style mix on (seed + counter * golden_ratio).
// Each call advances counter by exactly 1. See random_service.cpp for
// the constants and rationale. The algorithm is documented and stable;
// if it ever changes we will add an `algorithm_version` field to the
// save format (see docs/m0-5-rng-service.md §4 and RandomState's
// determinism note).

#ifndef LEVIATHAN_SYSTEMS_RANDOM_SERVICE_HPP
#define LEVIATHAN_SYSTEMS_RANDOM_SERVICE_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "leviathan/core/random_state.hpp"

namespace leviathan::systems::random {

// Raw 64-bit draw. Advances rng.counter by 1.
std::uint64_t next_u64(core::RandomState& rng, std::string_view tag = "");

// Uniform integer in [min_inclusive, max_inclusive]. Consumes one draw.
// Precondition: min_inclusive <= max_inclusive.
int draw_int(core::RandomState& rng, int min_inclusive, int max_inclusive,
             std::string_view tag = "");

// Uniform double in [0, 1). Consumes one draw. Uses the top 53 bits of
// the raw value, which fits exactly in an IEEE-754 double - no rounding
// drift across platforms.
double draw_unit(core::RandomState& rng, std::string_view tag = "");

// Uniform double in [min_inclusive, max_exclusive). Consumes one draw.
// Precondition: both bounds finite and min_inclusive < max_exclusive.
double draw_double(core::RandomState& rng,
                   double min_inclusive, double max_exclusive,
                   std::string_view tag = "");

// True with probability `probability`. Probability is clamped to [0, 1]
// (NaN clamps to 0). Consumes one draw regardless of `probability`.
bool draw_bool(core::RandomState& rng, double probability,
               std::string_view tag = "");

// Selects an index in [0, weights.size()) with weights[i] / sum(weights).
//
// Edge cases:
//   - weights.empty()        -> precondition violation (assert)
//   - any weight < 0         -> precondition violation (assert)
//   - any weight non-finite  -> precondition violation (assert)
//   - sum of weights == 0    -> returns 0; one draw is still consumed
//                               so a caller's counter advances by
//                               exactly the same amount on both the
//                               "real choice" and "all-zero" paths.
//
// Every other valid input consumes exactly one draw.
std::size_t weighted_choice(core::RandomState& rng,
                            const std::vector<double>& weights,
                            std::string_view tag = "");

// Optional process-wide trace hook.
//
// If set, called exactly once per RNG draw, AFTER rng.counter has been
// advanced. Arguments are (caller's tag, new counter value, raw u64
// produced by the algorithm). Default = nullptr (no overhead).
//
// Tests use this to record a draw sequence and verify it matches a
// reference run. The tag does NOT affect the draw output - it is a
// pure label for debugging non-determinism.
using TraceCallback = void (*)(std::string_view tag,
                               std::uint64_t counter,
                               std::uint64_t raw);

// Install or clear the trace callback. Pass nullptr to disable.
void set_trace_callback(TraceCallback callback);

}  // namespace leviathan::systems::random

#endif  // LEVIATHAN_SYSTEMS_RANDOM_SERVICE_HPP
