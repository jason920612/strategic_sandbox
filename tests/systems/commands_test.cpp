#include <doctest/doctest.h>

#include <limits>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/player_commands.hpp"
#include "leviathan/systems/commands.hpp"
#include "leviathan/systems/runner.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::PlayerCommand;
using leviathan::core::PlayerCommandKind;
using leviathan::core::PolicyData;
using leviathan::core::PolicyEffect;
using leviathan::core::PolicyId;
namespace cmd = leviathan::systems::commands;

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------

namespace {

CountryState germany_baseline() {
    CountryState g;
    g.id           = CountryId{0};
    g.id_code      = "GER";
    g.name         = "Germany";
    g.gdp                       = 100.0;
    g.tax_revenue               = 18.5;
    g.budget_balance            = -3.2;
    g.legal_tax_burden          = 0.20;
    g.fiscal_capacity           = 0.50;
    g.administrative_efficiency = 0.55;
    g.central_control           = 0.60;
    g.corruption                = 0.25;
    g.stability                 = 0.55;
    g.legitimacy                = 0.55;
    g.military_power            = 0.50;
    g.threat_perception         = 0.30;
    g.budget.administration     = 0.25;
    g.budget.military           = 0.35;
    g.budget.education          = 0.10;
    g.budget.welfare            = 0.10;
    g.budget.intelligence       = 0.05;
    g.budget.infrastructure     = 0.10;
    g.budget.industry           = 0.05;
    return g;
}

PolicyData raise_taxes_policy() {
    PolicyData p;
    p.id            = PolicyId{0};
    p.id_code       = "raise_taxes";
    p.name          = "Raise Taxes";
    p.category      = "tax";
    p.duration_days = 60;
    p.admin_cost    = 0.12;
    p.effects = {
        {"country.legal_tax_burden", "add", 0.05},
    };
    return p;
}

PolicyData increase_education_policy() {
    PolicyData p;
    p.id            = PolicyId{1};
    p.id_code       = "increase_education";
    p.name          = "Increase Education";
    p.category      = "budget";
    p.duration_days = 90;
    p.admin_cost    = 0.10;
    p.effects = {
        {"country.budget.education", "add", 0.05},
    };
    return p;
}

PolicyData bad_target_policy() {
    PolicyData p;
    p.id            = PolicyId{2};
    p.id_code       = "bad_policy";
    p.name          = "Bad Policy";
    p.category      = "test";
    p.duration_days = 30;
    p.admin_cost    = 0.10;
    p.effects = {
        {"country.does_not_exist", "add", 0.05},
    };
    return p;
}

GameState ger_state_with_player_selected() {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(germany_baseline());
    s.policies.push_back(raise_taxes_policy());
    s.policies.push_back(increase_education_policy());
    s.player_country = CountryId{0};
    return s;
}

}  // namespace

// =====================================================================
// Happy paths
// =====================================================================

TEST_CASE("apply_pending: empty queue is a no-op success") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.ok());
    CHECK(r.value().commands_applied == 0);
    CHECK(q.pending.empty());
}

TEST_CASE("apply_pending: single EnactPolicy drains the queue and applies the policy") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.ok());
    CHECK(r.value().commands_applied == 1);
    CHECK(q.pending.empty());

    // policy::apply_policy_effects added 0.05 to legal_tax_burden.
    CHECK(s.countries[0].legal_tax_burden == doctest::Approx(0.25));
}

TEST_CASE("apply_pending: successful enact chains into active_policies (M1.15)") {
    // M2.3 reuses policy::apply_policy_effects, which is the same
    // function the scenario loader uses for day-0 enactment. The
    // M1.15 active_policies side effect therefore fires here too.
    GameState s = ger_state_with_player_selected();
    REQUIRE(s.countries[0].active_policies.empty());

    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    REQUIRE(cmd::apply_pending(s, q).ok());
    REQUIRE(s.countries[0].active_policies.size() == 1);
    CHECK(s.countries[0].active_policies[0].policy_id_code == "raise_taxes");
    // 1930-01-01 + 60 days = 1930-03-02 (no leap year involved).
    CHECK(s.countries[0].active_policies[0].expires_on ==
          GameDate(1930, 3, 2));
}

TEST_CASE("apply_pending: multiple commands apply in insertion order") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "increase_education"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.ok());
    CHECK(r.value().commands_applied == 2);
    CHECK(q.pending.empty());

    // Both effects landed.
    CHECK(s.countries[0].legal_tax_burden    == doctest::Approx(0.25));
    CHECK(s.countries[0].budget.education    == doctest::Approx(0.15));
    REQUIRE(s.countries[0].active_policies.size() == 2);
    CHECK(s.countries[0].active_policies[0].policy_id_code == "raise_taxes");
    CHECK(s.countries[0].active_policies[1].policy_id_code == "increase_education");
}

// =====================================================================
// Precondition: player_country
// =====================================================================

TEST_CASE("apply_pending: no player_country selected -> rejected, queue untouched") {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(germany_baseline());
    s.policies.push_back(raise_taxes_policy());
    // player_country intentionally left at the default invalid().

    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
    CHECK(q.pending.size() == 1);            // queue untouched
    CHECK(s.countries[0].legal_tax_burden == doctest::Approx(0.20));
    CHECK(s.countries[0].active_policies.empty());
}

TEST_CASE("apply_pending: player_country out of range -> rejected") {
    GameState s = ger_state_with_player_selected();
    s.player_country = CountryId{5};   // valid()==true but past size

    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
    CHECK(q.pending.size() == 1);
}

// =====================================================================
// Mid-list failure (non-atomic across commands)
// =====================================================================

TEST_CASE("apply_pending: unknown policy id_code stops at the failed command") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "nope_not_a_real_policy"});
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "increase_education"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("nope_not_a_real_policy") != std::string::npos);
    CHECK(r.error().find("unknown policy id_code") != std::string::npos);

    // First command popped + applied; the failing one stays at the
    // head; the trailing valid one stays after it.
    REQUIRE(q.pending.size() == 2u);
    CHECK(q.pending[0].policy_id_code == "nope_not_a_real_policy");
    CHECK(q.pending[1].policy_id_code == "increase_education");

    // raise_taxes was applied; increase_education was NOT.
    CHECK(s.countries[0].legal_tax_burden == doctest::Approx(0.25));
    CHECK(s.countries[0].budget.education == doctest::Approx(0.10));
}

TEST_CASE("apply_pending: policy with bad effect target stops with M1.5 error") {
    GameState s = ger_state_with_player_selected();
    s.policies.push_back(bad_target_policy());

    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "bad_policy"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("bad_policy") != std::string::npos);
    CHECK(r.error().find("does_not_exist") != std::string::npos);

    // raise_taxes applied; bad_policy left at head for inspection.
    REQUIRE(q.pending.size() == 1u);
    CHECK(q.pending[0].policy_id_code == "bad_policy");
    CHECK(s.countries[0].legal_tax_burden == doctest::Approx(0.25));
}

// =====================================================================
// M2.4: command log appended on success
// =====================================================================

TEST_CASE("M2.4 apply_pending: successful enact appends one log entry") {
    GameState s = ger_state_with_player_selected();
    REQUIRE(s.applied_commands.empty());
    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    REQUIRE(cmd::apply_pending(s, q).ok());
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].applied_on == GameDate(1930, 1, 1));
    CHECK(s.applied_commands[0].command.kind ==
          PlayerCommandKind::EnactPolicy);
    CHECK(s.applied_commands[0].command.policy_id_code == "raise_taxes");
}

TEST_CASE("M2.4 apply_pending: multiple successes append in insertion order") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "increase_education"});

    REQUIRE(cmd::apply_pending(s, q).ok());
    REQUIRE(s.applied_commands.size() == 2u);
    CHECK(s.applied_commands[0].command.policy_id_code == "raise_taxes");
    CHECK(s.applied_commands[1].command.policy_id_code == "increase_education");
}

TEST_CASE("M2.4 apply_pending: failed command does NOT append a log entry") {
    // M2.3 atomicity is per-command; M2.4 inherits that for the log:
    // the successful command logs once, the failing command does not
    // log at all (even though it stays at the head of the queue for
    // retry).
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "nope_not_a_policy"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    REQUIRE(s.applied_commands.size() == 1u);  // only raise_taxes logged
    CHECK(s.applied_commands[0].command.policy_id_code == "raise_taxes");
    REQUIRE(q.pending.size() == 1u);
    CHECK(q.pending[0].policy_id_code == "nope_not_a_policy");
}

