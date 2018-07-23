#pragma once

#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <cstring>
#include <iomanip>
#include <condition_variable>
#include <list>
#include <cassert>
#include <functional>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>

#include "settings.hpp"
#include "log.hpp"

class __myexception : public std::exception {
public:
        __myexception(const char* msg, const char* file, const char* fun, int line)
        {
                std::stringstream ss;
                ss << "THR[" << get_thread_id_str() << "]" << "[" << fun << "]: " << msg << " - " << file << ":" << line;

                str_ = ss.str();
        }

        const char* what() const noexcept override
        { return str_.c_str(); }
private:
        std::string str_;
};

#define throw_exception(msg) do { \
        std::stringstream ss; (ss << msg); \
        throw __myexception(ss.str().c_str(), \
                            __FILE__, __FUNCTION__, __LINE__); \
} while(0)

class spinlock {
public:
        void lock() {
                while (flag_.test_and_set(std::memory_order_acquire));
        }
        void unlock() {
                flag_.clear(std::memory_order_release);
        }

private:
        std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

template<typename MutexType>
class unique_guard
{
public:
        explicit
        unique_guard(std::unique_lock<MutexType>& lock, bool release_lock = true)
                : lock_(lock),
                  release_lock_(release_lock)
        {
                if(!lock_.owns_lock())
                        lock_.lock();
        }

        ~unique_guard()
        {
                if(release_lock_ && lock_.owns_lock())
                        lock_.unlock();
        }
private:
        std::unique_lock<MutexType>& lock_;
        bool release_lock_;
};

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

                if(que_.empty() && mtx_.try_lock())
                        return;

                if(mod != cmd_mod::skip_enque)
                        que_.push_back(id);

                cv_.wait(wait_lock, [this, id](){
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
                        throw_exception("error in barrier thread sync");

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

class perf_timer
{
public:
        using sc = std::chrono::seconds;
        using us = std::chrono::microseconds;
        using ms = std::chrono::milliseconds;
        using ns = std::chrono::nanoseconds;

        perf_timer() = default;

        perf_timer(const char* msg, std::function<void(void)>&& fn)
        {
                start();

                fn();

                end();

                info2() << msg << ": "
                               << std::chrono::duration_cast<std::chrono::milliseconds>(end_ - start_).count() << " ms";
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
        auto elapsed()
        {
                return std::chrono::duration_cast<T>(end_ - start_).count();
        }

private:
        std::chrono::high_resolution_clock::time_point start_, end_;
};

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

void file_write(const char* filename, const void* data, size_t size);

void gen_rnd_test_file(const char* filename, uint64_t size);

template<typename T>
void make_rnd_file_from(std::vector<T>& arr, const char* filename)
{
        std::random_device rd;
        std::mt19937 g(rd());

        std::shuffle(arr.begin(), arr.end(), g);

        file_write(filename, arr.data(), arr.size() * sizeof(T));
}

class raw_file_reader
{
public:
        explicit
        raw_file_reader(const std::string& filename)
                : filename_(filename)
        {
                is_.open(filename_, std::ios::in | std::ios::binary);

                if(!is_)
                        throw_exception("Cannot open the file '"
                                        << filename
                                        << "': "
                                        << strerror(errno));

                is_.rdbuf()->pubsetbuf(nullptr, 0);

                is_.seekg(0, std::ios::end);
                file_size_ = is_.tellg();
                is_.seekg(0, std::ios::beg);
        }

        raw_file_reader(const raw_file_reader&) = delete;
        raw_file_reader& operator=(const raw_file_reader&) = delete;


        raw_file_reader(raw_file_reader&& o) = default;
        raw_file_reader& operator=(raw_file_reader&& o) = default;

        void close()
        {
                if(is_)
                        is_.close();

                is_ = std::ifstream();
        }


        std::streamsize read(char* buff, std::streamsize size)
        {
                is_.read(buff, size);

                if(is_.bad() || (is_.bad() && !is_.eof()))
                        throw_exception("Cannot read the file '"
                                        << filename_
                                        << "': "
                                        << strerror(errno));

                std::streamsize r = is_.gcount();
                read_ += r;

                return r;
        }

        bool eof() const { return is_.eof() || read_ >= file_size_; }

        std::string filename() const { return filename_; }
        uint64_t file_size() const { return file_size_; }

private:
        std::ifstream is_;
        std::string filename_;
        uint64_t file_size_;
        uint64_t read_ = 0;
};

/* ====================================================
 * Config conditional macros.
 * Taken from the linux kernel sources, Kbuild system.
 * =====================================================
 */

#define __ARG_PLACEHOLDER_1 0,
#define __take_second_arg(__ignored, val, ...) val

/*
 * The use of "&&" / "||" is limited in certain expressions.
 * The following enable to calculate "and" / "or" with macro expansion only.
 */
#define __and(x, y)			___and(x, y)
#define ___and(x, y)			____and(__ARG_PLACEHOLDER_##x, y)
#define ____and(arg1_or_junk, y)	__take_second_arg(arg1_or_junk y, 0)

#define __or(x, y)			___or(x, y)
#define ___or(x, y)			____or(__ARG_PLACEHOLDER_##x, y)
#define ____or(arg1_or_junk, y)		__take_second_arg(arg1_or_junk 1, y)

/*
 * Helper macros to use CONFIG_ options in C/CPP expressions. Note that
 * these only work with boolean and tristate options.
 */

/*
 * Getting something that works in C and CPP for an arg that may or may
 * not be defined is tricky.  Here, if we have "#define CONFIG_BOOGER 1"
 * we match on the placeholder define, insert the "0," for arg1 and generate
 * the triplet (0, 1, 0).  Then the last step cherry picks the 2nd arg (a one).
 * When CONFIG_BOOGER is not defined, we generate a (... 1, 0) pair, and when
 * the last step cherry picks the 2nd arg, we get a zero.
 */
#define __is_defined(x)			___is_defined(x)
#define ___is_defined(val)		____is_defined(__ARG_PLACEHOLDER_##val)
#define ____is_defined(arg1_or_junk)	__take_second_arg(arg1_or_junk 1, 0)

#define IS_ENABLED(option) __is_defined(option)
