#pragma once

#include <cstddef>
#include <new>
#include "platform.h"

namespace pot
{

#if !defined(POT_PLATFORM_MACOS) && defined(__cpp_lib_hardware_interference_size)
    #if defined(POT_COMPILER_GCC)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Winterference-size"
    #endif
        constexpr std::size_t cache_line_alignment = std::hardware_destructive_interference_size;
    #if defined(POT_COMPILER_GCC)
        #pragma GCC diagnostic pop
    #endif
#else
    constexpr std::size_t cache_line_alignment = 64;
#endif

} // namespace pot
