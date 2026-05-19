// LoggingSystem - explicit, free-function logging into GameState::logs.
//
// Design rules (RFC-001 §4, M0.6 reviewer checklist):
//   * Logging is ALWAYS explicit. No other system writes to
//     state.logs as a side effect. The M0.4 non-interference test
//     enforces this for TimeSystem; new systems must keep it true.
//   * LoggingSystem performs no simulation decisions. It only
//     records what callers tell it to. Categorising, filtering, or
//     reacting to logs belongs in higher layers.
//   * JSONL format is documented and stable. The exporter never
//     reorders metadata keys (preserves insertion order) and never
//     mutates whitespace inside string fields beyond JSON escapes.

#ifndef LEVIATHAN_SYSTEMS_LOGGING_SYSTEM_HPP
#define LEVIATHAN_SYSTEMS_LOGGING_SYSTEM_HPP

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "leviathan/core/game_state.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::logging {

// Appends a single entry to state.logs, timestamped with
// state.current_date. metadata may be empty.
//
// This is the only function in the project that may push to
// state.logs. Other systems requesting a log must go through here.
void log(core::GameState& state,
         std::string category,
         core::LogSeverity severity,
         std::string source,
         std::string message,
         core::LogMetadata metadata = {});

// Severity-flavoured convenience wrappers. Pure shorthand; semantics
// are identical to calling log() with the named severity.
void log_debug(core::GameState& state,
               std::string category,
               std::string source,
               std::string message,
               core::LogMetadata metadata = {});
void log_info(core::GameState& state,
              std::string category,
              std::string source,
              std::string message,
              core::LogMetadata metadata = {});
void log_warn(core::GameState& state,
              std::string category,
              std::string source,
              std::string message,
              core::LogMetadata metadata = {});
void log_error(core::GameState& state,
               std::string category,
               std::string source,
               std::string message,
               core::LogMetadata metadata = {});

// Returns up to the last `n` entries from state.logs in insertion
// order (oldest first). If state.logs has fewer than `n` entries,
// returns all of them.
std::vector<core::LogEntry> recent(const core::GameState& state,
                                   std::size_t n);

// Writes one LogEntry to `out` as a single JSONL line (no trailing
// newline before the line, one '\n' after). Stable, byte-determinable
// format - see docs/m0-6-logging.md for the spec.
//
// M6.8 (RFC-090 §6.8 "debug 模式顯示真相"): when `debug_mode` is
// false (the default), the `true_cause` metadata key — appended
// unconditionally by `event_firer::record_match` /
// `record_followup` — is FILTERED OUT of the emitted line. When
// `debug_mode` is true, the key is preserved verbatim in
// insertion order so a downstream reader sees both the
// event-engine surface (event_id_code / actors) and the
// `EventDefinition.true_cause` (M6.1) text. Non-event-fired
// entries are unaffected (no `true_cause` metadata to filter).
void write_jsonl_line(std::ostream& out, const core::LogEntry& entry,
                      bool debug_mode = false);

// Writes every entry in state.logs as JSONL, one per line, in order.
// No header, no trailer. `debug_mode` is forwarded to each
// `write_jsonl_line` call; see that function for the M6.8 filter.
void export_jsonl(std::ostream& out, const core::GameState& state,
                  bool debug_mode = false);

// Writes the player-facing EventReport JSONL artefact. Only
// `event_fired` entries are exported. Required fields are emitted
// as top-level report fields and `true_cause` is never emitted,
// regardless of debug mode.
core::Result<bool> export_event_reports_jsonl(
    std::ostream& out,
    const core::GameState& state);

// Converts severity to its canonical lowercase string form.
// "debug" | "info" | "warn" | "error".
std::string severity_to_string(core::LogSeverity severity);

}  // namespace leviathan::systems::logging

#endif  // LEVIATHAN_SYSTEMS_LOGGING_SYSTEM_HPP
