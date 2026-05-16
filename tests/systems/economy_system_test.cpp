#include <doctest/doctest.h>

#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/systems/economy_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameState;
namespace econ = leviathan::systems::economy;

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------

namespace {

// Build a country with the canonical "GER 1930" shape from
// data/countries/germany.json. Individual tests override specific
// fields as needed.
CountryState germany_baseline(int id = 0) {
    CountryState c;
    c.id           = CountryId{id};
    c.id_code      = "GER";
    c.name         = "Germany";
    c.gdp                       = 100.0;
    c.tax_revenue               = 0.0;
    c.budget_balance            = 0.0;
    c.legal_tax_burden          = 0.20;
    c.fiscal_capacity           = 0.50;
    c.administrative_efficiency = 0.55;
    c.central_control           = 0.60;
    c.corruption                = 0.25;
    c.stability                 = 0.55;
    c.legitimacy                = 0.55;
    c.military_power            = 0.50;
    c.threat_perception         = 0.30;
    c.budget.administration     = 0.25;
    c.budget.military           = 0.35;
    c.budget.education          = 0.10;
    c.budget.welfare            = 0.10;
    c.budget.intelligence       = 0.05;
    c.budget.infrastructure     = 0.10;
    c.budget.industry           = 0.05;
    return c;
}

CountryState zero_country(int id = 0) {
    CountryState c;
    c.id      = CountryId{id};
    c.id_code = "ZZZ";
    c.name    = "Zero";
    // Everything else stays zeroed.
    return c;
}

}  // namespace

// =====================================================================
// Tax revenue formula exact
// =====================================================================

TEST_CASE("tick: tax_revenue uses the RFC-080 §3 formula exactly") {
    GameState state;
    state.countries.push_back(germany_baseline());
    // tax_revenue = 100 * 0.20 * 0.50 * 0.60 * (1 - 0.25)
    //             = 100 * 0.20 * 0.50 * 0.60 * 0.75
    //             = 4.5
    const auto r = econ::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().tax_revenue           == doctest::Approx(4.5));
    CHECK(state.countries[0].tax_revenue  == doctest::Approx(4.5));
}

TEST_CASE("tick: tax_revenue overwrites (not accumulates) on repeat calls") {
    GameState state;
    state.countries.push_back(germany_baseline());

    REQUIRE(econ::tick(state, CountryId{0}).ok());
    const double first = state.countries[0].tax_revenue;

    // tick again - tax_revenue should NOT be 2 * first
    REQUIRE(econ::tick(state, CountryId{0}).ok());
    const double second = state.countries[0].tax_revenue;

    // The second tick's gdp grew slightly, so revenue is slightly larger,
    // but nowhere near 2x.
    CHECK(second > first);
    CHECK(second < first * 1.10);
}

// =====================================================================
// Expenditure formula exact
// =====================================================================

TEST_CASE("tick: expenditure = gdp * sum_budget * kExpenditureScale") {
    GameState state;
    state.countries.push_back(germany_baseline());
    // sum_budget = 0.25 + 0.35 + 0.10 + 0.10 + 0.05 + 0.10 + 0.05 = 1.00
    // expenditure = 100 * 1.00 * 0.20 = 20.0
    const auto r = econ::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().expenditure == doctest::Approx(20.0));
}

TEST_CASE("tick: under-allocated budget produces proportionally smaller spend") {
    GameState state;
    auto g = germany_baseline();
    // Halve every budget category - sum_budget becomes 0.50.
    g.budget.administration  = 0.125;
    g.budget.military        = 0.175;
    g.budget.education       = 0.050;
    g.budget.welfare         = 0.050;
    g.budget.intelligence    = 0.025;
    g.budget.infrastructure  = 0.050;
    g.budget.industry        = 0.025;
    state.countries.push_back(g);

    const auto r = econ::tick(state, CountryId{0});
    REQUIRE(r.ok());
    // expenditure = 100 * 0.50 * 0.20 = 10.0
    CHECK(r.value().expenditure == doctest::Approx(10.0));
}

// =====================================================================
// Budget balance update
// =====================================================================

TEST_CASE("tick: budget_balance += (tax_revenue - expenditure)") {
    GameState state;
    auto g = germany_baseline();
    g.budget_balance = 0.0;
    state.countries.push_back(g);
    // revenue 4.5, expenditure 20.0 -> delta -15.5
    const auto r = econ::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().budget_delta              == doctest::Approx(4.5 - 20.0));
    CHECK(r.value().new_budget_balance        == doctest::Approx(-15.5));
    CHECK(state.countries[0].budget_balance   == doctest::Approx(-15.5));
}

TEST_CASE("tick: budget_balance accumulates across ticks") {
    GameState state;
    state.countries.push_back(germany_baseline());
    REQUIRE(econ::tick(state, CountryId{0}).ok());
    const double after_first = state.countries[0].budget_balance;

    REQUIRE(econ::tick(state, CountryId{0}).ok());
    // Second tick adds another (slightly different, due to GDP growth)
    // deficit. The total should be roughly 2x the first.
    CHECK(state.countries[0].budget_balance < after_first);
    CHECK(state.countries[0].budget_balance > 2.5 * after_first);
}

