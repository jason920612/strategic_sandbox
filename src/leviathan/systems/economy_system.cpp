#include "leviathan/systems/economy_system.hpp"

#include <cstddef>
#include <string>

namespace leviathan::systems::economy {

core::Result<EconomyOutcome> tick(core::GameState& state,
                                  core::CountryId  country) {
    if (!country.valid() ||
        country.value() < 0 ||
        static_cast<std::size_t>(country.value()) >= state.countries.size()) {
        return core::Result<EconomyOutcome>::failure(
            "economy::tick: country CountryId " +
            std::to_string(country.value()) +
            " is not a valid index into state.countries");
    }

    auto& c = state.countries[static_cast<std::size_t>(country.value())];

    EconomyOutcome out;
    out.previous_gdp = c.gdp;

    // ---- 1. Tax revenue (RFC-080 §3) ------------------------------
    out.tax_revenue =
          c.gdp
        * c.legal_tax_burden
        * c.fiscal_capacity
        * c.central_control
        * (1.0 - c.corruption);
    c.tax_revenue = out.tax_revenue;   // per-tick, overwrites prior value

    // ---- 2. Expenditure (M1.8 simplification) ---------------------
    const double sum_budget =
          c.budget.administration
        + c.budget.military
        + c.budget.education
        + c.budget.welfare
        + c.budget.intelligence
        + c.budget.infrastructure
        + c.budget.industry;
    out.expenditure = c.gdp * sum_budget * kExpenditureScale;

    // ---- 3. Budget balance ----------------------------------------
    out.budget_delta       = out.tax_revenue - out.expenditure;
    c.budget_balance      += out.budget_delta;
    out.new_budget_balance = c.budget_balance;

    // ---- 4. GDP growth (stripped-down RFC-080 §4) -----------------
    out.gdp_growth_rate =
          kBaseGrowth
        + kEducationGrowthWeight       * c.budget.education
        + kInfrastructureGrowthWeight  * c.budget.infrastructure
        + kIndustryGrowthWeight        * c.budget.industry
        + kAdminEfficiencyGrowthWeight * c.administrative_efficiency
        - kPoliticalInstabilityDrag    * (1.0 - c.stability)
        - kCorruptionGrowthDrag        * c.corruption;

    c.gdp *= (1.0 + out.gdp_growth_rate);
    if (c.gdp < 0.0) c.gdp = 0.0;   // floor: gdp can asymptote toward 0 but
                                    // shouldn't go negative through compounded
                                    // recessions
    out.new_gdp = c.gdp;

    // M1.12: publish the just-computed growth rate so the next
    // monthly stability::tick can pick it up as the RFC-080 §5
    // EconomicGrowth term. The write happens UNCONDITIONALLY for any
    // successful tick (including the gdp == 0 edge case); the
    // invalid-id failure path returned earlier without mutating state.
    c.last_gdp_growth_rate = out.gdp_growth_rate;

    return core::Result<EconomyOutcome>::success(out);
}

}  // namespace leviathan::systems::economy
