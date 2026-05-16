// Parameters that fully determine a simulation run.
//
// Field shape intentionally mirrors RFC-070 §6 (the simulation JSON
// example) so the M0.7 DataLoader has nothing to translate. The
// loader will populate these fields directly.
//
// SimulationConfig is plain data: no methods, no system logic. It
// flows through make_game_state() to seed a fresh GameState.

#ifndef LEVIATHAN_CORE_SIMULATION_CONFIG_HPP
#define LEVIATHAN_CORE_SIMULATION_CONFIG_HPP

#include <cstdint>

#include "leviathan/core/game_date.hpp"

namespace leviathan::core {

struct SimulationConfig {
    GameDate      start_date{1930, 1, 1};
    GameDate      end_date{2000, 12, 31};
    std::uint64_t seed        = 0;
    bool          daily_tick  = true;
};

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_SIMULATION_CONFIG_HPP
