// In-game date for the simulation core.
//
// Calendar: proleptic Gregorian with real leap years (year divisible by 4
// except centuries not divisible by 400). This matches the real-world
// 1930-2000 timeline that the prototype targets, including 1932, 1936, ...
// 1996, and the 2000 leap year edge case.
//
// See docs/m0-2-calendar-and-types.md for the rationale and what is
// explicitly NOT supported (BC dates, sub-day resolution, time zones).

#ifndef LEVIATHAN_CORE_GAME_DATE_HPP
#define LEVIATHAN_CORE_GAME_DATE_HPP

#include <string>
#include <string_view>

#include "leviathan/core/result.hpp"

namespace leviathan::core {

class GameDate {
public:
    // Default-constructs to 1930-01-01 (prototype start year).
    constexpr GameDate() noexcept = default;
    constexpr GameDate(int year, int month, int day) noexcept
        : year_(year), month_(month), day_(day) {}

    constexpr int year() const noexcept { return year_; }
    constexpr int month() const noexcept { return month_; }
    constexpr int day() const noexcept { return day_; }

    // True iff (year, month, day) names a real Gregorian calendar date
    // with year >= 1. Years <= 0 are rejected so the prototype does not
    // accidentally roll a date into BC territory.
    constexpr bool is_valid() const noexcept {
        if (year_ < 1) return false;
        if (month_ < 1 || month_ > 12) return false;
        if (day_ < 1) return false;
        return day_ <= days_in_month(year_, month_);
    }

    // Advances by exactly one day, rolling month and year forward as
    // needed. Precondition: is_valid().
    void advance_one_day() noexcept;

    // Advances by `days` days. `days` must be >= 0; negative deltas are
    // not supported in M0.2 (simulation time only moves forward).
    // Precondition: is_valid() and days >= 0.
    void advance_days(int days) noexcept;

    // ISO-8601 short form, e.g. "1930-01-01". Zero-padded.
    std::string to_string() const;

    // Parses ISO-8601 short form "YYYY-MM-DD". Surrounding whitespace
    // is tolerated. Returns failure with a human-readable message on
    // malformed input or on a calendar-invalid date (e.g. "1930-02-30").
    static Result<GameDate> parse(std::string_view text);

    static constexpr bool is_leap_year(int year) noexcept {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    static constexpr int days_in_month(int year, int month) noexcept {
        constexpr int common[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month < 1 || month > 12) return 0;
        if (month == 2 && is_leap_year(year)) return 29;
        return common[month - 1];
    }

    friend constexpr bool operator==(const GameDate& a, const GameDate& b) noexcept {
        return a.year_ == b.year_ && a.month_ == b.month_ && a.day_ == b.day_;
    }
    friend constexpr bool operator!=(const GameDate& a, const GameDate& b) noexcept {
        return !(a == b);
    }
    friend constexpr bool operator<(const GameDate& a, const GameDate& b) noexcept {
        if (a.year_ != b.year_) return a.year_ < b.year_;
        if (a.month_ != b.month_) return a.month_ < b.month_;
        return a.day_ < b.day_;
    }
    friend constexpr bool operator<=(const GameDate& a, const GameDate& b) noexcept {
        return !(b < a);
    }
    friend constexpr bool operator>(const GameDate& a, const GameDate& b) noexcept {
        return b < a;
    }
    friend constexpr bool operator>=(const GameDate& a, const GameDate& b) noexcept {
        return !(a < b);
    }

private:
    int year_  = 1930;
    int month_ = 1;
    int day_   = 1;
};

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_GAME_DATE_HPP
