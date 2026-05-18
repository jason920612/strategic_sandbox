#include <doctest/doctest.h>

#include <sstream>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/systems/diagnostics.hpp"

using leviathan::core::CountryId;
using leviathan::core::CountryState;
using leviathan::core::FactionId;
using leviathan::core::FactionState;
using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::LogEntry;
using leviathan::core::LogSeverity;
namespace dg = leviathan::systems::diagnostics;

// ---------------------------------------------------------------------
// snapshot
// ---------------------------------------------------------------------

TEST_CASE("snapshot: empty state yields zeros and the default date") {
    GameState s;
    const auto r = dg::snapshot(s);
    CHECK(r.date          == GameDate(1930, 1, 1));
    CHECK(r.country_count == 0u);
    CHECK(r.log_count     == 0u);
    CHECK(r.seed          == 0u);
}

TEST_CASE("snapshot: counts and seed reflect the live state") {
    GameState s;
    s.current_date = GameDate(1936, 7, 17);
    s.rng.seed     = 19360717u;

    CountryState a;
    a.id = CountryId{0};
    a.id_code = "GER";
    s.countries.push_back(a);
    CountryState b;
    b.id = CountryId{1};
    b.id_code = "FRA";
    s.countries.push_back(b);

    LogEntry l;
    l.date = s.current_date;
    l.message = "marker";
    s.logs.push_back(l);
    s.logs.push_back(l);
    s.logs.push_back(l);

    const auto r = dg::snapshot(s);
    CHECK(r.date          == GameDate(1936, 7, 17));
    CHECK(r.country_count == 2u);
    CHECK(r.log_count     == 3u);
    CHECK(r.seed          == 19360717u);
}

TEST_CASE("snapshot does not mutate the input GameState") {
    GameState s;
    s.current_date = GameDate(1930, 1, 1);
    s.rng.seed     = 42u;
    s.rng.counter  = 7u;
    const auto before_seed    = s.rng.seed;
    const auto before_counter = s.rng.counter;
    const auto before_date    = s.current_date;
    const auto before_countries = s.countries.size();
    const auto before_logs    = s.logs.size();

    (void) dg::snapshot(s);

    CHECK(s.rng.seed     == before_seed);
    CHECK(s.rng.counter  == before_counter);
    CHECK(s.current_date == before_date);
    CHECK(s.countries.size() == before_countries);
    CHECK(s.logs.size()      == before_logs);
}

// ---------------------------------------------------------------------
// CSV writers
// ---------------------------------------------------------------------

TEST_CASE("write_csv_header writes the canonical column order") {
    std::ostringstream out;
    dg::write_csv_header(out);
    CHECK(out.str() == "date,country_count,log_count,seed\n");
}

TEST_CASE("write_csv_row formats every field, no quoting needed") {
    dg::SummaryRow row;
    row.date          = GameDate(1930, 1, 1);
    row.country_count = 2;
    row.log_count     = 14;
    row.seed          = 19300101u;

    std::ostringstream out;
    dg::write_csv_row(out, row);
    CHECK(out.str() == "1930-01-01,2,14,19300101\n");
}

TEST_CASE("write_csv_row handles uint64 seeds at the upper range") {
    dg::SummaryRow row;
    row.date          = GameDate(2000, 12, 31);
    row.country_count = 0;
    row.log_count     = 0;
    row.seed          = static_cast<std::uint64_t>(-1);   // uint64_t::max

    std::ostringstream out;
    dg::write_csv_row(out, row);
    CHECK(out.str() == "2000-12-31,0,0,18446744073709551615\n");
}

// ---------------------------------------------------------------------
// sanity_check
// ---------------------------------------------------------------------

TEST_CASE("sanity_check: clean state returns no issues") {
    GameState s;
    CHECK(dg::sanity_check(s).empty());
}

TEST_CASE("sanity_check: clean state with countries returns no issues") {
    GameState s;
    CountryState a;
    a.id = CountryId{0};
    CountryState b;
    b.id = CountryId{1};
    s.countries.push_back(a);
    s.countries.push_back(b);
    CHECK(dg::sanity_check(s).empty());
}

TEST_CASE("sanity_check: invalid current_date is flagged") {
    GameState s;
    s.current_date = GameDate(1930, 2, 30);   // not a real date
    const auto issues = dg::sanity_check(s);
    REQUIRE(issues.size() == 1);
    CHECK(issues.front().code     == "invalid_date");
    CHECK(issues.front().severity == dg::Severity::Error);
    CHECK(issues.front().message.find("1930-02-30") != std::string::npos);
}

TEST_CASE("sanity_check: country with default (invalid) id is flagged") {
    GameState s;
    CountryState orphan;   // id stays at CountryId::invalid()
    orphan.id_code = "???";
    s.countries.push_back(orphan);

    const auto issues = dg::sanity_check(s);
    REQUIRE(issues.size() == 1);
    CHECK(issues.front().code == "invalid_country_id");
    CHECK(issues.front().message.find("countries[0]") != std::string::npos);
}

TEST_CASE("sanity_check: duplicate CountryId is detected") {
    // This is the explicit M0.10 acceptance criterion.
    GameState s;
    CountryState a;
    a.id = CountryId{7};
    a.id_code = "AAA";
    CountryState b;
    b.id = CountryId{7};
    b.id_code = "BBB";
    s.countries.push_back(a);
    s.countries.push_back(b);

    const auto issues = dg::sanity_check(s);
    REQUIRE(issues.size() == 1);
    CHECK(issues.front().code == "duplicate_country_id");
    CHECK(issues.front().severity == dg::Severity::Error);
    CHECK(issues.front().message.find("7") != std::string::npos);
}

TEST_CASE("sanity_check: each duplicate id is reported once even if it appears 3x") {
    GameState s;
    for (int i = 0; i < 3; ++i) {
        CountryState c;
        c.id = CountryId{42};
        s.countries.push_back(c);
    }
    const auto issues = dg::sanity_check(s);
    // First occurrence registers "seen", the second triggers
    // "duplicate", the third is suppressed (the same id has already
    // been reported once).
    REQUIRE(issues.size() == 1);
    CHECK(issues.front().code == "duplicate_country_id");
}

