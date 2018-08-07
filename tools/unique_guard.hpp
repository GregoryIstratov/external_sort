#pragma once
#include <mutex>

template<typename MutexType>
class unique_guard
{
public:
        explicit
                unique_guard(std::unique_lock<MutexType>& lock,
                             bool release_lock = true)
                : lock_(lock),
                release_lock_(release_lock)
        {
                if (!lock_.owns_lock())
                        lock_.lock();
        }

        ~unique_guard()
        {
                if (release_lock_ && lock_.owns_lock())
                        lock_.unlock();
        }
private:
        std::unique_lock<MutexType>& lock_;
        bool release_lock_;
};
