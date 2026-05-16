// LogEntry placeholder.
//
// M0.3 reserves the slot in GameState. M0.6 (LoggingSystem) fills in
// real fields - category, severity, source, optional metadata map.
// For now only `date` and `message` exist so unit tests can confirm
// the container is wired up.

#ifndef LEVIATHAN_CORE_LOG_ENTRY_HPP
#define LEVIATHAN_CORE_LOG_ENTRY_HPP

#include <string>

#include "leviathan/core/game_date.hpp"

namespace leviathan::core {

struct LogEntry {
    GameDate    date{};
    std::string message;
};

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_LOG_ENTRY_HPP
