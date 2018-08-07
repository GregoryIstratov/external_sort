#pragma once
#include <chrono>
#include "../log.hpp"

class perf_timer
{
public:
        using sc = std::chrono::seconds;
        using us = std::chrono::microseconds;
        using ms = std::chrono::milliseconds;
        using ns = std::chrono::nanoseconds;

        perf_timer() = default;

        perf_timer(const char* msg, std::function<void()>&& fn)
        {
                using std::chrono::duration_cast;
                using std::chrono::milliseconds;

                start();

                fn();

                end();

                info2() << msg << " "
                        << duration_cast<milliseconds>(end_ - start_).count()
                        << " ms";
        }


        void start()
        {
                start_ = std::chrono::high_resolution_clock::now();
        }

        void end()
        {
                end_ = std::chrono::high_resolution_clock::now();
        }

        template<typename T>
        auto elapsed() const
        {
                return std::chrono::duration_cast<T>(end_ - start_).count();
        }

private:
        std::chrono::high_resolution_clock::time_point start_, end_;
};

/* Can be possibly improved with rdtsc
 * to reduce overhead in measuring */
class call_perf_timer
{
public:
        explicit
                call_perf_timer(const char* msg_literal)
                : msg_literal_(msg_literal) {}

        ~call_perf_timer()
        {
                std::cout << msg_literal_
                        << ": calls - " << calls_n_
                        << " total ns - " << total_ns_
                        << " avg. call ns - " << (total_ns_ / calls_n_)
                        << std::endl;
        }

        void start()
        {
                ++calls_n_;

                tm_.start();
        }

        void end()
        {
                tm_.end();
                total_ns_ += tm_.elapsed<std::chrono::nanoseconds>();
        }

private:
        perf_timer tm_;
        uint64_t total_ns_ = 0;
        uint64_t calls_n_ = 0;
        const char* msg_literal_;
};

