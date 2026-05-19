#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "leviathan/core/random_state.hpp"
#include "leviathan/systems/random_service.hpp"

using leviathan::core::RandomState;
namespace lr = leviathan::systems::random;

namespace {

// Drains `n` raw draws into a vector. Useful for "same seed -> same
// sequence" diffs.
std::vector<std::uint64_t> first_n_u64(std::uint64_t seed, int n) {
    RandomState rng;
    rng.seed = seed;
    std::vector<std::uint64_t> out;
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        out.push_back(lr::next_u64(rng));
    }
    return out;
}

}  // namespace

TEST_CASE("Same seed produces an identical raw sequence") {
    const auto a = first_n_u64(123456789ull, 64);
    const auto b = first_n_u64(123456789ull, 64);
    CHECK(a == b);
}

TEST_CASE("Different seeds diverge from the first draw onward") {
    const auto a = first_n_u64(111ull, 64);
    const auto b = first_n_u64(222ull, 64);
    CHECK(a != b);
    CHECK(a.front() != b.front());
}

TEST_CASE("Counter advances by exactly 1 per next_u64 call") {
    RandomState rng;
    rng.seed = 42ull;
    CHECK(rng.counter == 0ull);
    lr::next_u64(rng);
    CHECK(rng.counter == 1ull);
    lr::next_u64(rng);
    CHECK(rng.counter == 2ull);
    for (int i = 0; i < 100; ++i) {
        lr::next_u64(rng);
    }
    CHECK(rng.counter == 102ull);
}

TEST_CASE("Every higher-level helper consumes exactly one draw") {
    RandomState rng;
    rng.seed = 7ull;

    lr::draw_int(rng, 0, 9);
    CHECK(rng.counter == 1ull);

    lr::draw_unit(rng);
    CHECK(rng.counter == 2ull);

    lr::draw_double(rng, -1.0, 1.0);
    CHECK(rng.counter == 3ull);

    {
        const auto r = lr::draw_bool(rng, 0.5);
        REQUIRE(r);
    }
    CHECK(rng.counter == 4ull);

    lr::weighted_choice(rng, std::vector<double>{1.0, 2.0, 3.0});
    CHECK(rng.counter == 5ull);
}

TEST_CASE("draw_int respects the inclusive range") {
    RandomState rng;
    rng.seed = 0xDEADBEEFull;
    for (int i = 0; i < 10000; ++i) {
        const int v = lr::draw_int(rng, -3, 7);
        CHECK(v >= -3);
        CHECK(v <= 7);
    }
}

TEST_CASE("draw_int with a singleton range always returns that value") {
    RandomState rng;
    rng.seed = 1ull;
    for (int i = 0; i < 100; ++i) {
        CHECK(lr::draw_int(rng, 42, 42) == 42);
    }
}

TEST_CASE("draw_unit stays in 0-to-1 exclusive range") {
    RandomState rng;
    rng.seed = 999ull;
    for (int i = 0; i < 10000; ++i) {
        const double v = lr::draw_unit(rng);
        CHECK(v >= 0.0);
        CHECK(v <  1.0);
    }
}

TEST_CASE("draw_double respects min-to-max range and produces spread") {
    RandomState rng;
    rng.seed = 17ull;
    double seen_min = 100.0;
    double seen_max = 0.0;
    for (int i = 0; i < 5000; ++i) {
        const double v = lr::draw_double(rng, 10.0, 20.0);
        CHECK(v >= 10.0);
        CHECK(v <  20.0);
        seen_min = std::min(seen_min, v);
        seen_max = std::max(seen_max, v);
    }
    // The interval is 10 wide; after 5000 draws we expect coverage of
    // at least half of it in both directions.
    CHECK(seen_min < 12.5);
    CHECK(seen_max > 17.5);
}

TEST_CASE("draw_bool: p=0 always false, p=1 always true") {
    RandomState rng;
    rng.seed = 5ull;
    for (int i = 0; i < 500; ++i) {
        const auto a = lr::draw_bool(rng, 0.0);
        REQUIRE(a);
        CHECK_FALSE(a.value());
        const auto b = lr::draw_bool(rng, 1.0);
        REQUIRE(b);
        CHECK(b.value());
    }
    // Both paths still advanced the counter, exactly once each.
    CHECK(rng.counter == 1000ull);
}

