# Include Catch2
include(${CMAKE_SOURCE_DIR}/cmake/Catch2.cmake)

# Create test executable
set(TEST_SOURCES
  # test_async_lock.cpp
  # test_async_lock_lf.cpp
  # test_task.cpp
  # test_parfor.cpp
  # test_executor.cpp
  # test_LU.cpp
  # test_fill.cpp
  # test_thread.cpp
  # test_resume_on.cpp
  # test_exp.cpp
  # test_mandelbrot.cpp
  # test_disb.cpp
  # test_meta.cpp
  test_thread_pools_bench.cpp
)

find_package(Eigen3 REQUIRED)

add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests PRIVATE pot Catch2::Catch2WithMain)
target_link_libraries(tests PRIVATE Eigen3::Eigen)
target_link_libraries(tests PRIVATE pot concurrencpp::concurrencpp)
target_link_libraries(tests PRIVATE pot fmt::fmt)
target_link_libraries(tests PRIVATE pot OpenMP::OpenMP_CXX)

# apply_warning_flags_to_targets(tests) # Apply warning flags
set_target_properties(
    tests
      PROPERTIES
        #CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

# Enable CTest
include(CTest)
include(Catch)
catch_discover_tests(tests)
