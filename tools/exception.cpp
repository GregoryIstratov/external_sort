#include "exception.hpp"
#include "util.hpp"
#include <iostream>

__errno_except_mod put_errno;

void print_exception(const std::exception& e, int level)
{
        std::cerr << std::string(level, ' ') << "exception: " << e.what() << '\n';
        try {
                std::rethrow_if_nested(e);
        }
        catch (const std::exception& e) {
                print_exception(e, level + 1);
        }
        catch (...) {}
}
