cmake_minimum_required(VERSION 3.28)

project(
    pot
    VERSION 0.0.1
    DESCRIPTION "Parallel Operations Toolkit"
    LANGUAGES CXX
)

get_directory_property(IS_SUBPROJECT PARENT_DIRECTORY)
                                
option(POT_BUILD_SHARED_LIBS "Build shared libraries"                                 OFF )
option(POT_BUILD_TESTS       "Enable unit testing"                                    ON  )
option(POT_CHECK_WARNINGS    "Strict compiler checks"                                 ON  )
option(POT_USE_MODULES       "Use C++20 modules (.cppm/.ixx/.mpp) instead of headers" OFF )

add_compile_definitions(_SILENCE_ALL_CXX23_DEPRECATION_WARNINGS)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/CompilerFlags.cmake OPTIONAL)
if(COMMAND apply_compiler_flags)
    apply_compiler_flags()
endif()

set(POT_HEADERS
    include/${PROJECT_NAME}/simd/simd_traits.h
    include/${PROJECT_NAME}/simd/simd_auto.h
    include/${PROJECT_NAME}/simd/simd_forced.h

    include/${PROJECT_NAME}/sync/sync_object.h

    include/${PROJECT_NAME}/traits/pot_traits.h
    include/${PROJECT_NAME}/traits/compare.h
    include/${PROJECT_NAME}/traits/guards.h

    include/${PROJECT_NAME}/utils/time_it.h
    include/${PROJECT_NAME}/utils/platform.h
    # include/${PROJECT_NAME}/utils/this_thread.h
    include/${PROJECT_NAME}/utils/cache_line.h

    include/${PROJECT_NAME}/executors/executor.h
    include/${PROJECT_NAME}/executors/inline_executor.h
    include/${PROJECT_NAME}/executors/thread_executor.h
    include/${PROJECT_NAME}/executors/thread_pool_executor.h
    include/${PROJECT_NAME}/executors/thread_pool_executor_lfgq.h

    include/${PROJECT_NAME}/coroutines/task.h
    include/${PROJECT_NAME}/coroutines/async_condition_variable.h
    include/${PROJECT_NAME}/coroutines/when_all.h
    include/${PROJECT_NAME}/coroutines/async_barrier.h

    include/${PROJECT_NAME}/algorithms/parfor.h
    include/${PROJECT_NAME}/algorithms/lfqueue.h
    include/${PROJECT_NAME}/algorithms/dot_mt.h
)

set(POT_SOURCES
    # src/utils/this_thread.cpp
    src/threads/thread.cpp
    src/executors/inline_executor.cpp
    # src/executors/thread_executor.cpp
)

file(GLOB_RECURSE POT_MODULES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/modules/*.cppm"
)

set(POT_HEADERS_FOR_MODULE_BUILD
    include/${PROJECT_NAME}/utils/platform.h
    include/${PROJECT_NAME}/utils/cache_line.h
)

add_library(${PROJECT_NAME})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

if(POT_CHECK_WARNINGS AND COMMAND apply_warning_flags_to_targets)
    apply_warning_flags_to_targets(${PROJECT_NAME})
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

if(NOT POT_USE_MODULES)
    target_sources(${PROJECT_NAME}
        PRIVATE
            ${POT_SOURCES}
        PUBLIC
            FILE_SET HEADERS
            BASE_DIRS
                ${PROJECT_SOURCE_DIR}/include
            FILES
                ${POT_HEADERS}
    )
else()
    if(POT_MODULES STREQUAL "")
        message(FATAL_ERROR "POT_USE_MODULES=ON, but /modules is empty")
    endif()

    target_sources(${PROJECT_NAME}
        PRIVATE
            ${POT_SOURCES}
        PUBLIC
            FILE_SET CXX_MODULES
            BASE_DIRS ${PROJECT_SOURCE_DIR}
            FILES     ${POT_MODULES}

            FILE_SET HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/include
            FILES     ${POT_HEADERS_FOR_MODULE_BUILD}
    )
endif()

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin

    FILE_SET HEADERS DESTINATION include

    FILE_SET CXX_MODULES DESTINATION share/${PROJECT_NAME}/modules
)

if(POT_BUILD_TESTS AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(test)
endif()
