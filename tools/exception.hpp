#pragma once
#include <exception>
#include <string>
#include <sstream>
#include <thread>

#if __BOOST_FOUND
#include <boost/stacktrace.hpp>
#include <boost/exception/all.hpp>
#endif

#include "../config.hpp"

inline
std::string quote(const std::string& s)
{
        return "'" + s + "'";
}

struct exception : public std::exception
{
        virtual void set_text(std::string&& s) = 0;
};

class common_exception : public exception
{
public:

        void set_text(std::string&& s) override
        {
                str_ = std::move(s);
        }

        const char* what() const noexcept override
        {
                return str_.c_str();
        }

protected:
        std::string str_;
};

class file_exception : public common_exception
{
public:
        file_exception() = default;

        explicit
                file_exception(std::string filename, int err)
                : filename_(std::move(filename)), err_(err)
        {}

        void set_text(std::string&& s) override
        {
                std::stringstream ss;
                ss << s << "[file: '" + filename_ + "']";
                ss << " [errno: " << strerror(err_) << "]";

                common_exception::set_text(ss.str());
        }

private:
        std::string filename_;
        int err_;
};

struct __errno_except_mod {};

extern __errno_except_mod put_errno;

template<typename Exception>
class ___throw_exception
{
public:
        template<typename... Args>
        ___throw_exception(const char* file,
                const char* fun, int line, int err, Args&&... args) noexcept
                : errno_(err)
        {
                try
                {
                        e_ = Exception(std::forward<Args>(args)...);

                        ss_ << "[" << std::this_thread::get_id() << "]"
                                << "[" << fun << "] " << " - "
                                << file << ":" << line << ": ";
                }
                catch (...)
                {
                        intr_e_ = std::current_exception();
                }
        }

        ~___throw_exception() noexcept(false)
        {
                try
                {
#if __BOOST_FOUND
                        if(IS_ENABLED(CONFIG_ENABLE_STACKTRACE))
                                ss_ << "\n" << boost::stacktrace::stacktrace();
#endif
                        e_.set_text(ss_.str());
                        throw e_;
                }
                catch (const Exception&)
                {
                        std::rethrow_exception(std::current_exception());
                }
                catch (...)
                {
                        std::throw_with_nested(std::runtime_error("Cannot throw exception"));
                }
        }

        template<typename T>
        friend ___throw_exception&& operator<<(___throw_exception&& e, T&& v) noexcept
        {
                if (e.intr_e_)
                        return std::move(e);

                try
                {
                        e.ss_ << v;
                }
                catch (...)
                {
                        e.intr_e_ = std::current_exception();
                }

                return std::move(e);
        }

        friend ___throw_exception&& operator<<(___throw_exception&& e,
                __errno_except_mod) noexcept
        {
                if (e.intr_e_)
                        return std::move(e);

                try
                {
                        e.ss_ << " [errno: " << strerror(e.errno_) << "]";
                }
                catch (...)
                {
                        e.intr_e_ = std::current_exception();
                }

                return std::move(e);
        }
private:
        std::stringstream ss_;
        std::exception_ptr intr_e_;
        int errno_;
        Exception e_;
};



#define THROW_EXCEPTION \
        ___throw_exception<common_exception>( __FILE__, __FUNCTION__, __LINE__, errno)

#define THROW_FILE_EXCEPTION(filename) \
        ___throw_exception<file_exception>( __FILE__, __FUNCTION__, __LINE__, errno, (filename), errno)

#define THROW_EXCEPTION_EX(type, ...) \
        ___throw_exception<type>(__FILE__, __FUNCTION__, __LINE__, errno, ##__VA_ARGS__)

// prints the explanatory string of an exception. If the exception is nested,
// recurses to print the explanatory of the exception it holds
void print_exception(const std::exception& e, int level = 0);