// =====================================================================
// GDP growth formula exact
// =====================================================================

TEST_CASE("tick: gdp_growth_rate uses the documented formula exactly") {
    GameState state;
    state.countries.push_back(germany_baseline());
    // growth = 0.005
    //        + 0.005*0.10 = 0.0005 (education)
    //        + 0.005*0.10 = 0.0005 (infrastructure)
    //        + 0.010*0.05 = 0.0005 (industry)
    //        + 0.005*0.55 = 0.00275 (admin efficiency)
    //        - 0.010*(1-0.55) = -0.00450 (political instability)
    //        - 0.005*0.25     = -0.00125 (corruption)
    //        = 0.005 + 0.0005 + 0.0005 + 0.0005 + 0.00275 - 0.00450 - 0.00125
    //        = 0.005 + 0.0015 + 0.00275 - 0.00575
    //        = 0.005 + 0.00425 - 0.00575
    //        = 0.00350
    const auto r = econ::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().gdp_growth_rate == doctest::Approx(0.00350));
    // gdp 100 * (1 + 0.00350) = 100.350
    CHECK(r.value().new_gdp           == doctest::Approx(100.350));
    CHECK(state.countries[0].gdp      == doctest::Approx(100.350));
}

TEST_CASE("tick: growth constants match the spec") {
    CHECK(econ::kBaseGrowth                  == doctest::Approx(0.005));
    CHECK(econ::kEducationGrowthWeight       == doctest::Approx(0.005));
    CHECK(econ::kInfrastructureGrowthWeight  == doctest::Approx(0.005));
    CHECK(econ::kIndustryGrowthWeight        == doctest::Approx(0.010));
    CHECK(econ::kAdminEfficiencyGrowthWeight == doctest::Approx(0.005));
    CHECK(econ::kPoliticalInstabilityDrag    == doctest::Approx(0.010));
    CHECK(econ::kCorruptionGrowthDrag        == doctest::Approx(0.005));
    CHECK(econ::kExpenditureScale            == doctest::Approx(0.20));
}

// =====================================================================
// Edge cases
// =====================================================================

TEST_CASE("tick: gdp=0 produces zero revenue / expenditure / new_gdp") {
    GameState state;
    auto z = zero_country();
    z.budget.administration = 0.10;   // some budget set but no GDP
    state.countries.push_back(z);

    const auto r = econ::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().tax_revenue == doctest::Approx(0.0));
    CHECK(r.value().expenditure == doctest::Approx(0.0));
    CHECK(r.value().new_gdp     == doctest::Approx(0.0));
    // Growth rate is still computed (it doesn't depend on gdp), but
    // applying it to 0 GDP yields 0.
    // For zero country, growth = kBaseGrowth + 0 + 0 + 0 + 0
    //                          - kPoliticalInstabilityDrag * 1.0
    //                          - 0
    //                          = 0.005 - 0.010 = -0.005
    CHECK(r.value().gdp_growth_rate == doctest::Approx(-0.005));
}

TEST_CASE("tick: positive growth increases GDP") {
    GameState state;
    auto g = germany_baseline();
    // Boost growth-positive inputs to high values.
    g.budget.education      = 1.0;
    g.budget.infrastructure = 1.0;
    g.budget.industry       = 1.0;
    g.administrative_efficiency = 1.0;
    g.stability  = 1.0;
    g.corruption = 0.0;
    state.countries.push_back(g);

    const auto r = econ::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().new_gdp > 100.0);
    CHECK(r.value().gdp_growth_rate > 0.0);
}

TEST_CASE("tick: recession case shrinks GDP") {
    GameState state;
    auto g = germany_baseline();
    g.budget.education      = 0.0;
    g.budget.infrastructure = 0.0;
    g.budget.industry       = 0.0;
    g.administrative_efficiency = 0.0;
    g.stability  = 0.0;
    g.corruption = 1.0;
    state.countries.push_back(g);
    // growth = 0.005 - 0.010 - 0.005 = -0.010
    const auto r = econ::tick(state, CountryId{0});
    REQUIRE(r.ok());
    CHECK(r.value().gdp_growth_rate == doctest::Approx(-0.010));
    CHECK(r.value().new_gdp         == doctest::Approx(99.0));
}

TEST_CASE("tick: 12 monthly ticks compound to roughly annual baseline") {
    GameState state;
    state.countries.push_back(germany_baseline());
    for (int i = 0; i < 12; ++i) {
        REQUIRE(econ::tick(state, CountryId{0}).ok());
    }
    // baseline monthly growth ~= 0.00350 (the value from the earlier
    // exact test). Compounded 12 times: (1.00350)^12 ~= 1.0428.
    // GDP starts at 100 -> ends near 104.28. Use generous epsilon
    // because GDP affects nothing here (no feedback).
    CHECK(state.countries[0].gdp == doctest::Approx(104.28).epsilon(0.01));
}

