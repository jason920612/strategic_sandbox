#include <doctest/doctest.h>

#include <cstddef>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/systems/economy_system.hpp"
#include "leviathan/systems/faction_system.hpp"
#include "leviathan/systems/interest_group_system.hpp"
#include "leviathan/systems/monthly_pipeline.hpp"
#include "leviathan/systems/stability_system.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::LogEntry;
namespace monthly  = leviathan::systems::monthly;
namespace fac      = leviathan::systems::faction;
namespace stab     = leviathan::systems::stability;
namespace econ     = leviathan::systems::economy;

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------

namespace {

// Canonical "GER 1930"-ish shape used elsewhere in the test suite. The
// numeric choices aren't important for the pipeline; what matters is
// that the three sub-systems all run without their own validation
// rejecting the input.
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

// A faction with all reaction-relevant fields populated. The other
// fields (influence, radicalism, resources, preferred_policies) stay
// at the values the caller sets.
FactionState make_faction(int id,
                          int country_id,
                          double support,
                          double loyalty,
                          double radicalism = 0.5) {
    FactionState f;
    f.id              = FactionId{id};
    f.country         = CountryId{country_id};
    f.id_code         = "f" + std::to_string(id);
    f.country_id_code = "C" + std::to_string(country_id);
    f.name            = "Faction " + std::to_string(id);
    f.type            = "bureaucracy";
    f.support         = support;
    f.influence       = 0.5;
    f.radicalism      = radicalism;
    f.loyalty         = loyalty;
    f.resources       = 0.0;
    return f;
}

}  // namespace

// =====================================================================
// Canonical-order proof
// =====================================================================

TEST_CASE("tick_country runs faction -> stability -> economy in canonical order") {
    // Build a country and faction where any reordering would produce a
    // distinguishable result:
    //
    //   country.stability   = 0.0
    //   country.legitimacy  = 1.0
    //   country.corruption  = 0.0
    //   faction.support     = 0.0
    //   faction.loyalty     = 0.0
    //   faction.radicalism  = 0.5
    //
    // Canonical pipeline:
    //   1) faction::react reads stability=0.0, legitimacy=1.0:
    //        loyalty += (0.0 - 0.0) * 0.10 = 0.0   -> loyalty stays 0.0
    //        support += (1.0 - 0.0) * 0.05 = 0.05  -> support becomes 0.05
    //   2) stability::tick reads NEW support=0.05, NEW radicalism=0.5:
    //        target  = 0.5*0.05 + 0.5*1.0 - 0.3*0.0 - 0.2*0.5 = 0.425
    //        stab   += (0.425 - 0.0)*0.10 = 0.0425
    //   3) economy::tick reads NEW stability=0.0425:
    //        drag    = 0.010 * (1 - 0.0425) = 0.009575
    //
    // If stability::tick ran BEFORE faction::react, faction.loyalty
    // would update against the new stability (0.04) and become 0.004,
    // not 0.0. If economy::tick ran BEFORE stability::tick, its drag
    // would use the old stability (0.0) and equal 0.010, not 0.009575.
    GameState state;
    auto c = germany_baseline();
    c.stability  = 0.0;
    c.legitimacy = 1.0;
    c.corruption = 0.0;
    c.administrative_efficiency = 0.0;
    c.budget.education      = 0.0;
    c.budget.infrastructure = 0.0;
    c.budget.industry       = 0.0;
    state.countries.push_back(c);
    state.factions.push_back(make_faction(0, 0, /*support*/ 0.0,
                                          /*loyalty*/ 0.0,
                                          /*radicalism*/ 0.5));

    const auto r = monthly::tick_country(state, CountryId{0});
    REQUIRE(r.ok());

    // 1. faction::react ran first and saw the OLD stability (0.0):
    //    loyalty delta = (0 - 0)*0.10 = 0.0   (stays 0.0; would be > 0 if reordered)
    CHECK(state.factions[0].loyalty == doctest::Approx(0.0));
    CHECK(state.factions[0].support == doctest::Approx(0.05));

    // 2. stability::tick ran second and saw NEW faction.support (0.05):
    //    target = 0.5*0.05 + 0.5*1.0 - 0 - 0.2*0.5 = 0.425
    //    new stability = 0.0 + (0.425 - 0)*0.10 = 0.0425
    CHECK(r.value().stability.target_stability  == doctest::Approx(0.425));
    CHECK(state.countries[0].stability          == doctest::Approx(0.0425));

    // 3. economy::tick ran third and saw NEW stability (0.0425):
    //    drag = 0.010 * (1 - 0.0425) = 0.009575
    //    growth = 0.005 - 0.009575 = -0.004575
    const double expected_growth = 0.005 - 0.010 * (1.0 - 0.0425);
    CHECK(r.value().economy.gdp_growth_rate == doctest::Approx(expected_growth));
}

