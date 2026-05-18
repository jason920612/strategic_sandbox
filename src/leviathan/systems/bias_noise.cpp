// M6.5: BiasNoise implementation.
//
// See include/leviathan/systems/bias_noise.hpp for the public
// contract (deterministic-hash design choice, placeholder
// amplitude, composition with M6 pipeline, deliberate
// non-goals). This file is the small hash-mixer behind the
// helper: FNV-1a for stable string-to-uint64 mixing,
// splitmix64 finalize for output scrambling.
//
// Why FNV-1a + splitmix64 (not std::hash):
//
//   * `std::hash<std::string>` is implementation-defined; two
//     different compilers (and even different std-lib
//     versions) produce different values for the same input.
//     M6.5 needs deterministic output across builds, so
//     `std::hash` is rejected.
//   * FNV-1a (Fowler–Noll–Vo) is a 5-line standardised
//     integer-arithmetic hash. The 64-bit variant uses the
//     project's existing `kGoldenRatio64`-adjacent prime
//     constants (FNV-1a 64-bit prime
//     `0x100000001b3` and offset basis `0xcbf29ce484222325`).
//   * splitmix64 finalize is already used in the M0.5 RNG
//     service (anonymous-namespace `finalize` in
//     random_service.cpp). Reproduced here verbatim so
//     bias_noise has zero coupling to the RNG service.
//     If future M6.x extracts a shared mixer header, both
//     sites can switch at once.

#include "leviathan/systems/bias_noise.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace leviathan::systems::bias_noise {
namespace {

// FNV-1a 64-bit. Mix `data` byte-by-byte into the running
// hash. Stable across compilers; bit-identical for the same
// input bytes.
//
// Standard constants from the FNV-1a 64-bit reference:
//   offset basis : 0xcbf29ce484222325
//   prime        : 0x100000001b3
constexpr std::uint64_t kFnvOffsetBasis = 0xcbf29ce484222325ull;
constexpr std::uint64_t kFnvPrime       = 0x100000001b3ull;

std::uint64_t fnv1a_64(std::string_view s,
                       std::uint64_t hash = kFnvOffsetBasis) noexcept {
    for (unsigned char c : s) {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= kFnvPrime;
    }
    return hash;
}

// Mix one additional uint64 into a running FNV-1a hash, byte
// by byte (little-endian byte order, fixed regardless of
// host endianness — see test pin).
std::uint64_t fnv1a_64_mix_u64(std::uint64_t value,
                               std::uint64_t hash) noexcept {
    for (int i = 0; i < 8; ++i) {
        const unsigned char byte =
            static_cast<unsigned char>(value & 0xffull);
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= kFnvPrime;
        value >>= 8;
    }
    return hash;
}

// splitmix64 finaliser (Sebastiano Vigna, public domain).
// Three mix steps of XOR-shift + multiply. Reproduced from
// the M0.5 RNG service's anonymous-namespace `finalize`.
// Identical behaviour; bias_noise stays decoupled from the
// RNG service module.
std::uint64_t splitmix64_finalize(std::uint64_t z) noexcept {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z =  z ^ (z >> 31);
    return z;
}

// Combine inputs into a single deterministic uint64. Order
// matters and is part of the contract (different field order
// would produce different output for the same logical input).
std::uint64_t hash_event_fire(const std::string&    event_id_code,
                              const std::string&    country_id_code,
                              const core::GameDate& fired_on) noexcept {
    std::uint64_t h = kFnvOffsetBasis;
    h = fnv1a_64(event_id_code,   h);
    // A NUL separator between strings so "abcd" + "ef" hashes
    // differently from "abc" + "def".
    h ^= 0ull; h *= kFnvPrime;
    h = fnv1a_64(country_id_code, h);
    // Pack the date as (year * 10000 + month * 100 + day) and
    // mix as a uint64. Keeps the date influence stable and
    // byte-order-independent.
    const std::uint64_t date_packed =
        static_cast<std::uint64_t>(
            fired_on.year() * 10000
          + fired_on.month() * 100
          + fired_on.day());
    h = fnv1a_64_mix_u64(date_packed, h);
    return splitmix64_finalize(h);
}

// Scale a uint64 hash into the open interval [0, 1), then
// recentre and scale into `[-amplitude, +amplitude]`.
// Uses the upper 53 bits to map cleanly into double's mantissa
// (the same trick `std::uniform_real_distribution` uses, but
// hand-rolled so the output is deterministic across compilers).
double scale_to_signed_amplitude(std::uint64_t hash,
                                 double        amplitude) noexcept {
    // Upper 53 bits -> [0, 2^53) -> [0, 1).
    const double unit =
        static_cast<double>(hash >> 11) /
        static_cast<double>(1ull << 53);
    // Shift to [-0.5, +0.5).
    const double centered = unit - 0.5;
    // Scale to [-amplitude, +amplitude).
    return centered * 2.0 * amplitude;
}

}  // namespace

core::Result<double> sample_for_event(
        const std::string&     event_id_code,
        const std::string&     country_id_code,
        const core::GameDate&  fired_on,
        double                 amplitude) {
    if (event_id_code.empty()) {
        return core::Result<double>::failure(
            "bias_noise::sample_for_event: "
            "event_id_code must be non-empty");
    }
    if (country_id_code.empty()) {
        return core::Result<double>::failure(
            "bias_noise::sample_for_event: "
            "country_id_code must be non-empty");
    }
    if (!std::isfinite(amplitude)) {
        return core::Result<double>::failure(
            "bias_noise::sample_for_event: "
            "amplitude " + std::to_string(amplitude) +
            " is not finite");
    }
    if (amplitude < 0.0 || amplitude > 1.0) {
        return core::Result<double>::failure(
            "bias_noise::sample_for_event: "
            "amplitude " + std::to_string(amplitude) +
            " is outside the [0, 1] range");
    }

    // Placeholder fast path: amplitude == 0 ⇒ no noise.
    // Keeps M6.5 a strict no-op until M6.9 starts passing
    // positive amplitudes. The fast-path skips hashing; the
    // hash itself would still produce a non-zero raw value
    // but `* 0.0` would zero it. Skipping for clarity.
    if (amplitude == kPlaceholderNoiseAmplitude) {
        return core::Result<double>::success(0.0);
    }

    const std::uint64_t hash = hash_event_fire(
        event_id_code, country_id_code, fired_on);
    return core::Result<double>::success(
        scale_to_signed_amplitude(hash, amplitude));
}

}  // namespace leviathan::systems::bias_noise
