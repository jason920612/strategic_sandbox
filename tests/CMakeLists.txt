# Test suite root.
#
# M0.2 introduces doctest as the unit-test framework. doctest is fetched
# at configure time via FetchContent (shallow clone, fixed tag). It is a
# single-header header-only library with very fast compile and no link-
# time runtime, which suits a deterministic-simulation project well.
#
# All tests live in one executable (leviathan_tests). Individual
# TEST_CASE entries are then registered with CTest by doctest's
# discovery helper.

include(FetchContent)

# doctest 2.4.11 ships a cmake_minimum_required from the CMake 2.x era,
# which CMake >= 4.0 refuses to honour. Setting this cache variable
# tells CMake to apply the policies of 3.5 to that older project. This
# is the upstream-recommended workaround until doctest tags a release
# with a modern minimum.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(doctest)

add_executable(leviathan_tests
    test_main.cpp
    systems/ai_policy_test.cpp
    systems/asymptotic_add_trajectory_test.cpp
    systems/bias_noise_test.cpp
    systems/rcr_1_event_extensions_test.cpp
    core/ids_test.cpp
    core/result_test.cpp
    core/game_date_test.cpp
    core/string_utils_test.cpp
    core/simulation_config_test.cpp
    core/game_state_test.cpp
    systems/commands_test.cpp
    systems/data_loader_test.cpp
    systems/diagnostics_test.cpp
    systems/economy_system_test.cpp
    systems/event_effects_test.cpp
    systems/event_engine_test.cpp
    systems/event_evaluator_test.cpp
    systems/event_firer_test.cpp
    systems/faction_conflict_test.cpp
    systems/faction_system_test.cpp
    systems/information_accuracy_test.cpp
    systems/interest_group_system_test.cpp
    systems/logging_system_test.cpp
    systems/monthly_pipeline_test.cpp
    systems/order_execution_test.cpp
    systems/policy_system_test.cpp
    systems/propaganda_bias_test.cpp
    systems/random_service_test.cpp
    systems/reported_value_test.cpp
    systems/runner_test.cpp
    systems/save_system_test.cpp
    systems/scenario_loader_test.cpp
    systems/stability_system_test.cpp
    systems/svg_export_test.cpp
    systems/time_system_test.cpp
    integration/m0_end_to_end_test.cpp
    integration/m1_end_to_end_test.cpp
    integration/m2_end_to_end_test.cpp
    integration/m3_end_to_end_test.cpp
    integration/m4_dom_contract_test.cpp
    integration/m5_event_pipeline_test.cpp
    integration/rcr_1_rfc_compliance_test.cpp
    integration/issue_110_ai_scorer_test.cpp
    systems/event_engine_issue_112_test.cpp
    systems/commands_event_choice_test.cpp
    integration/issue_112_player_choice_runner_test.cpp
)

target_compile_definitions(leviathan_tests PRIVATE
    LEVIATHAN_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
)

target_link_libraries(leviathan_tests
    PRIVATE
        doctest::doctest
        leviathan::systems
        leviathan::core
        leviathan::compile_options
        leviathan::public_headers
)

# doctest's CMake helper registers each TEST_CASE as an individual
# CTest test, so `ctest -R foo` works as expected and failures show
# the failing case rather than the whole binary.
list(APPEND CMAKE_MODULE_PATH "${doctest_SOURCE_DIR}/scripts/cmake")
include(doctest)
doctest_discover_tests(leviathan_tests
    ADD_LABELS 1
)
