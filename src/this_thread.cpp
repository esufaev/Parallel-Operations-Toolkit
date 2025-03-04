#include "pot/this_thread.h"

#include <codecvt>

#include "pot/platform.h"
#include "pot/traits/compare.h"

void pot::details::this_thread::init_thread_variables(const int64_t local_id, const std::weak_ptr<executor>& owner_executor)
{
    static std::atomic<int64_t> thread_counter{ 0 };
    tl_local_id = local_id;
    tl_global_id = thread_counter++;

    tl_owner_executor = owner_executor;
}

int64_t pot::this_thread::system_id()
{
    return static_cast<int64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

int64_t pot::this_thread::local_id()
{
    return details::this_thread::tl_local_id;
}

int64_t pot::this_thread::global_id()
{
    return details::this_thread::tl_global_id;
}

#if defined(POT_PLATFORM_WINDOWS)
#include <Windows.h>
#include <VersionHelpers.h>

void pot::this_thread::set_name(const std::string& name)
{
    if (IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WIN10), LOBYTE(_WIN32_WINNT_WIN10), 14393)) {
        // Windows 10 1607 or greater, use SetThreadDescription
        const int count = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), static_cast<int>(name.length()), nullptr, 0);
        std::wstring wname(count, 0);
        MultiByteToWideChar(CP_UTF8, 0, name.c_str(), static_cast<int>(name.length()), wname.data(), count);
        (void)SetThreadDescription(GetCurrentThread(), wname.c_str());
    }
    else {
        // Windows version is less than Windows 10, do nothing
    }
}

#elif defined(POT_PLATFORM_LINUX) || defined(POT_PLATFORM_ANDROID)
#include <sys/prctl.h>
// #include <sys/system_properties.h>

void pot::this_thread::set_name(const std::string& name)
{
    (void)prctl(PR_SET_NAME, name.c_str());
}

#elif defined(POT_PLATFORM_UNIX) || defined(POT_PLATFORM_IPHONE) || defined(POT_PLATFORM_MACOS)
#include <pthread.h>

void pot::this_thread::set_name(const std::string& name)
{
    pthread_setname_np(pthread_self(), name.c_str());
}

#else
// defined(POT_PLATFORM_UNKNOWN)

#endif

void pot::this_thread::yield()
{
    std::this_thread::yield();
}