TEST_CASE("sanity_check: multiple distinct duplicates each report once") {
    GameState s;
    CountryState a, b, c, d;
    a.id = CountryId{1};
    b.id = CountryId{1};
    c.id = CountryId{2};
    d.id = CountryId{2};
    s.countries.push_back(a);
    s.countries.push_back(b);
    s.countries.push_back(c);
    s.countries.push_back(d);

    const auto issues = dg::sanity_check(s);
    REQUIRE(issues.size() == 2);
    CHECK(issues[0].code == "duplicate_country_id");
    CHECK(issues[1].code == "duplicate_country_id");
}

TEST_CASE("sanity_check does not mutate the input GameState") {
    GameState s;
    s.current_date = GameDate(1930, 2, 30);   // intentionally invalid
    CountryState dup;
    dup.id = CountryId{1};
    s.countries.push_back(dup);
    s.countries.push_back(dup);

    const auto before_countries = s.countries.size();
    const auto before_logs      = s.logs.size();
    const auto before_seed      = s.rng.seed;

    (void) dg::sanity_check(s);

    CHECK(s.countries.size() == before_countries);
    CHECK(s.logs.size()      == before_logs);
    CHECK(s.rng.seed         == before_seed);
    // current_date is still the broken one we set up.
    CHECK_FALSE(s.current_date.is_valid());
}

// ---------------------------------------------------------------------
// M1.14 - per-country snapshot + CSV
// ---------------------------------------------------------------------

namespace {

CountryState m114_country(int idx,
                          const std::string& id_code,
                          double gdp,
                          double last_growth) {
    CountryState c;
    c.id          = CountryId{idx};
    c.id_code     = id_code;
    c.name        = id_code;
    c.gdp                  = gdp;
    c.tax_revenue          = 1.5;
    c.budget_balance       = -4.0;
    c.stability            = 0.55;
    c.legitimacy           = 0.60;
    c.last_gdp_growth_rate = last_growth;
    return c;
}

}  // namespace

TEST_CASE("country_snapshot: reads every documented field verbatim") {
    GameState s;
    s.current_date = GameDate(1930, 6, 1);
    s.countries.push_back(m114_country(0, "GER", 123.5, 0.00350));

    const auto r = dg::country_snapshot(s, CountryId{0});
    REQUIRE(r.ok());
    const auto& row = r.value();
    CHECK(row.date                 == GameDate(1930, 6, 1));
    CHECK(row.id_code              == "GER");
    CHECK(row.gdp                  == doctest::Approx(123.5));
    CHECK(row.tax_revenue          == doctest::Approx(1.5));
    CHECK(row.budget_balance       == doctest::Approx(-4.0));
    CHECK(row.stability            == doctest::Approx(0.55));
    CHECK(row.legitimacy           == doctest::Approx(0.60));
    CHECK(row.last_gdp_growth_rate == doctest::Approx(0.00350));
}

TEST_CASE("country_snapshot: invalid CountryId is rejected with the bad index in the message") {
    GameState s;
    s.countries.push_back(m114_country(0, "GER", 100.0, 0.0));

    const auto r = dg::country_snapshot(s, CountryId{99});
    REQUIRE(r.failed());
    CHECK(r.error().find("CountryId 99") != std::string::npos);
}

TEST_CASE("country_snapshot: default-constructed CountryId is rejected") {
    GameState s;
    s.countries.push_back(m114_country(0, "GER", 100.0, 0.0));
    const auto r = dg::country_snapshot(s, CountryId{});
    REQUIRE(r.failed());
}

TEST_CASE("country_snapshot: empty state.countries -> any index rejected") {
    GameState s;
    const auto r = dg::country_snapshot(s, CountryId{0});
    REQUIRE(r.failed());
}

TEST_CASE("write_country_csv_header: emits the documented column list") {
    std::ostringstream out;
    dg::write_country_csv_header(out);
    CHECK(out.str() ==
          "date,id_code,gdp,tax_revenue,budget_balance,"
          "stability,legitimacy,last_gdp_growth_rate\n");
}

TEST_CASE("write_country_csv_row: emits a well-formed line with all eight fields") {
    GameState s;
    s.current_date = GameDate(1930, 2, 1);
    s.countries.push_back(m114_country(0, "GER", 100.0, 0.00350));
    const auto r = dg::country_snapshot(s, CountryId{0});
    REQUIRE(r.ok());

    std::ostringstream out;
    dg::write_country_csv_row(out, r.value());
    const std::string line = out.str();
    // Date + id_code prefixed verbatim.
    CHECK(line.substr(0, 14) == "1930-02-01,GER");
    // Seven commas separate eight columns.
    int commas = 0;
    for (char c : line) if (c == ',') ++commas;
    CHECK(commas == 7);
    // Ends in newline; no trailing comma.
    CHECK(line.back() == '\n');
    // last_gdp_growth_rate is the last column and renders with full
    // round-trip precision (std::scientific + setprecision(17)).
    CHECK(line.find("3.50") != std::string::npos);  // partial match on 0.0035 mantissa
}

TEST_CASE("write_country_csv_row: negative budget_balance survives the format") {
    GameState s;
    s.current_date = GameDate(1930, 2, 1);
    CountryState c = m114_country(0, "FRA", 75.0, -0.001);
    c.budget_balance = -12.345;
    s.countries.push_back(c);
    const auto r = dg::country_snapshot(s, CountryId{0});
    REQUIRE(r.ok());

    std::ostringstream out;
    dg::write_country_csv_row(out, r.value());
    const std::string line = out.str();
    CHECK(line.find("FRA") != std::string::npos);
    // Negative sign survives the formatter.
    CHECK(line.find("-1.234") != std::string::npos);
}

TEST_CASE("write_country_csv_row: byte-identical for the same row twice") {
    GameState s;
    s.current_date = GameDate(1930, 3, 1);
    s.countries.push_back(m114_country(0, "JPN", 60.0, 0.0042));
    const auto r = dg::country_snapshot(s, CountryId{0});
    REQUIRE(r.ok());

    std::ostringstream a, b;
    dg::write_country_csv_row(a, r.value());
    dg::write_country_csv_row(b, r.value());
    CHECK(a.str() == b.str());
}

