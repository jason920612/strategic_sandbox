#include "leviathan/core/game_state.hpp"

#include <cassert>

namespace leviathan::core {

GameState make_game_state(const SimulationConfig& config) {
    assert(config.start_date.is_valid() &&
           "make_game_state requires a valid start_date");

    GameState state;
    state.current_date = config.start_date;
    state.rng.seed     = config.seed;
    state.rng.counter  = 0;
    // Entity containers stay empty: M0.7 (DataLoader) populates them
    // from JSON, not the factory.
    return state;
}

}  // namespace leviathan::core