// =====================================================================
// Filter / isolation
// =====================================================================

TEST_CASE("tick: only target country is modified") {
    GameState state;
    state.countries.push_back(germany_baseline(0));
    state.countries.push_back(germany_baseline(1));
    const double c1_gdp_before     = state.countries[1].gdp;
    const double c1_balance_before = state.countries[1].budget_balance;

    REQUIRE(econ::tick(state, CountryId{0}).ok());

    CHECK(state.countries[0].gdp != doctest::Approx(c1_gdp_before));
    CHECK(state.countries[1].gdp            == doctest::Approx(c1_gdp_before));
    CHECK(state.countries[1].budget_balance == doctest::Approx(c1_balance_before));
    CHECK(state.countries[1].tax_revenue    == doctest::Approx(0.0));
}

TEST_CASE("tick: does NOT modify any faction state") {
    GameState state;
    state.countries.push_back(germany_baseline());
    FactionState f;
    f.id              = FactionId{0};
    f.country         = CountryId{0};
    f.id_code         = "GER_military";
    f.country_id_code = "GER";
    f.type            = "military";
    f.support         = 0.42;
    f.influence       = 0.50;
    f.radicalism      = 0.30;
    f.loyalty         = 0.55;
    f.resources       = 1.20;
    state.factions.push_back(f);

    REQUIRE(econ::tick(state, CountryId{0}).ok());

    CHECK(state.factions[0].support    == doctest::Approx(0.42));
    CHECK(state.factions[0].influence  == doctest::Approx(0.50));
    CHECK(state.factions[0].radicalism == doctest::Approx(0.30));
    CHECK(state.factions[0].loyalty    == doctest::Approx(0.55));
    CHECK(state.factions[0].resources  == doctest::Approx(1.20));
}

TEST_CASE("tick: does NOT modify country fields outside the economy") {
    GameState state;
    state.countries.push_back(germany_baseline());

    REQUIRE(econ::tick(state, CountryId{0}).ok());

    // legal_tax_burden / fiscal_capacity / central_control / corruption
    // / stability / legitimacy / military_power / threat_perception /
    // budget.* are all READ but not WRITTEN by economy::tick.
    const auto& c = state.countries[0];
    CHECK(c.legal_tax_burden          == doctest::Approx(0.20));
    CHECK(c.fiscal_capacity           == doctest::Approx(0.50));
    CHECK(c.administrative_efficiency == doctest::Approx(0.55));
    CHECK(c.central_control           == doctest::Approx(0.60));
    CHECK(c.corruption                == doctest::Approx(0.25));
    CHECK(c.stability                 == doctest::Approx(0.55));
    CHECK(c.legitimacy                == doctest::Approx(0.55));
    CHECK(c.military_power            == doctest::Approx(0.50));
    CHECK(c.threat_perception         == doctest::Approx(0.30));
    CHECK(c.budget.education          == doctest::Approx(0.10));
    CHECK(c.budget.industry           == doctest::Approx(0.05));
}

// =====================================================================
// Outcome struct
// =====================================================================

TEST_CASE("tick: outcome struct carries every field") {
    GameState state;
    state.countries.push_back(germany_baseline());

    const auto r = econ::tick(state, CountryId{0});
    REQUIRE(r.ok());
    const auto& o = r.value();
    CHECK(o.previous_gdp          == doctest::Approx(100.0));
    CHECK(o.new_gdp               == doctest::Approx(100.350));
    CHECK(o.tax_revenue           == doctest::Approx(4.5));
    CHECK(o.expenditure           == doctest::Approx(20.0));
    CHECK(o.budget_delta          == doctest::Approx(-15.5));
    CHECK(o.new_budget_balance    == doctest::Approx(-15.5));
    CHECK(o.gdp_growth_rate       == doctest::Approx(0.00350));
}

// =====================================================================
// Error paths
// =====================================================================

TEST_CASE("tick: invalid CountryId is rejected, state unchanged") {
    GameState state;
    state.countries.push_back(germany_baseline());
    const auto before_gdp     = state.countries[0].gdp;
    const auto before_balance = state.countries[0].budget_balance;

    const auto r = econ::tick(state, CountryId{99});
    REQUIRE(r.failed());
    CHECK(r.error().find("CountryId 99") != std::string::npos);
    CHECK(state.countries[0].gdp            == doctest::Approx(before_gdp));
    CHECK(state.countries[0].budget_balance == doctest::Approx(before_balance));
}

TEST_CASE("tick: default-constructed (invalid) CountryId is rejected") {
    GameState state;
    state.countries.push_back(germany_baseline());
    const auto r = econ::tick(state, CountryId{});
    REQUIRE(r.failed());
}

TEST_CASE("tick: empty state (no countries) -> invalid id rejected") {
    GameState state;
    const auto r = econ::tick(state, CountryId{0});
    REQUIRE(r.failed());
}
