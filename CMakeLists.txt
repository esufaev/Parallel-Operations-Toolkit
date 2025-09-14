cmake_minimum_required(VERSION 3.28)

project(
    pot
    VERSION 0.0.1
    DESCRIPTION "Parallel Operations Toolkit"
    LANGUAGES CXX
)

get_directory_property(IS_SUBPROJECT PARENT_DIRECTORY)

option(POT_BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(POT_BUILD_TESTS       "Enable unit testing"    ON )
option(POT_CHECK_WARNINGS    "Strict compiler checks" ON )

# set(CMAKE_CXX_STANDARD 23)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_definitions(_SILENCE_ALL_CXX23_DEPRECATION_WARNINGS)

# Apply compiler flags
include(${CMAKE_CURRENT_LIST_DIR}/cmake/CompilerFlags.cmake)
apply_compiler_flags()

set(POT_HEADERS
    include/${PROJECT_NAME}/simd/simd_traits.h
    include/${PROJECT_NAME}/simd/simd_auto.h
    include/${PROJECT_NAME}/simd/simd_forced.h

    include/${PROJECT_NAME}/sync/sync_object.h
    # include/${PROJECT_NAME}/sync/async_lock.h

    include/${PROJECT_NAME}/traits/pot_traits.h
    include/${PROJECT_NAME}/traits/compare.h
    include/${PROJECT_NAME}/traits/guards.h

    include/${PROJECT_NAME}/utils/time_it.h
    include/${PROJECT_NAME}/utils/platform.h
    include/${PROJECT_NAME}/utils/this_thread.h
    include/${PROJECT_NAME}/utils/cache_line.h
    include/${PROJECT_NAME}/utils/unique_function.h    
    include/${PROJECT_NAME}/executors/executor.h
    include/${PROJECT_NAME}/executors/inline_executor.h
    include/${PROJECT_NAME}/executors/thread_executor.h
    include/${PROJECT_NAME}/executors/thread_pool_executor.h

    include/${PROJECT_NAME}/coroutines/task.h
    include/${PROJECT_NAME}/coroutines/async_condition_variable.h
    include/${PROJECT_NAME}/coroutines/when_all.h
    include/${PROJECT_NAME}/coroutines/async_barrier.h
    include/${PROJECT_NAME}/coroutines/resume_on.h

    include/${PROJECT_NAME}/algorithms/parfor.h
    include/${PROJECT_NAME}/algorithms/lfqueue.h
    include/${PROJECT_NAME}/algorithms/reduce.h
    include/${PROJECT_NAME}/algorithms/dot.h
    include/${PROJECT_NAME}/algorithms/parsections.h

    include/${PROJECT_NAME}/pot.h
)

set(POT_SOURCES
    src/utils/this_thread.cpp
    
    # src/executors/inline_executor.cpp
    # src/executors/thread_executor.cpp
)

add_library(${PROJECT_NAME}
    ${POT_HEADERS}
    ${POT_SOURCES}
)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

set_target_properties(
    ${PROJECT_NAME}
      PROPERTIES
        #CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        LINKER_LANGUAGE CXX
)
target_compile_features(pot PUBLIC cxx_std_23)
# Apply warning flags
if(COMMAND apply_warning_flags_to_targets)
    apply_warning_flags_to_targets(pot)
endif()

target_include_directories(${PROJECT_NAME}
        PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")


# Installation
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME}
)


# Testing
if(POT_BUILD_TESTS AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(test)
endif()