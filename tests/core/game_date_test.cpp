#include <doctest/doctest.h>

#include <string>

#include "leviathan/core/game_date.hpp"

using leviathan::core::GameDate;

TEST_CASE("GameDate default-constructs to 1930-01-01") {
    GameDate d;
    CHECK(d.year()  == 1930);
    CHECK(d.month() == 1);
    CHECK(d.day()   == 1);
    CHECK(d.is_valid());
    CHECK(d.to_string() == "1930-01-01");
}

TEST_CASE("is_leap_year matches the Gregorian rule") {
    CHECK(GameDate::is_leap_year(2000));   // century div by 400
    CHECK_FALSE(GameDate::is_leap_year(1900));  // century not div by 400
    CHECK(GameDate::is_leap_year(1996));   // ordinary leap
    CHECK_FALSE(GameDate::is_leap_year(1999));
    CHECK(GameDate::is_leap_year(1932));
    CHECK_FALSE(GameDate::is_leap_year(1933));
}

TEST_CASE("days_in_month uses leap year for February") {
    CHECK(GameDate::days_in_month(1930, 1)  == 31);
    CHECK(GameDate::days_in_month(1930, 2)  == 28);
    CHECK(GameDate::days_in_month(1932, 2)  == 29);  // leap
    CHECK(GameDate::days_in_month(1900, 2)  == 28);  // century not div 400
    CHECK(GameDate::days_in_month(2000, 2)  == 29);  // century div 400
    CHECK(GameDate::days_in_month(1930, 4)  == 30);
    CHECK(GameDate::days_in_month(1930, 12) == 31);
    CHECK(GameDate::days_in_month(1930, 0)  == 0);
    CHECK(GameDate::days_in_month(1930, 13) == 0);
}

TEST_CASE("is_valid rejects malformed dates") {
    CHECK_FALSE(GameDate(1930, 0,  1).is_valid());
    CHECK_FALSE(GameDate(1930, 13, 1).is_valid());
    CHECK_FALSE(GameDate(1930, 2,  30).is_valid());
    CHECK_FALSE(GameDate(1930, 2,  29).is_valid());  // 1930 is not leap
    CHECK(GameDate(1932, 2, 29).is_valid());
    CHECK_FALSE(GameDate(0,   1,  1).is_valid());
    CHECK_FALSE(GameDate(-5,  1,  1).is_valid());
}

TEST_CASE("advance_one_day rolls inside a month") {
    GameDate d{1930, 1, 1};
    d.advance_one_day();
    CHECK(d == GameDate{1930, 1, 2});
}

TEST_CASE("advance_one_day rolls over end of January") {
    GameDate d{1930, 1, 31};
    d.advance_one_day();
    CHECK(d == GameDate{1930, 2, 1});
}

TEST_CASE("advance_one_day rolls over end of February (non-leap)") {
    GameDate d{1930, 2, 28};
    d.advance_one_day();
    CHECK(d == GameDate{1930, 3, 1});
}

TEST_CASE("advance_one_day handles leap-day correctly") {
    GameDate d{1932, 2, 28};
    d.advance_one_day();
    CHECK(d == GameDate{1932, 2, 29});
    d.advance_one_day();
    CHECK(d == GameDate{1932, 3, 1});
}

TEST_CASE("advance_one_day rolls over end of year") {
    GameDate d{1930, 12, 31};
    d.advance_one_day();
    CHECK(d == GameDate{1931, 1, 1});
}

TEST_CASE("advance_days(0) is a no-op") {
    GameDate d{1930, 6, 15};
    d.advance_days(0);
    CHECK(d == GameDate{1930, 6, 15});
}

TEST_CASE("advance_days(365) on a non-leap year hits the same date next year") {
    // 1930 is not a leap year -> +365 days lands on 1931-01-01.
    GameDate d{1930, 1, 1};
    d.advance_days(365);
    CHECK(d == GameDate{1931, 1, 1});
}

TEST_CASE("advance_days(366) on a leap year hits the same date next year") {
    // 1932 is a leap year -> +366 days lands on 1933-01-01.
    GameDate d{1932, 1, 1};
    d.advance_days(366);
    CHECK(d == GameDate{1933, 1, 1});
}

TEST_CASE("advance_days(365) on a leap year is one day short") {
    GameDate d{1932, 1, 1};
    d.advance_days(365);
    CHECK(d == GameDate{1932, 12, 31});
}

TEST_CASE("advance_days crosses the 1999->2000 boundary") {
    // Smoke-tests the 2000 leap year (century divisible by 400).
    GameDate d{1999, 12, 31};
    d.advance_days(60);
    CHECK(d == GameDate{2000, 2, 29});
    d.advance_days(1);
    CHECK(d == GameDate{2000, 3, 1});
}

TEST_CASE("to_string is zero-padded ISO-8601") {
    CHECK(GameDate(1930, 1, 1).to_string()   == "1930-01-01");
    CHECK(GameDate(1932, 2, 29).to_string()  == "1932-02-29");
    CHECK(GameDate(2000, 12, 31).to_string() == "2000-12-31");
}

TEST_CASE("parse accepts canonical ISO-8601") {
    auto r = GameDate::parse("1930-01-01");
    REQUIRE(r.ok());
    CHECK(r.value() == GameDate(1930, 1, 1));
}

TEST_CASE("parse tolerates surrounding whitespace") {
    auto r = GameDate::parse("  1932-02-29\n");
    REQUIRE(r.ok());
    CHECK(r.value() == GameDate(1932, 2, 29));
}

TEST_CASE("parse rejects wrong shapes") {
    CHECK(GameDate::parse("").failed());
    CHECK(GameDate::parse("1930/01/01").failed());
    CHECK(GameDate::parse("1930-1-1").failed());
    CHECK(GameDate::parse("01-01-1930").failed());
    CHECK(GameDate::parse("1930-01-01extra").failed());
}

TEST_CASE("parse rejects non-numeric components") {
    CHECK(GameDate::parse("19a0-01-01").failed());
    CHECK(GameDate::parse("1930-XX-01").failed());
}

TEST_CASE("parse rejects calendar-invalid dates") {
    CHECK(GameDate::parse("1930-02-30").failed());
    CHECK(GameDate::parse("1930-02-29").failed());  // non-leap
    CHECK(GameDate::parse("1930-13-01").failed());
    CHECK(GameDate::parse("1930-00-01").failed());
}

TEST_CASE("comparison operators are total over (y, m, d)") {
    CHECK(GameDate(1930, 1, 1) <  GameDate(1930, 1, 2));
    CHECK(GameDate(1930, 1, 2) <  GameDate(1930, 2, 1));
    CHECK(GameDate(1930, 2, 1) <  GameDate(1931, 1, 1));
    CHECK(GameDate(1930, 1, 1) <= GameDate(1930, 1, 1));
    CHECK(GameDate(1930, 1, 1) >= GameDate(1930, 1, 1));
    CHECK(GameDate(1931, 1, 1) >  GameDate(1930, 12, 31));
    CHECK(GameDate(1930, 1, 1) == GameDate(1930, 1, 1));
    CHECK(GameDate(1930, 1, 1) != GameDate(1930, 1, 2));
}
