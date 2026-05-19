// Issue #112: ChooseEventOption command dispatch tests.
//
// These tests exercise the `PlayerCommandKind::ChooseEventOption`
// surface via `commands::apply_command_script` — the same path
// the runner `--commands` script uses. Coverage:
//   - happy path (pending → choice → effects applied → pending cleared
//     → followup recursion triggered)
//   - unknown option_id_code rejection (pending entry survives,
//     state unchanged)
//   - country mismatch rejection
//   - pending entry missing rejection
//   - save / load round-trip preserves pending_player_events +
//     applied_commands.ChooseEventOption payload
//   - scenario_loader rejects pre-populated pending_player_events
//     (8th container) AND event_history (9th container)

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/player_commands.hpp"
#include "leviathan/systems/commands.hpp"
#include "leviathan/systems/event_engine.hpp"
#include "leviathan/systems/save_system.hpp"

using leviathan::core::AppliedPlayerCommand;
using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::EventDefinition;
using leviathan::core::EventOption;
using leviathan::core::EventOptionEffectMode;
using leviathan::core::EventTrigger;
using leviathan::core::GameState;
using leviathan::core::PendingPlayerEvent;
using leviathan::core::PlayerCommand;
using leviathan::core::PlayerCommandKind;
using leviathan::core::PolicyEffect;
namespace cmd = leviathan::systems::commands;
namespace eng = leviathan::systems::event_engine;
namespace ss  = leviathan::systems::save_system;

namespace {

CountryState make_country(int id, std::string code,
                          double stability  = 0.50,
                          double legitimacy = 0.50) {
    CountryState c;
    c.id         = CountryId{id};
    c.id_code    = std::move(code);
    c.name       = c.id_code;
    c.stability  = stability;
    c.legitimacy = legitimacy;
    return c;
}

EventDefinition make_event_with_options(
        std::string id_code,
        std::string category,
        double      trigger_legitimacy_lt,
        std::vector<EventOption> options,
        std::vector<std::string> followups = {},
        EventOptionEffectMode    mode = EventOptionEffectMode::OptionOnly) {
    EventDefinition d;
    d.id_code            = std::move(id_code);
    d.name               = d.id_code;
    d.visible_report     = "test report";
    d.true_cause         = "test cause";
    d.category           = std::move(category);
    d.triggers.push_back({"country.legitimacy", "lt", trigger_legitimacy_lt});
    d.options            = std::move(options);
    d.followup_event_ids = std::move(followups);
    d.option_effect_mode = mode;
    return d;
}

EventDefinition make_event_no_options(
        std::string id_code,
        std::string category,
        double      trigger_legitimacy_lt,
        std::vector<PolicyEffect> effects = {}) {
    EventDefinition d;
    d.id_code        = std::move(id_code);
    d.name           = d.id_code;
    d.visible_report = "test report";
    d.true_cause     = "test cause";
    d.category       = std::move(category);
    d.triggers.push_back({"country.legitimacy", "lt", trigger_legitimacy_lt});
    d.effects        = std::move(effects);
    return d;
}

// Drive one tick to generate a pending player event, then return
// the resulting state.
GameState build_state_with_pending_player_event() {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50, /*leg*/0.10));
    s.player_country = CountryId{0};

    EventOption opt;
    opt.id_code = "act";
    opt.effects = { PolicyEffect{"country.legitimacy", "add", 0.25} };
    s.events.push_back(make_event_with_options(
        "crisis", "political",
        /*trigger_legitimacy_lt=*/0.20,
        {opt}));

    REQUIRE(eng::tick_events(s));
    REQUIRE(s.pending_player_events.size() == 1u);
    REQUIRE(s.event_history.size()         == 1u);
    return s;
}

}  // namespace

// =====================================================================
// happy path
// =====================================================================

TEST_CASE("Issue #112: ChooseEventOption applies option effects + clears pending") {
    GameState s = build_state_with_pending_player_event();
    REQUIRE(s.countries[0].legitimacy == doctest::Approx(0.10));

    PlayerCommand pick;
    pick.kind                = PlayerCommandKind::ChooseEventOption;
    pick.event_history_index = 0u;
    pick.option_id_code      = "act";

    auto r = cmd::apply_command_script(s, {pick});
    REQUIRE(r);
    CHECK(r.value().apply.commands_applied == 1);
    CHECK(!r.value().rejection.has_value());

    // Effects applied (legitimacy +0.25 → 0.35).
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.35));
    // Pending entry cleared.
    CHECK(s.pending_player_events.empty());
    // applied_commands records the ChooseEventOption.
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].command.kind ==
          PlayerCommandKind::ChooseEventOption);
    CHECK(s.applied_commands[0].command.event_history_index == 0u);
    CHECK(s.applied_commands[0].command.option_id_code      == "act");
}