TEST_CASE("M2.4 apply_pending: applied_on captures current_date at apply time") {
    // The log records the date the command was actually applied, not
    // the date it was submitted. Bump current_date between submits to
    // pin the rule.
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    REQUIRE(cmd::apply_pending(s, q).ok());
    CHECK(s.applied_commands.back().applied_on == GameDate(1930, 1, 1));

    // Move the simulation date forward and apply another command.
    s.current_date = GameDate(1930, 6, 15);
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "increase_education"});
    REQUIRE(cmd::apply_pending(s, q).ok());
    REQUIRE(s.applied_commands.size() == 2u);
    CHECK(s.applied_commands[1].applied_on == GameDate(1930, 6, 15));
}

TEST_CASE("M2.4 apply_pending: precondition failure leaves applied_commands untouched") {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(germany_baseline());
    s.policies.push_back(raise_taxes_policy());
    // player_country deliberately left at invalid()

    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    REQUIRE(cmd::apply_pending(s, q).failed());
    CHECK(s.applied_commands.empty());
    CHECK(q.pending.size() == 1u);
}

// =====================================================================
// M2.5: AdjustBudget command kind
// =====================================================================

namespace {

PlayerCommand make_adjust_budget(const std::string& category, double delta) {
    PlayerCommand c;
    c.kind            = PlayerCommandKind::AdjustBudget;
    c.budget_category = category;
    c.budget_delta    = delta;
    return c;
}

}  // namespace

TEST_CASE("M2.5 apply_pending: AdjustBudget military += delta mutates the budget") {
    GameState s = ger_state_with_player_selected();
    REQUIRE(s.countries[0].budget.military == doctest::Approx(0.35));
    cmd::CommandQueue q;
    q.pending.push_back(make_adjust_budget("military", 0.05));

    REQUIRE(cmd::apply_pending(s, q).ok());
    CHECK(s.countries[0].budget.military == doctest::Approx(0.40));
    CHECK(q.pending.empty());
}

TEST_CASE("M2.5 apply_pending: AdjustBudget negative delta shrinks the budget") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    q.pending.push_back(make_adjust_budget("education", -0.05));

    REQUIRE(cmd::apply_pending(s, q).ok());
    // education started at 0.10; - 0.05 -> 0.05
    CHECK(s.countries[0].budget.education == doctest::Approx(0.05));
}

TEST_CASE("M2.5 apply_pending: AdjustBudget clamps above 1.0") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    // military starts at 0.35; +1.0 overshoots, clamps to 1.0.
    q.pending.push_back(make_adjust_budget("military", 1.0));

    REQUIRE(cmd::apply_pending(s, q).ok());
    CHECK(s.countries[0].budget.military == doctest::Approx(1.0));
}

TEST_CASE("M2.5 apply_pending: AdjustBudget clamps below 0.0") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    // welfare starts at 0.10; -1.0 undershoots, clamps to 0.0.
    q.pending.push_back(make_adjust_budget("welfare", -1.0));

    REQUIRE(cmd::apply_pending(s, q).ok());
    CHECK(s.countries[0].budget.welfare == doctest::Approx(0.0));
}

TEST_CASE("M2.5 apply_pending: unknown budget_category is rejected with no mutation") {
    GameState s = ger_state_with_player_selected();
    const auto before_military = s.countries[0].budget.military;
    cmd::CommandQueue q;
    q.pending.push_back(make_adjust_budget("research", 0.05));

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("AdjustBudget") != std::string::npos);
    CHECK(r.error().find("research")     != std::string::npos);
    CHECK(s.countries[0].budget.military == doctest::Approx(before_military));
    REQUIRE(q.pending.size() == 1u);
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.5 apply_pending: non-finite budget_delta is rejected") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    q.pending.push_back(make_adjust_budget(
        "military", std::numeric_limits<double>::quiet_NaN()));

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("AdjustBudget")   != std::string::npos);
    CHECK(r.error().find("not finite")     != std::string::npos);
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.5 apply_pending: AdjustBudget appends correct log entry") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    q.pending.push_back(make_adjust_budget("infrastructure", 0.03));

    REQUIRE(cmd::apply_pending(s, q).ok());
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].applied_on            == GameDate(1930, 1, 1));
    CHECK(s.applied_commands[0].command.kind          ==
          PlayerCommandKind::AdjustBudget);
    CHECK(s.applied_commands[0].command.budget_category == "infrastructure");
    CHECK(s.applied_commands[0].command.budget_delta    == doctest::Approx(0.03));
}

TEST_CASE("M2.5 apply_pending: mixed-kind queue applies both in insertion order") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});
    q.pending.push_back(make_adjust_budget("welfare", 0.02));

    REQUIRE(cmd::apply_pending(s, q).ok());
    CHECK(q.pending.empty());
    CHECK(s.countries[0].legal_tax_burden == doctest::Approx(0.25));
    CHECK(s.countries[0].budget.welfare   == doctest::Approx(0.12));
    REQUIRE(s.applied_commands.size() == 2u);
    CHECK(s.applied_commands[0].command.kind ==
          PlayerCommandKind::EnactPolicy);
    CHECK(s.applied_commands[1].command.kind ==
          PlayerCommandKind::AdjustBudget);
}

// =====================================================================
// M2.6: replay applied-command log
// =====================================================================

TEST_CASE("M2.6 replay: empty log is a no-op success") {
    GameState s = ger_state_with_player_selected();
    REQUIRE(s.applied_commands.empty());

    const std::vector<leviathan::core::AppliedPlayerCommand> log;
    const auto r = cmd::replay(s, log);
    REQUIRE(r.ok());
    CHECK(r.value().commands_replayed == 0);
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.6 replay: single EnactPolicy entry replays and re-logs") {
    GameState s = ger_state_with_player_selected();
    std::vector<leviathan::core::AppliedPlayerCommand> log;
    leviathan::core::AppliedPlayerCommand entry;
    entry.applied_on            = GameDate(1930, 2, 15);
    entry.command.kind          = PlayerCommandKind::EnactPolicy;
    entry.command.policy_id_code = "raise_taxes";
    log.push_back(entry);

    REQUIRE(cmd::replay(s, log).ok());
    CHECK(s.countries[0].legal_tax_burden == doctest::Approx(0.25));
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].applied_on            == GameDate(1930, 2, 15));
    CHECK(s.applied_commands[0].command.kind          ==
          PlayerCommandKind::EnactPolicy);
    CHECK(s.applied_commands[0].command.policy_id_code == "raise_taxes");
}

TEST_CASE("M2.6 replay: single AdjustBudget entry replays and re-logs") {
    GameState s = ger_state_with_player_selected();
    std::vector<leviathan::core::AppliedPlayerCommand> log;
    leviathan::core::AppliedPlayerCommand entry;
    entry.applied_on              = GameDate(1930, 3, 1);
    entry.command.kind            = PlayerCommandKind::AdjustBudget;
    entry.command.budget_category = "military";
    entry.command.budget_delta    = 0.05;
    log.push_back(entry);

    REQUIRE(cmd::replay(s, log).ok());
    CHECK(s.countries[0].budget.military == doctest::Approx(0.40));
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].applied_on              == GameDate(1930, 3, 1));
    CHECK(s.applied_commands[0].command.budget_category == "military");
    CHECK(s.applied_commands[0].command.budget_delta    == doctest::Approx(0.05));
}

TEST_CASE("M2.6 replay: mixed-kind log replays in insertion order") {
    GameState s = ger_state_with_player_selected();
    std::vector<leviathan::core::AppliedPlayerCommand> log;
    leviathan::core::AppliedPlayerCommand e1;
    e1.applied_on            = GameDate(1930, 1, 10);
    e1.command.kind          = PlayerCommandKind::EnactPolicy;
    e1.command.policy_id_code = "raise_taxes";
    log.push_back(e1);
    leviathan::core::AppliedPlayerCommand e2;
    e2.applied_on              = GameDate(1930, 2, 1);
    e2.command.kind            = PlayerCommandKind::AdjustBudget;
    e2.command.budget_category = "welfare";
    e2.command.budget_delta    = 0.02;
    log.push_back(e2);

    REQUIRE(cmd::replay(s, log).ok());
    CHECK(s.countries[0].legal_tax_burden == doctest::Approx(0.25));
    CHECK(s.countries[0].budget.welfare   == doctest::Approx(0.12));
    REQUIRE(s.applied_commands.size() == 2u);
    CHECK(s.applied_commands[0].command.kind == PlayerCommandKind::EnactPolicy);
    CHECK(s.applied_commands[1].command.kind == PlayerCommandKind::AdjustBudget);
}

