#include "exception.hpp"
#include "util.hpp"

__myexception::__myexception(const char* msg, const char* file,
                             const char* fun, int line)
{
        std::stringstream ss;
        ss << "THR[" << get_thread_id_str() << "]" << "[" << fun << "]: "
           << msg << " - " << file << ":" << line;

        str_ = ss.str();
}

const char* __myexception::what() const noexcept
{
        return str_.c_str();
}