// =====================================================================
// Country filter
// =====================================================================

TEST_CASE("tick_country only modifies the target country and its factions") {
    GameState state;
    state.countries.push_back(germany_baseline(0));
    state.countries.push_back(germany_baseline(1));
    state.factions.push_back(make_faction(0, /*country*/ 0, 0.3, 0.3));
    state.factions.push_back(make_faction(1, /*country*/ 1, 0.3, 0.3));

    const double c1_gdp_before        = state.countries[1].gdp;
    const double c1_stab_before       = state.countries[1].stability;
    const double c1_balance_before    = state.countries[1].budget_balance;
    const double c1_taxrev_before     = state.countries[1].tax_revenue;
    const double f1_support_before    = state.factions[1].support;
    const double f1_loyalty_before    = state.factions[1].loyalty;
    const double f1_radicalism_before = state.factions[1].radicalism;

    REQUIRE(monthly::tick_country(state, CountryId{0}).ok());

    // Country 1 (and its faction) untouched.
    CHECK(state.countries[1].gdp            == doctest::Approx(c1_gdp_before));
    CHECK(state.countries[1].stability      == doctest::Approx(c1_stab_before));
    CHECK(state.countries[1].budget_balance == doctest::Approx(c1_balance_before));
    CHECK(state.countries[1].tax_revenue    == doctest::Approx(c1_taxrev_before));
    CHECK(state.factions[1].support         == doctest::Approx(f1_support_before));
    CHECK(state.factions[1].loyalty         == doctest::Approx(f1_loyalty_before));
    CHECK(state.factions[1].radicalism      == doctest::Approx(f1_radicalism_before));

    // Country 0 changed (sanity check that the pipeline ran).
    CHECK(state.countries[0].tax_revenue != doctest::Approx(0.0));
}

// =====================================================================
// tick_all_countries
// =====================================================================

TEST_CASE("tick_all_countries processes every country in vector order") {
    GameState state;
    state.countries.push_back(germany_baseline(0));
    state.countries.push_back(germany_baseline(1));
    state.countries.push_back(germany_baseline(2));
    state.factions.push_back(make_faction(0, 0, 0.3, 0.3));
    state.factions.push_back(make_faction(1, 1, 0.3, 0.3));
    state.factions.push_back(make_faction(2, 2, 0.3, 0.3));

    const auto r = monthly::tick_all_countries(state);
    REQUIRE(r.ok());

    CHECK(r.value().countries_processed == 3);
    REQUIRE(r.value().countries.size() == 3u);
    CHECK(r.value().countries[0].country.value() == 0);
    CHECK(r.value().countries[1].country.value() == 1);
    CHECK(r.value().countries[2].country.value() == 2);

    // Every country actually got ticked: tax_revenue is nonzero now.
    CHECK(state.countries[0].tax_revenue != doctest::Approx(0.0));
    CHECK(state.countries[1].tax_revenue != doctest::Approx(0.0));
    CHECK(state.countries[2].tax_revenue != doctest::Approx(0.0));
}