TEST_CASE("M2.6 replay: replayed log mirrors source log byte-equivalent") {
    // The core replay guarantee: same input log -> same output log.
    // Build a source log by submitting through apply_pending, then
    // replay it into a fresh state and compare entry-by-entry.
    GameState source = ger_state_with_player_selected();
    cmd::CommandQueue q;
    source.current_date = GameDate(1930, 1, 5);
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});
    REQUIRE(cmd::apply_pending(source, q).ok());
    source.current_date = GameDate(1930, 4, 12);
    q.pending.push_back(make_adjust_budget("infrastructure", 0.04));
    REQUIRE(cmd::apply_pending(source, q).ok());

    REQUIRE(source.applied_commands.size() == 2u);

    GameState target = ger_state_with_player_selected();   // fresh
    REQUIRE(cmd::replay(target, source.applied_commands).ok());
    REQUIRE(target.applied_commands.size() == source.applied_commands.size());
    for (std::size_t i = 0; i < source.applied_commands.size(); ++i) {
        const auto& src = source.applied_commands[i];
        const auto& tgt = target.applied_commands[i];
        CHECK(tgt.applied_on   == src.applied_on);
        CHECK(tgt.command.kind == src.command.kind);
        CHECK(tgt.command.policy_id_code  == src.command.policy_id_code);
        CHECK(tgt.command.budget_category == src.command.budget_category);
        CHECK(tgt.command.budget_delta    == doctest::Approx(src.command.budget_delta));
    }
    // State effects also match (the commands mutated the same fields).
    CHECK(target.countries[0].legal_tax_burden       ==
          doctest::Approx(source.countries[0].legal_tax_burden));
    CHECK(target.countries[0].budget.infrastructure  ==
          doctest::Approx(source.countries[0].budget.infrastructure));
}

TEST_CASE("M2.6 replay: target.current_date is forced to the last entry's applied_on") {
    // Prototype limit pinned: after replay, target.current_date is the
    // LAST entry's applied_on, not whatever the source state's final
    // current_date was. This is documented in the header.
    GameState s = ger_state_with_player_selected();
    s.current_date = GameDate(1930, 1, 1);
    std::vector<leviathan::core::AppliedPlayerCommand> log;
    leviathan::core::AppliedPlayerCommand e1;
    e1.applied_on = GameDate(1930, 6, 15);
    e1.command.kind          = PlayerCommandKind::EnactPolicy;
    e1.command.policy_id_code = "raise_taxes";
    log.push_back(e1);

    REQUIRE(cmd::replay(s, log).ok());
    CHECK(s.current_date == GameDate(1930, 6, 15));
}

TEST_CASE("M2.6 replay: unknown policy_id_code stops with the entry index in the error") {
    GameState s = ger_state_with_player_selected();
    std::vector<leviathan::core::AppliedPlayerCommand> log;
    leviathan::core::AppliedPlayerCommand e1;
    e1.applied_on = GameDate(1930, 2, 1);
    e1.command.kind          = PlayerCommandKind::EnactPolicy;
    e1.command.policy_id_code = "raise_taxes";
    log.push_back(e1);
    leviathan::core::AppliedPlayerCommand e2;
    e2.applied_on = GameDate(1930, 3, 1);
    e2.command.kind          = PlayerCommandKind::EnactPolicy;
    e2.command.policy_id_code = "not_a_real_policy";
    log.push_back(e2);

    const auto r = cmd::replay(s, log);
    REQUIRE(r.failed());
    CHECK(r.error().find("replay[1]")           != std::string::npos);
    CHECK(r.error().find("not_a_real_policy")   != std::string::npos);
    // First entry applied + logged; second left out.
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].command.policy_id_code == "raise_taxes");
}

TEST_CASE("M2.6 replay: no player_country selected -> rejected before any replay") {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(germany_baseline());
    s.policies.push_back(raise_taxes_policy());
    // player_country deliberately left at invalid().

    std::vector<leviathan::core::AppliedPlayerCommand> log;
    leviathan::core::AppliedPlayerCommand e;
    e.applied_on            = GameDate(1930, 1, 1);
    e.command.kind          = PlayerCommandKind::EnactPolicy;
    e.command.policy_id_code = "raise_taxes";
    log.push_back(e);

    const auto r = cmd::replay(s, log);
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.6 replay: non-empty applied_commands rejects the precondition") {
    GameState s = ger_state_with_player_selected();
    leviathan::core::AppliedPlayerCommand prior;
    prior.applied_on = GameDate(1930, 1, 1);
    prior.command.kind          = PlayerCommandKind::EnactPolicy;
    prior.command.policy_id_code = "raise_taxes";
    s.applied_commands.push_back(prior);   // simulate "already-replayed"

    std::vector<leviathan::core::AppliedPlayerCommand> log;
    log.push_back(prior);

    const auto r = cmd::replay(s, log);
    REQUIRE(r.failed());
    CHECK(r.error().find("applied_commands must be") != std::string::npos);
    // The pre-existing entry stays untouched.
    REQUIRE(s.applied_commands.size() == 1u);
}

// =====================================================================
// M2.7: replay_with_time
// =====================================================================

namespace {

namespace rn = leviathan::systems::runner;

// Build a tick-control bundle on top of `ger_state_with_player_selected()`
// so the M2.7 tests don't have to repeat the begin_tick boilerplate.
struct M27Bundle {
    GameState           state;
    rn::RunnerOptions   opts;
    rn::TickController  ctrl;
};

M27Bundle make_m27_bundle() {
    M27Bundle b;
    b.state = ger_state_with_player_selected();
    // RunnerOptions defaults are fine — no CSV output, no scenario path.
    REQUIRE(rn::begin_tick(b.state, b.opts, b.ctrl).ok());
    return b;
}

leviathan::core::AppliedPlayerCommand log_entry_enact(
        GameDate when, const std::string& policy_id_code) {
    leviathan::core::AppliedPlayerCommand e;
    e.applied_on            = when;
    e.command.kind          = PlayerCommandKind::EnactPolicy;
    e.command.policy_id_code = policy_id_code;
    return e;
}

leviathan::core::AppliedPlayerCommand log_entry_budget(
        GameDate when, const std::string& cat, double delta) {
    leviathan::core::AppliedPlayerCommand e;
    e.applied_on              = when;
    e.command.kind            = PlayerCommandKind::AdjustBudget;
    e.command.budget_category = cat;
    e.command.budget_delta    = delta;
    return e;
}

}  // namespace

TEST_CASE("M2.7 replay_with_time: empty log is a no-op success") {
    auto b = make_m27_bundle();
    const std::vector<leviathan::core::AppliedPlayerCommand> log;

    const auto r = cmd::replay_with_time(b.state, b.opts, b.ctrl, log);
    REQUIRE(r.ok());
    CHECK(r.value().commands_replayed == 0);
    CHECK(b.ctrl.days_stepped == 0);
    CHECK(b.state.current_date == GameDate(1930, 1, 1));
}

TEST_CASE("M2.7 replay_with_time: command at start_date applies with no advance") {
    auto b = make_m27_bundle();
    std::vector<leviathan::core::AppliedPlayerCommand> log;
    log.push_back(log_entry_enact(GameDate(1930, 1, 1), "raise_taxes"));

    REQUIRE(cmd::replay_with_time(b.state, b.opts, b.ctrl, log).ok());
    CHECK(b.ctrl.days_stepped == 0);
    CHECK(b.state.current_date == GameDate(1930, 1, 1));
    CHECK(b.state.countries[0].legal_tax_burden == doctest::Approx(0.25));
    REQUIRE(b.state.applied_commands.size() == 1u);
    CHECK(b.state.applied_commands[0].applied_on == GameDate(1930, 1, 1));
}

TEST_CASE("M2.7 replay_with_time: command 5 days later advances exactly 5 days") {
    auto b = make_m27_bundle();
    std::vector<leviathan::core::AppliedPlayerCommand> log;
    log.push_back(log_entry_enact(GameDate(1930, 1, 6), "raise_taxes"));

    REQUIRE(cmd::replay_with_time(b.state, b.opts, b.ctrl, log).ok());
    CHECK(b.ctrl.days_stepped == 5);
    CHECK(b.state.current_date == GameDate(1930, 1, 6));
    CHECK(b.state.applied_commands[0].applied_on == GameDate(1930, 1, 6));
}

