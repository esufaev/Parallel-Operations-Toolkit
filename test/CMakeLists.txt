# Include Catch2
include(${CMAKE_SOURCE_DIR}/cmake/Catch2.cmake)

# Create test executable

set(TEST_SOURCES
    test_pot.cpp
    test_thread_pool_fpe.cpp
)

add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests PRIVATE pot Catch2::Catch2WithMain)

# apply_warning_flags_to_targets(tests) # Apply warning flags

set_target_properties(
    ${PROJECT_NAME}
      PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

# Enable CTest
include(CTest)
include(Catch)
catch_discover_tests(tests)