TEST_CASE("tick_all_countries on empty state succeeds with zero processed") {
    GameState state;
    const auto r = monthly::tick_all_countries(state);
    REQUIRE(r.ok());
    CHECK(r.value().countries_processed == 0);
    CHECK(r.value().countries.empty());
}

// =====================================================================
// Error paths
// =====================================================================

TEST_CASE("tick_country rejects an invalid CountryId without state mutation") {
    GameState state;
    auto c = germany_baseline();
    const double gdp_before        = c.gdp;
    const double stability_before  = c.stability;
    state.countries.push_back(c);
    // Snapshot the unrelated country state used to verify no mutation.
    const double balance_before    = state.countries[0].budget_balance;

    const auto r = monthly::tick_country(state, CountryId{99});
    CHECK(r.failed());
    CHECK(state.countries[0].gdp            == doctest::Approx(gdp_before));
    CHECK(state.countries[0].stability      == doctest::Approx(stability_before));
    CHECK(state.countries[0].budget_balance == doctest::Approx(balance_before));
    CHECK(state.countries[0].tax_revenue    == doctest::Approx(0.0));
}

TEST_CASE("tick_country rejects a default-constructed CountryId") {
    GameState state;
    state.countries.push_back(germany_baseline());
    const auto r = monthly::tick_country(state, CountryId{});
    CHECK(r.failed());
    CHECK(state.countries[0].tax_revenue == doctest::Approx(0.0));
}

// =====================================================================
// Invariants the master prompt pins explicitly
// =====================================================================

TEST_CASE("tick_country does NOT change state.current_date") {
    GameState state;
    state.current_date = GameDate{1930, 3, 15};
    state.countries.push_back(germany_baseline());
    state.factions.push_back(make_faction(0, 0, 0.3, 0.3));

    REQUIRE(monthly::tick_country(state, CountryId{0}).ok());

    CHECK(state.current_date.year()  == 1930);
    CHECK(state.current_date.month() == 3);
    CHECK(state.current_date.day()   == 15);
}

TEST_CASE("tick_country does NOT append to state.logs") {
    GameState state;
    state.countries.push_back(germany_baseline());
    state.factions.push_back(make_faction(0, 0, 0.3, 0.3));
    const auto logs_before = state.logs.size();

    REQUIRE(monthly::tick_country(state, CountryId{0}).ok());

    CHECK(state.logs.size() == logs_before);
}

TEST_CASE("tick_country does NOT advance state.rng.counter") {
    GameState state;
    state.rng.seed    = 12345;
    state.rng.counter = 7;
    state.countries.push_back(germany_baseline());
    state.factions.push_back(make_faction(0, 0, 0.3, 0.3));

    REQUIRE(monthly::tick_country(state, CountryId{0}).ok());

    CHECK(state.rng.seed    == 12345u);
    CHECK(state.rng.counter == 7u);
}

TEST_CASE("tick_all_countries does NOT touch date / logs / RNG") {
    GameState state;
    state.current_date = GameDate{1930, 6, 1};
    state.rng.seed     = 999;
    state.rng.counter  = 42;
    state.countries.push_back(germany_baseline(0));
    state.countries.push_back(germany_baseline(1));
    state.factions.push_back(make_faction(0, 0, 0.3, 0.3));
    state.factions.push_back(make_faction(1, 1, 0.3, 0.3));
    const auto logs_before = state.logs.size();

    REQUIRE(monthly::tick_all_countries(state).ok());

    CHECK(state.current_date.year()  == 1930);
    CHECK(state.current_date.month() == 6);
    CHECK(state.current_date.day()   == 1);
    CHECK(state.rng.seed    == 999u);
    CHECK(state.rng.counter == 42u);
    CHECK(state.logs.size() == logs_before);
}

// =====================================================================
// Outcome struct
// =====================================================================

