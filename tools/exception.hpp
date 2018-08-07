#pragma once
#include <exception>
#include <string>
#include <sstream>

class __myexception : public std::exception {
public:
        __myexception(const char* msg, const char* file,
                      const char* fun, int line);

        const char* what() const noexcept override;
private:
        std::string str_;
};

#define throw_exception(msg) do { \
        std::stringstream ss; (ss << msg); \
        throw __myexception(ss.str().c_str(), \
                            __FILE__, __FUNCTION__, __LINE__); \
} while(0)
