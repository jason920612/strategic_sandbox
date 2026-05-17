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