TEST_CASE("M2.7 replay_with_time: command past month boundary fires monthly pipeline") {
    auto b = make_m27_bundle();
    std::vector<leviathan::core::AppliedPlayerCommand> log;
    // Jan 1 -> Feb 15 crosses one month boundary on Feb 1.
    log.push_back(log_entry_budget(GameDate(1930, 2, 15), "military", 0.05));

    REQUIRE(cmd::replay_with_time(b.state, b.opts, b.ctrl, log).ok());
    CHECK(b.ctrl.days_stepped   == 45);
    CHECK(b.ctrl.monthly_ticks  == 1);
    CHECK(b.state.current_date  == GameDate(1930, 2, 15));
    CHECK(b.state.countries[0].budget.military == doctest::Approx(0.40));
}

TEST_CASE("M2.7 replay_with_time: multiple commands at different dates each block advances") {
    auto b = make_m27_bundle();
    std::vector<leviathan::core::AppliedPlayerCommand> log;
    log.push_back(log_entry_enact(GameDate(1930, 1, 6), "raise_taxes"));
    log.push_back(log_entry_budget(GameDate(1930, 2, 15), "military", 0.05));

    REQUIRE(cmd::replay_with_time(b.state, b.opts, b.ctrl, log).ok());
    CHECK(b.ctrl.days_stepped  == 45);   // 5 + 40
    CHECK(b.ctrl.monthly_ticks == 1);
    CHECK(b.state.current_date == GameDate(1930, 2, 15));
    REQUIRE(b.state.applied_commands.size() == 2u);
    CHECK(b.state.applied_commands[0].applied_on == GameDate(1930, 1, 6));
    CHECK(b.state.applied_commands[1].applied_on == GameDate(1930, 2, 15));
}

TEST_CASE("M2.7 replay_with_time: out-of-order log rejected with index in error") {
    auto b = make_m27_bundle();
    std::vector<leviathan::core::AppliedPlayerCommand> log;
    log.push_back(log_entry_enact(GameDate(1930, 3, 1), "raise_taxes"));
    log.push_back(log_entry_enact(GameDate(1930, 2, 1), "increase_education"));

    const auto r = cmd::replay_with_time(b.state, b.opts, b.ctrl, log);
    REQUIRE(r.failed());
    CHECK(r.error().find("replay_with_time[1]") != std::string::npos);
    CHECK(r.error().find("out-of-order")        != std::string::npos);
    // First entry already applied + logged before the failure.
    REQUIRE(b.state.applied_commands.size() == 1u);
    CHECK(b.state.applied_commands[0].applied_on == GameDate(1930, 3, 1));
}

TEST_CASE("M2.7 replay_with_time: controller not started is rejected") {
    GameState s = ger_state_with_player_selected();
    rn::RunnerOptions opts;
    rn::TickController ctrl;   // never begin_tick'd

    std::vector<leviathan::core::AppliedPlayerCommand> log;
    log.push_back(log_entry_enact(GameDate(1930, 1, 1), "raise_taxes"));

    const auto r = cmd::replay_with_time(s, opts, ctrl, log);
    REQUIRE(r.failed());
    CHECK(r.error().find("not been started") != std::string::npos);
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.7 replay_with_time: no player_country is rejected before begin_tick check") {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(germany_baseline());
    s.policies.push_back(raise_taxes_policy());
    // player_country deliberately left at invalid()
    rn::RunnerOptions opts;
    rn::TickController ctrl;

    std::vector<leviathan::core::AppliedPlayerCommand> log;
    log.push_back(log_entry_enact(GameDate(1930, 1, 1), "raise_taxes"));

    const auto r = cmd::replay_with_time(s, opts, ctrl, log);
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.7 replay_with_time: non-empty applied_commands rejects the precondition") {
    GameState s = ger_state_with_player_selected();
    s.applied_commands.push_back(log_entry_enact(GameDate(1930, 1, 1), "raise_taxes"));
    rn::RunnerOptions opts;
    rn::TickController ctrl;

    std::vector<leviathan::core::AppliedPlayerCommand> log;
    log.push_back(log_entry_enact(GameDate(1930, 1, 1), "raise_taxes"));

    const auto r = cmd::replay_with_time(s, opts, ctrl, log);
    REQUIRE(r.failed());
    CHECK(r.error().find("applied_commands must be") != std::string::npos);
    REQUIRE(s.applied_commands.size() == 1u);  // unchanged
}

TEST_CASE("M2.7 replay_with_time: full equivalence with original simulation") {
    // The killer test: drive a source state with step_one_day +
    // apply_pending interleaved, capture its applied_commands log,
    // then replay that log onto a fresh target via replay_with_time
    // and check the resulting state matches.
    // 1930-01-01 -> step 5 -> apply EnactPolicy raise_taxes
    //            -> step 40 (crosses Feb 1 month boundary)
    //            -> apply AdjustBudget military +0.05
    rn::RunnerOptions opts;

    // ---- source ------------------------------------------------------------
    GameState source = ger_state_with_player_selected();
    rn::TickController source_ctrl;
    REQUIRE(rn::begin_tick(source, opts, source_ctrl).ok());
    for (int i = 0; i < 5; ++i) {
        REQUIRE(rn::step_one_day(source, opts, source_ctrl).ok());
    }
    {
        cmd::CommandQueue q;
        q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});
        REQUIRE(cmd::apply_pending(source, q).ok());
    }
    for (int i = 0; i < 40; ++i) {
        REQUIRE(rn::step_one_day(source, opts, source_ctrl).ok());
    }
    {
        cmd::CommandQueue q;
        q.pending.push_back(make_adjust_budget("military", 0.05));
        REQUIRE(cmd::apply_pending(source, q).ok());
    }
    REQUIRE(source.applied_commands.size() == 2u);
    REQUIRE(source.current_date == GameDate(1930, 2, 15));
    REQUIRE(source_ctrl.monthly_ticks == 1);

    // ---- target ------------------------------------------------------------
    GameState target = ger_state_with_player_selected();
    rn::TickController target_ctrl;
    REQUIRE(rn::begin_tick(target, opts, target_ctrl).ok());
    REQUIRE(cmd::replay_with_time(target, opts, target_ctrl,
                                   source.applied_commands).ok());

    // ---- compare -----------------------------------------------------------
    // The replayed state should mirror the source up to the last
    // command's date.
    CHECK(target.current_date        == source.current_date);
    CHECK(target_ctrl.days_stepped   == source_ctrl.days_stepped);
    CHECK(target_ctrl.monthly_ticks  == source_ctrl.monthly_ticks);
    REQUIRE(target.applied_commands.size() == source.applied_commands.size());
    for (std::size_t i = 0; i < source.applied_commands.size(); ++i) {
        CHECK(target.applied_commands[i].applied_on ==
              source.applied_commands[i].applied_on);
        CHECK(target.applied_commands[i].command.kind ==
              source.applied_commands[i].command.kind);
    }
    // The command effects landed identically.
    CHECK(target.countries[0].legal_tax_burden ==
          doctest::Approx(source.countries[0].legal_tax_burden));
    CHECK(target.countries[0].budget.military  ==
          doctest::Approx(source.countries[0].budget.military));
    // The monthly pipeline also mutated economy / stability identically.
    CHECK(target.countries[0].gdp                  ==
          doctest::Approx(source.countries[0].gdp));
    CHECK(target.countries[0].stability            ==
          doctest::Approx(source.countries[0].stability));
    CHECK(target.countries[0].last_gdp_growth_rate ==
          doctest::Approx(source.countries[0].last_gdp_growth_rate));
}

// =====================================================================
// M2.18 - apply_pending honours the order_execution gate
// =====================================================================

TEST_CASE("M2.18 apply_pending: EnactPolicy is accepted at the default 0.5 compliance") {
    // Regression: scenario-loaded countries default to 0.5 across
    // every government_authority field; the M2.18 gate must not
    // silently start rejecting them.
    GameState s = ger_state_with_player_selected();
    REQUIRE(s.countries[0].government_authority.bureaucratic_compliance ==
            doctest::Approx(0.5));
    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.ok());
    CHECK(r.value().commands_applied == 1);
    CHECK(q.pending.empty());
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].command.policy_id_code == "raise_taxes");
}

