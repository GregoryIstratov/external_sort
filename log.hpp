#pragma once

#include <iostream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <ctime>
#include <cstring>
#include <type_traits>
#include "config.hpp"
#include "tools/exception.hpp"
#include "tools/spinlock.hpp"
#include "tools/util.hpp"

namespace logging {

enum class fmt : uint32_t
{
        endl = 1,
        clock = 1 << 1,
        time = 1 << 2,
        thread = 1 << 3,
        append = 1 << 4
};

constexpr fmt operator|(fmt a, fmt b)
{
        return static_cast<fmt>(static_cast<uint32_t>(a) 
                | static_cast<uint32_t>(b));
}

constexpr fmt operator&(fmt a, fmt b)
{
        return static_cast<fmt>(static_cast<uint32_t>(a)
                & static_cast<uint32_t>(b));
}

constexpr fmt operator^(fmt a, fmt b)
{
        return static_cast<fmt>(static_cast<uint32_t>(a)
                ^ static_cast<uint32_t>(b));
}

constexpr fmt operator~(fmt x)
{
        return static_cast<fmt>(~static_cast<uint32_t>(x));
}


struct fmt_set
{
        explicit fmt_set(fmt _value) : value(_value) {}

        fmt value;
};

struct fmt_clear
{
        explicit fmt_clear(fmt _value) : value(_value) {}

        fmt value;
};

static constexpr auto default_log_format = fmt::endl | fmt::clock
                                           | fmt::time | fmt::thread;

class logger
{
public:
        logger() = delete;

        ~logger()
        {
                try
                {
                        if (test_flag(fmt_, fmt::endl))
                                oss_ << "\n";

                        std::lock_guard<std::mutex> lk(mtx_);

                        if (fos_.is_open())
                                fos_ << oss_.rdbuf();                        

                        oss_.seekg(0);

                        os_ << oss_.rdbuf();
                        os_ << std::flush;
                }
                catch (const std::exception& e)
                {
                        std::cerr << "Caught exception in ~logger(): " 
                                  << e.what() << std::endl;
                }
                catch (...)
                {
                        std::cerr << "Caught unknown exception in ~logger()" 
                                  << std::endl;
                }
        }

        logger(logger&&) = delete;
        logger(const logger&) = delete;

        logger& operator=(logger&&) = delete;
        logger& operator=(const logger&) = delete;

        static void enable_file_logging(std::string&& filename)
        {
                fos_.rdbuf()->pubsetbuf(nullptr, 0);

                fos_.open(filename, std::ios::out | std::ios::app);

                if (!fos_)
                        throw_exception("Cannot open the file '" 
                                        << filename << "': " 
                                        << strerror(errno));
        }


protected:
        explicit
        logger(const char* const type, std::ostream& os = std::cout)
                : type_(type), os_(os)
        {
        }

private:
        void _print_time()
        {
                std::time_t t = std::time(nullptr);

                /* specification says that std::localtime may not be thread-safe
                 * so we need apply a lock before calling it */
                std::unique_lock<spinlock> lk(spinlock_);
                std::tm tm = *std::localtime(&t);
                lk.unlock();

                oss_ << std::put_time(&tm, "[%H:%M:%S][%m/%d/%y]");
        }

        void _print_clock()
        {
                oss_ 
                << "[+"
                << std::fixed << std::setprecision(3)
                << (clock() / (float)CLOCKS_PER_SEC) 
                << "]";

        }

        void _print_thread_id()
        {
                oss_ << "[" << get_thread_id_str() << "]";
        }

        void _init()
        {
                init_ = true;

                if (test_flag(fmt_, fmt::append))
                        return;

                if (test_flag(fmt_, fmt::clock))
                        _print_clock();

                if (test_flag(fmt_, fmt::time))
                        _print_time();

                if (test_flag(fmt_, fmt::thread))
                        _print_thread_id();

                oss_ << "[" << type_ << "]: ";
        }

        friend logger&& operator<<(logger&& _log, fmt_set fmt)
        {
                set_flag(&_log.fmt_, fmt.value);

                return std::move(_log);
        }

        friend logger&& operator<<(logger&& _log, fmt_clear fmt)
        {
                clear_flag(&_log.fmt_, fmt.value);

                return std::move(_log);
        }

        template<typename T>
        friend logger&& operator<<(logger&& _log, T&& v)
        {
                if (!_log.init_)
                        _log._init();

                _log.oss_ << v;
                return std::move(_log);
        }

private:
        static std::mutex mtx_;
        static spinlock spinlock_;
        static std::ofstream fos_;

        const char* const type_;

        std::ostream& os_;
        std::stringstream oss_;

        fmt fmt_ = default_log_format;

        bool init_ = false;        
};



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

template<bool Enabled>
struct _info2;

template<>
struct _info2<true> : public logger
{
        _info2() : logger("INF") {}
};

template<>
struct _info2<false> {};

using info2 = _info2<CONFIG_INFO_LEVEL >= 2>;

template<typename T>
_info2<false>&& operator<<(_info2<false>&& _log, const T &)
{
        return std::move(_log);
}

template<bool Enabled>
struct _debug;

template<>
struct _debug<true> : public logger
{
        _debug() : logger("DBG") {}
};

template<>
struct _debug<false> { };

#if defined(NDEBUG)
using debug = _debug<IS_ENABLED(CONFIG_FORCE_DEBUG)>;
#else
using debug = _debug<true>;
#endif

template<typename T>
_debug<false>&& operator<<(_debug<false>&& log, const T&)
{
        return std::move(log);
}

} // namespace logging

using logging::debug;
using logging::error;
using logging::info;
using logging::info2;
