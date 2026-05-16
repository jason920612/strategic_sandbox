# leviathan_systems: free-function systems that act on GameState.
#
# Per RFC-060 §2, systems live separately from the data layer. The core
# library owns plain data types (GameDate, GameState, entity stubs);
# this library owns the free functions that mutate them (TimeSystem
# today; RandomService, LoggingSystem, DataLoader, ... arriving in
# subsequent M0 sub-milestones).
#
# Dependency direction is strictly systems -> core. Core never depends
# on systems.

add_library(leviathan_systems STATIC
    logging_system.cpp
    random_service.cpp
    time_system.cpp
)
add_library(leviathan::systems ALIAS leviathan_systems)

target_link_libraries(leviathan_systems
    PUBLIC
        leviathan::core
        leviathan::public_headers
    PRIVATE
        leviathan::compile_options
)

set_target_properties(leviathan_systems PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
