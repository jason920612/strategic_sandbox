# Test suite root.
#
# M0.1 ships one smoke test that uses <cassert> directly so the project
# has zero third-party dependencies at this point. A proper unit-test
# framework will be introduced in a later milestone (M0.2 or M0.3) when
# we have real types worth testing.

add_executable(smoke_test smoke_test.cpp)

target_link_libraries(smoke_test
    PRIVATE
        leviathan::compile_options
        leviathan::public_headers
)

add_test(NAME smoke_test COMMAND smoke_test)
set_tests_properties(smoke_test PROPERTIES LABELS "smoke")
