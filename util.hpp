#ifndef EXTERNAL_SORT_UTIL_HPP
#define EXTERNAL_SORT_UTIL_HPP

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <cstring>
#include <iomanip>
#include <condition_variable>

#include "settings.hpp"

class barrier
{
public:
        explicit
        barrier(uint32_t n)
                : n_((int32_t)n), init_n_(n_)
        {}

        barrier(barrier&&) = delete;
        barrier& operator=(barrier&&) = delete;


        void wait()
        {
                std::unique_lock<std::mutex> lk(m_);

                --n_;

                if (n_ < 0)
                        throw std::runtime_error("error in barrier thread sync");

                if (n_ == 0)
                        cv_.notify_all();

                cv_.wait(lk, [this]() { return n_ == 0; });
        }

        void reset() { n_ = init_n_; }
        void reset(int32_t n) { n_ = n; }

private:
        int32_t n_, init_n_;
        std::mutex m_;
        std::condition_variable cv_;

};

class perf_timer
{
public:
        using sc = std::chrono::seconds;
        using us = std::chrono::microseconds;
        using ms = std::chrono::milliseconds;
        using ns = std::chrono::nanoseconds;

        void start()
        {
                start_ = std::chrono::high_resolution_clock::now();
        }

        void end()
        {
                end_ = std::chrono::high_resolution_clock::now();
        }

        template<typename T>
        auto elapsed()
        {
                return std::chrono::duration_cast<T>(end_ - start_).count();
        }

private:
        std::chrono::high_resolution_clock::time_point start_, end_;
};

class logger
{
public:
        ~logger()
        {
                os_ << std::endl;
        }

        logger(logger&&) = delete;
        logger(const logger&) = delete;

        logger& operator=(logger&&) = delete;
        logger& operator=(const logger&) = delete;

protected:
        logger(const char* type, std::ostream& os = std::cout)
                : lock_(mtx_), os_(os), fmt_guard_(os_)
        {
                static const auto flags = std::ios_base::hex
                                          | std::ios_base::uppercase
                                          | std::ios_base::showbase;

                os_ << "[+"
                    << std::fixed << std::setprecision(3)
                    << (clock() / (float)CLOCKS_PER_SEC) << "]"
                    << "[" << type << "]["
                    << std::resetiosflags(std::ios_base::dec)
                    << std::setiosflags(flags)
                    << std::this_thread::get_id() << "]: "
                    << std::resetiosflags(flags)
                    << std::setiosflags(std::ios::dec)
                                                     ;
        }

        class fmt_guard
        {
        public:
                explicit
                fmt_guard(std::ios& _ios) : ios_(_ios), init_(nullptr)
                {
                        init_.copyfmt(ios_);
                }

                ~fmt_guard()
                {
                        ios_.copyfmt(init_);
                }

        private:
                std::ios& ios_;
                std::ios init_;
        };

protected:
        static std::mutex mtx_;

        std::unique_lock<std::mutex> lock_;
        std::ostream& os_;
        fmt_guard fmt_guard_;
private:
        template<typename T>
        friend logger&& operator<<(logger&& _log, T&& v);
};

template<typename T>
logger&& operator<<(logger&& _log, T&& v)
{
        _log.os_ << v;
        return std::move(_log);
}

class info : public logger
{
public:
        info() : logger("INF") {}
};

class error : public logger
{
public:
        error() : logger("ERR", std::cerr) {}
};

#if CONFIG_INFO_LEVEL >= 2

class info2 : public logger
{
public:
        info2() : logger("INF") {}
};

#else
struct info2
{
};

template<typename T>
info2&& operator<<(info2&& _log, const T &v)
{
        return std::move(_log);
}
#endif

#if !defined(NDEBUG)
class debug : public logger
{
public:
        debug() : logger("DBG") {}
};
#else

struct debug
{
};

template<typename T>
debug&& operator<<(debug&& _log, const T &v)
{
        return std::move(_log);
}

#endif


inline
std::string size_format(uint64_t bytes)
{
        std::stringstream ss;

        if (bytes < 1024)
                ss << bytes << "Bytes";
        else if (bytes < MEGABYTE)
                ss << (bytes / KILOBYTE) << "KiB";
        else if(bytes < GIGABYTE)
                ss << (bytes / MEGABYTE) << "MiB";
        else
                ss << (bytes / GIGABYTE) << "GiB";

        return ss.str();
}

inline
std::string num_format(uint64_t n)
{
        std::stringstream ss;

        if (n < 1000)
                ss << n;
        else if (n < 1000 * 1000)
                ss << (n / 1000) << "K";
        else
                ss << (n / 1000 / 1000) << "M";

        return ss.str();
}

void gen_rnd_test_file(const char* filename, uint64_t size);

template<typename T>
inline T div_up(T a, T b)
{
        static_assert(std::is_integral<T>::value, "T must be of integral type");

        return (a + b - 1) / b;
}

template<typename T>
inline T round_up(T i, T mod)
{
        static_assert(std::is_integral<T>::value, "T must be of integral type");

        return ((i + mod - 1) / mod) * mod;
}

template<typename T>
inline T round_down(T i, T mod)
{
        static_assert(std::is_integral<T>::value, "T must be of integral type");

        return div_up(i - mod + 1, mod) * mod;
}

#endif //EXTERNAL_SORT_UTIL_HPP
