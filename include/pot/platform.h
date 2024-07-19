#pragma once

namespace pot::platform
{
    enum class OperatingSystem {
        Unknown,
        Windows,
        MacOS,
        Linux,
        Unix,
        Android,
        IOS
    };

    enum class Compiler {
        Unknown,
        MSVC,
        Clang,
        GCC,
        Intel,
        MinGW,
        Borland
    };

    enum class Bitness {
        Unknown,
        x32,
        x64
    };

    // Determining the current operating system
    #if defined(_WIN32) || defined(_WIN64)
        #define POT_PLATFORM_WINDOWS
        constexpr auto current_OS = OperatingSystem::Windows;
    #elif defined(__APPLE__) || defined(__MACH__)
        #include <TargetConditionals.h>
        #if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
            #define POT_PLATFORM_IPHONE
            constexpr auto current_OS = OperatingSystem::IOS;
        #elif TARGET_OS_MAC == 1
            #define POT_PLATFORM_MACOS
            constexpr auto current_OS = OperatingSystem::MacOS;
        #else
            #define POT_PLATFORM_UNKNOWN
            constexpr auto current_OS = OperatingSystem::Unknown;
        #endif
    #elif defined(__linux__)
        #define POT_PLATFORM_LINUX
        constexpr auto current_OS = OperatingSystem::Linux;
    #elif defined(__ANDROID__)
        #define POT_PLATFORM_ANDROID
        constexpr auto current_OS = OperatingSystem::Android;
    #elif defined(__unix__) || defined(_POSIX_VERSION)
        #define POT_PLATFORM_UNIX
        constexpr auto current_OS = OperatingSystem::Unix;
    #else
        #define POT_PLATFORM_UNKNOWN
        constexpr auto current_OS = OperatingSystem::Unknown;
    #endif

    // Determining the current compiler
    #if defined(_MSC_VER)
        #define POT_COMPILER_MSVC
        constexpr auto current_сompiler = Compiler::MSVC;
    #elif defined(__clang__)
        #define POT_COMPILER_CLANG
        constexpr auto current_сompiler = Compiler::Clang;
    #elif defined(__GNUC__) && !defined(__clang__)
        #define POT_COMPILER_GCC
        constexpr auto current_сompiler = Compiler::GCC;
    #elif defined(__INTEL_COMPILER) || defined(__ICL)
        #define POT_COMPILER_INTEL
        constexpr auto current_сompiler = Compiler::Intel;
    #elif defined(__MINGW32__) || defined(__MINGW64__)
        #define POT_COMPILER_MINGW
        constexpr auto current_сompiler = Compiler::MinGW;
    #elif defined(__BORLANDC__)
        #define POT_COMPILER_BORLAND
        constexpr auto current_сompiler = Compiler::Borland;
    #else
        #define POT_COMPILER_UNKNOWN
        constexpr auto current_сompiler = Compiler::Unknown;
    #endif

    // Определяем разрядность
    #if INTPTR_MAX == INT64_MAX
        #define POT_BITNESS_X64
        constexpr auto currentBitness = Bitness::x64;
    #elif INTPTR_MAX == INT32_MAX
        #define POT_BITNESS_X86
        constexpr auto currentBitness = Bitness::x32;
    #else
        #define POT_BITNESS_UNKNOWN
        constexpr auto currentBitness = Bitness::Unknown;
    #endif

} // namespace pot::platform

