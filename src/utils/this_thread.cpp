#include "pot/utils/this_thread.h"

#include <codecvt>
#include <pthread.h>

#include "pot/traits/compare.h"
#include "pot/utils/platform.h"

void pot::details::this_thread::init_thread_variables(const int64_t local_id,
                                                      executor *owner_executor)
{
    static std::atomic<int64_t> thread_counter{0};
    tl_local_id = local_id;
    tl_global_id = thread_counter++;

    tl_owner_executor = owner_executor;
}

void pot::this_thread::init_thread_variables(const int64_t local_id, executor *owner_executor)
{
    pot::details::this_thread::init_thread_variables(local_id, owner_executor);
}

int64_t pot::this_thread::system_id()
{
    return static_cast<int64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

std::string pot::this_thread::executor_name()
{
    if (pot::details::this_thread::tl_owner_executor)
    {
        return pot::details::this_thread::tl_owner_executor->name();
    }
    return "None";
}

int64_t pot::this_thread::local_id() { return details::this_thread::tl_local_id; }

int64_t pot::this_thread::global_id() { return details::this_thread::tl_global_id; }

#if defined(POT_PLATFORM_WINDOWS)
#include <VersionHelpers.h>
#include <Windows.h>

void pot::this_thread::set_name(const std::string &name)
{
    if (IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WIN10), LOBYTE(_WIN32_WINNT_WIN10), 14393))
    {

        const int count = MultiByteToWideChar(CP_UTF8, 0, name.c_str(),
                                              static_cast<int>(name.length()), nullptr, 0);
        std::wstring wname(count, 0);
        MultiByteToWideChar(CP_UTF8, 0, name.c_str(), static_cast<int>(name.length()), wname.data(),
                            count);
        (void)SetThreadDescription(GetCurrentThread(), wname.c_str());
    }
    else
    {
    }
}

#elif defined(POT_PLATFORM_LINUX) || defined(POT_PLATFORM_ANDROID)
#include <sys/prctl.h>

void pot::this_thread::set_name(const std::string &name) { (void)prctl(PR_SET_NAME, name.c_str()); }

std::string pot::this_thread::name()
{
    std::array<char, 16> buf{};
    if (prctl(PR_GET_NAME, buf.data()) == 0)
        return std::string(buf.data());
    return {};
}

#elif defined(POT_PLATFORM_UNIX) || defined(POT_PLATFORM_IPHONE) || defined(POT_PLATFORM_MACOS)
#include <pthread.h>

void pot::this_thread::set_name(const std::string &name)
{
    pthread_setname_np(pthread_self(), name.c_str());
}

#else

#endif

void pot::this_thread::yield() { std::this_thread::yield(); }

#if defined(POT_PLATFORM_LINUX) || defined(POT_PLATFORM_ANDROID)

bool pot::this_thread::set_params(int policy, int priority)
{
    if (policy != SCHED_OTHER && policy != SCHED_FIFO && policy != SCHED_RR)
    {
        errno = EINVAL;
        return false;
    }

    int pmin = sched_get_priority_min(policy);
    if (pmin == -1)
        return false;

    int pmax = sched_get_priority_max(policy);
    if (pmax == -1)
        return false;

    int desired = std::clamp(priority, pmin, pmax);

    struct sched_param param;
    param.sched_priority = desired;

    int err = pthread_setschedparam(pthread_self(), policy, &param);
    if (err != 0)
    {
        errno = err;
        return false;
    }

    return true;
}

std::pair<int, int> pot::this_thread::get_params()
{
    struct sched_param param;
    int policy;
    int err = pthread_getschedparam(pthread_self(), &policy, &param);

    if (err != 0)
    {
        errno = err;
        return {-1, -1};
    }

    return {policy, param.sched_priority};
}

#endif