TEST_CASE("M2.18 apply_pending: EnactPolicy rejected when compliance < 0.3") {
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.1;
    const double tax_before = s.countries[0].legal_tax_burden;
    const auto active_before = s.countries[0].active_policies.size();
    const auto log_before    = s.applied_commands.size();

    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("order_execution") != std::string::npos);
    CHECK(r.error().find("rejected")        != std::string::npos);
    CHECK(r.error().find("raise_taxes")     != std::string::npos);

    // Per-command atomicity: state unchanged, queue head intact,
    // applied_commands log untouched.
    CHECK(s.countries[0].legal_tax_burden    == doctest::Approx(tax_before));
    CHECK(s.countries[0].active_policies.size() == active_before);
    REQUIRE(q.pending.size() == 1u);
    CHECK(q.pending[0].kind == PlayerCommandKind::EnactPolicy);
    CHECK(q.pending[0].policy_id_code == "raise_taxes");
    CHECK(s.applied_commands.size() == log_before);
}

TEST_CASE("M2.18 apply_pending: rejected EnactPolicy stops a mid-list queue") {
    // Mid-list rejection mirrors M2.3's unknown-policy semantics:
    // the previous command applied, the rejected one stays at the
    // head, and the trailing command stays queued.
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.1;

    cmd::CommandQueue q;
    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "military";
    adjust.budget_delta    = 0.02;
    q.pending.push_back(adjust);                                          // accepted (AdjustBudget bypasses gate)
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"}); // rejected at gate
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "increase_education"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("raise_taxes") != std::string::npos);

    // AdjustBudget applied + popped + logged.
    CHECK(s.countries[0].budget.military == doctest::Approx(0.37));
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].command.kind ==
          PlayerCommandKind::AdjustBudget);

    // Rejected EnactPolicy still at head; trailing entry still queued.
    REQUIRE(q.pending.size() == 2u);
    CHECK(q.pending[0].kind            == PlayerCommandKind::EnactPolicy);
    CHECK(q.pending[0].policy_id_code  == "raise_taxes");
    CHECK(q.pending[1].policy_id_code  == "increase_education");
}

TEST_CASE("M2.19 apply_pending: AdjustBudget(military) low bureaucratic_compliance still applies if military_loyalty is high") {
    // Pre-M2.19 the same test pinned AdjustBudget "bypassed" the
    // gate. M2.19 routes the "military" category through
    // military_loyalty; with the default 0.5 from
    // germany_baseline() that path still Accepts even when
    // bureaucratic_compliance is lowered.
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.05;
    REQUIRE(s.countries[0].government_authority.military_loyalty ==
            doctest::Approx(0.5));
    const double mil_before = s.countries[0].budget.military;

    cmd::CommandQueue q;
    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "military";
    adjust.budget_delta    = 0.03;
    q.pending.push_back(adjust);

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.ok());
    CHECK(s.countries[0].budget.military ==
          doctest::Approx(mil_before + 0.03));
    REQUIRE(s.applied_commands.size() == 1u);
}

// =====================================================================
// M2.19 - apply_pending honours the category-aware AdjustBudget gate
// =====================================================================

TEST_CASE("M2.19 apply_pending: AdjustBudget(military) rejected when military_loyalty < 0.3") {
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.military_loyalty = 0.1;
    const double mil_before = s.countries[0].budget.military;

    cmd::CommandQueue q;
    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "military";
    adjust.budget_delta    = 0.05;
    q.pending.push_back(adjust);

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("order_execution") != std::string::npos);
    CHECK(r.error().find("rejected")        != std::string::npos);
    CHECK(r.error().find("AdjustBudget")    != std::string::npos);
    CHECK(r.error().find("military")        != std::string::npos);

    // No mutation, queue head intact, no applied_commands entry.
    CHECK(s.countries[0].budget.military == doctest::Approx(mil_before));
    REQUIRE(q.pending.size() == 1u);
    CHECK(q.pending[0].budget_category == "military");
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.19 apply_pending: AdjustBudget(welfare) rejected when bureaucratic_compliance < 0.3 even if military_loyalty high") {
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.1;
    s.countries[0].government_authority.military_loyalty        = 0.95;
    const double wel_before = s.countries[0].budget.welfare;

    cmd::CommandQueue q;
    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "welfare";
    adjust.budget_delta    = 0.04;
    q.pending.push_back(adjust);

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("order_execution") != std::string::npos);
    CHECK(r.error().find("welfare")         != std::string::npos);

    CHECK(s.countries[0].budget.welfare == doctest::Approx(wel_before));
    REQUIRE(q.pending.size() == 1u);
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.19 apply_pending: AdjustBudget(military) accepted by high military_loyalty even when bureaucratic_compliance is low") {
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.05;
    s.countries[0].government_authority.military_loyalty        = 0.8;
    const double mil_before = s.countries[0].budget.military;

    cmd::CommandQueue q;
    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "military";
    adjust.budget_delta    = 0.02;
    q.pending.push_back(adjust);

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.ok());
    CHECK(s.countries[0].budget.military ==
          doctest::Approx(mil_before + 0.02));
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].command.kind ==
          PlayerCommandKind::AdjustBudget);
}

// =====================================================================
// M2.20 - try_apply_pending: structured rejection surface
// =====================================================================

TEST_CASE("M2.20 try_apply_pending: full drain returns success with empty rejection") {
    GameState s = ger_state_with_player_selected();
    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    const auto r = cmd::try_apply_pending(s, q);
    REQUIRE(r.ok());
    CHECK(r.value().apply.commands_applied == 1);
    CHECK_FALSE(r.value().rejection.has_value());
    CHECK(q.pending.empty());
    REQUIRE(s.applied_commands.size() == 1u);
}

TEST_CASE("M2.20 try_apply_pending: EnactPolicy rejection surfaces a structured record") {
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.1;
    const double tax_before = s.countries[0].legal_tax_burden;

    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    const auto r = cmd::try_apply_pending(s, q);
    REQUIRE(r.ok());
    REQUIRE(r.value().rejection.has_value());
    const auto& rj = r.value().rejection.value();
    CHECK(rj.kind            == PlayerCommandKind::EnactPolicy);
    CHECK(rj.policy_id_code  == "raise_taxes");
    CHECK(rj.budget_category == "");
    CHECK(rj.compliance      == doctest::Approx(0.1));
    CHECK(rj.threshold       == doctest::Approx(0.3));
    CHECK(rj.resistance      == doctest::Approx(0.9));
    CHECK(r.value().apply.commands_applied == 0);

    // Atomicity preserved: state unchanged, queue head intact,
    // applied_commands empty.
    CHECK(s.countries[0].legal_tax_burden == doctest::Approx(tax_before));
    REQUIRE(q.pending.size() == 1u);
    CHECK(q.pending[0].policy_id_code == "raise_taxes");
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.20 try_apply_pending: AdjustBudget(military) rejection records military_loyalty as compliance") {
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.military_loyalty = 0.05;

    cmd::CommandQueue q;
    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "military";
    adjust.budget_delta    = 0.04;
    q.pending.push_back(adjust);

    const auto r = cmd::try_apply_pending(s, q);
    REQUIRE(r.ok());
    REQUIRE(r.value().rejection.has_value());
    const auto& rj = r.value().rejection.value();
    CHECK(rj.kind            == PlayerCommandKind::AdjustBudget);
    CHECK(rj.policy_id_code  == "");
    CHECK(rj.budget_category == "military");
    CHECK(rj.compliance      == doctest::Approx(0.05));   // military_loyalty
    CHECK(rj.threshold       == doctest::Approx(0.3));
    CHECK(rj.resistance      == doctest::Approx(0.95));
    REQUIRE(q.pending.size() == 1u);
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.20 try_apply_pending: AdjustBudget(welfare) rejection records bureaucratic_compliance") {
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.2;
    s.countries[0].government_authority.military_loyalty        = 0.95;

    cmd::CommandQueue q;
    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "welfare";
    adjust.budget_delta    = 0.05;
    q.pending.push_back(adjust);

    const auto r = cmd::try_apply_pending(s, q);
    REQUIRE(r.ok());
    REQUIRE(r.value().rejection.has_value());
    const auto& rj = r.value().rejection.value();
    CHECK(rj.budget_category == "welfare");
    // Selected input is bureaucratic_compliance, NOT the (high)
    // military_loyalty.
    CHECK(rj.compliance == doctest::Approx(0.2));
}

TEST_CASE("M2.20 try_apply_pending: unknown policy id_code still returns failure") {
    GameState s = ger_state_with_player_selected();

    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "no_such_policy"});

    const auto r = cmd::try_apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("unknown policy id_code") != std::string::npos);
    // Failure path: queue still has the offending command.
    REQUIRE(q.pending.size() == 1u);
}

