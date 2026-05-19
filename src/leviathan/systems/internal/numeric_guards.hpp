// Shared numeric-validation guards for runtime helpers under
// `leviathan::systems`. Companion to `internal/json_helpers.hpp`:
// that header validates JSON inputs at load time; this header
// validates ratio / finite-double inputs at runtime, inside
// tick / apply / score functions.
//
// Captured 2026-05-19 as part of the cross-cutting hardening
// sweep that follows M6.7 (PR #114). The pattern is modelled on
// `information_accuracy.cpp`'s anonymous-namespace
// `require_unit_ratio` helper (the PR #114 reference); the
// shared header generalises it for every system that reads
// CountryState / FactionState / InterestGroupState ratios or
// runtime-computed doubles.
//
// Two project rules govern usage:
//
//   1. `feedback_no_silent_degradation`
//      Abnormal numerics (NaN, +/-Inf, out-of-range ratios)
//      must surface as `Result::failure` naming the offending
//      field. Never silently clamp.
//
//   2. `feedback_api_signature_expresses_failure`
//      A helper whose body now adds a runtime check that could
//      reject the input must return `core::Result<T>`, and
//      every caller must propagate. No `value_or(default)`,
//      no fallback score, no skip-and-continue.
//
// This header is PRIVATE to the leviathan_systems library; it
// lives under `src/leviathan/systems/internal/` (not under
// `include/`) and is only reachable through the library's
// private include path.
//
// Stable field-path conventions (used in `field_name`):
//   - `country.stability`, `country.legitimacy`,
//     `country.corruption`, `country.gdp`,
//     `country.budget_balance`, `country.legal_tax_burden`,
//     `country.last_gdp_growth_rate`, `country.military_strength`,
//     `country.military_power`
//   - `country.government_authority.intelligence_capability`,
//     `country.government_authority.administrative_efficiency`, ...
//   - `country.budget.military`, `country.budget.intelligence`, ...
//   - `faction.loyalty`, `faction.support`, `faction.influence`,
//     `faction.radicalism`, `faction.resources`
//   - `interest_group.loyalty`, `interest_group.radicalism`,
//     `interest_group.influence`
//   - candidate paths for mid-computation values:
//     `<formula>.<step>` (e.g. `economy.gdp_growth_rate`,
//     `stability.target_raw`)
//
// Error message format (verbatim from PR #114):
//   "<module_name>: <entity_kind> '<id_code>' <field_name>
//    = <value> is not a finite ratio in [0, 1]"
//
// `entity_kind` is one of: "country", "faction",
// "interest_group", or a domain-specific kind like "candidate"
// when the value is an intermediate computation rather than a
// field on a tracked entity.

#ifndef LEVIATHAN_SYSTEMS_INTERNAL_NUMERIC_GUARDS_HPP
#define LEVIATHAN_SYSTEMS_INTERNAL_NUMERIC_GUARDS_HPP

#include <cmath>
#include <optional>
#include <string>

#include "leviathan/core/result.hpp"

namespace leviathan::systems::detail {

// Builds the canonical failure message for a unit-ratio
// constraint violation. Separated from `require_unit_ratio`
// so callers that prefer the predicate-then-build style (or
// that need a Result<T> with a non-default error type) can
// still use the canonical message format.
inline std::string make_unit_ratio_error(
        const char*        module_name,
        const char*        entity_kind,
        const std::string& entity_id_code,
        const char*        field_name,
        double             value) {
    return std::string(module_name) + ": " + entity_kind +
        " '" + entity_id_code + "' " + field_name +
        " = " + std::to_string(value) +
        " is not a finite ratio in [0, 1]";
}

inline std::string make_finite_error(
        const char*        module_name,
        const char*        entity_kind,
        const std::string& entity_id_code,
        const char*        field_name,
        double             value) {
    return std::string(module_name) + ": " + entity_kind +
        " '" + entity_id_code + "' " + field_name +
        " = " + std::to_string(value) + " is not finite";
}

inline std::string make_nonneg_finite_error(
        const char*        module_name,
        const char*        entity_kind,
        const std::string& entity_id_code,
        const char*        field_name,
        double             value) {
    return std::string(module_name) + ": " + entity_kind +
        " '" + entity_id_code + "' " + field_name +
        " = " + std::to_string(value) +
        " is not a finite non-negative value";
}

// Predicates (cheap; inline; no allocation). Use these when
// you only need a yes/no answer and will build the error
// message yourself.
inline bool is_unit_ratio(double v) noexcept {
    return std::isfinite(v) && v >= 0.0 && v <= 1.0;
}

inline bool is_finite_double(double v) noexcept {
    return std::isfinite(v);
}

inline bool is_nonneg_finite(double v) noexcept {
    return std::isfinite(v) && v >= 0.0;
}

// require_<predicate><T> — returns std::nullopt on success;
// a populated failure `Result<T>` on violation. Mirrors the
// PR #114 `require_unit_ratio` shape in
// `information_accuracy.cpp` so callers can write:
//
//     if (auto err = require_unit_ratio<MonthlyOutcome>(
//             "stability_system::tick", "country", c.id_code,
//             "country.stability", c.stability)) {
//         return *err;
//     }
//
// The template parameter `T` is the success type of the
// caller's own `Result<T>`. Failure passes through with the
// canonical error message; the success branch never builds a
// `Result<T>`, so the caller's `T` need not be default-
// constructible.
template <typename T>
std::optional<core::Result<T>> require_unit_ratio(
        const char*        module_name,
        const char*        entity_kind,
        const std::string& entity_id_code,
        const char*        field_name,
        double             value) {
    if (!is_unit_ratio(value)) {
        return core::Result<T>::failure(make_unit_ratio_error(
            module_name, entity_kind, entity_id_code,
            field_name, value));
    }
    return std::nullopt;
}

template <typename T>
std::optional<core::Result<T>> require_finite_double(
        const char*        module_name,
        const char*        entity_kind,
        const std::string& entity_id_code,
        const char*        field_name,
        double             value) {
    if (!is_finite_double(value)) {
        return core::Result<T>::failure(make_finite_error(
            module_name, entity_kind, entity_id_code,
            field_name, value));
    }
    return std::nullopt;
}

template <typename T>
std::optional<core::Result<T>> require_nonneg_finite(
        const char*        module_name,
        const char*        entity_kind,
        const std::string& entity_id_code,
        const char*        field_name,
        double             value) {
    if (!is_nonneg_finite(value)) {
        return core::Result<T>::failure(make_nonneg_finite_error(
            module_name, entity_kind, entity_id_code,
            field_name, value));
    }
    return std::nullopt;
}

}  // namespace leviathan::systems::detail

#endif  // LEVIATHAN_SYSTEMS_INTERNAL_NUMERIC_GUARDS_HPP
