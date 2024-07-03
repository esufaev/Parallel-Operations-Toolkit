# CompilerFlags.cmake

# GNU and Clang flags
set(GNU_CLANG_FLAGS
    -finput-charset=UTF-8
    -fexec-charset=UTF-8
    -std=c++2a
    -mavx2
    -fexceptions
)

set(GNU_CLANG_WARNING_FLAGS
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

# MSVC flags
set(MSVC_FLAGS
    /utf-8
    /std:c++latest
    /permissive-
    /arch:AVX2
    /EHsc
)

set(MSVC_WARNING_FLAGS
    /W4
    /WX
)

# Function to apply compiler flags
function(apply_compiler_flags)
    if((CMAKE_CXX_COMPILER_ID MATCHES "GNU") OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
        add_compile_options(${GNU_CLANG_FLAGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        add_compile_options(${MSVC_FLAGS})
    endif()
endfunction()

# Function to apply warning flags to specified targets
function(apply_warning_flags_to_targets)
    if(NOT POT_CHECK_WARNINGS)
        return()
    endif()

    if((CMAKE_CXX_COMPILER_ID MATCHES "GNU") OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
        set(WARNING_FLAGS ${GNU_CLANG_WARNING_FLAGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(WARNING_FLAGS ${MSVC_WARNING_FLAGS})
    else()
        return()
    endif()

    foreach(target IN LISTS ARGV)
        target_compile_options(${target} PRIVATE ${WARNING_FLAGS})
    endforeach()
endfunction()
