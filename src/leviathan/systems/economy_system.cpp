#include "leviathan/systems/economy_system.hpp"

#include <cstddef>
#include <string>

#include "internal/numeric_guards.hpp"

namespace leviathan::systems::economy {

namespace ng = leviathan::systems::detail;

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

    // ---- Validate every input the formulas read --------------------
    // gdp is non-ratio non-negative; budget_balance is signed-finite.
    if (auto err = ng::require_nonneg_finite<EconomyOutcome>(
            "economy::tick", "country", c.id_code,
            "country.gdp", c.gdp)) {
        return *err;
    }
    if (auto err = ng::require_finite_double<EconomyOutcome>(
            "economy::tick", "country", c.id_code,
            "country.budget_balance", c.budget_balance)) {
        return *err;
    }
    // Ratio inputs the tax / expenditure / growth formulas read.
    struct RatioInput { const char* path; double value; };
    const RatioInput ratio_inputs[] = {
        {"country.legal_tax_burden",          c.legal_tax_burden},
        {"country.fiscal_capacity",           c.fiscal_capacity},
        {"country.central_control",           c.central_control},
        {"country.corruption",                c.corruption},
        {"country.stability",                 c.stability},
        {"country.administrative_efficiency", c.administrative_efficiency},
        {"country.budget.administration",     c.budget.administration},
        {"country.budget.military",           c.budget.military},
        {"country.budget.education",          c.budget.education},
        {"country.budget.welfare",            c.budget.welfare},
        {"country.budget.intelligence",       c.budget.intelligence},
        {"country.budget.infrastructure",     c.budget.infrastructure},
        {"country.budget.industry",           c.budget.industry},
    };
    for (const auto& ri : ratio_inputs) {
        if (auto err = ng::require_unit_ratio<EconomyOutcome>(
                "economy::tick", "country", c.id_code,
                ri.path, ri.value)) {
            return *err;
        }
    }

    // ---- 1. Tax revenue (RFC-080 §3) ------------------------------
    EconomyOutcome out;
    out.previous_gdp = c.gdp;
    out.tax_revenue =
          c.gdp
        * c.legal_tax_burden
        * c.fiscal_capacity
        * c.central_control
        * (1.0 - c.corruption);

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

    // ---- 3. Candidate budget balance ------------------------------
    out.budget_delta = out.tax_revenue - out.expenditure;
    const double candidate_balance = c.budget_balance + out.budget_delta;
    if (auto err = ng::require_finite_double<EconomyOutcome>(
            "economy::tick", "country", c.id_code,
            "country.budget_balance (candidate)", candidate_balance)) {
        return *err;
    }

    // ---- 4. GDP growth (stripped-down RFC-080 §4) -----------------
    out.gdp_growth_rate =
          kBaseGrowth
        + kEducationGrowthWeight       * c.budget.education
        + kInfrastructureGrowthWeight  * c.budget.infrastructure
        + kIndustryGrowthWeight        * c.budget.industry
        + kAdminEfficiencyGrowthWeight * c.administrative_efficiency
        - kPoliticalInstabilityDrag    * (1.0 - c.stability)
        - kCorruptionGrowthDrag        * c.corruption;

    if (auto err = ng::require_finite_double<EconomyOutcome>(
            "economy::tick", "country", c.id_code,
            "economy.gdp_growth_rate (formula candidate)",
            out.gdp_growth_rate)) {
        return *err;
    }

    const double candidate_gdp = c.gdp * (1.0 + out.gdp_growth_rate);
    // Post-M6.7 hardening: the previous `if (c.gdp < 0.0) c.gdp = 0.0;`
    // silent floor is gone. A growth_rate so negative that gdp would
    // turn negative signals an upstream coefficient or input bug; the
    // tick rejects rather than masking it.
    if (auto err = ng::require_nonneg_finite<EconomyOutcome>(
            "economy::tick", "country", c.id_code,
            "country.gdp (candidate)", candidate_gdp)) {
        return *err;
    }

    // ---- Commit (all candidates validated above) -------------------
    c.tax_revenue          = out.tax_revenue;
    c.budget_balance       = candidate_balance;
    out.new_budget_balance = c.budget_balance;
    c.gdp                  = candidate_gdp;
    out.new_gdp            = c.gdp;
    // M1.12: publish the just-computed growth rate so the next monthly
    // stability::tick can pick it up as the RFC-080 §5 EconomicGrowth
    // term. Reaches here only after the growth_rate finiteness check
    // above, so c.last_gdp_growth_rate is guaranteed finite.
    c.last_gdp_growth_rate = out.gdp_growth_rate;

    return core::Result<EconomyOutcome>::success(out);
}

}  // namespace leviathan::systems::economy