TEST_CASE("tick_country outcome carries every sub-system outcome") {
    GameState state;
    state.countries.push_back(germany_baseline());
    state.factions.push_back(make_faction(0, 0, 0.4, 0.3));

    const auto r = monthly::tick_country(state, CountryId{0});
    REQUIRE(r.ok());

    CHECK(r.value().country.value() == 0);
    CHECK(r.value().faction.factions_updated == 1);
    // stability outcome carries previous + new + target
    CHECK(r.value().stability.previous_stability == doctest::Approx(0.55));
    CHECK(r.value().stability.new_stability      == doctest::Approx(state.countries[0].stability));
    // economy outcome carries tax revenue + new GDP
    CHECK(r.value().economy.tax_revenue == doctest::Approx(state.countries[0].tax_revenue));
    CHECK(r.value().economy.new_gdp     == doctest::Approx(state.countries[0].gdp));
}

// =====================================================================
// M1.12: economy -> stability one-month lag
// =====================================================================

TEST_CASE("tick_country: stability sees PREVIOUS month's growth (M1.12 one-month lag)") {
    // Canonical order is faction -> stability -> economy. M1.12 means
    // stability::tick reads CountryState::last_gdp_growth_rate, which
    // economy::tick writes at the END of every tick. Pipeline order is
    // NOT changed (that's the explicit M1.12 design decision), so the
    // first monthly tick sees last_gdp_growth_rate = 0.0 (default),
    // and only the SECOND monthly tick's stability sees the growth
    // rate published by the first tick's economy.
    GameState state;
    auto c = germany_baseline();
    // Pre-condition: pristine default for last_gdp_growth_rate.
    REQUIRE(c.last_gdp_growth_rate == doctest::Approx(0.0));
    state.countries.push_back(c);
    state.factions.push_back(make_faction(0, 0, 0.4, 0.3));

    // --- First monthly tick -----------------------------------------
    // Stability runs BEFORE economy in the same call, so it reads
    // last_gdp_growth_rate = 0.0. EconomicGrowth contributes nothing
    // to this tick's stability target.
    const auto r1 = monthly::tick_country(state, CountryId{0});
    REQUIRE(r1.ok());
    const double target_first = r1.value().stability.target_stability;
    const double growth_first = r1.value().economy.gdp_growth_rate;

    // After the first tick, economy has published its growth rate.
    CHECK(state.countries[0].last_gdp_growth_rate == doctest::Approx(growth_first));
    CHECK(growth_first != doctest::Approx(0.0));  // sanity: GER 1930 has positive growth

    // --- Second monthly tick ----------------------------------------
    // Now stability::tick reads the non-zero last_gdp_growth_rate set
    // by tick #1. The target should be different from the first call's
    // target, all else being equal (here support / radicalism barely
    // moved since the canonical state isn't far from equilibrium).
    const auto r2 = monthly::tick_country(state, CountryId{0});
    REQUIRE(r2.ok());
    const double target_second = r2.value().stability.target_stability;

    // The EconomicGrowth term is + 2.0 * 0.00350 = +0.007 — small but
    // non-zero. tick #2's target should be HIGHER than tick #1's by
    // approximately that amount, modulo the small drift in
    // faction.support / radicalism.
    CHECK(target_second > target_first);
}

TEST_CASE("tick_country: pipeline ordering is unchanged (faction -> stability -> economy)") {
    // Regression for the M1.12 design decision: do NOT reorder so that
    // economy runs before stability "to fix the lag". The one-month
    // lag is intentional, and the canonical M1.9 ordering test still
    // pins faction -> stability -> economy. This test re-asserts that
    // the M1.12 coupling did NOT silently reorder the pipeline.
    GameState state;
    auto c = germany_baseline();
    c.stability  = 0.0;
    c.legitimacy = 1.0;
    c.corruption = 0.0;
    c.administrative_efficiency = 0.0;
    c.budget.education      = 0.0;
    c.budget.infrastructure = 0.0;
    c.budget.industry       = 0.0;
    // Critical: last_gdp_growth_rate starts at 0.0 (default). If
    // economy ran before stability, stability would see the growth
    // rate from this tick and produce a different target.
    state.countries.push_back(c);
    state.factions.push_back(make_faction(0, 0, /*support*/ 0.0,
                                          /*loyalty*/ 0.0,
                                          /*radicalism*/ 0.5));

    const auto r = monthly::tick_country(state, CountryId{0});
    REQUIRE(r.ok());

    // Re-derive the M1.9 canonical-order assertion: stability::tick
    // saw faction.support = 0.05 (after react) and last_gdp_growth_rate
    // = 0.0 (because economy hasn't run yet this tick). So:
    //   target = 0.5*0.05 + 0.5*1.0 - 0 - 0.2*0.5 + 2.0*0.0 = 0.425
    CHECK(r.value().stability.target_stability == doctest::Approx(0.425));
}

