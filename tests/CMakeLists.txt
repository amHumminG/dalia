include(FetchContent)

# Prevent GoogleTest from overriding compiler/linker options on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Unit Tests
set(UNIT_TEST_FILES
    unit/core/SPSCRingBuffer_UT.cpp
)

add_executable(dalia_unit_tests EXCLUDE_FROM_ALL ${UNIT_TEST_FILES})
dalia_treat_warnings_as_errors(dalia_unit_tests)

target_include_directories(dalia_unit_tests
    PRIVATE ${CMAKE_SOURCE_DIR}/engine/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/helpers
)

target_link_libraries(dalia_unit_tests
    PRIVATE dalia::engine
    PRIVATE GTest::gtest_main
)

# Integration Tests
set(INTEGRATION_TEST_FILES
    integration/api/EngineLifecycle_IT.cpp
)

add_executable(dalia_integration_tests EXCLUDE_FROM_ALL ${INTEGRATION_TEST_FILES})
dalia_treat_warnings_as_errors(dalia_integration_tests)

target_link_libraries(dalia_integration_tests
    PRIVATE dalia::engine
    PRIVATE GTest::gtest_main
)

gtest_discover_tests(dalia_unit_tests)
gtest_discover_tests(dalia_integration_tests)
