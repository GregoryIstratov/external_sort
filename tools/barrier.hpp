#pragma once
#include <condition_variable>
#include "exception.hpp"

class barrier
{
public:
        explicit barrier(uint32_t n)
                : n_((int32_t)n), init_n_(n_)
        {}

        barrier(barrier&&) = delete;
        barrier& operator=(barrier&&) = delete;


        void wait()
        {
                std::unique_lock<std::mutex> lk(m_);

                --n_;

                if (n_ < 0)
                        THROW_EXCEPTION("error in barrier thread sync");

                if (n_ == 0)
                {
                        cv_.notify_all();
                        return;
                }

                cv_.wait(lk, [this]() { return n_ == 0; });
        }

        void reset() { n_ = init_n_; }
        void reset(int32_t n) { n_ = n; }

private:
        int32_t n_, init_n_;
        std::mutex m_;
        std::condition_variable cv_;

};