TEST_CASE("country_snapshot: does NOT mutate state") {
    GameState s;
    s.current_date  = GameDate(1930, 4, 1);
    s.rng.seed      = 42;
    s.rng.counter   = 7;
    s.countries.push_back(m114_country(0, "GER", 100.0, 0.0035));
    const auto logs_before = s.logs.size();

    REQUIRE(dg::country_snapshot(s, CountryId{0}).ok());

    CHECK(s.current_date == GameDate(1930, 4, 1));
    CHECK(s.rng.seed     == 42u);
    CHECK(s.rng.counter  == 7u);
    CHECK(s.logs.size()  == logs_before);
    // The CountryState fields are unchanged.
    CHECK(s.countries[0].gdp                  == doctest::Approx(100.0));
    CHECK(s.countries[0].last_gdp_growth_rate == doctest::Approx(0.0035));
}

// ---------------------------------------------------------------------
// M1.16 - per-faction snapshot + CSV
// ---------------------------------------------------------------------

namespace {

FactionState m116_faction(int idx,
                          const std::string& id_code,
                          const std::string& country_id_code,
                          const std::string& type,
                          double support,
                          double resources = 1.0) {
    FactionState f;
    f.id              = FactionId{idx};
    f.country         = CountryId{0};
    f.id_code         = id_code;
    f.country_id_code = country_id_code;
    f.name            = id_code;
    f.type            = type;
    f.support         = support;
    f.influence       = 0.50;
    f.radicalism      = 0.30;
    f.loyalty         = 0.55;
    f.resources       = resources;
    return f;
}

}  // namespace

TEST_CASE("faction_snapshot: reads every documented field verbatim") {
    GameState s;
    s.current_date = GameDate(1930, 6, 1);
    s.factions.push_back(m116_faction(0, "GER_military", "GER", "military",
                                      0.45, 1.20));

    const auto r = dg::faction_snapshot(s, FactionId{0});
    REQUIRE(r.ok());
    const auto& row = r.value();
    CHECK(row.date            == GameDate(1930, 6, 1));
    CHECK(row.id_code         == "GER_military");
    CHECK(row.country_id_code == "GER");
    CHECK(row.type            == "military");
    CHECK(row.support    == doctest::Approx(0.45));
    CHECK(row.influence  == doctest::Approx(0.50));
    CHECK(row.radicalism == doctest::Approx(0.30));
    CHECK(row.loyalty    == doctest::Approx(0.55));
    CHECK(row.resources  == doctest::Approx(1.20));
}

TEST_CASE("faction_snapshot: invalid FactionId is rejected with the bad index in the message") {
    GameState s;
    s.factions.push_back(m116_faction(0, "GER_military", "GER", "military", 0.45));

    const auto r = dg::faction_snapshot(s, FactionId{99});
    REQUIRE(r.failed());
    CHECK(r.error().find("FactionId 99") != std::string::npos);
}

TEST_CASE("faction_snapshot: default-constructed FactionId is rejected") {
    GameState s;
    s.factions.push_back(m116_faction(0, "GER_military", "GER", "military", 0.45));
    const auto r = dg::faction_snapshot(s, FactionId{});
    REQUIRE(r.failed());
}

TEST_CASE("faction_snapshot: empty state.factions -> any index rejected") {
    GameState s;
    const auto r = dg::faction_snapshot(s, FactionId{0});
    REQUIRE(r.failed());
}

TEST_CASE("write_faction_csv_header: emits the documented column list") {
    std::ostringstream out;
    dg::write_faction_csv_header(out);
    CHECK(out.str() ==
          "date,id_code,country_id_code,type,support,influence,"
          "radicalism,loyalty,resources\n");
}

TEST_CASE("write_faction_csv_row: emits a well-formed line with all nine fields") {
    GameState s;
    s.current_date = GameDate(1930, 2, 1);
    s.factions.push_back(m116_faction(0, "GER_military", "GER", "military", 0.45));
    const auto r = dg::faction_snapshot(s, FactionId{0});
    REQUIRE(r.ok());

    std::ostringstream out;
    dg::write_faction_csv_row(out, r.value());
    const std::string line = out.str();
    // Date + id_code + country_id_code + type prefixed verbatim.
    CHECK(line.substr(0, 36) == "1930-02-01,GER_military,GER,military");
    // Eight commas separate nine columns.
    int commas = 0;
    for (char c : line) if (c == ',') ++commas;
    CHECK(commas == 8);
    CHECK(line.back() == '\n');
    // Numeric columns use scientific notation; partial mantissa match.
    CHECK(line.find("4.5") != std::string::npos);  // 0.45 support
}

TEST_CASE("write_faction_csv_row: negative resources still survives the format") {
    GameState s;
    s.current_date = GameDate(1930, 2, 1);
    FactionState f = m116_faction(0, "GER_workers", "GER", "workers", 0.50, -2.5);
    s.factions.push_back(f);
    const auto r = dg::faction_snapshot(s, FactionId{0});
    REQUIRE(r.ok());

    std::ostringstream out;
    dg::write_faction_csv_row(out, r.value());
    const std::string line = out.str();
    CHECK(line.find("GER_workers") != std::string::npos);
    CHECK(line.find("-2.5")        != std::string::npos);
}

TEST_CASE("write_faction_csv_row: byte-identical for the same row twice") {
    GameState s;
    s.current_date = GameDate(1930, 3, 1);
    s.factions.push_back(m116_faction(0, "JPN_intelligence", "JPN", "intelligence",
                                      0.40, 0.80));
    const auto r = dg::faction_snapshot(s, FactionId{0});
    REQUIRE(r.ok());

    std::ostringstream a, b;
    dg::write_faction_csv_row(a, r.value());
    dg::write_faction_csv_row(b, r.value());
    CHECK(a.str() == b.str());
}

