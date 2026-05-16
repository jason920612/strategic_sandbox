# leviathan_systems: free-function systems that act on GameState.
#
# Per RFC-060 §2, systems live separately from the data layer. The core
# library owns plain data types (GameDate, GameState, entity stubs);
# this library owns the free functions that mutate them (TimeSystem,
# RandomService, LoggingSystem, DataLoader today; more arriving in
# subsequent M0 sub-milestones).
#
# Dependency direction is strictly systems -> core. Core never depends
# on systems.

include(FetchContent)

# nlohmann/json ships a cmake_minimum_required older than CMake 4.x
# tolerates by default. Same workaround we used for doctest in tests/.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

# Disable nlohmann_json's own tests / examples to keep our configure
# fast and our build tree clean.
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(leviathan_systems STATIC
    data_loader.cpp
    logging_system.cpp
    random_service.cpp
    runner.cpp
    save_system.cpp
    time_system.cpp
)
add_library(leviathan::systems ALIAS leviathan_systems)

target_link_libraries(leviathan_systems
    PUBLIC
        leviathan::core
        leviathan::public_headers
    PRIVATE
        leviathan::compile_options
        nlohmann_json::nlohmann_json
)

set_target_properties(leviathan_systems PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
