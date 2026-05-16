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
