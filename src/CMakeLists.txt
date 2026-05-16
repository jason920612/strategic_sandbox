# Main simulation executable.
#
# M0.1 only ships a stub entry point. Later milestones will move most
# simulation logic into a static library so it can be linked into both
# the main executable and the test suite.

add_executable(leviathan_main main.cpp)

target_link_libraries(leviathan_main
    PRIVATE
        leviathan::compile_options
        leviathan::public_headers
)

set_target_properties(leviathan_main PROPERTIES
    OUTPUT_NAME leviathan
)