TEST_CASE("faction_snapshot: does NOT mutate state") {
    GameState s;
    s.current_date  = GameDate(1930, 4, 1);
    s.rng.seed      = 42;
    s.rng.counter   = 7;
    s.factions.push_back(m116_faction(0, "GER_military", "GER", "military", 0.45));
    const auto logs_before = s.logs.size();

    REQUIRE(dg::faction_snapshot(s, FactionId{0}).ok());

    CHECK(s.current_date == GameDate(1930, 4, 1));
    CHECK(s.rng.seed     == 42u);
    CHECK(s.rng.counter  == 7u);
    CHECK(s.logs.size()  == logs_before);
    CHECK(s.factions[0].support   == doctest::Approx(0.45));
    CHECK(s.factions[0].resources == doctest::Approx(1.0));
}

// ---------------------------------------------------------------------
// M2.10 - compare_states
// ---------------------------------------------------------------------

namespace {

CountryState m210_country(int idx, const std::string& id_code, double gdp) {
    CountryState c;
    c.id          = CountryId{idx};
    c.id_code     = id_code;
    c.name        = id_code;
    c.display_name = id_code;
    c.gdp                       = gdp;
    c.tax_revenue               = 1.0;
    c.budget_balance            = 0.0;
    c.legal_tax_burden          = 0.20;
    c.fiscal_capacity           = 0.50;
    c.administrative_efficiency = 0.50;
    c.central_control           = 0.50;
    c.corruption                = 0.20;
    c.stability                 = 0.55;
    c.legitimacy                = 0.55;
    c.military_power            = 0.50;
    c.threat_perception         = 0.30;
    c.last_gdp_growth_rate      = 0.0;
    c.budget.administration     = 0.20;
    c.budget.military           = 0.30;
    c.budget.education          = 0.10;
    c.budget.welfare            = 0.10;
    c.budget.intelligence       = 0.05;
    c.budget.infrastructure     = 0.15;
    c.budget.industry           = 0.10;
    return c;
}

}  // namespace

TEST_CASE("compare_states: two empty GameStates match") {
    GameState a;
    GameState b;
    const auto m = dg::compare_states(a, b);
    CHECK(m.empty());
}

TEST_CASE("compare_states: identical seeded states match") {
    GameState a;
    a.current_date    = GameDate(1930, 6, 1);
    a.player_country  = CountryId{0};
    a.countries.push_back(m210_country(0, "GER", 100.0));

    GameState b = a;   // deep copy
    const auto m = dg::compare_states(a, b);
    CHECK(m.empty());
}

TEST_CASE("compare_states: different current_date is reported with path 'current_date'") {
    GameState a;
    GameState b;
    b.current_date = GameDate(1945, 5, 8);
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 1u);
    CHECK(m[0].field_path == "current_date");
    CHECK(m[0].detail.find("1930-01-01") != std::string::npos);
    CHECK(m[0].detail.find("1945-05-08") != std::string::npos);
}

TEST_CASE("compare_states: different player_country is reported") {
    GameState a;
    GameState b;
    b.player_country = CountryId{2};
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 1u);
    CHECK(m[0].field_path == "player_country");
    CHECK(m[0].detail.find("-1") != std::string::npos);
    CHECK(m[0].detail.find("2")  != std::string::npos);
}

TEST_CASE("compare_states: different country count reported on countries.size()") {
    GameState a;
    GameState b;
    b.countries.push_back(m210_country(0, "GER", 100.0));
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 1u);
    CHECK(m[0].field_path == "countries.size()");
    CHECK(m[0].detail == "0 != 1");
}

TEST_CASE("compare_states: gdp diff on country[0] reports correct path") {
    GameState a;
    GameState b;
    a.countries.push_back(m210_country(0, "GER", 100.0));
    b.countries.push_back(m210_country(0, "GER", 105.5));
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 1u);
    CHECK(m[0].field_path == "countries[0].gdp");
    CHECK(m[0].detail.find("1.000") != std::string::npos);  // 1.0e+02
    CHECK(m[0].detail.find("1.055") != std::string::npos);  // 1.055e+02
}

TEST_CASE("compare_states: tolerance — diff within tolerance is silent") {
    GameState a;
    GameState b;
    a.countries.push_back(m210_country(0, "GER", 100.0));
    auto& bc = b.countries.emplace_back(m210_country(0, "GER", 100.0));
    bc.gdp = 100.0 + 1e-12;   // well below default 1e-9 tolerance
    const auto m = dg::compare_states(a, b);
    CHECK(m.empty());
}

TEST_CASE("compare_states: tolerance — diff outside tolerance is reported") {
    GameState a;
    GameState b;
    a.countries.push_back(m210_country(0, "GER", 100.0));
    auto& bc = b.countries.emplace_back(m210_country(0, "GER", 100.0));
    bc.gdp = 100.0 + 1e-6;   // above default 1e-9 tolerance
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 1u);
    CHECK(m[0].field_path == "countries[0].gdp");
}

TEST_CASE("compare_states: active_policies size mismatch caught with array path") {
    GameState a;
    GameState b;
    a.countries.push_back(m210_country(0, "GER", 100.0));
    b.countries.push_back(m210_country(0, "GER", 100.0));
    b.countries[0].active_policies.push_back(
        {"raise_taxes", GameDate(1930, 3, 2)});
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 1u);
    CHECK(m[0].field_path == "countries[0].active_policies.size()");
    CHECK(m[0].detail == "0 != 1");
}

TEST_CASE("compare_states: applied_commands size mismatch caught") {
    GameState a;
    GameState b;
    leviathan::core::AppliedPlayerCommand ac;
    ac.applied_on = GameDate(1930, 1, 1);
    ac.command.kind = leviathan::core::PlayerCommandKind::EnactPolicy;
    ac.command.policy_id_code = "x";
    b.applied_commands.push_back(ac);
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 1u);
    CHECK(m[0].field_path == "applied_commands.size()");
}

