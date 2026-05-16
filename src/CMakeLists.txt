# Source root.
#
# Order matters: core must be defined before systems (systems link
# core), and both must be defined before the executable.

add_subdirectory(leviathan/core)
add_subdirectory(leviathan/systems)

add_executable(leviathan_main main.cpp)

target_link_libraries(leviathan_main
    PRIVATE
        leviathan::systems
        leviathan::core
        leviathan::compile_options
        leviathan::public_headers
)

set_target_properties(leviathan_main PROPERTIES
    OUTPUT_NAME leviathan
)