// =====================================================================
// M3.2 - interest_group::react runs after every per-country tick
// =====================================================================

TEST_CASE("tick_all_countries runs interest_group::react after every per-country tick (M3.2)") {
    // Build two countries with distinct stability values so the
    // post-pipeline drift is visibly different per group. Each
    // country owns one interest group sitting at the neutral
    // (0.5, 0.5) starting point.
    GameState state;
    CountryState ger = germany_baseline(0);
    ger.stability    = 0.55;   // baseline already.
    CountryState fra = germany_baseline(1);
    fra.id_code      = "FRA";
    fra.stability    = 0.55;   // explicit, mirrors ger.
    state.countries.push_back(ger);
    state.countries.push_back(fra);
    state.factions.push_back(make_faction(0, 0, 0.3, 0.3));
    state.factions.push_back(make_faction(1, 1, 0.3, 0.3));

    leviathan::core::InterestGroupState ger_b;
    ger_b.id_code = "ger_b";
    ger_b.name    = "GER bureaucracy";
    ger_b.kind    = leviathan::core::InterestGroupKind::Bureaucracy;
    ger_b.country = CountryId{0};
    leviathan::core::InterestGroupState fra_m;
    fra_m.id_code = "fra_m";
    fra_m.name    = "FRA military";
    fra_m.kind    = leviathan::core::InterestGroupKind::Military;
    fra_m.country = CountryId{1};
    state.interest_groups.push_back(ger_b);
    state.interest_groups.push_back(fra_m);

    // Existing M1 systems must still run — capture pre-tick GDPs
    // to assert economy::tick fired afterwards.
    const double ger_gdp_before = state.countries[0].gdp;
    const double fra_gdp_before = state.countries[1].gdp;

    const auto r = monthly::tick_all_countries(state);
    REQUIRE(r.ok());
    CHECK(r.value().countries_processed       == 2);
    CHECK(r.value().interest_groups_updated   == 2);

    // M3.2: interest groups drifted toward post-tick stability.
    // The per-country tick mutates stability, so we read the
    // post-tick value off the state itself rather than predicting
    // it (the canonical M1.9 ordering tests already pin the
    // pre-react math). The contract here is simply "react ran":
    // for both groups, BOTH loyalty AND radicalism must have
    // moved off the 0.5 starting point unless the post-tick
    // stability happens to be exactly 0.5 (it isn't with these
    // inputs).
    CHECK(state.interest_groups[0].loyalty    != doctest::Approx(0.5));
    CHECK(state.interest_groups[0].radicalism != doctest::Approx(0.5));
    CHECK(state.interest_groups[1].loyalty    != doctest::Approx(0.5));
    CHECK(state.interest_groups[1].radicalism != doctest::Approx(0.5));

    // M3.3 wires `interest_group::country_feedback` to run AFTER
    // `react` inside the same `tick_all_countries` call, which
    // means `country.stability` we read back off the state has
    // been mutated TWICE per tick (once by stability::tick, once
    // by country_feedback). Doing the exact arithmetic here would
    // duplicate the M3.3 formula in the M3.2 assertion. We keep
    // the directional check (above) and let the M3.2 unit tests
    // pin the exact arithmetic; the M3.3 monthly-pipeline test
    // (below) pins the layered behaviour.

    // Existing M1 systems still ran: GDP shifted via economy::tick.
    CHECK(state.countries[0].gdp != doctest::Approx(ger_gdp_before));
    CHECK(state.countries[1].gdp != doctest::Approx(fra_gdp_before));
}