TEST_CASE("compare_states: multiple mismatches collected in canonical order") {
    GameState a;
    GameState b;
    // current_date diff
    b.current_date = GameDate(1931, 1, 1);
    // player_country diff
    b.player_country = CountryId{0};
    // country gdp diff
    a.countries.push_back(m210_country(0, "GER", 100.0));
    b.countries.push_back(m210_country(0, "GER", 200.0));
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 3u);
    CHECK(m[0].field_path == "current_date");
    CHECK(m[1].field_path == "player_country");
    CHECK(m[2].field_path == "countries[0].gdp");
}

TEST_CASE("compare_states: respects custom CompareOptions tolerance") {
    GameState a;
    GameState b;
    a.countries.push_back(m210_country(0, "GER", 100.0));
    auto& bc = b.countries.emplace_back(m210_country(0, "GER", 100.0));
    bc.gdp = 100.0 + 1e-3;   // above 1e-9 default but below 1e-2 custom
    dg::CompareOptions opts;
    opts.double_tolerance = 1e-2;
    const auto m = dg::compare_states(a, b, opts);
    CHECK(m.empty());
}

TEST_CASE("compare_states: M2.16 government_authority differences are reported per sub-field") {
    GameState a;
    GameState b;
    a.countries.push_back(m210_country(0, "GER", 100.0));
    b.countries.push_back(m210_country(0, "GER", 100.0));
    b.countries[0].government_authority.bureaucratic_compliance = 0.7;  // a stays 0.5
    b.countries[0].government_authority.media_control           = 0.3;  // a stays 0.5
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 2u);
    CHECK(m[0].field_path == "countries[0].government_authority.bureaucratic_compliance");
    CHECK(m[1].field_path == "countries[0].government_authority.media_control");
}

TEST_CASE("compare_states: M3.1 interest_groups size mismatch reported") {
    GameState a;
    GameState b;
    leviathan::core::InterestGroupState g;
    g.id_code = "ger_bureaucracy";
    g.name    = "German Bureaucracy";
    g.kind    = leviathan::core::InterestGroupKind::Bureaucracy;
    g.country = CountryId{0};
    b.interest_groups.push_back(g);
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 1u);
    CHECK(m[0].field_path == "interest_groups.size()");
    CHECK(m[0].detail     == "0 != 1");
}

TEST_CASE("compare_states: M3.1 interest_groups per-field differences are reported per path") {
    GameState a;
    GameState b;
    leviathan::core::InterestGroupState g;
    g.id_code   = "ger_bureaucracy";
    g.name      = "German Bureaucracy";
    g.kind      = leviathan::core::InterestGroupKind::Bureaucracy;
    g.country   = CountryId{0};
    g.influence = 0.5;
    g.loyalty   = 0.5;
    a.interest_groups.push_back(g);
    leviathan::core::InterestGroupState gb = g;
    gb.influence  = 0.7;
    gb.radicalism = 0.2;
    b.interest_groups.push_back(gb);
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 2u);
    CHECK(m[0].field_path == "interest_groups[0].influence");
    CHECK(m[1].field_path == "interest_groups[0].radicalism");
}

// ---------------------------------------------------------------------
// M4.1 - provinces compare_states walk
// ---------------------------------------------------------------------

TEST_CASE("compare_states: M4.1 identical provinces produce no mismatch") {
    GameState a;
    GameState b;
    leviathan::core::ProvinceNode p;
    p.id_code = "berlin";
    p.name    = "Berlin";
    p.owner   = CountryId{0};
    p.x       = 0.5;
    p.y       = 0.4;
    a.provinces.push_back(p);
    b.provinces.push_back(p);
    const auto m = dg::compare_states(a, b);
    CHECK(m.empty());
}

TEST_CASE("compare_states: M4.1 provinces size mismatch reported") {
    GameState a;
    GameState b;
    leviathan::core::ProvinceNode p;
    p.id_code = "berlin";
    p.name    = "Berlin";
    p.owner   = CountryId{0};
    b.provinces.push_back(p);
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 1u);
    CHECK(m[0].field_path == "provinces.size()");
    CHECK(m[0].detail     == "0 != 1");
}

TEST_CASE("compare_states: M4.1 provinces per-field differences are reported per path") {
    GameState a;
    GameState b;
    leviathan::core::ProvinceNode pa;
    pa.id_code = "berlin";
    pa.name    = "Berlin";
    pa.owner   = CountryId{0};
    pa.x       = 0.5;
    pa.y       = 0.4;
    a.provinces.push_back(pa);
    leviathan::core::ProvinceNode pb = pa;
    pb.name  = "Berlin-West";
    pb.owner = CountryId{1};
    pb.x     = 0.51;
    b.provinces.push_back(pb);
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 3u);
    CHECK(m[0].field_path == "provinces[0].name");
    CHECK(m[1].field_path == "provinces[0].owner");
    CHECK(m[2].field_path == "provinces[0].x");
}

// ---------------------------------------------------------------------
// M5.1 - EventDefinition compare_states walk
// ---------------------------------------------------------------------

namespace {

leviathan::core::EventDefinition m51_event(
        const std::string& id_code,
        const std::string& name,
        const std::string& description,
        const std::string& trig_target = "country.stability",
        const std::string& trig_op     = "lt",
        double             trig_value  = 0.30,
        const std::string& eff_target  = "country.stability",
        const std::string& eff_op      = "add",
        double             eff_value   = -0.02) {
    leviathan::core::EventDefinition ev;
    ev.id_code     = id_code;
    ev.name        = name;
    ev.description = description;
    leviathan::core::EventTrigger t;
    t.target = trig_target;
    t.op     = trig_op;
    t.value  = trig_value;
    ev.triggers.push_back(t);
    leviathan::core::PolicyEffect e;
    e.target = eff_target;
    e.op     = eff_op;
    e.value  = eff_value;
    ev.effects.push_back(e);
    return ev;
}

}  // namespace

TEST_CASE("compare_states: M5.1 identical events produce no mismatch") {
    GameState a;
    GameState b;
    a.events.push_back(m51_event("x", "X", "desc"));
    b.events.push_back(m51_event("x", "X", "desc"));
    const auto m = dg::compare_states(a, b);
    CHECK(m.empty());
}

