// EconomySystem - one month-end economy step.
//
// M1.8 ships the second country-side dynamic (after M1.7 stability).
// Same pattern: explicit-call free function over GameState&. No
// monthly tick wiring, no runner integration, no AI.
//
// Three formulas, all using inputs that already exist on
// CountryState as of M1.3:
//
//   1. Tax revenue (RFC-080 §3 verbatim):
//
//        tax_revenue = gdp * legal_tax_burden * fiscal_capacity
//                    * central_control * (1 - corruption)
//
//      Overwritten on each tick - it is "this tick's revenue", not a
//      running cumulative total. Cumulative tracking lives in
//      budget_balance.
//
//   2. Expenditure (M1.8 simplification):
//
//        sum_budget   = administration + military + education + welfare
//                     + intelligence + infrastructure + industry
//        expenditure  = gdp * sum_budget * kExpenditureScale
//
//      Each tick the state "owes" gdp * sum_budget * 0.20 in spending.
//      With sum_budget = 1.0 that's 20% of GDP. Under- or
//      over-allocated budgets produce proportionally smaller / larger
//      spending obligations.
//
//   3. Budget balance update (this tick's net):
//
//        budget_balance += (tax_revenue - expenditure)
//
//      Can go negative (deficit accumulates). No clamp.
//
//   4. GDP growth (stripped-down RFC-080 §4):
//
//        growth_rate = kBaseGrowth
//                    + kEducationGrowthWeight       * budget.education
//                    + kInfrastructureGrowthWeight  * budget.infrastructure
//                    + kIndustryGrowthWeight        * budget.industry
//                    + kAdminEfficiencyGrowthWeight * administrative_efficiency
//                    - kPoliticalInstabilityDrag    * (1 - stability)
//                    - kCorruptionGrowthDrag        * corruption
//        gdp *= (1 + growth_rate)
//
//      Constants are sized so a country in the canonical "GER 1930"
//      shape (sum_budget = 1.0, stability = 0.55, corruption = 0.25)
//      produces a monthly growth_rate = 0.00350, which compounds
//      across 12 ticks to ~4.3% annual ((1.0035)^12 ≈ 1.0428).
//
// RFC-080 §4 also references `InflationPressure` and `WarDamage`.
// Neither has input data yet; they're additive future extensions
// (M1.x once inflation / war systems land).

#ifndef LEVIATHAN_SYSTEMS_ECONOMY_SYSTEM_HPP
#define LEVIATHAN_SYSTEMS_ECONOMY_SYSTEM_HPP

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::economy {

struct EconomyOutcome {
    double previous_gdp        = 0.0;
    double new_gdp             = 0.0;
    double tax_revenue         = 0.0;   // also overwrites state.country.tax_revenue
    double expenditure         = 0.0;   // not stored on state
    double budget_delta        = 0.0;   // tax_revenue - expenditure
    double new_budget_balance  = 0.0;
    double gdp_growth_rate     = 0.0;   // fractional, e.g. 0.005 = +0.5%
};

// ---- Constants -----------------------------------------------------
// Tax / expenditure
inline constexpr double kExpenditureScale            = 0.20;

// GDP growth weights (positive contributors)
inline constexpr double kBaseGrowth                  = 0.005;
inline constexpr double kEducationGrowthWeight       = 0.005;
inline constexpr double kInfrastructureGrowthWeight  = 0.005;
inline constexpr double kIndustryGrowthWeight        = 0.010;
inline constexpr double kAdminEfficiencyGrowthWeight = 0.005;   // rule-of-law proxy

// GDP growth drag (negative contributors, both [0,1] inputs)
inline constexpr double kPoliticalInstabilityDrag    = 0.010;   // multiplies (1 - stability)
inline constexpr double kCorruptionGrowthDrag        = 0.005;

// Apply one month-end economy step to `country` in `state`.
// See the formulas at the top of this header.
//
// Failure cases:
//   - `country` is not a valid index into `state.countries`
// On failure, no state is modified.
//
// Only state.countries[country] fields are modified: gdp,
// tax_revenue, budget_balance. Faction state is never touched.
core::Result<EconomyOutcome> tick(core::GameState& state,
                                  core::CountryId  country);

}  // namespace leviathan::systems::economy

#endif  // LEVIATHAN_SYSTEMS_ECONOMY_SYSTEM_HPP
