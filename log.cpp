#include "log.hpp"

std::mutex logger::mtx_;

std::ofstream logger::fos_;