TEST_CASE("compare_states: M5.1 events size mismatch reported") {
    GameState a;
    GameState b;
    a.events.push_back(m51_event("x", "X", ""));
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 1u);
    CHECK(m[0].field_path == "events.size()");
}

TEST_CASE("compare_states: M5.1 events per-field differences reported per path") {
    GameState a;
    GameState b;
    a.events.push_back(m51_event("x", "X", "desc-A"));
    leviathan::core::EventDefinition eb = m51_event("x", "X-renamed", "desc-B");
    eb.triggers[0].target = "country.legitimacy";
    eb.triggers[0].op     = "gt";
    eb.triggers[0].value  = 0.50;
    eb.effects[0].target  = "country.legitimacy";
    eb.effects[0].op      = "set";
    eb.effects[0].value   = 0.10;
    b.events.push_back(std::move(eb));
    const auto m = dg::compare_states(a, b);
    // Expect: name, description, triggers[0].target, .op, .value,
    // effects[0].target, .op, .value
    REQUIRE(m.size() == 8u);
    CHECK(m[0].field_path == "events[0].name");
    CHECK(m[1].field_path == "events[0].description");
    CHECK(m[2].field_path == "events[0].triggers[0].target");
    CHECK(m[3].field_path == "events[0].triggers[0].op");
    CHECK(m[4].field_path == "events[0].triggers[0].value");
    CHECK(m[5].field_path == "events[0].effects[0].target");
    CHECK(m[6].field_path == "events[0].effects[0].op");
    CHECK(m[7].field_path == "events[0].effects[0].value");
}

TEST_CASE("compare_states: M5.1 events triggers.size mismatch reported (skips per-trigger walk)") {
    GameState a;
    GameState b;
    a.events.push_back(m51_event("x", "X", "d"));
    auto eb = m51_event("x", "X", "d");
    // Append a second trigger to b.
    leviathan::core::EventTrigger t2;
    t2.target = "interest_group.loyalty";
    t2.op     = "gte";
    t2.value  = 0.20;
    eb.triggers.push_back(t2);
    b.events.push_back(std::move(eb));
    const auto m = dg::compare_states(a, b);
    REQUIRE(m.size() == 1u);
    CHECK(m[0].field_path == "events[0].triggers.size()");
}

// ---------------------------------------------------------------------
// M3.5 - per-interest-group snapshot + CSV + csv_escape
// ---------------------------------------------------------------------

namespace {

using leviathan::core::InterestGroupKind;
using leviathan::core::InterestGroupState;

CountryState m35_country(int idx, const std::string& id_code) {
    CountryState c;
    c.id      = CountryId{idx};
    c.id_code = id_code;
    c.name    = id_code;
    return c;
}

InterestGroupState m35_group(const std::string& id_code,
                             const std::string& name,
                             InterestGroupKind kind,
                             int country_value,
                             double influence  = 0.4,
                             double loyalty    = 0.6,
                             double radicalism = 0.2) {
    InterestGroupState g;
    g.id_code    = id_code;
    g.name       = name;
    g.kind       = kind;
    g.country    = CountryId{country_value};
    g.influence  = influence;
    g.loyalty    = loyalty;
    g.radicalism = radicalism;
    return g;
}

}  // namespace

TEST_CASE("csv_escape: plain field passes through unchanged") {
    CHECK(dg::csv_escape("Bureaucracy")  == "Bureaucracy");
    CHECK(dg::csv_escape("GER")          == "GER");
    CHECK(dg::csv_escape("ger_workers")  == "ger_workers");
    CHECK(dg::csv_escape("")             == "");
}

TEST_CASE("csv_escape: comma triggers quoting") {
    CHECK(dg::csv_escape("Hello, World") == "\"Hello, World\"");
}

TEST_CASE("csv_escape: embedded double quote is doubled and the field is wrapped") {
    CHECK(dg::csv_escape("she said \"hi\"") ==
          "\"she said \"\"hi\"\"\"");
}

TEST_CASE("csv_escape: newline / carriage return trigger quoting") {
    CHECK(dg::csv_escape("a\nb")   == "\"a\nb\"");
    CHECK(dg::csv_escape("a\r\nb") == "\"a\r\nb\"");
}

TEST_CASE("interest_group_snapshot: reads every documented field verbatim") {
    GameState s;
    s.current_date = GameDate(1930, 6, 1);
    s.countries.push_back(m35_country(0, "GER"));
    s.interest_groups.push_back(m35_group(
        "ger_bureaucracy", "German Bureaucracy",
        InterestGroupKind::Bureaucracy, 0,
        /*influence*/ 0.30, /*loyalty*/ 0.55, /*radicalism*/ 0.15));

    const auto r = dg::interest_group_snapshot(s, 0u);
    REQUIRE(r.ok());
    const auto& row = r.value();
    CHECK(row.date            == GameDate(1930, 6, 1));
    CHECK(row.id_code         == "ger_bureaucracy");
    CHECK(row.name            == "German Bureaucracy");
    CHECK(row.kind            == "Bureaucracy");
    CHECK(row.country_id      == 0);
    CHECK(row.country_id_code == "GER");
    CHECK(row.influence       == doctest::Approx(0.30));
    CHECK(row.loyalty         == doctest::Approx(0.55));
    CHECK(row.radicalism      == doctest::Approx(0.15));
}

TEST_CASE("interest_group_snapshot: index past the end is rejected with the bad index in the message") {
    GameState s;
    s.countries.push_back(m35_country(0, "GER"));
    s.interest_groups.push_back(m35_group(
        "ger_workers", "German Workers", InterestGroupKind::Workers, 0));

    const auto r = dg::interest_group_snapshot(s, 99u);
    REQUIRE(r.failed());
    CHECK(r.error().find("99") != std::string::npos);
    CHECK(r.error().find("interest_groups") != std::string::npos);
}

TEST_CASE("interest_group_snapshot: empty interest_groups -> any index rejected") {
    GameState s;
    s.countries.push_back(m35_country(0, "GER"));
    const auto r = dg::interest_group_snapshot(s, 0u);
    REQUIRE(r.failed());
}

