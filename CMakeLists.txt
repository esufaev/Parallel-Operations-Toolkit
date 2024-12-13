cmake_minimum_required(VERSION 3.16)

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
include(${CMAKE_SOURCE_DIR}/cmake/CompilerFlags.cmake)
apply_compiler_flags()


set(POT_HEADERS
    include/${PROJECT_NAME}/pot_traits.h
    include/${PROJECT_NAME}/when_all.h
    include/${PROJECT_NAME}/executor.h
    include/${PROJECT_NAME}/platform.h
    include/${PROJECT_NAME}/future.h
    include/${PROJECT_NAME}/promise.h
    include/${PROJECT_NAME}/this_thread.h

    include/${PROJECT_NAME}/executors/inline_executor.h
    include/${PROJECT_NAME}/executors/thread_executor.h
    include/${PROJECT_NAME}/executors/thread_pool_executor.h

    include/${PROJECT_NAME}/simd/simd_traits.h
    include/${PROJECT_NAME}/simd/simd_auto.h
    include/${PROJECT_NAME}/simd/simd_forced.h

    include/${PROJECT_NAME}/sync/sync_object.h

    include/${PROJECT_NAME}/traits/compare.h
    include/${PROJECT_NAME}/traits/guards.h


    include/${PROJECT_NAME}/utils/time_it.h
    include/${PROJECT_NAME}/utils/progress.h

    include/${PROJECT_NAME}/executors/inline_executor.h
    include/${PROJECT_NAME}/executors/thread_executor.h
    include/${PROJECT_NAME}/executors/thread_pool_executor.h

    include/${PROJECT_NAME}/experimental/thread_pool/thread_pool_gq_esu.h
    include/${PROJECT_NAME}/experimental/thread_pool/thread_pool_lq_esu.h
    include/${PROJECT_NAME}/experimental/thread_pool/thread_pool_fpe.h

    include/${PROJECT_NAME}/allocators/shared_allocator.h

    include/${PROJECT_NAME}/tasks/stack_task.h
    include/${PROJECT_NAME}/tasks/task.h
    include/${PROJECT_NAME}/tasks/lazy_task.h
    include/${PROJECT_NAME}/tasks/big_task.h

    include/${PROJECT_NAME}/tasks/impl/shared_state.h
    include/${PROJECT_NAME}/tasks/impl/lazy_shared_state.h
    include/${PROJECT_NAME}/tasks/impl/big_shared_state.h

    include/${PROJECT_NAME}/coroutines/coroutine.h
    include/${PROJECT_NAME}/coroutines/task_coroutine.h

    include/${PROJECT_NAME}/algorithms/parfor.h

)

set(POT_SOURCES
    src/this_thread.cpp
    src/thread.cpp

    src/executors/inline_executor.cpp
    src/executors/thread_executor.cpp

    src/experimental/thread_pool/thread_pool_fpe.cpp

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
)
target_compile_features(pot PUBLIC cxx_std_23)
# Apply warning flags
apply_warning_flags_to_targets(pot)

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
if(POT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()