TEST_CASE("M2.20 try_apply_pending: unknown budget_category still returns failure") {
    GameState s = ger_state_with_player_selected();

    cmd::CommandQueue q;
    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "spaceforce";   // not a real category.
    adjust.budget_delta    = 0.05;
    q.pending.push_back(adjust);

    const auto r = cmd::try_apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("unknown budget_category") != std::string::npos);
}

TEST_CASE("M2.20 try_apply_pending: non-finite budget_delta still returns failure") {
    GameState s = ger_state_with_player_selected();

    cmd::CommandQueue q;
    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "military";
    adjust.budget_delta    = std::numeric_limits<double>::quiet_NaN();
    q.pending.push_back(adjust);

    const auto r = cmd::try_apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("not finite") != std::string::npos);
}

TEST_CASE("M2.20 try_apply_pending: invalid player_country returns failure naming the helper") {
    GameState s;   // no countries, no player.
    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    const auto r = cmd::try_apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("try_apply_pending") != std::string::npos);
    CHECK(r.error().find("player_country")    != std::string::npos);
}

TEST_CASE("M2.20 try_apply_pending: mid-list rejection preserves prior successful commands") {
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.05;
    s.countries[0].government_authority.military_loyalty        = 0.9;

    cmd::CommandQueue q;
    // 1. AdjustBudget(military) — military_loyalty 0.9 ⇒ accepted.
    PlayerCommand mil_adjust;
    mil_adjust.kind            = PlayerCommandKind::AdjustBudget;
    mil_adjust.budget_category = "military";
    mil_adjust.budget_delta    = 0.03;
    q.pending.push_back(mil_adjust);
    // 2. EnactPolicy — bureaucratic 0.05 ⇒ rejected at the gate.
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});
    // 3. AdjustBudget(welfare) — would also reject but never reached.
    PlayerCommand wel_adjust;
    wel_adjust.kind            = PlayerCommandKind::AdjustBudget;
    wel_adjust.budget_category = "welfare";
    wel_adjust.budget_delta    = 0.04;
    q.pending.push_back(wel_adjust);

    const auto r = cmd::try_apply_pending(s, q);
    REQUIRE(r.ok());
    REQUIRE(r.value().rejection.has_value());
    CHECK(r.value().rejection.value().kind ==
          PlayerCommandKind::EnactPolicy);
    CHECK(r.value().rejection.value().policy_id_code == "raise_taxes");
    CHECK(r.value().apply.commands_applied == 1);

    // Prior AdjustBudget(military) landed.
    CHECK(s.countries[0].budget.military == doctest::Approx(0.38));
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].command.kind ==
          PlayerCommandKind::AdjustBudget);

    // Rejected EnactPolicy stays at head; trailing AdjustBudget still queued.
    REQUIRE(q.pending.size() == 2u);
    CHECK(q.pending[0].kind            == PlayerCommandKind::EnactPolicy);
    CHECK(q.pending[0].policy_id_code  == "raise_taxes");
    CHECK(q.pending[1].budget_category == "welfare");
}

TEST_CASE("M2.20 apply_pending: rejection still returns Result::failure (backward compat)") {
    // The structured surface lives behind `try_apply_pending`;
    // the legacy `apply_pending` must keep failing on rejection
    // so existing callers (M2.18 / M2.19 tests, replay_with_time)
    // see no behaviour change.
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.1;

    cmd::CommandQueue q;
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("order_execution") != std::string::npos);
    CHECK(r.error().find("rejected")        != std::string::npos);
    CHECK(r.error().find("raise_taxes")     != std::string::npos);
}

TEST_CASE("M2.19 apply_pending: rejected AdjustBudget stops a mid-list queue") {
    // Mirrors the M2.18 mid-list test: prior command applies +
    // logs, rejected command stays at head, trailing command
    // stays queued.
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.05;
    s.countries[0].government_authority.military_loyalty        = 0.9;

    cmd::CommandQueue q;
    // 1. AdjustBudget(military) — military_loyalty 0.9 ⇒ accepted.
    PlayerCommand mil_adjust;
    mil_adjust.kind            = PlayerCommandKind::AdjustBudget;
    mil_adjust.budget_category = "military";
    mil_adjust.budget_delta    = 0.03;
    q.pending.push_back(mil_adjust);
    // 2. AdjustBudget(welfare) — bureaucratic 0.05 ⇒ rejected.
    PlayerCommand wel_adjust;
    wel_adjust.kind            = PlayerCommandKind::AdjustBudget;
    wel_adjust.budget_category = "welfare";
    wel_adjust.budget_delta    = 0.04;
    q.pending.push_back(wel_adjust);
    // 3. EnactPolicy — would have been rejected too (compliance
    // < 0.3) but the queue stops at the first rejection.
    q.pending.push_back({PlayerCommandKind::EnactPolicy, "raise_taxes"});

    const auto r = cmd::apply_pending(s, q);
    REQUIRE(r.failed());
    CHECK(r.error().find("welfare") != std::string::npos);

    // Military adjustment landed and logged.
    CHECK(s.countries[0].budget.military == doctest::Approx(0.38));
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].command.budget_category == "military");

    // Welfare rejection sits at the head; EnactPolicy still queued.
    REQUIRE(q.pending.size() == 2u);
    CHECK(q.pending[0].budget_category == "welfare");
    CHECK(q.pending[1].kind            == PlayerCommandKind::EnactPolicy);
    CHECK(q.pending[1].policy_id_code  == "raise_taxes");
}

// =====================================================================
// M2.21 - apply_command_script: library-only scripted driver helper
// =====================================================================

TEST_CASE("M2.21 apply_command_script: empty script is a success no-op") {
    GameState s = ger_state_with_player_selected();
    std::vector<PlayerCommand> script;

    const auto r = cmd::apply_command_script(s, script);
    REQUIRE(r.ok());
    CHECK(r.value().apply.commands_applied == 0);
    CHECK_FALSE(r.value().rejection.has_value());
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.21 apply_command_script: full success script applies every command + logs") {
    GameState s = ger_state_with_player_selected();
    const double tax_before  = s.countries[0].legal_tax_burden;
    const double mil_before  = s.countries[0].budget.military;

    PlayerCommand enact;
    enact.kind            = PlayerCommandKind::EnactPolicy;
    enact.policy_id_code = "raise_taxes";

    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "military";
    adjust.budget_delta    = 0.02;

    const std::vector<PlayerCommand> script{enact, adjust};
    const auto r = cmd::apply_command_script(s, script);
    REQUIRE(r.ok());
    CHECK(r.value().apply.commands_applied == 2);
    CHECK_FALSE(r.value().rejection.has_value());

    // Both effects landed.
    CHECK(s.countries[0].legal_tax_burden > tax_before);
    CHECK(s.countries[0].budget.military ==
          doctest::Approx(mil_before + 0.02));
    // Both commands logged in order.
    REQUIRE(s.applied_commands.size() == 2u);
    CHECK(s.applied_commands[0].command.kind ==
          PlayerCommandKind::EnactPolicy);
    CHECK(s.applied_commands[1].command.kind ==
          PlayerCommandKind::AdjustBudget);
}

TEST_CASE("M2.21 apply_command_script: EnactPolicy rejected surfaces a structured record") {
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.1;

    PlayerCommand enact;
    enact.kind            = PlayerCommandKind::EnactPolicy;
    enact.policy_id_code = "raise_taxes";
    const std::vector<PlayerCommand> script{enact};

    const auto r = cmd::apply_command_script(s, script);
    REQUIRE(r.ok());
    REQUIRE(r.value().rejection.has_value());
    const auto& rj = r.value().rejection.value();
    CHECK(rj.kind            == PlayerCommandKind::EnactPolicy);
    CHECK(rj.policy_id_code  == "raise_taxes");
    CHECK(rj.compliance      == doctest::Approx(0.1));
    CHECK(rj.threshold       == doctest::Approx(0.3));
    CHECK(r.value().apply.commands_applied == 0);
    CHECK(s.applied_commands.empty());
}

TEST_CASE("M2.21 apply_command_script: AdjustBudget(military) rejection records military_loyalty") {
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.military_loyalty = 0.05;

    PlayerCommand adjust;
    adjust.kind            = PlayerCommandKind::AdjustBudget;
    adjust.budget_category = "military";
    adjust.budget_delta    = 0.03;
    const std::vector<PlayerCommand> script{adjust};

    const auto r = cmd::apply_command_script(s, script);
    REQUIRE(r.ok());
    REQUIRE(r.value().rejection.has_value());
    CHECK(r.value().rejection.value().kind ==
          PlayerCommandKind::AdjustBudget);
    CHECK(r.value().rejection.value().budget_category == "military");
    CHECK(r.value().rejection.value().compliance ==
          doctest::Approx(0.05));   // military_loyalty
}

