#include <doctest/doctest.h>

#include <sstream>
#include <string>
#include <vector>

#include "leviathan/core/game_date.hpp"
#include "leviathan/core/game_state.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/core/simulation_config.hpp"
#include "leviathan/systems/logging_system.hpp"
#include "leviathan/systems/time_system.hpp"

using leviathan::core::GameDate;
using leviathan::core::GameState;
using leviathan::core::LogEntry;
using leviathan::core::LogMetadata;
using leviathan::core::LogSeverity;
namespace lg = leviathan::systems::logging;
namespace lt = leviathan::systems::time;

TEST_CASE("log() appends one entry timestamped with current_date") {
    GameState state;
    state.current_date = GameDate(1936, 7, 17);

    lg::log(state, "time", LogSeverity::Info, "TimeSystem", "tick");

    REQUIRE(state.logs.size() == 1);
    const auto& e = state.logs.front();
    CHECK(e.date     == GameDate(1936, 7, 17));
    CHECK(e.category == "time");
    CHECK(e.severity == LogSeverity::Info);
    CHECK(e.source   == "TimeSystem");
    CHECK(e.message  == "tick");
    CHECK(e.metadata.empty());
}

TEST_CASE("severity convenience wrappers set the right severity") {
    GameState state;
    state.current_date = GameDate(1930, 1, 1);

    lg::log_debug(state, "c", "s", "d");
    lg::log_info (state, "c", "s", "i");
    lg::log_warn (state, "c", "s", "w");
    lg::log_error(state, "c", "s", "e");

    REQUIRE(state.logs.size() == 4);
    CHECK(state.logs[0].severity == LogSeverity::Debug);
    CHECK(state.logs[1].severity == LogSeverity::Info);
    CHECK(state.logs[2].severity == LogSeverity::Warn);
    CHECK(state.logs[3].severity == LogSeverity::Error);
}

TEST_CASE("log() preserves metadata insertion order") {
    GameState state;
    state.current_date = GameDate(1945, 5, 8);
    LogMetadata md;
    md.emplace_back("k_b", "v_b");
    md.emplace_back("k_a", "v_a");
    md.emplace_back("k_c", "v_c");

    lg::log(state, "test", LogSeverity::Info, "src", "msg", md);

    REQUIRE(state.logs.size() == 1);
    const auto& m = state.logs.front().metadata;
    REQUIRE(m.size() == 3);
    CHECK(m[0].first == "k_b");
    CHECK(m[1].first == "k_a");
    CHECK(m[2].first == "k_c");
}

TEST_CASE("recent(n) returns the last n entries in insertion order") {
    GameState state;
    state.current_date = GameDate(1930, 1, 1);
    for (int i = 0; i < 10; ++i) {
        lg::log_info(state, "test", "src", "msg-" + std::to_string(i));
    }

    const auto last5 = lg::recent(state, 5);
    REQUIRE(last5.size() == 5);
    CHECK(last5[0].message == "msg-5");
    CHECK(last5[4].message == "msg-9");
}

TEST_CASE("recent(n) returns all entries if fewer than n exist") {
    GameState state;
    state.current_date = GameDate(1930, 1, 1);
    lg::log_info(state, "test", "src", "only");

    const auto all = lg::recent(state, 100);
    REQUIRE(all.size() == 1);
    CHECK(all.front().message == "only");
}

TEST_CASE("recent(0) returns an empty vector") {
    GameState state;
    state.current_date = GameDate(1930, 1, 1);
    lg::log_info(state, "test", "src", "one");
    lg::log_info(state, "test", "src", "two");

    CHECK(lg::recent(state, 0).empty());
}

TEST_CASE("write_jsonl_line produces the documented byte-stable shape") {
    LogEntry entry;
    entry.date     = GameDate(1930, 1, 1);
    entry.category = "time";
    entry.severity = LogSeverity::Info;
    entry.source   = "TimeSystem";
    entry.message  = "Advanced to next day";

    std::ostringstream out;
    lg::write_jsonl_line(out, entry);

    const std::string expected =
        "{\"date\":\"1930-01-01\","
        "\"category\":\"time\","
        "\"severity\":\"info\","
        "\"source\":\"TimeSystem\","
        "\"message\":\"Advanced to next day\","
        "\"metadata\":{}}\n";
    CHECK(out.str() == expected);
}

