#include <doctest/doctest.h>

#include <limits>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/player_commands.hpp"
#include "leviathan/systems/commands.hpp"

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
