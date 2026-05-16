// Central simulation state container.
//
// GameState is intentionally a plain data struct with NO methods. All
// behaviour - advancing time, applying policies, computing budgets,
// evaluating events - lives in system free functions that receive a
// GameState& and mutate it (see RFC-060 §2: "資料集中、系統分離").
//
// The only construction helper is the free function make_game_state(),
// which seeds a fresh GameState from a SimulationConfig. We
// deliberately do not provide a constructor that takes a config: a
// free function keeps the "container is dumb" rule visible, and lets
// later subsystems compose initialisation pipelines without inheriting
// from or extending this struct.

#ifndef LEVIATHAN_CORE_GAME_STATE_HPP
#define LEVIATHAN_CORE_GAME_STATE_HPP

#include <vector>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/log_entry.hpp"
#include "leviathan/core/random_state.hpp"
#include "leviathan/core/simulation_config.hpp"

namespace leviathan::core {

struct GameState {
    GameDate    current_date{1930, 1, 1};
    RandomState rng{};

    std::vector<CountryState>    countries;
    std::vector<ProvinceState>   provinces;
    std::vector<FactionState>    factions;
    std::vector<PolicyData>      policies;
    std::vector<EventDefinition> events;
    std::vector<LogEntry>        logs;
};

// Builds a fresh GameState from `config`:
//   - current_date = config.start_date
//   - rng.seed     = config.seed
//   - rng.counter  = 0
//   - all entity containers start empty
//
// Precondition: config.start_date.is_valid(). The factory does not
// load data, run systems, or emit logs - those concerns belong to
// later milestones (M0.7 DataLoader, M0.4 TimeSystem, M0.6
// LoggingSystem).
GameState make_game_state(const SimulationConfig& config);

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_GAME_STATE_HPP
