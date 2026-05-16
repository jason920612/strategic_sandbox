# Source root.
#
# Order matters: leviathan_core must be defined before leviathan_main so
# the executable can link against the alias.

add_subdirectory(leviathan/core)

add_executable(leviathan_main main.cpp)

target_link_libraries(leviathan_main
    PRIVATE
        leviathan::core
        leviathan::compile_options
        leviathan::public_headers
)

set_target_properties(leviathan_main PROPERTIES
    OUTPUT_NAME leviathan
)