TEST_CASE("JSONL output escapes quotes, backslashes, and control characters") {
    LogEntry entry;
    entry.date     = GameDate(1930, 1, 1);
    entry.category = "test";
    entry.severity = LogSeverity::Warn;
    entry.source   = "test";
    entry.message  = "quote \" backslash \\ newline \n tab \t bel \x01";

    std::ostringstream out;
    lg::write_jsonl_line(out, entry);

    const std::string s = out.str();
    // Inspect just the escape patterns that appear inside the message field.
    CHECK(s.find("\\\"") != std::string::npos);   // \"  for the inner quote
    CHECK(s.find("\\\\") != std::string::npos);   // \\  for the inner backslash
    CHECK(s.find("\\n")  != std::string::npos);
    CHECK(s.find("\\t")  != std::string::npos);
    CHECK(s.find("\\u0001") != std::string::npos);
}

TEST_CASE("JSONL output emits metadata as a JSON object with quoted keys") {
    LogEntry entry;
    entry.date     = GameDate(1930, 1, 1);
    entry.category = "policy";
    entry.severity = LogSeverity::Info;
    entry.source   = "PolicySystem";
    entry.message  = "policy enacted";
    entry.metadata.emplace_back("policy_id", "increase_military_budget");
    entry.metadata.emplace_back("country",   "GER");

    std::ostringstream out;
    lg::write_jsonl_line(out, entry);

    const std::string expected =
        "{\"date\":\"1930-01-01\","
        "\"category\":\"policy\","
        "\"severity\":\"info\","
        "\"source\":\"PolicySystem\","
        "\"message\":\"policy enacted\","
        "\"metadata\":{"
            "\"policy_id\":\"increase_military_budget\","
            "\"country\":\"GER\""
        "}}\n";
    CHECK(out.str() == expected);
}

TEST_CASE("export_jsonl writes one line per entry, in order") {
    GameState state;
    state.current_date = GameDate(1930, 1, 1);
    lg::log_info(state, "a", "S", "msg-1");
    state.current_date = GameDate(1930, 1, 2);
    lg::log_info(state, "a", "S", "msg-2");

    std::ostringstream out;
    lg::export_jsonl(out, state);

    const std::string text = out.str();
    // Two trailing newlines means three split parts (last is empty).
    std::size_t newlines = 0;
    for (char c : text) if (c == '\n') ++newlines;
    CHECK(newlines == 2);
    CHECK(text.find("\"date\":\"1930-01-01\"") != std::string::npos);
    CHECK(text.find("\"date\":\"1930-01-02\"") != std::string::npos);
}

TEST_CASE("severity_to_string is canonical lowercase") {
    CHECK(lg::severity_to_string(LogSeverity::Debug) == "debug");
    CHECK(lg::severity_to_string(LogSeverity::Info)  == "info");
    CHECK(lg::severity_to_string(LogSeverity::Warn)  == "warn");
    CHECK(lg::severity_to_string(LogSeverity::Error) == "error");
}

TEST_CASE("TimeSystem still does not auto-log") {
    // Regression for the M0.4 non-interference contract. M0.6 adds
    // LoggingSystem; TimeSystem must NOT incidentally start writing
    // to state.logs.
    GameState state;
    state.current_date = GameDate(1930, 1, 1);
    REQUIRE(state.logs.empty());

    lt::advance_days(state, 100);

    CHECK(state.logs.empty());
}

TEST_CASE("log() entries from across multiple ticks land in date order") {
    // Explicit logging during a tick loop should produce a strictly
    // non-decreasing date sequence in state.logs (modulo the caller
    // logging twice on the same day, which is fine).
    GameState state;
    state.current_date = GameDate(1930, 12, 30);
    for (int i = 0; i < 5; ++i) {
        lg::log_info(state, "time", "TimeSystem", "tick");
        lt::advance_one_day(state);
    }
    REQUIRE(state.logs.size() == 5);
    for (std::size_t i = 1; i < state.logs.size(); ++i) {
        CHECK(state.logs[i - 1].date <= state.logs[i].date);
    }
    // Year crossing happened between iterations 1 and 2; logs span 1930
    // and 1931, and the last entry was stamped on 1931-01-03.
    CHECK(state.logs.front().date == GameDate(1930, 12, 30));
    CHECK(state.logs.back().date  == GameDate(1931, 1, 3));
}
