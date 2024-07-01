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


if((CMAKE_CXX_COMPILER_ID MATCHES "GNU") OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    add_compile_options(
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
        -std=c++2a
        -mavx2
        -fexceptions
    )
    if(POT_CHECK_WARNINGS)
        add_compile_options(
            -Werror

            -Wall
            -Wextra
            -Wpedantic

            -Wcast-align
            -Wcast-qual
            -Wconversion
            -Wctor-dtor-privacy
            -Wenum-compare
            -Wfloat-equal
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
            -Wredundant-decls
            -Wsign-conversion
            -Wsign-promo
        )
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(
        /utf-8
        /std:c++latest
        /permissive-
        /arch:AVX2
        /EHsc
    )
    if(POT_CHECK_WARNINGS)
        add_compile_options(
            /W4
            /WX
        )
    endif()
endif()


set(POT_HEADERS
    include/${PROJECT_NAME}/experimental/thread_pool/thread_pool_ol.h
)

set(POT_SOURCES
    src/experimental/thread_pool/thread_pool_GQ_fpe.cpp
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