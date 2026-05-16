#include "leviathan/core/game_date.hpp"

#include <array>
#include <cassert>
#include <charconv>
#include <cstdio>
#include <string>
#include <string_view>

#include "leviathan/core/string_utils.hpp"

namespace leviathan::core {

void GameDate::advance_one_day() noexcept {
    assert(is_valid() && "GameDate::advance_one_day on invalid date");
    ++day_;
    if (day_ > days_in_month(year_, month_)) {
        day_ = 1;
        ++month_;
        if (month_ > 12) {
            month_ = 1;
            ++year_;
        }
    }
}

void GameDate::advance_days(int days) noexcept {
    assert(is_valid() && "GameDate::advance_days on invalid date");
    assert(days >= 0 && "GameDate::advance_days does not accept negative deltas");
    // Simple loop. Fast enough for the M0.2 unit-test ranges (~365 ticks)
    // and trivially correct. A day-of-epoch fast path can land later if
    // a profile shows it's needed.
    for (int i = 0; i < days; ++i) {
        advance_one_day();
    }
}

std::string GameDate::to_string() const {
    // Fixed width "YYYY-MM-DD". snprintf keeps this deterministic regardless
    // of the user's locale (std::stringstream is locale-sensitive).
    std::array<char, 16> buf{};
    const int written = std::snprintf(buf.data(), buf.size(),
                                      "%04d-%02d-%02d", year_, month_, day_);
    if (written <= 0) {
        return std::string{};
    }
    return std::string(buf.data(), static_cast<std::size_t>(written));
}

namespace {

bool parse_fixed_int(std::string_view text, int& out) noexcept {
    int value = 0;
    const auto* first = text.data();
    const auto* last  = text.data() + text.size();
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last) {
        return false;
    }
    out = value;
    return true;
}

}  // namespace

Result<GameDate> GameDate::parse(std::string_view text) {
    const std::string trimmed = string_utils::trim(text);
    // ISO-8601 short form is exactly 10 chars: YYYY-MM-DD.
    if (trimmed.size() != 10 || trimmed[4] != '-' || trimmed[7] != '-') {
        return Result<GameDate>::failure(
            "GameDate::parse: expected YYYY-MM-DD, got \"" + std::string(text) + "\"");
    }

    int year  = 0;
    int month = 0;
    int day   = 0;
    if (!parse_fixed_int(std::string_view(trimmed).substr(0, 4), year) ||
        !parse_fixed_int(std::string_view(trimmed).substr(5, 2), month) ||
        !parse_fixed_int(std::string_view(trimmed).substr(8, 2), day)) {
        return Result<GameDate>::failure(
            "GameDate::parse: non-numeric component in \"" + std::string(text) + "\"");
    }

    GameDate candidate(year, month, day);
    if (!candidate.is_valid()) {
        return Result<GameDate>::failure(
            "GameDate::parse: not a real Gregorian date: \"" + std::string(text) + "\"");
    }
    return Result<GameDate>::success(candidate);
}

}  // namespace leviathan::core