TEST_CASE("Hardening: draw_bool REJECTS out-of-range probabilities") {
    // Post-M6.7 strict numeric validation: the previous silent
    // clamp of out-of-range probabilities is gone. p > 1 / p < 0
    // both return Result::failure and consume NO draw (preserves
    // rng.counter deterministically on the rejection path).
    RandomState rng;
    rng.seed = 5ull;
    const auto over = lr::draw_bool(rng, 5.0);
    REQUIRE(over.failed());
    CHECK(over.error().find("not a finite ratio in [0, 1]")
          != std::string::npos);
    const auto under = lr::draw_bool(rng, -5.0);
    REQUIRE(under.failed());
    CHECK(under.error().find("not a finite ratio in [0, 1]")
          != std::string::npos);
    CHECK(rng.counter == 0ull);  // no draw consumed on failure
}

TEST_CASE("Hardening: draw_bool REJECTS NaN probability") {
    RandomState rng;
    rng.seed = 1ull;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto r = lr::draw_bool(rng, nan);
    REQUIRE(r.failed());
    CHECK(r.error().find("NaN") != std::string::npos);
    CHECK(rng.counter == 0ull);
}

TEST_CASE("Hardening: draw_bool REJECTS +Inf / -Inf probability") {
    RandomState rng;
    rng.seed = 2ull;
    const double pinf = std::numeric_limits<double>::infinity();
    const auto r1 = lr::draw_bool(rng, pinf);
    REQUIRE(r1.failed());
    CHECK(r1.error().find("+Inf") != std::string::npos);
    const auto r2 = lr::draw_bool(rng, -pinf);
    REQUIRE(r2.failed());
    CHECK(r2.error().find("-Inf") != std::string::npos);
    CHECK(rng.counter == 0ull);
}

TEST_CASE("weighted_choice returns an in-bounds index for normal inputs") {
    RandomState rng;
    rng.seed = 12345ull;
    const std::vector<double> w{1.0, 2.0, 3.0, 4.0};
    for (int i = 0; i < 1000; ++i) {
        const std::size_t idx = lr::weighted_choice(rng, w);
        CHECK(idx < 4);
    }
}

TEST_CASE("weighted_choice frequencies are roughly proportional to weights") {
    RandomState rng;
    rng.seed = 0xABCDEF01ull;
    const std::vector<double> w{1.0, 3.0};  // 25% / 75%
    int low = 0, hi = 0;
    for (int i = 0; i < 20000; ++i) {
        if (lr::weighted_choice(rng, w) == 0) ++low; else ++hi;
    }
    // Allow generous slack so the test doesn't flake. Expected
    // proportions are 5000 vs 15000; we just check it's in the
    // right ballpark.
    CHECK(low > 4000);
    CHECK(low < 6000);
    CHECK(hi  > 14000);
    CHECK(hi  < 16000);
}

TEST_CASE("weighted_choice with a zero weight never picks that index") {
    RandomState rng;
    rng.seed = 99ull;
    const std::vector<double> w{1.0, 0.0, 1.0};
    for (int i = 0; i < 5000; ++i) {
        CHECK(lr::weighted_choice(rng, w) != 1);
    }
}

TEST_CASE("weighted_choice with all-zero weights returns 0 and consumes one draw") {
    RandomState rng;
    rng.seed = 1ull;
    const std::vector<double> w{0.0, 0.0, 0.0};

    CHECK(lr::weighted_choice(rng, w) == 0);
    CHECK(rng.counter == 1ull);

    CHECK(lr::weighted_choice(rng, w) == 0);
    CHECK(rng.counter == 2ull);
}

TEST_CASE("weighted_choice with a single-element list always returns 0") {
    RandomState rng;
    rng.seed = 7ull;
    const std::vector<double> w{1.5};
    for (int i = 0; i < 100; ++i) {
        CHECK(lr::weighted_choice(rng, w) == 0);
    }
}

