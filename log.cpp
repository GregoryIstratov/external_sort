#include "log.hpp"

namespace logging {

        std::mutex logger::mtx_;
        spinlock logger::spinlock_;
        std::ofstream logger::fos_;
}