TEST_CASE("M2.21 apply_command_script: mid-script rejection preserves prior success") {
    // First command applies; second is rejected; third never runs.
    GameState s = ger_state_with_player_selected();
    s.countries[0].government_authority.bureaucratic_compliance = 0.05;
    s.countries[0].government_authority.military_loyalty        = 0.9;

    PlayerCommand mil;
    mil.kind            = PlayerCommandKind::AdjustBudget;
    mil.budget_category = "military";
    mil.budget_delta    = 0.03;

    PlayerCommand enact;
    enact.kind            = PlayerCommandKind::EnactPolicy;
    enact.policy_id_code = "raise_taxes";   // bureaucratic 0.05 -> rejected.

    PlayerCommand wel;
    wel.kind            = PlayerCommandKind::AdjustBudget;
    wel.budget_category = "welfare";
    wel.budget_delta    = 0.05;

    const std::vector<PlayerCommand> script{mil, enact, wel};

    const auto r = cmd::apply_command_script(s, script);
    REQUIRE(r.ok());
    REQUIRE(r.value().rejection.has_value());
    CHECK(r.value().rejection.value().kind ==
          PlayerCommandKind::EnactPolicy);
    CHECK(r.value().rejection.value().policy_id_code == "raise_taxes");
    CHECK(r.value().apply.commands_applied == 1);

    // Prior AdjustBudget(military) applied + logged.
    CHECK(s.countries[0].budget.military == doctest::Approx(0.38));
    REQUIRE(s.applied_commands.size() == 1u);
    CHECK(s.applied_commands[0].command.kind ==
          PlayerCommandKind::AdjustBudget);
    // Trailing AdjustBudget(welfare) was NOT applied. The script
    // helper does not surface remaining commands by design.
    CHECK(s.countries[0].budget.welfare == doctest::Approx(0.10));
}

TEST_CASE("M2.21 apply_command_script: unknown policy id_code returns failure") {
    GameState s = ger_state_with_player_selected();

    PlayerCommand enact;
    enact.kind            = PlayerCommandKind::EnactPolicy;
    enact.policy_id_code = "no_such_policy";
    const std::vector<PlayerCommand> script{enact};

    const auto r = cmd::apply_command_script(s, script);
    REQUIRE(r.failed());
    CHECK(r.error().find("unknown policy id_code") != std::string::npos);
}

TEST_CASE("M2.21 apply_command_script: invalid player_country returns failure") {
    GameState s;   // no countries, no player.
    PlayerCommand enact;
    enact.kind            = PlayerCommandKind::EnactPolicy;
    enact.policy_id_code = "raise_taxes";
    const std::vector<PlayerCommand> script{enact};

    const auto r = cmd::apply_command_script(s, script);
    REQUIRE(r.failed());
    CHECK(r.error().find("player_country") != std::string::npos);
}

TEST_CASE("M2.21 apply_command_script: input vector is not mutated") {
    // The helper builds a local CommandQueue and drains that;
    // the caller's vector must be left exactly as it was.
    GameState s = ger_state_with_player_selected();

    PlayerCommand a;
    a.kind            = PlayerCommandKind::EnactPolicy;
    a.policy_id_code = "raise_taxes";
    PlayerCommand b;
    b.kind            = PlayerCommandKind::AdjustBudget;
    b.budget_category = "military";
    b.budget_delta    = 0.01;

    const std::vector<PlayerCommand> script_before{a, b};
    std::vector<PlayerCommand> script = script_before;

    REQUIRE(cmd::apply_command_script(s, script).ok());

    REQUIRE(script.size() == script_before.size());
    CHECK(script[0].kind            == script_before[0].kind);
    CHECK(script[0].policy_id_code  == script_before[0].policy_id_code);
    CHECK(script[1].kind            == script_before[1].kind);
    CHECK(script[1].budget_category == script_before[1].budget_category);
    CHECK(script[1].budget_delta    == script_before[1].budget_delta);
}

// =====================================================================
// M4.1 - command gate diagnostics surface
//
// Pure-read helpers that explain how the existing M2.18 / M2.19
// command-execution gates would evaluate on a country in its
// current `government_authority` state. The diagnostics must not
// drift from the gate `apply_pending` actually uses; the last group
// of tests in this section pins that "mirror" invariant by
// comparing the diagnostic's `allowed` flag against what
// `apply_pending` does on the same state.
// =====================================================================

namespace {

// Build a country with all four `government_authority` sub-fields
// dialled to chosen values so a single test can vary one input
// without touching the others.
CountryState country_with_authority(const std::string& id_code,
                                    double bureaucratic_compliance,
                                    double military_loyalty,
                                    double intelligence_capability = 0.5,
                                    double media_control = 0.5) {
    CountryState c;
    c.id      = CountryId{0};
    c.id_code = id_code;
    c.name    = id_code;
    c.government_authority.bureaucratic_compliance =
        bureaucratic_compliance;
    c.government_authority.military_loyalty         = military_loyalty;
    c.government_authority.intelligence_capability  =
        intelligence_capability;
    c.government_authority.media_control            = media_control;
    return c;
}

GameState single_country_state(double bureaucratic_compliance,
                               double military_loyalty) {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.countries.push_back(country_with_authority(
        "GER", bureaucratic_compliance, military_loyalty));
    s.player_country = CountryId{0};
    return s;
}

}  // namespace

TEST_CASE("diagnose_enact_policy_gate: allowed when bureaucratic_compliance == threshold (0.3)") {
    // Boundary: the M2.18 rule is `>= threshold` so an exact 0.3
    // must be Accepted (matches order_execution::evaluate).
    GameState s = single_country_state(0.30, 0.50);
    auto r = cmd::diagnose_enact_policy_gate(
        s, CountryId{0}, "raise_taxes");
    REQUIRE(r.ok());
    const auto& d = r.value();
    CHECK(d.gate            == cmd::CommandGateKind::EnactPolicy);
    CHECK(d.country.value() == 0);
    CHECK(d.country_id_code == "GER");
    CHECK(d.target          == "policy:raise_taxes");
    CHECK(d.authority_field == "bureaucratic_compliance");
    CHECK(d.authority_value == doctest::Approx(0.30));
    CHECK(d.threshold       == doctest::Approx(0.30));
    CHECK(d.allowed);
}

TEST_CASE("diagnose_enact_policy_gate: rejected below threshold") {
    GameState s = single_country_state(0.29, 0.99);
    auto r = cmd::diagnose_enact_policy_gate(
        s, CountryId{0}, "raise_taxes");
    REQUIRE(r.ok());
    const auto& d = r.value();
    CHECK(d.authority_field == "bureaucratic_compliance");
    CHECK(d.authority_value == doctest::Approx(0.29));
    CHECK_FALSE(d.allowed);
}

TEST_CASE("diagnose_enact_policy_gate: ignores military_loyalty") {
    // Even with military_loyalty pinned to 1.0, an EnactPolicy gate
    // reading only bureaucratic_compliance still rejects when the
    // bureaucratic field is below the threshold.
    GameState s = single_country_state(0.10, 1.00);
    auto r = cmd::diagnose_enact_policy_gate(
        s, CountryId{0}, "some_policy");
    REQUIRE(r.ok());
    CHECK(r.value().authority_field == "bureaucratic_compliance");
    CHECK(r.value().authority_value == doctest::Approx(0.10));
    CHECK_FALSE(r.value().allowed);
}

TEST_CASE("diagnose_enact_policy_gate: does NOT validate that the policy exists") {
    // The gate is a pure authority check; the diagnostic must not
    // depend on state.policies.
    GameState s = single_country_state(0.80, 0.50);
    s.policies.clear();  // no policies loaded at all
    auto r = cmd::diagnose_enact_policy_gate(
        s, CountryId{0}, "no_such_policy");
    REQUIRE(r.ok());
    CHECK(r.value().target  == "policy:no_such_policy");
    CHECK(r.value().allowed);
}

TEST_CASE("diagnose_adjust_budget_gate: military category reads military_loyalty") {
    GameState s = single_country_state(/*bureaucratic=*/0.10,
                                       /*military=*/0.80);
    auto r = cmd::diagnose_adjust_budget_gate(
        s, CountryId{0}, "military");
    REQUIRE(r.ok());
    const auto& d = r.value();
    CHECK(d.gate            == cmd::CommandGateKind::AdjustBudget);
    CHECK(d.target          == "budget:military");
    CHECK(d.authority_field == "military_loyalty");
    CHECK(d.authority_value == doctest::Approx(0.80));
    CHECK(d.threshold       == doctest::Approx(0.30));
    CHECK(d.allowed);
    // The bureaucratic field is NOT consulted: 0.10 would have
    // rejected if it were, but military_loyalty is what matters.
}

