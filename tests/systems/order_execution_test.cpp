#include <doctest/doctest.h>

#include <string>

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

TEST_CASE("evaluate: EnactPolicy and AdjustBudget snapshot the same inputs but compute different resistance") {
    // M2.18 wired the EnactPolicy gate on bureaucratic_compliance;
    // M2.19 wired the AdjustBudget gate on military_loyalty for
    // the "military" category. Both arms now fill resistance, but
    // from different inputs: EnactPolicy from bureaucratic
    // compliance (0.62), AdjustBudget(military) from military
    // loyalty (0.84). At the seeded values both clear the 0.3
    // threshold so status is Accepted in both cases.
    GameState state;
    state.countries.push_back(seeded_country());
    state.player_country = CountryId{0};

    const auto enact  = oe::evaluate(state, enact_raise_taxes());
    const auto adjust = oe::evaluate(state, adjust_military_budget());
    REQUIRE(enact.ok());
    REQUIRE(adjust.ok());

    CHECK(enact.value().status  == oe::ExecutionStatus::Accepted);
    CHECK(adjust.value().status == oe::ExecutionStatus::Accepted);

    // Inputs are kind-independent.
    CHECK(enact.value().inputs.bureaucratic_compliance ==
          doctest::Approx(adjust.value().inputs.bureaucratic_compliance));
    CHECK(enact.value().inputs.military_loyalty ==
          doctest::Approx(adjust.value().inputs.military_loyalty));
    CHECK(enact.value().inputs.intelligence_capability ==
          doctest::Approx(adjust.value().inputs.intelligence_capability));
    CHECK(enact.value().inputs.media_control ==
          doctest::Approx(adjust.value().inputs.media_control));

    // Resistance differs: EnactPolicy reads bureaucratic
    // compliance; AdjustBudget(military) reads military loyalty.
    CHECK(enact.value().resistance  == doctest::Approx(1.0 - 0.62));
    CHECK(adjust.value().resistance == doctest::Approx(1.0 - 0.84));
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

TEST_CASE("default OrderExecutionOutcome has the M2.18 baseline") {
    oe::OrderExecutionOutcome o;
    CHECK(o.status == oe::ExecutionStatus::Accepted);
    CHECK(o.resistance == doctest::Approx(0.0));
    CHECK(o.inputs.bureaucratic_compliance  == doctest::Approx(0.5));
    CHECK(o.inputs.military_loyalty         == doctest::Approx(0.5));
    CHECK(o.inputs.intelligence_capability  == doctest::Approx(0.5));
    CHECK(o.inputs.media_control            == doctest::Approx(0.5));
}

// ---------------------------------------------------------------------
// M2.18 - EnactPolicy gate
// ---------------------------------------------------------------------

namespace {

GameState state_with_compliance(double compliance) {
    GameState s;
    CountryState c = seeded_country();
    c.government_authority.bureaucratic_compliance = compliance;
    s.countries.push_back(c);
    s.player_country = CountryId{0};
    return s;
}

}  // namespace

TEST_CASE("evaluate EnactPolicy: compliance at threshold (0.3) accepts") {
    auto s = state_with_compliance(0.3);
    const auto r = oe::evaluate(s, enact_raise_taxes());
    REQUIRE(r.ok());
    CHECK(r.value().status == oe::ExecutionStatus::Accepted);
    CHECK(r.value().resistance == doctest::Approx(0.7));
}

TEST_CASE("evaluate EnactPolicy: compliance just below threshold (0.299) rejects") {
    auto s = state_with_compliance(0.299);
    const auto r = oe::evaluate(s, enact_raise_taxes());
    REQUIRE(r.ok());
    CHECK(r.value().status == oe::ExecutionStatus::Rejected);
    CHECK(r.value().resistance == doctest::Approx(1.0 - 0.299));
}

TEST_CASE("evaluate EnactPolicy: resistance is 1.0 - bureaucratic_compliance") {
    // Spot-check the formula across the [0, 1] range.
    for (const double c : {0.0, 0.1, 0.5, 0.75, 1.0}) {
        auto s = state_with_compliance(c);
        const auto r = oe::evaluate(s, enact_raise_taxes());
        REQUIRE(r.ok());
        CHECK(r.value().resistance == doctest::Approx(1.0 - c));
    }
}

TEST_CASE("evaluate EnactPolicy: default 0.5 compliance still accepts") {
    // Regression: canonical scenarios load with default 0.5; the
    // M2.18 gate must not silently start rejecting them.
    auto s = state_with_compliance(0.5);
    const auto r = oe::evaluate(s, enact_raise_taxes());
    REQUIRE(r.ok());
    CHECK(r.value().status == oe::ExecutionStatus::Accepted);
}

TEST_CASE("evaluate AdjustBudget(military): low bureaucratic_compliance is irrelevant if military_loyalty is high") {
    // state_with_compliance() only lowers bureaucratic_compliance;
    // military_loyalty stays at the seeded 0.84. M2.19 routes the
    // "military" category through military_loyalty, so the
    // command should still Accept and resistance reflects military
    // loyalty rather than the (low) bureaucratic compliance.
    auto s = state_with_compliance(0.01);
    const auto r = oe::evaluate(s, adjust_military_budget());
    REQUIRE(r.ok());
    CHECK(r.value().status == oe::ExecutionStatus::Accepted);
    CHECK(r.value().resistance == doctest::Approx(1.0 - 0.84));
}

TEST_CASE("evaluate EnactPolicy: rejected path is non-mutating") {
    auto before = state_with_compliance(0.1);
    auto after  = before;
    const auto r = oe::evaluate(after, enact_raise_taxes());
    REQUIRE(r.ok());
    CHECK(r.value().status == oe::ExecutionStatus::Rejected);
    // The country and root-level state stay byte-identical.
    CHECK(after.countries[0].government_authority.bureaucratic_compliance ==
          before.countries[0].government_authority.bureaucratic_compliance);
    CHECK(after.logs.size()             == before.logs.size());
    CHECK(after.applied_commands.size() == before.applied_commands.size());
}

// ---------------------------------------------------------------------
// M2.19 - AdjustBudget category-aware gate
// ---------------------------------------------------------------------

namespace {

GameState state_with_authority(double bureaucratic, double military) {
    GameState s;
    CountryState c = seeded_country();
    c.government_authority.bureaucratic_compliance = bureaucratic;
    c.government_authority.military_loyalty        = military;
    s.countries.push_back(c);
    s.player_country = CountryId{0};
    return s;
}

PlayerCommand adjust_with_category(const std::string& category) {
    PlayerCommand cmd;
    cmd.kind            = PlayerCommandKind::AdjustBudget;
    cmd.budget_category = category;
    cmd.budget_delta    = 0.02;
    return cmd;
}

}  // namespace

TEST_CASE("evaluate AdjustBudget(military): loyalty at threshold (0.3) accepts") {
    auto s = state_with_authority(/*bureau=*/0.5, /*military=*/0.3);
    const auto r = oe::evaluate(s, adjust_with_category("military"));
    REQUIRE(r.ok());
    CHECK(r.value().status == oe::ExecutionStatus::Accepted);
    CHECK(r.value().resistance == doctest::Approx(0.7));
}

TEST_CASE("evaluate AdjustBudget(military): loyalty just below threshold (0.299) rejects") {
    auto s = state_with_authority(/*bureau=*/0.5, /*military=*/0.299);
    const auto r = oe::evaluate(s, adjust_with_category("military"));
    REQUIRE(r.ok());
    CHECK(r.value().status == oe::ExecutionStatus::Rejected);
    CHECK(r.value().resistance == doctest::Approx(1.0 - 0.299));
}

TEST_CASE("evaluate AdjustBudget(military): ignores high bureaucratic_compliance when military_loyalty is low") {
    // Even with perfect bureaucratic compliance, military category
    // depends solely on military_loyalty.
    auto s = state_with_authority(/*bureau=*/1.0, /*military=*/0.1);
    const auto r = oe::evaluate(s, adjust_with_category("military"));
    REQUIRE(r.ok());
    CHECK(r.value().status == oe::ExecutionStatus::Rejected);
    CHECK(r.value().resistance == doctest::Approx(1.0 - 0.1));
}

TEST_CASE("evaluate AdjustBudget(non-military): uses bureaucratic_compliance regardless of military_loyalty") {
    // High military_loyalty must not rescue a non-military
    // category whose bureaucratic compliance is below threshold.
    auto s = state_with_authority(/*bureau=*/0.2, /*military=*/0.9);
    for (const char* category : {"administration", "education", "welfare",
                                  "intelligence", "infrastructure", "industry"}) {
        const auto r = oe::evaluate(s, adjust_with_category(category));
        REQUIRE(r.ok());
        CHECK(r.value().status == oe::ExecutionStatus::Rejected);
        CHECK(r.value().resistance == doctest::Approx(1.0 - 0.2));
    }
}

TEST_CASE("evaluate AdjustBudget(non-military): high bureaucratic_compliance accepts even with low military_loyalty") {
    auto s = state_with_authority(/*bureau=*/0.9, /*military=*/0.01);
    const auto r = oe::evaluate(s, adjust_with_category("welfare"));
    REQUIRE(r.ok());
    CHECK(r.value().status == oe::ExecutionStatus::Accepted);
    CHECK(r.value().resistance == doctest::Approx(1.0 - 0.9));
}

TEST_CASE("evaluate AdjustBudget: default 0.5 authority across the board still accepts") {
    // Canonical scenario / fixture defaults: every authority
    // field 0.5. Both the "military" and a non-military category
    // must Accept so existing apply_pending tests continue to
    // succeed.
    GameState s;
    s.countries.push_back(CountryState{});  // every authority field 0.5 by default.
    s.player_country = CountryId{0};
    const auto m = oe::evaluate(s, adjust_with_category("military"));
    const auto a = oe::evaluate(s, adjust_with_category("administration"));
    REQUIRE(m.ok());
    REQUIRE(a.ok());
    CHECK(m.value().status == oe::ExecutionStatus::Accepted);
    CHECK(a.value().status == oe::ExecutionStatus::Accepted);
}

TEST_CASE("evaluate AdjustBudget: rejected path is non-mutating") {
    auto before = state_with_authority(/*bureau=*/0.1, /*military=*/0.1);
    auto after  = before;
    const auto r = oe::evaluate(after, adjust_with_category("military"));
    REQUIRE(r.ok());
    CHECK(r.value().status == oe::ExecutionStatus::Rejected);
    CHECK(after.countries[0].government_authority.military_loyalty ==
          before.countries[0].government_authority.military_loyalty);
    CHECK(after.logs.size()             == before.logs.size());
    CHECK(after.applied_commands.size() == before.applied_commands.size());
}