TEST_CASE("interest_group_snapshot: invalid country reference is rejected loudly") {
    GameState s;
    s.countries.push_back(m35_country(0, "GER"));
    // group.country points at index 5 — no such country.
    s.interest_groups.push_back(m35_group(
        "phantom", "Phantom Group", InterestGroupKind::LocalElites, 5));

    const auto r = dg::interest_group_snapshot(s, 0u);
    REQUIRE(r.failed());
    CHECK(r.error().find("phantom") != std::string::npos);
    CHECK(r.error().find("invalid country") != std::string::npos);
}

TEST_CASE("interest_group_snapshot: default-CountryId{} (-1) is rejected") {
    GameState s;
    s.countries.push_back(m35_country(0, "GER"));
    InterestGroupState g;
    g.id_code = "orphan";
    g.name    = "Orphan";
    g.kind    = InterestGroupKind::Religious;
    // g.country left as CountryId::invalid() (-1).
    s.interest_groups.push_back(std::move(g));

    const auto r = dg::interest_group_snapshot(s, 0u);
    REQUIRE(r.failed());
    CHECK(r.error().find("invalid country") != std::string::npos);
}

TEST_CASE("interest_group_snapshot: does NOT mutate state") {
    GameState s;
    s.current_date = GameDate(1930, 4, 1);
    s.rng.seed     = 42;
    s.rng.counter  = 7;
    s.countries.push_back(m35_country(0, "GER"));
    s.interest_groups.push_back(m35_group(
        "ger_media", "GER Media", InterestGroupKind::Media, 0));
    const auto logs_before = s.logs.size();

    REQUIRE(dg::interest_group_snapshot(s, 0u).ok());

    CHECK(s.current_date == GameDate(1930, 4, 1));
    CHECK(s.rng.seed     == 42u);
    CHECK(s.rng.counter  == 7u);
    CHECK(s.logs.size()  == logs_before);
    CHECK(s.interest_groups[0].influence == doctest::Approx(0.4));
}

TEST_CASE("write_interest_group_csv_header: emits the documented column list") {
    std::ostringstream out;
    dg::write_interest_group_csv_header(out);
    CHECK(out.str() ==
          "date,id_code,name,kind,country_id,country_id_code,"
          "influence,loyalty,radicalism\n");
}

TEST_CASE("write_interest_group_csv_row: emits a well-formed line with all nine fields") {
    GameState s;
    s.current_date = GameDate(1930, 2, 1);
    s.countries.push_back(m35_country(0, "GER"));
    s.interest_groups.push_back(m35_group(
        "ger_bureaucracy", "German Bureaucracy",
        InterestGroupKind::Bureaucracy, 0,
        /*influence*/ 0.40, /*loyalty*/ 0.60, /*radicalism*/ 0.10));
    const auto r = dg::interest_group_snapshot(s, 0u);
    REQUIRE(r.ok());

    std::ostringstream out;
    dg::write_interest_group_csv_row(out, r.value());
    const std::string line = out.str();
    // First six columns appear in canonical order.
    const std::string expected_prefix =
        "1930-02-01,ger_bureaucracy,German Bureaucracy,Bureaucracy,0,GER,";
    CHECK(line.substr(0, expected_prefix.size()) == expected_prefix);
    // Eight commas separate nine columns.
    int commas = 0;
    for (char c : line) if (c == ',') ++commas;
    CHECK(commas == 8);
    CHECK(line.back() == '\n');
    // Numeric columns use scientific notation.
    CHECK(line.find("e-") != std::string::npos);
}

TEST_CASE("write_interest_group_csv_row: name containing comma is quoted") {
    GameState s;
    s.current_date = GameDate(1930, 5, 1);
    s.countries.push_back(m35_country(0, "GER"));
    s.interest_groups.push_back(m35_group(
        "ger_workers", "Workers, Engineers, and Allies",
        InterestGroupKind::Workers, 0));
    const auto r = dg::interest_group_snapshot(s, 0u);
    REQUIRE(r.ok());

    std::ostringstream out;
    dg::write_interest_group_csv_row(out, r.value());
    const std::string line = out.str();
    // The quoted name should appear unbroken in the line.
    CHECK(line.find("\"Workers, Engineers, and Allies\"") != std::string::npos);
}

TEST_CASE("write_interest_group_csv_row: name containing double quote is escaped per RFC 4180") {
    GameState s;
    s.current_date = GameDate(1930, 5, 1);
    s.countries.push_back(m35_country(0, "GER"));
    s.interest_groups.push_back(m35_group(
        "rebels", "The \"Iron\" Workers",
        InterestGroupKind::Workers, 0));
    const auto r = dg::interest_group_snapshot(s, 0u);
    REQUIRE(r.ok());

    std::ostringstream out;
    dg::write_interest_group_csv_row(out, r.value());
    CHECK(out.str().find("\"The \"\"Iron\"\" Workers\"") != std::string::npos);
}

TEST_CASE("write_interest_group_csv_row: byte-identical for the same row twice") {
    GameState s;
    s.current_date = GameDate(1930, 3, 1);
    s.countries.push_back(m35_country(0, "JPN"));
    s.interest_groups.push_back(m35_group(
        "jpn_technocrats", "JPN Technocrats", InterestGroupKind::Technocrats, 0,
        0.25, 0.70, 0.05));
    const auto r = dg::interest_group_snapshot(s, 0u);
    REQUIRE(r.ok());

    std::ostringstream a, b;
    dg::write_interest_group_csv_row(a, r.value());
    dg::write_interest_group_csv_row(b, r.value());
    CHECK(a.str() == b.str());
}

