# leviathan_core: foundational types (IDs, GameDate, Result, string utils).
#
# Everything that the executable, the test suite, and (eventually) the
# headless runner all share lives here.

add_library(leviathan_core STATIC
    game_date.cpp
    game_state.cpp
    interest_group_kind.cpp
    string_utils.cpp
)
add_library(leviathan::core ALIAS leviathan_core)

target_link_libraries(leviathan_core
    PUBLIC
        leviathan::public_headers
    PRIVATE
        leviathan::compile_options
)

set_target_properties(leviathan_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
