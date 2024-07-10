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

    include/${PROJECT_NAME}/traits/guards.h
    include/${PROJECT_NAME}/utils/time_it.h

    include/${PROJECT_NAME}/executors/inline_executor.h

    include/${PROJECT_NAME}/experimental/parfor.h
    include/${PROJECT_NAME}/experimental/thread_pool/thread_pool_gq_esu.h
    include/${PROJECT_NAME}/experimental/thread_pool/thread_pool_lq_esu.h
    include/${PROJECT_NAME}/experimental/thread_pool/thread_pool_fpe.h
)

set(POT_SOURCES
    src/executors/inline_executor.cpp

    src/experimental/thread_pool/thread_pool_fpe.cpp
    # src/main.cpp
)


add_library(${PROJECT_NAME}
    ${POT_HEADERS}
    ${POT_SOURCES}
)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

set_target_properties(
    ${PROJECT_NAME}
      PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
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