TEST_CASE("write_interest_group_csv_row: every InterestGroupKind variant round-trips through kind column") {
    // Spot-check that the shared kind ↔ string helper covers every
    // variant the diagnostics writer might emit. A new variant
    // without a string mapping would surface here as
    // "UnknownInterestGroupKind" or similar.
    const std::vector<std::pair<InterestGroupKind, std::string>> cases = {
        {InterestGroupKind::Bureaucracy, "Bureaucracy"},
        {InterestGroupKind::Military,    "Military"},
        {InterestGroupKind::Workers,     "Workers"},
        {InterestGroupKind::Farmers,     "Farmers"},
        {InterestGroupKind::Religious,   "Religious"},
        {InterestGroupKind::Media,       "Media"},
        {InterestGroupKind::Students,    "Students"},
        {InterestGroupKind::LocalElites, "LocalElites"},
        {InterestGroupKind::Business,    "Business"},
        {InterestGroupKind::Technocrats, "Technocrats"},
    };
    for (const auto& [k, label] : cases) {
        GameState s;
        s.countries.push_back(m35_country(0, "ZZZ"));
        s.interest_groups.push_back(m35_group("g", "G", k, 0));
        const auto r = dg::interest_group_snapshot(s, 0u);
        REQUIRE(r.ok());
        CHECK(r.value().kind == label);
    }
}

// ---------------------------------------------------------------------
// M3.6 - per-system formula-trace CSV writers
// ---------------------------------------------------------------------

namespace ig = leviathan::systems::interest_group;

TEST_CASE("write_country_feedback_csv_header: emits the documented column list") {
    std::ostringstream out;
    dg::write_country_feedback_csv_header(out);
    CHECK(out.str() ==
          "date,country_id,country_id_code,matched_groups,"
          "weight_sum,weighted_radicalism,target_stability,"
          "stability_before,stability_after,stability_delta\n");
}

TEST_CASE("write_country_feedback_csv_row: emits a well-formed line with ten fields") {
    ig::CountryFeedbackTraceRow row;
    row.date                = GameDate(1930, 2, 1);
    row.country_id          = 0;
    row.country_id_code     = "GER";
    row.matched_groups      = 2;
    row.weight_sum          = 1.0;
    row.weighted_radicalism = 0.36;
    row.target_stability    = 0.64;
    row.stability_before    = 0.5;
    row.stability_after     = 0.5028;
    row.stability_delta     = 0.0028;

    std::ostringstream out;
    dg::write_country_feedback_csv_row(out, row);
    const std::string line = out.str();
    // First four columns prefixed verbatim.
    const std::string expected_prefix = "1930-02-01,0,GER,2,";
    CHECK(line.substr(0, expected_prefix.size()) == expected_prefix);
    int commas = 0;
    for (char c : line) if (c == ',') ++commas;
    CHECK(commas == 9);  // 10 columns → 9 separators
    CHECK(line.back() == '\n');
    // Scientific notation present.
    CHECK(line.find("e-") != std::string::npos);
}

TEST_CASE("write_country_feedback_csv_row: country_id_code with comma is quoted") {
    ig::CountryFeedbackTraceRow row;
    row.date            = GameDate(1930, 2, 1);
    row.country_id      = 0;
    row.country_id_code = "X, Y";
    row.matched_groups  = 1;
    row.weight_sum      = 0.5;
    std::ostringstream out;
    dg::write_country_feedback_csv_row(out, row);
    CHECK(out.str().find("\"X, Y\"") != std::string::npos);
}

TEST_CASE("write_country_feedback_csv_row: byte-identical for the same row twice") {
    ig::CountryFeedbackTraceRow row;
    row.date                = GameDate(1930, 3, 1);
    row.country_id          = 1;
    row.country_id_code     = "JPN";
    row.matched_groups      = 3;
    row.weight_sum          = 0.9;
    row.weighted_radicalism = 0.41;
    row.target_stability    = 0.59;
    row.stability_before    = 0.5;
    row.stability_after     = 0.5018;
    row.stability_delta     = 0.0018;
    std::ostringstream a, b;
    dg::write_country_feedback_csv_row(a, row);
    dg::write_country_feedback_csv_row(b, row);
    CHECK(a.str() == b.str());
}

TEST_CASE("write_authority_pressure_csv_header: emits the documented column list") {
    std::ostringstream out;
    dg::write_authority_pressure_csv_header(out);
    CHECK(out.str() ==
          "date,country_id,country_id_code,matched_groups,"
          "weight_sum,weighted_bureaucracy_loyalty,"
          "target_bureaucratic_compliance,"
          "bureaucratic_compliance_before,"
          "bureaucratic_compliance_after,"
          "bureaucratic_compliance_delta\n");
}

TEST_CASE("write_authority_pressure_csv_row: emits a well-formed line with ten fields") {
    ig::AuthorityPressureTraceRow row;
    row.date                              = GameDate(1930, 2, 1);
    row.country_id                        = 0;
    row.country_id_code                   = "GER";
    row.matched_groups                    = 1;
    row.weight_sum                        = 0.6;
    row.weighted_bureaucracy_loyalty      = 0.8;
    row.target_bureaucratic_compliance    = 0.8;
    row.bureaucratic_compliance_before    = 0.4;
    row.bureaucratic_compliance_after     = 0.404;
    row.bureaucratic_compliance_delta     = 0.004;

    std::ostringstream out;
    dg::write_authority_pressure_csv_row(out, row);
    const std::string line = out.str();
    const std::string expected_prefix = "1930-02-01,0,GER,1,";
    CHECK(line.substr(0, expected_prefix.size()) == expected_prefix);
    int commas = 0;
    for (char c : line) if (c == ',') ++commas;
    CHECK(commas == 9);
    CHECK(line.back() == '\n');
    CHECK(line.find("e-") != std::string::npos);
}

TEST_CASE("write_authority_pressure_csv_row: byte-identical for the same row twice") {
    ig::AuthorityPressureTraceRow row;
    row.date                              = GameDate(1930, 5, 1);
    row.country_id                        = 2;
    row.country_id_code                   = "FRA";
    row.matched_groups                    = 2;
    row.weight_sum                        = 1.2;
    row.weighted_bureaucracy_loyalty      = 0.55;
    row.target_bureaucratic_compliance    = 0.55;
    row.bureaucratic_compliance_before    = 0.5;
    row.bureaucratic_compliance_after     = 0.5005;
    row.bureaucratic_compliance_delta     = 0.0005;
    std::ostringstream a, b;
    dg::write_authority_pressure_csv_row(a, row);
    dg::write_authority_pressure_csv_row(b, row);
    CHECK(a.str() == b.str());
}