TEST_CASE("Tag parameter does not affect the draw value") {
    // Same seed, same call order, different tags -> identical results.
    RandomState a, b;
    a.seed = 13ull;
    b.seed = 13ull;
    for (int i = 0; i < 64; ++i) {
        const auto x = lr::next_u64(a, "category-A");
        const auto y = lr::next_u64(b, "different-category");
        CHECK(x == y);
        CHECK(a.counter == b.counter);
    }
}

namespace {

struct TraceCapture {
    struct Row {
        std::string       tag;
        std::uint64_t     counter;
        std::uint64_t     raw;
    };
    static std::vector<Row> g_rows;

    static void hook(std::string_view tag, std::uint64_t counter, std::uint64_t raw) {
        g_rows.push_back(Row{std::string(tag), counter, raw});
    }

    static void reset() { g_rows.clear(); }
};

std::vector<TraceCapture::Row> TraceCapture::g_rows{};

}  // namespace

TEST_CASE("Trace callback receives one entry per draw, in order") {
    TraceCapture::reset();
    lr::set_trace_callback(&TraceCapture::hook);

    RandomState rng;
    rng.seed = 100ull;
    lr::next_u64(rng, "alpha");
    lr::draw_int(rng, 0, 9, "beta");
    lr::draw_unit(rng, "gamma");
    lr::weighted_choice(rng, std::vector<double>{1.0, 2.0}, "delta");

    lr::set_trace_callback(nullptr);

    REQUIRE(TraceCapture::g_rows.size() == 4);
    CHECK(TraceCapture::g_rows[0].tag == "alpha");
    CHECK(TraceCapture::g_rows[0].counter == 1ull);
    CHECK(TraceCapture::g_rows[1].tag == "beta");
    CHECK(TraceCapture::g_rows[1].counter == 2ull);
    CHECK(TraceCapture::g_rows[2].tag == "gamma");
    CHECK(TraceCapture::g_rows[2].counter == 3ull);
    CHECK(TraceCapture::g_rows[3].tag == "delta");
    CHECK(TraceCapture::g_rows[3].counter == 4ull);

    // Cross-check: the raw values from trace should match what next_u64
    // would have produced from the same seed and counter.
    {
        RandomState shadow;
        shadow.seed = 100ull;
        for (std::size_t i = 0; i < TraceCapture::g_rows.size(); ++i) {
            const std::uint64_t expected = lr::next_u64(shadow);
            CHECK(TraceCapture::g_rows[i].raw == expected);
        }
    }
}

TEST_CASE("Trace callback fires for the all-zero weighted_choice path too") {
    TraceCapture::reset();
    lr::set_trace_callback(&TraceCapture::hook);

    RandomState rng;
    rng.seed = 1ull;
    const std::vector<double> zeros{0.0, 0.0, 0.0};
    const auto idx = lr::weighted_choice(rng, zeros, "all-zero");

    lr::set_trace_callback(nullptr);

    CHECK(idx == 0);
    REQUIRE(TraceCapture::g_rows.size() == 1);
    CHECK(TraceCapture::g_rows[0].tag == "all-zero");
    CHECK(TraceCapture::g_rows[0].counter == 1ull);
}

TEST_CASE("Replays from the same RandomState reproduce earlier draws") {
    // Snapshot determinism: capture (seed, counter), do more draws,
    // restore the snapshot, and verify the post-snapshot replays match.
    RandomState rng;
    rng.seed = 0xC0FFEEull;
    for (int i = 0; i < 17; ++i) {
        lr::draw_int(rng, 0, 999);
    }

    const RandomState snapshot = rng;

    std::vector<int> first_pass;
    for (int i = 0; i < 32; ++i) {
        first_pass.push_back(lr::draw_int(rng, 0, 999));
    }

    rng = snapshot;  // rewind
    std::vector<int> second_pass;
    for (int i = 0; i < 32; ++i) {
        second_pass.push_back(lr::draw_int(rng, 0, 999));
    }

    CHECK(first_pass == second_pass);
}