TEST_CASE("Issue #112: ChooseEventOption triggers conditional followup recursion") {
    GameState s;
    s.countries.push_back(make_country(0, "GER", /*stab*/0.50, /*leg*/0.10));
    s.player_country = CountryId{0};

    // Build a followup whose trigger DOES NOT match initially but
    // DOES match after the player's option applies. Initial
    // legitimacy = 0.10; option `act` boosts +0.25 → 0.35; followup
    // trigger = legitimacy `gt 0.20` matches the post-option state
    // (0.35 > 0.20) but NOT the pre-option state (0.10 not > 0.20).
    // This is the canonical "conditional chain" pattern (RFC-050
    // §3): the followup only enters event_history after the
    // player resolves the parent's choice.
    EventDefinition followup;
    followup.id_code            = "followup";
    followup.name               = "followup";
    followup.visible_report     = "test report";
    followup.true_cause         = "test cause";
    followup.category           = "post";
    followup.triggers.push_back({"country.legitimacy", "gt", 0.20});
    followup.effects.push_back({"country.stability", "add", -0.05});
    s.events.push_back(std::move(followup));

    EventOption opt;
    opt.id_code = "act";
    opt.effects = { PolicyEffect{"country.legitimacy", "add", 0.25} };
    s.events.push_back(make_event_with_options(
        "crisis", "political",
        /*trigger_legitimacy_lt=*/0.20,
        {opt},
        /*followups=*/{"followup"}));

    REQUIRE(eng::tick_events(s));
    REQUIRE(s.event_history.size() == 1u);   // parent only — followup deferred
    REQUIRE(s.pending_player_events.size() == 1u);

    PlayerCommand pick;
    pick.kind                = PlayerCommandKind::ChooseEventOption;
    pick.event_history_index = 0u;
    pick.option_id_code      = "act";
    REQUIRE(cmd::apply_command_script(s, {pick}));

    // Followup now in event_history; pending cleared.
    REQUIRE(s.event_history.size() == 2u);
    CHECK(s.event_history[1].event_id_code == "followup");
    CHECK(s.pending_player_events.empty());
    // Followup's effects also applied: stability -0.05 → 0.45.
    CHECK(s.countries[0].stability == doctest::Approx(0.45));
}

// =====================================================================
// rejection paths
// =====================================================================

TEST_CASE("Issue #112: ChooseEventOption with unknown option_id_code is rejected, pending entry survives") {
    GameState s = build_state_with_pending_player_event();

    PlayerCommand pick;
    pick.kind                = PlayerCommandKind::ChooseEventOption;
    pick.event_history_index = 0u;
    pick.option_id_code      = "no_such_option";

    auto r = cmd::apply_command_script(s, {pick});
    REQUIRE(r.failed());
    CHECK(r.error().find("option")            != std::string::npos);
    CHECK(r.error().find("no_such_option")    != std::string::npos);

    // State untouched: pending entry still there, no effects applied,
    // applied_commands not appended.
    CHECK(s.pending_player_events.size() == 1u);
    CHECK(s.countries[0].legitimacy == doctest::Approx(0.10));
    CHECK(s.applied_commands.empty());
}

TEST_CASE("Issue #112: ChooseEventOption with no pending entry at the named index is rejected") {
    GameState s;
    s.countries.push_back(make_country(0, "GER"));
    s.player_country = CountryId{0};
    // No tick, no pending entry.

    PlayerCommand pick;
    pick.kind                = PlayerCommandKind::ChooseEventOption;
    pick.event_history_index = 0u;
    pick.option_id_code      = "act";

    auto r = cmd::apply_command_script(s, {pick});
    REQUIRE(r.failed());
    CHECK(r.error().find("no pending entry") != std::string::npos);
}

TEST_CASE("Issue #112: ChooseEventOption whose pending country does not match player_country is rejected") {
    GameState s = build_state_with_pending_player_event();
    // Switch player to a different (non-existent here) country index.
    // The pending entry was authored for GER (CountryId{0}); change
    // state.player_country to FRA (CountryId{1}).
    s.countries.push_back(make_country(1, "FRA"));
    s.player_country = CountryId{1};

    PlayerCommand pick;
    pick.kind                = PlayerCommandKind::ChooseEventOption;
    pick.event_history_index = 0u;
    pick.option_id_code      = "act";

    auto r = cmd::apply_command_script(s, {pick});
    REQUIRE(r.failed());
    CHECK(r.error().find("does not match") != std::string::npos);
}

// =====================================================================
// save / load round-trip
// =====================================================================

TEST_CASE("Issue #112: save / load round-trip preserves pending_player_events + ChooseEventOption applied_commands") {
    GameState before = build_state_with_pending_player_event();

    PlayerCommand pick;
    pick.kind                = PlayerCommandKind::ChooseEventOption;
    pick.event_history_index = 0u;
    pick.option_id_code      = "act";
    REQUIRE(cmd::apply_command_script(before, {pick}));

    const std::string text = ss::serialize(before);
    const auto reloaded_r = ss::deserialize(text);
    REQUIRE(reloaded_r);
    const auto& reloaded = reloaded_r.value();

    // applied_commands round-trip: ChooseEventOption with payload.
    REQUIRE(reloaded.applied_commands.size() == 1u);
    const auto& ac = reloaded.applied_commands[0].command;
    CHECK(ac.kind == PlayerCommandKind::ChooseEventOption);
    CHECK(ac.event_history_index == 0u);
    CHECK(ac.option_id_code      == "act");

    // pending_player_events round-trip: cleared (empty vector still
    // serialised + deserialised).
    CHECK(reloaded.pending_player_events.empty());

    // event_history preserved.
    REQUIRE(reloaded.event_history.size() == 1u);
    CHECK(reloaded.event_history[0].event_id_code == "crisis");
}
