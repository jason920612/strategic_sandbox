// LogEntry - one record in the simulation's event log.
//
// M0.3 reserved this type as a placeholder with only `date` and
// `message`. M0.6 expands it to the full shape spec'd in RFC-090
// task 0.8: timestamp, category, severity, source, message, and an
// ordered metadata list.
//
// Design notes:
//   * Metadata is `vector<pair<string, string>>`, not
//     `unordered_map<string, string>`, because we need deterministic
//     iteration order for JSONL output. Insertion order is preserved.
//   * Severity is an `enum class`, not an int or a free enum, so a
//     misplaced int does not silently slot into the wrong field.
//   * The struct still has no methods. All logging behaviour lives in
//     leviathan::systems::logging.

#ifndef LEVIATHAN_CORE_LOG_ENTRY_HPP
#define LEVIATHAN_CORE_LOG_ENTRY_HPP

#include <string>
#include <utility>
#include <vector>

#include "leviathan/core/game_date.hpp"

namespace leviathan::core {

enum class LogSeverity {
    Debug,
    Info,
    Warn,
    Error,
};

// Ordered key-value metadata. Order is preserved as inserted so the
// JSONL exporter produces byte-stable output across runs.
using LogMetadata = std::vector<std::pair<std::string, std::string>>;

struct LogEntry {
    GameDate    date{};
    std::string category;
    LogSeverity severity = LogSeverity::Info;
    std::string source;
    std::string message;
    LogMetadata metadata;
};

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_LOG_ENTRY_HPP
