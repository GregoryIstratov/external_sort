#pragma once

#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include "settings.hpp"

inline
std::string get_thread_id_str()
{
        static const auto flags = std::ios_base::hex
                                  | std::ios_base::uppercase
                                  | std::ios_base::showbase;

        std::stringstream ss;
        ss  << std::resetiosflags(std::ios_base::dec)
            << std::setiosflags(flags)
            << std::this_thread::get_id();


        return ss.str();
}

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
        explicit
        logger(const char* type, std::ostream& os = std::cout)
                : lock_(mtx_), os_(os), fmt_guard_(os_)
        {
                os_ << "[+"
                    << std::fixed << std::setprecision(3)
                    << (clock() / (float)CLOCKS_PER_SEC) << "]"
                    << "[" << type << "]["
                    << get_thread_id_str() << "]: ";
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

#if !defined(NDEBUG) || CONFIG_FORCE_DEBUG
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