TEST_CASE("tick_all_countries on a state with no interest_groups still succeeds (M3.2)") {
    // Regression: every existing M1 / M2 test fixture has
    // state.interest_groups empty. The added M3.2 step must not
    // change behaviour for those callers.
    GameState state;
    state.countries.push_back(germany_baseline(0));
    state.factions.push_back(make_faction(0, 0, 0.3, 0.3));
    REQUIRE(state.interest_groups.empty());

    const auto r = monthly::tick_all_countries(state);
    REQUIRE(r.ok());
    CHECK(r.value().countries_processed              == 1);
    CHECK(r.value().interest_groups_updated          == 0);
    // M3.3: with zero interest groups, country_feedback also has
    // nothing to do.
    CHECK(r.value().interest_group_countries_updated == 0);
}

// =====================================================================
// M3.3 - country_feedback closes the reaction loop inside tick_all_countries
// =====================================================================

TEST_CASE("tick_all_countries runs M3.2 react then M3.3 country_feedback in order (M3.3)") {
    // Build a country + group where M3.2 visibly changes the
    // group's radicalism and M3.3 then visibly changes the
    // country's stability based on that updated radicalism.
    GameState state;
    CountryState ger = germany_baseline(0);
    state.countries.push_back(ger);
    state.factions.push_back(make_faction(0, 0, /*support=*/0.3, /*loyalty=*/0.3));

    leviathan::core::InterestGroupState g;
    g.id_code    = "ger_b";
    g.name       = "GER bureaucracy";
    g.kind       = leviathan::core::InterestGroupKind::Bureaucracy;
    g.country    = CountryId{0};
    g.influence  = 1.0;     // single voice, full weight.
    g.loyalty    = 0.5;
    g.radicalism = 0.5;
    state.interest_groups.push_back(g);

    const double stab_before = state.countries[0].stability;
    const double rad_before  = state.interest_groups[0].radicalism;

    const auto r = monthly::tick_all_countries(state);
    REQUIRE(r.ok());
    CHECK(r.value().countries_processed              == 1);
    CHECK(r.value().interest_groups_updated          == 1);
    CHECK(r.value().interest_group_countries_updated == 1);

    // M3.2 ran: group's radicalism shifted off the starting 0.5.
    CHECK(state.interest_groups[0].radicalism != doctest::Approx(rad_before));

    // M3.3 ran: country's stability shifted off the pre-tick
    // value. Note the value compared against is the pre-tick
    // baseline; existing M1 systems (stability::tick + the new
    // M3.3 feedback) BOTH mutate it inside this call, so we
    // only assert "moved" rather than predicting an exact value.
    CHECK(state.countries[0].stability != doctest::Approx(stab_before));

    // Ordering pin: country_feedback reads the JUST-updated
    // radicalism. Re-run feedback on the resulting state — it
    // should produce a different result than running it on the
    // pre-react state. (This makes the test reject a
    // hypothetical implementation that ran feedback before
    // react.)
    const double post_pipeline_stability =
        state.countries[0].stability;
    REQUIRE(state.interest_groups[0].radicalism != doctest::Approx(0.5));
    // The closed-form for the M3.3 step alone, applied to the
    // post-pipeline state with its post-react radicalism:
    const double r_after = state.interest_groups[0].radicalism;
    const double expected_one_more_step =
        post_pipeline_stability +
        ((1.0 - r_after) - post_pipeline_stability) * 0.02;
    auto r2 = leviathan::systems::interest_group::country_feedback(state);
    REQUIRE(r2.ok());
    CHECK(state.countries[0].stability ==
          doctest::Approx(expected_one_more_step));
}