TEST_CASE("diagnose_adjust_budget_gate: non-military category reads bureaucratic_compliance") {
    // Same state as the military test, but ask about a different
    // category — must flip to the bureaucratic field and reject.
    GameState s = single_country_state(/*bureaucratic=*/0.10,
                                       /*military=*/0.80);
    auto r = cmd::diagnose_adjust_budget_gate(
        s, CountryId{0}, "welfare");
    REQUIRE(r.ok());
    const auto& d = r.value();
    CHECK(d.target          == "budget:welfare");
    CHECK(d.authority_field == "bureaucratic_compliance");
    CHECK(d.authority_value == doctest::Approx(0.10));
    CHECK_FALSE(d.allowed);
}

TEST_CASE("diagnose_adjust_budget_gate: unknown category still routes to bureaucratic_compliance") {
    // Mirrors order_execution::evaluate: ONLY the exact string
    // "military" routes to military_loyalty; every other string
    // (including unknown ones the apply path would reject later)
    // routes to bureaucratic_compliance. This is the gate's
    // behaviour, not the apply path's — the gate doesn't enforce
    // the seven-field whitelist.
    GameState s = single_country_state(0.40, 0.99);
    auto r = cmd::diagnose_adjust_budget_gate(
        s, CountryId{0}, "ponies");
    REQUIRE(r.ok());
    CHECK(r.value().authority_field == "bureaucratic_compliance");
    CHECK(r.value().authority_value == doctest::Approx(0.40));
    CHECK(r.value().allowed);
}

TEST_CASE("diagnose_enact_policy_gate: invalid country id returns failure") {
    GameState s = single_country_state(0.50, 0.50);
    auto r = cmd::diagnose_enact_policy_gate(
        s, CountryId{99}, "raise_taxes");
    REQUIRE(r.failed());
    CHECK(r.error().find("99") != std::string::npos);
}

TEST_CASE("diagnose_adjust_budget_gate: invalid country id returns failure") {
    GameState s = single_country_state(0.50, 0.50);
    auto r = cmd::diagnose_adjust_budget_gate(
        s, CountryId{99}, "military");
    REQUIRE(r.failed());
    CHECK(r.error().find("99") != std::string::npos);
}

TEST_CASE("diagnose_enact_policy_gate: default-constructed (-1) CountryId fails") {
    GameState s = single_country_state(0.50, 0.50);
    auto r = cmd::diagnose_enact_policy_gate(s, CountryId{}, "p");
    REQUIRE(r.failed());
}

// ---------------------------------------------------------------------
// Mirror: the diagnostic's `allowed` flag agrees with what
// apply_pending actually does on the same state. This is the
// no-formula-drift gate; it would fail loudly if the diagnostic
// helper diverged from order_execution's real gate (different
// threshold, wrong field-selection rule, off-by-one comparison).
// ---------------------------------------------------------------------

TEST_CASE("diagnose_enact_policy_gate: agrees with apply_pending on an accepted command") {
    GameState s = single_country_state(/*bureaucratic=*/0.50,
                                       /*military=*/0.50);
    s.policies.push_back(raise_taxes_policy());

    // Diagnostic says allowed.
    auto diag = cmd::diagnose_enact_policy_gate(
        s, s.player_country, "raise_taxes");
    REQUIRE(diag.ok());
    REQUIRE(diag.value().allowed);

    // apply_pending actually accepts and applies.
    PlayerCommand cmd_p;
    cmd_p.kind            = PlayerCommandKind::EnactPolicy;
    cmd_p.policy_id_code  = "raise_taxes";
    cmd::CommandQueue q;
    q.pending.push_back(cmd_p);
    const auto apply_r = cmd::apply_pending(s, q);
    REQUIRE(apply_r.ok());
    CHECK(apply_r.value().commands_applied == 1);
}

TEST_CASE("diagnose_enact_policy_gate: agrees with apply_pending on a rejected command") {
    GameState s = single_country_state(/*bureaucratic=*/0.10,
                                       /*military=*/0.50);
    s.policies.push_back(raise_taxes_policy());

    auto diag = cmd::diagnose_enact_policy_gate(
        s, s.player_country, "raise_taxes");
    REQUIRE(diag.ok());
    REQUIRE_FALSE(diag.value().allowed);

    PlayerCommand cmd_p;
    cmd_p.kind            = PlayerCommandKind::EnactPolicy;
    cmd_p.policy_id_code  = "raise_taxes";
    cmd::CommandQueue q;
    q.pending.push_back(cmd_p);
    const auto apply_r = cmd::apply_pending(s, q);
    REQUIRE(apply_r.failed());
    CHECK(apply_r.error().find("rejected") != std::string::npos);
}

TEST_CASE("diagnose_adjust_budget_gate: agrees with apply_pending on an accepted military command") {
    // Low bureaucratic, high military: the military adjust must
    // accept (selects military_loyalty) and the diagnostic must
    // agree.
    GameState s = single_country_state(/*bureaucratic=*/0.10,
                                       /*military=*/0.80);
    // Pre-existing budget value so the AdjustBudget delta lands
    // cleanly inside [0, 1].
    s.countries[0].budget.military = 0.30;

    auto diag = cmd::diagnose_adjust_budget_gate(
        s, s.player_country, "military");
    REQUIRE(diag.ok());
    REQUIRE(diag.value().allowed);

    PlayerCommand cmd_p;
    cmd_p.kind            = PlayerCommandKind::AdjustBudget;
    cmd_p.budget_category = "military";
    cmd_p.budget_delta    = 0.05;
    cmd::CommandQueue q;
    q.pending.push_back(cmd_p);
    const auto apply_r = cmd::apply_pending(s, q);
    REQUIRE(apply_r.ok());
    CHECK(apply_r.value().commands_applied == 1);
}

TEST_CASE("diagnose_adjust_budget_gate: agrees with apply_pending on a rejected military command") {
    // High bureaucratic, low military: the military adjust must
    // reject and the diagnostic must agree.
    GameState s = single_country_state(/*bureaucratic=*/0.90,
                                       /*military=*/0.10);
    s.countries[0].budget.military = 0.30;

    auto diag = cmd::diagnose_adjust_budget_gate(
        s, s.player_country, "military");
    REQUIRE(diag.ok());
    REQUIRE_FALSE(diag.value().allowed);

    PlayerCommand cmd_p;
    cmd_p.kind            = PlayerCommandKind::AdjustBudget;
    cmd_p.budget_category = "military";
    cmd_p.budget_delta    = 0.05;
    cmd::CommandQueue q;
    q.pending.push_back(cmd_p);
    const auto apply_r = cmd::apply_pending(s, q);
    REQUIRE(apply_r.failed());
    CHECK(apply_r.error().find("rejected") != std::string::npos);
    CHECK(apply_r.error().find("military") != std::string::npos);
}

TEST_CASE("diagnose_*: diagnostic does NOT mutate state") {
    // Both helpers must be byte-identical reads. Spot-check the
    // common fields after both calls.
    GameState s = single_country_state(0.50, 0.50);
    s.policies.push_back(raise_taxes_policy());
    const auto current_date_before = s.current_date;
    const std::size_t logs_before  = s.logs.size();
    const std::size_t applied_before = s.applied_commands.size();
    const double bc_before =
        s.countries[0].government_authority.bureaucratic_compliance;
    const double ml_before =
        s.countries[0].government_authority.military_loyalty;

    REQUIRE(cmd::diagnose_enact_policy_gate(
        s, CountryId{0}, "raise_taxes").ok());
    REQUIRE(cmd::diagnose_adjust_budget_gate(
        s, CountryId{0}, "military").ok());
    REQUIRE(cmd::diagnose_adjust_budget_gate(
        s, CountryId{0}, "welfare").ok());

    CHECK(s.current_date     == current_date_before);
    CHECK(s.logs.size()      == logs_before);
    CHECK(s.applied_commands.size() == applied_before);
    CHECK(s.countries[0].government_authority.bureaucratic_compliance
          == doctest::Approx(bc_before));
    CHECK(s.countries[0].government_authority.military_loyalty
          == doctest::Approx(ml_before));
}
