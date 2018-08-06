#pragma once
#include <condition_variable>
#include <list>
#include <cassert>

enum class cmd_mod
{
        nomod,
        skip_enque
};

class sequential_mutex
{
public:
        sequential_mutex() = default;

        void lock(uint32_t id, cmd_mod mod = cmd_mod::nomod)
        {
                std::unique_lock<std::mutex> wait_lock(wait_mtx_);

                if (que_.empty() && mtx_.try_lock())
                        return;

                if (mod != cmd_mod::skip_enque)
                        que_.push_back(id);

                cv_.wait(wait_lock, [this, id]() {
                        return id == que_.front();
                });

                assert(id == que_.front());
                que_.pop_front();

                mtx_.lock();
        }

        void unlock()
        {
                mtx_.unlock();
                cv_.notify_all();
        }

        void enque(uint32_t tid)
        {
                std::unique_lock<std::mutex> wait_lock(wait_mtx_);

                que_.push_back(tid);
        }

private:
        std::list<uint32_t> que_;
        std::mutex wait_mtx_;
        std::mutex mtx_;
        std::condition_variable cv_;
};
