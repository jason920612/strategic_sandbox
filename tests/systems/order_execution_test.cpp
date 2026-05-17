#include <doctest/doctest.h>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/player_commands.hpp"
#include "leviathan/systems/order_execution.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::GameState;
using leviathan::core::PlayerCommand;
using leviathan::core::PlayerCommandKind;
namespace oe = leviathan::systems::order_execution;

namespace {

CountryState seeded_country() {
    CountryState c;
    c.id      = CountryId{0};
    c.id_code = "GER";
    c.name    = "Germany";
    c.government_authority.bureaucratic_compliance = 0.62;
    c.government_authority.military_loyalty        = 0.84;
    c.government_authority.intelligence_capability = 0.41;
    c.government_authority.media_control           = 0.17;
    return c;
}

PlayerCommand enact_raise_taxes() {
    PlayerCommand cmd;
    cmd.kind            = PlayerCommandKind::EnactPolicy;
    cmd.policy_id_code  = "raise_taxes";
    return cmd;
}

PlayerCommand adjust_military_budget() {
    PlayerCommand cmd;
    cmd.kind            = PlayerCommandKind::AdjustBudget;
    cmd.budget_category = "military";
    cmd.budget_delta    = 0.05;
    return cmd;
}

}  // namespace

// ---------------------------------------------------------------------
// preconditions
// ---------------------------------------------------------------------

TEST_CASE("evaluate: no player_country selected is rejected") {
    GameState state;
    state.countries.push_back(seeded_country());
    // state.player_country defaults to invalid (-1).
    REQUIRE_FALSE(state.player_country.valid());

    const auto r = oe::evaluate(state, enact_raise_taxes());
    REQUIRE(r.failed());
    CHECK(r.error().find("order_execution::evaluate") != std::string::npos);
    CHECK(r.error().find("player_country")            != std::string::npos);
}

TEST_CASE("evaluate: player_country out of range is rejected") {
    GameState state;
    state.countries.push_back(seeded_country());
    state.player_country = CountryId{5};  // only index 0 exists.

    const auto r = oe::evaluate(state, enact_raise_taxes());
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
}

TEST_CASE("evaluate: empty countries with any selection is rejected") {
    GameState state;
    state.player_country = CountryId{0};  // nothing to index into.

    const auto r = oe::evaluate(state, enact_raise_taxes());
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
}

// ---------------------------------------------------------------------
// success path
// ---------------------------------------------------------------------

TEST_CASE("evaluate: valid selection returns Accepted") {
    GameState state;
    state.countries.push_back(seeded_country());
    state.player_country = CountryId{0};

    const auto r = oe::evaluate(state, enact_raise_taxes());
    REQUIRE(r.ok());
    CHECK(r.value().status == oe::ExecutionStatus::Accepted);
}

TEST_CASE("evaluate: inputs mirror the actor country's government_authority") {
    GameState state;
    state.countries.push_back(seeded_country());
    state.player_country = CountryId{0};

    const auto r = oe::evaluate(state, enact_raise_taxes());
    REQUIRE(r.ok());
    const auto& in = r.value().inputs;
    CHECK(in.bureaucratic_compliance  == doctest::Approx(0.62));
    CHECK(in.military_loyalty         == doctest::Approx(0.84));
    CHECK(in.intelligence_capability  == doctest::Approx(0.41));
    CHECK(in.media_control            == doctest::Approx(0.17));
}

TEST_CASE("evaluate: reads the selected country, not country[0]") {
    GameState state;
    CountryState a = seeded_country();          // index 0 - DEFAULT 0.5s.
    a.government_authority = leviathan::core::GovernmentAuthorityState{};
    state.countries.push_back(a);
    CountryState b = seeded_country();          // index 1 - the seeded values.
    b.id      = CountryId{1};
    b.id_code = "FRA";
    state.countries.push_back(b);
    state.player_country = CountryId{1};

    const auto r = oe::evaluate(state, enact_raise_taxes());
    REQUIRE(r.ok());
    // The skeleton snapshotted index 1, not the all-0.5 default at index 0.
    CHECK(r.value().inputs.military_loyalty ==
          doctest::Approx(0.84));
}

// ---------------------------------------------------------------------
// non-mutation + kind independence
// ---------------------------------------------------------------------

TEST_CASE("evaluate: leaves the state byte-identical") {
    GameState before;
    before.countries.push_back(seeded_country());
    before.player_country = CountryId{0};

    GameState after = before;
    REQUIRE(oe::evaluate(after, enact_raise_taxes()).ok());

    // Compare the fields the skeleton reads. State.logs, RNG, dates
    // and the authority block all stay byte-identical.
    CHECK(after.current_date          == before.current_date);
    CHECK(after.player_country.value() == before.player_country.value());
    CHECK(after.countries.size()       == before.countries.size());
    CHECK(after.countries[0].government_authority.bureaucratic_compliance ==
          before.countries[0].government_authority.bureaucratic_compliance);
    CHECK(after.countries[0].government_authority.military_loyalty ==
          before.countries[0].government_authority.military_loyalty);
    CHECK(after.countries[0].government_authority.intelligence_capability ==
          before.countries[0].government_authority.intelligence_capability);
    CHECK(after.countries[0].government_authority.media_control ==
          before.countries[0].government_authority.media_control);
    CHECK(after.logs.size()            == before.logs.size());
    CHECK(after.applied_commands.size() == before.applied_commands.size());
}

TEST_CASE("evaluate: EnactPolicy and AdjustBudget produce identical outcomes") {
    GameState state;
    state.countries.push_back(seeded_country());
    state.player_country = CountryId{0};

    const auto enact  = oe::evaluate(state, enact_raise_taxes());
    const auto adjust = oe::evaluate(state, adjust_military_budget());
    REQUIRE(enact.ok());
    REQUIRE(adjust.ok());
    CHECK(enact.value().status == adjust.value().status);
    CHECK(enact.value().inputs.bureaucratic_compliance ==
          doctest::Approx(adjust.value().inputs.bureaucratic_compliance));
    CHECK(enact.value().inputs.military_loyalty ==
          doctest::Approx(adjust.value().inputs.military_loyalty));
    CHECK(enact.value().inputs.intelligence_capability ==
          doctest::Approx(adjust.value().inputs.intelligence_capability));
    CHECK(enact.value().inputs.media_control ==
          doctest::Approx(adjust.value().inputs.media_control));
}

TEST_CASE("evaluate: repeated calls are deterministic") {
    GameState state;
    state.countries.push_back(seeded_country());
    state.player_country = CountryId{0};

    const auto a = oe::evaluate(state, enact_raise_taxes());
    const auto b = oe::evaluate(state, enact_raise_taxes());
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    CHECK(a.value().status == b.value().status);
    CHECK(a.value().inputs.bureaucratic_compliance ==
          doctest::Approx(b.value().inputs.bureaucratic_compliance));
    CHECK(a.value().inputs.media_control ==
          doctest::Approx(b.value().inputs.media_control));
}

// ---------------------------------------------------------------------
// default constructions
// ---------------------------------------------------------------------

TEST_CASE("default OrderExecutionOutcome has the M2.17 baseline") {
    oe::OrderExecutionOutcome o;
    CHECK(o.status == oe::ExecutionStatus::Accepted);
    CHECK(o.inputs.bureaucratic_compliance  == doctest::Approx(0.5));
    CHECK(o.inputs.military_loyalty         == doctest::Approx(0.5));
    CHECK(o.inputs.intelligence_capability  == doctest::Approx(0.5));
    CHECK(o.inputs.media_control            == doctest::Approx(0.5));
}
