#include "leviathan/systems/random_service.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

namespace leviathan::systems::random {

namespace {

// 2^64 / phi, the same "golden ratio" constant splitmix64 uses to walk
// the state on each call. Picked because successive multiples spread
// roughly uniformly across the 64-bit range, giving good decorrelation
// when fed into the finaliser below.
constexpr std::uint64_t kGoldenRatio64 = 0x9E3779B97F4A7C15ull;

// splitmix64 finaliser (Sebastiano Vigna, public domain). Three mix
// rounds; each multiplier is chosen to have high algebraic order modulo
// 2^64 and a small number of low bits set so the xor-shift chains
// avalanche fast.
std::uint64_t finalize(std::uint64_t z) noexcept {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// Process-wide trace hook. nullptr in the common case; reading it is
// a single load on the fast path.
TraceCallback g_trace_cb = nullptr;

void trace(std::string_view tag, std::uint64_t counter, std::uint64_t raw) noexcept {
    if (g_trace_cb != nullptr) {
        g_trace_cb(tag, counter, raw);
    }
}

}  // namespace

void set_trace_callback(TraceCallback callback) {
    g_trace_cb = callback;
}

std::uint64_t next_u64(core::RandomState& rng, std::string_view tag) {
    // Step the counter first so the very first draw uses counter == 1.
    // counter == 0 is reserved for "no draws yet" (see make_game_state
    // in M0.3, which zeros the counter on construction).
    rng.counter += 1;

    // Counter-indexed state: any caller that knows (seed, counter) can
    // reproduce the draw without replaying the whole sequence. Unsigned
    // overflow is well-defined.
    const std::uint64_t z   = rng.seed + rng.counter * kGoldenRatio64;
    const std::uint64_t raw = finalize(z);
    trace(tag, rng.counter, raw);
    return raw;
}

int draw_int(core::RandomState& rng, int min_inclusive, int max_inclusive,
             std::string_view tag) {
    assert(min_inclusive <= max_inclusive &&
           "draw_int: min_inclusive must be <= max_inclusive");

    // Work in int64_t so [INT_MIN, INT_MAX] fits cleanly: hi - lo + 1
    // is at most 2^32, well within int64_t.
    const std::int64_t lo = static_cast<std::int64_t>(min_inclusive);
    const std::int64_t hi = static_cast<std::int64_t>(max_inclusive);
    const std::uint64_t span = static_cast<std::uint64_t>(hi - lo + 1);

    // Modulo bias: at most `range / 2^64`, utterly negligible at the
    // scales this simulation uses (range << 2^32). If a future use case
    // needs uniformly-distributed-mod-range we'll add rejection
    // sampling under the same API.
    const std::uint64_t raw = next_u64(rng, tag);
    const std::int64_t  off = static_cast<std::int64_t>(raw % span);
    return static_cast<int>(lo + off);
}

double draw_unit(core::RandomState& rng, std::string_view tag) {
    const std::uint64_t raw = next_u64(rng, tag);

    // Top 53 bits / 2^53. 53 bits is exactly the IEEE-754 double
    // mantissa width, so this division is exact: the result is one of
    // 2^53 evenly-spaced doubles in [0, 1). No platform-dependent
    // rounding.
    constexpr double kScale = static_cast<double>(1ull << 53);
    return static_cast<double>(raw >> 11) / kScale;
}

double draw_double(core::RandomState& rng,
                   double min_inclusive, double max_exclusive,
                   std::string_view tag) {
    assert(std::isfinite(min_inclusive) &&
           "draw_double: min_inclusive must be finite");
    assert(std::isfinite(max_exclusive) &&
           "draw_double: max_exclusive must be finite");
    assert(min_inclusive < max_exclusive &&
           "draw_double: requires min_inclusive < max_exclusive");

    const double u = draw_unit(rng, tag);
    return min_inclusive + u * (max_exclusive - min_inclusive);
}

bool draw_bool(core::RandomState& rng, double probability,
               std::string_view tag) {
    // std::clamp's contract: returns hi if v > hi, lo if v < lo, else v.
    // For NaN: NaN < lo is false and lo < NaN is false, so the chain
    // returns NaN. Use a NaN check up front so callers do not need to
    // worry about NaN propagation through downstream comparisons.
    if (std::isnan(probability)) {
        probability = 0.0;
    }
    const double clamped = std::clamp(probability, 0.0, 1.0);
    return draw_unit(rng, tag) < clamped;
}

std::size_t weighted_choice(core::RandomState& rng,
                            const std::vector<double>& weights,
                            std::string_view tag) {
    assert(!weights.empty() &&
           "weighted_choice: weights must not be empty");

    double total = 0.0;
    for (double w : weights) {
        assert(std::isfinite(w) &&
               "weighted_choice: weights must be finite");
        assert(w >= 0.0 &&
               "weighted_choice: weights must be non-negative");
        total += w;
    }

    if (total <= 0.0) {
        // All-zero case. Still consume exactly one draw so the caller's
        // counter advances identically to the normal path; this matters
        // for deterministic replays where one run may hit all-zero
        // weights while another (with a slightly different state)
        // hits the normal path.
        (void) draw_unit(rng, tag);
        return 0;
    }

    const double r = draw_unit(rng, tag) * total;
    double cumulative = 0.0;
    // Loop stops one short: the final index is the float-edge fallback.
    for (std::size_t i = 0; i + 1 < weights.size(); ++i) {
        cumulative += weights[i];
        if (r < cumulative) {
            return i;
        }
    }
    return weights.size() - 1;
}

}  // namespace leviathan::systems::random
