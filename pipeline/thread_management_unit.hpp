#pragma once
#include <unordered_map>
#include <atomic>
#include <future>

#include "../tools/spinlock.hpp"
#include "../tools/barrier.hpp"
#include "../log.hpp"

class thread_management_unit
{
public:
        explicit
                thread_management_unit(uint32_t threads_n)
                : threads_n_(threads_n), active_threads_(0)
        {}

        void spawn_and_join(std::function<void(uint32_t)>&& fun)
        {
                std::vector<std::thread> threads;
                std::vector<std::future<void>> futures;

                for (uint32_t i = 1; i < threads_n_; ++i)
                {
                        std::packaged_task<void()> task([this, i, &fun]() {
                                ++active_threads_;
                                fun(i);
                                --active_threads_;
                        });

                        futures.emplace_back(task.get_future());
                        threads.emplace_back(std::move(task));

                }

                std::packaged_task<void()> task([this, &fun]() {
                        ++active_threads_;
                        fun(0);
                        --active_threads_;
                });

                futures.emplace_back(task.get_future());

                task();

                for (auto& t : threads)
                        t.join();

                bool any_failed = false;
                for (auto& f : futures)
                {
                        try
                        {
                                f.get();
                        }
                        catch (const std::exception& e)
                        {
                                error() << e.what();
                                any_failed = true;
                        }
                }

                if (any_failed)
                        THROW_EXCEPTION << "Something went wrong";
        }

        uint32_t active_threads() const { return active_threads_; }

        template<typename LockMod>
        std::unique_lock<std::mutex> get_lock(LockMod lock_mod)
        {
                return std::unique_lock<std::mutex>(mtx_, lock_mod);
        }

        std::unique_lock<std::mutex> get_lock()
        {
                return std::unique_lock<std::mutex>(mtx_);
        }

        void condition_wait(uint32_t id, std::unique_lock<std::mutex>& lock,
                std::function<bool()>&& cond)
        {
                std::unique_lock<spinlock> lk(spin_);

                auto& cv = cv_map_[id];

                lk.unlock();

                cv.wait(lock, cond);
        }

        void condition_notify_all(uint32_t id)
        {
                std::unique_lock<spinlock> lk(spin_);

                auto& cv = cv_map_[id];

                lk.unlock();

                cv.notify_all();
        }

        void barrier_wait(uint32_t id)
        {
                std::unique_lock<spinlock> lk(spin_);

                auto found = bar_map_.find(id);

                if (found != bar_map_.end())
                {
                        auto& bar = found->second;
                        lk.unlock();

                        bar->wait();
                }
                else
                {

                        std::unique_ptr<barrier> new_bar(new barrier(threads_n_));
                        auto res = bar_map_.emplace(id, std::move(new_bar));

                        if (!res.second)
                                THROW_EXCEPTION << "Failed to insert to hash map";

                        auto& bar = res.first->second;
                        lk.unlock();

                        bar->wait();
                }
        }
private:
        uint32_t threads_n_;
        std::mutex mtx_;
        std::atomic<uint32_t> active_threads_;

        std::unordered_map<uint32_t, std::condition_variable> cv_map_;
        std::unordered_map<uint32_t, std::unique_ptr<barrier>> bar_map_;
        spinlock spin_;
};
